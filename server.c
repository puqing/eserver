#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>

#include "connection.h"
#include "server.h"

/***************************
*    Connection Manager    *
***************************/
struct conn_queue
{
	struct connection **free_conn;
	unsigned int head;
	unsigned int tail;

	unsigned int mask;
	pthread_mutex_t lock;

	struct connection *all_conn;
//	size_t number;
	size_t size;
};

#define FD_BASE 1000

static void push_conn(struct conn_queue *cm, struct connection *conn);

static struct conn_queue *create_conn_queue(size_t size, server_handler *sh)
{
	unsigned int cap;
	int i;

	struct conn_queue *cq = malloc(sizeof(struct conn_queue));

	cap = 1;
	while (cap <= size) cap <<= 1;
	cq->mask = cap - 1;
	cq->free_conn = malloc(sizeof(struct connection*) * cap);
	cq->head = cq->tail = 0;

	cq->size = size;
	cq->all_conn = allocate_connections(size);

	for (i = 0; i < size; ++i)
	{
		struct connection *conn = get_conn(cq->all_conn, i);
		init_connection(conn, FD_BASE +i, sh);
		push_conn(cq, conn);
	}

	pthread_mutex_init(&cq->lock, NULL);

	return cq;
}

static void destroy_conn_queue(struct conn_queue *cm)
{
	free(cm->free_conn);
	free(cm->all_conn);
	free(cm);
}

static struct connection *pop_conn(struct conn_queue *cm);

static void push_conn(struct conn_queue *cm, struct connection *conn)
{
	pthread_mutex_lock(&cm->lock);
	cm->free_conn[cm->tail & cm->mask]= conn;
	++cm->tail;
	pthread_mutex_unlock(&cm->lock);
}

static struct connection *pop_conn(struct conn_queue *cm)
{
	struct connection *conn;

	pthread_mutex_lock(&cm->lock);
	if (cm->head == cm->tail) {
		conn = NULL;
	} else {
		conn = cm->free_conn[cm->head & cm->mask];
		++cm->head;
	}
	pthread_mutex_unlock(&cm->lock);

	return conn;
}

#define SYSLOG_ERROR(x) syslog(LOG_ERR, "[%s:%d]%s: %s", __FILE__, __LINE__, x, strerror(errno))

/**********************
 *   server           *
 *********************/
struct server
{
	int fd;
//	server_handler handler;
	struct conn_queue *cq;
};

static int create_socket_and_bind(int port)
{
	int res, sfd;

	struct sockaddr_in addr;

	sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sfd == -1) {
		SYSLOG_ERROR("socket");
		return -1;
	}

	memset(&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	res = bind(sfd, (struct sockaddr*)&addr, sizeof addr);
	if (res == -1) {
		SYSLOG_ERROR("bind");
		return -1;
	}

	return sfd;
}

static int make_socket_non_blocking(int sfd)
{
	int flags, res;

	flags = fcntl(sfd, F_GETFL, 0);
	if (flags == -1)
	{
		SYSLOG_ERROR("fcntl");
		close(sfd);
		return -1;
	}

	flags |= O_NONBLOCK;
	res = fcntl (sfd, F_SETFL, flags);
	if (res == -1)
	{
		SYSLOG_ERROR("fcntl");
		close(sfd);
		return -1;
	}

	return 0;
}

static int shrink_socket_send_buffer(int sfd)
{
	int size;
	int res;

	size = 20;

	res = setsockopt(sfd, SOL_SOCKET, SO_SNDBUF, &size, sizeof size);
	if (res == -1) {
		perror("setsockopt");
		return -1;
	}

	return 0;
}

struct connection *accept_connection(struct server *s)
{
	struct sockaddr in_addr;
	socklen_t in_len;
	int infd;
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	int res;

	in_len = sizeof in_addr;
	infd = accept(s->fd, &in_addr, &in_len);
	if (infd == -1)
	{
		if ((errno == EAGAIN) ||
				(errno == EWOULDBLOCK))
		{
			return NULL;
		}
		else
		{
			SYSLOG_ERROR("accept");
			return NULL;
		}
	}

	res = getnameinfo (&in_addr, in_len,
			hbuf, sizeof hbuf,
			sbuf, sizeof sbuf,
			NI_NUMERICHOST | NI_NUMERICSERV);
	if (res == 0)
	{
		syslog(LOG_INFO, "[%x:%d] Accepted connection"
				"(host=%s, port=%s)\n",
				 (unsigned int)pthread_self(), infd, hbuf, sbuf);
	}

	res = make_socket_non_blocking(infd);
	if (res == -1) {
		return NULL;
	}

	struct connection *conn = pop_conn(s->cq);
	assert(conn != NULL);

	set_conn_fd(conn, infd);

	return conn;
}

#if 0
void accept_all_connection(struct server *svr)
{
	struct epoll_event event;

	while (1) {
		int infd = accept_connection(svr->fd);
		if (infd == -1) {
			break;
		}
		
/*		int res = make_socket_non_blocking(infd);
		if (res == -1) {
			break;
		}*/

/*		res = shrink_socket_send_buffer(infd);
		if (res == -1) {
			break;
		}*/

#if 0
		struct connection *conn = get_conn(g_cm, infd);
		assert(conn != NULL);
		event.data.ptr = conn;
		event.events = EPOLLIN | EPOLLET;
		res = epoll_ctl(svr->epfd, EPOLL_CTL_ADD, get_fd(conn), &event);
		if (res == -1)
		{
			SYSLOG_ERROR("epoll_ctl");
			abort ();
		}
#endif
		struct connection *conn = get_conn(g_cm, infd);
		assert(conn != NULL);
		poller_add_conn(poller, infd, conn);
	}
}
#endif

struct server *create_server(char *ip, int port, size_t max_conn_num,
		size_t recv_buf_size, size_t send_buf_size,
		server_handler *sh)
{
	int fd;
	int res;
	struct server *svr;

	fd = create_socket_and_bind(port);
	if (fd == -1)
		abort ();

	res = make_socket_non_blocking(fd);
	if (res == -1)
		abort ();

	res = listen(fd, SOMAXCONN);
	if (res == -1)
	{
		SYSLOG_ERROR("listen");
		abort ();
	}

	svr = malloc(sizeof(struct server));

	svr->fd = fd;
//	svr->handler = sh;
	svr->cq = create_conn_queue(10000, sh);

	return svr;
}

void recycle_connection(struct server *s, struct connection *conn)
{
	push_conn(s->cq, conn);
}

size_t get_conn_num(struct server *s)
{
	return s->cq->size - (s->cq->tail - s->cq->head);
}

int get_server_fd(struct server *s)
{
	return s->fd;
}


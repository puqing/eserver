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

#include <esvr.h>

#include "connmgr.h"
#include "connection.h"
#include "service.h"
#include "connmgr.h"

#define SYSLOG_ERROR(x) syslog(LOG_ERR, "[%s:%d]%s: %s", __FILE__, __LINE__, x, strerror(errno))

/**********************
 *   service           *
 *********************/
struct service
{
	int fd;
	message_handler *msg_handler;
	connection_handler *conn_handler;
	connection_close_handler *conn_close_handler;
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

#if 0
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
#endif

struct connection *accept_connection(struct service *s)
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
	set_conn_handlers(conn, s->msg_handler, \
			s->conn_close_handler);

	s->conn_handler(conn);

	return conn;
}

struct service *create_service(char *ip, int port, size_t max_conn_num,
		size_t read_buf_size, size_t write_buf_size,
		message_handler *mh, connection_handler *ch, connection_close_handler *cch)
{
	int fd;
	int res;
	struct service *svr;

	fd = create_socket_and_bind(port);
	if (fd == -1)
		abort ();

	res = make_socket_non_blocking(fd);
	if (res == -1)
		abort ();

	res = listen(fd, 8192);
	if (res == -1)
	{
		SYSLOG_ERROR("listen");
		abort ();
	}

	svr = malloc(sizeof(struct service));

	svr->fd = fd;
	svr->msg_handler = mh;
	svr->conn_handler = ch;
	svr->conn_close_handler = cch;
	svr->cq = create_conn_queue(max_conn_num, read_buf_size, write_buf_size);

	return svr;
}

size_t get_conn_num(struct service *s)
{
	return get_active_conn_num(s->cq);
//	return s->cq->size - (s->cq->tail - s->cq->head);
}

int get_service_fd(struct service *s)
{
	return s->fd;
}

message_handler *get_handler(struct service *s)
{
	return s->msg_handler;
}


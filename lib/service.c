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

/**********************
 *   service           *
 *********************/
struct service
{
	int fd;
	connection_handler *conn_handler;
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
		syslog(LOG_DEBUG, "[%x:%d] Accepted connection"
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

	s->conn_handler(conn);

	return conn;
}

struct service *create_service(char *ip, int port, struct conn_queue *cq, connection_handler *ch)
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

	res = listen(fd, SOMAXCONN);
	if (res == -1)
	{
		SYSLOG_ERROR("listen");
		abort ();
	}

	svr = malloc(sizeof(struct service));

	svr->fd = fd;
	svr->conn_handler = ch;
	svr->cq = cq;

	return svr;
}

size_t get_conn_num(struct service *s)
{
	return get_active_conn_num(s->cq);
}

int get_service_fd(struct service *s)
{
	return s->fd;
}


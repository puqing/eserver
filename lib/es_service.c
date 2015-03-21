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

#include "es_connmgr.h"
#include "es_conn.h"
#include "es_service.h"
#include "es_connmgr.h"

/**********************
 *   es_service           *
 *********************/
struct es_service
{
	int fd;
	es_connhandler *conn_handler;
	struct es_connmgr *cq;
};

static int create_socket_and_bind(int port)
{
	int res, sfd;
	int option = 1;

	struct sockaddr_in addr;

	sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sfd == -1) {
		SYSLOG_ERROR("socket");
		return -1;
	}

	res = setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof option);
	if (res == -1) {
		perror("setsockopt");
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

struct es_conn *accept_connection(struct es_service *s)
{
	struct sockaddr in_addr;
	socklen_t in_len;
	int infd;
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	int res;
	struct es_conn *conn;

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
		syslog(LOG_DEBUG, "[%x:%d] Accepted es_conn"
				"(host=%s, port=%s)\n",
				 (unsigned int)pthread_self(), infd, hbuf, sbuf);
	}

	res = make_socket_non_blocking(infd);
	if (res == -1) {
		return NULL;
	}

	conn = pop_conn(s->cq);
	assert(conn != NULL);

	set_conn_fd(conn, infd);

	s->conn_handler(conn);

	return conn;
}

struct es_service *es_newservice(char *ip, int port, struct es_connmgr *cq, es_connhandler *ch)
{
	int fd;
	int res;
	struct es_service *svr;

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

	svr = malloc(sizeof(struct es_service));

	svr->fd = fd;
	svr->conn_handler = ch;
	svr->cq = cq;

	return svr;
}

size_t get_conn_num(struct es_service *s)
{
	return get_active_conn_num(s->cq);
}

int get_service_fd(struct es_service *s)
{
	return s->fd;
}


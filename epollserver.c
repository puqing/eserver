#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <syslog.h>
#include <assert.h>

#include "epollserver.h"
#include "connection.h"

#define SYSLOG_ERROR(x) syslog(LOG_ERR, "[%s:%d]%s: %s", __FILE__, __LINE__, x, strerror(errno))

struct epollserver {
	int fd;
	int epfd;
};

struct epollserver *g_es;

static int create_and_bind (int port)
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

static int make_socket_non_blocking (int sfd)
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

static int accept_connection(int sfd)
{
	struct sockaddr in_addr;
	socklen_t in_len;
	int infd;
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	int res;

	in_len = sizeof in_addr;
	infd = accept(sfd, &in_addr, &in_len);
	if (infd == -1)
	{
		if ((errno == EAGAIN) ||
				(errno == EWOULDBLOCK))
		{
			return -1;
		}
		else
		{
			SYSLOG_ERROR("accept");
			return -1;
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

	return infd;
}

void accept_all_connection(struct epollserver *es)
{
	struct epoll_event event;

	while (1) {
		int infd = accept_connection(es->fd);
		if (infd == -1) {
			break;
		}
		
		int res = make_socket_non_blocking(infd);
		if (res == -1) {
			break;
		}

		res = shrink_socket_send_buffer(infd);
		if (res == -1) {
			break;
		}

		struct connection *conn = get_conn(g_cm, infd);
		assert(conn != NULL);
		event.data.ptr = conn;
		event.events = EPOLLIN | EPOLLET;
		res = epoll_ctl(es->epfd, EPOLL_CTL_ADD, get_fd(conn), &event);
		if (res == -1)
		{
			SYSLOG_ERROR("epoll_ctl");
			abort ();
		}
	}
}

struct epollserver * init_server(int port)
{
	int sfd;
	int res;
	struct epollserver *es;
	struct epoll_event event;

	sfd = create_and_bind(port);
	if (sfd == -1)
		abort ();

	res = make_socket_non_blocking(sfd);
	if (res == -1)
		abort ();

	res = listen(sfd, SOMAXCONN);
	if (res == -1)
	{
		SYSLOG_ERROR("listen");
		abort ();
	}

	g_es = es = malloc(sizeof(struct epollserver));

	es->fd = sfd;

	es->epfd = epoll_create1 (0);
	if (es->epfd == -1)
	{
		SYSLOG_ERROR("epoll_create");
		abort ();
	}

	event.data.ptr = es;
	event.events = EPOLLIN | EPOLLET;
	res = epoll_ctl (es->epfd, EPOLL_CTL_ADD, sfd, &event);
	if (res == -1)
	{
		SYSLOG_ERROR("epoll_ctl");
		abort ();
	}

	return es;
}

void stop_server(struct epollserver *es)
{
	return;
}

int rearm_out(struct epollserver *es, struct connection *conn, int rearm)
{
	struct epoll_event event;
	int res;

	syslog(LOG_INFO, "[%x:%x:%d:] Rearm out %d", (unsigned int)conn, (unsigned int)pthread_self(), get_fd(conn), rearm);
	event.data.ptr = conn;
	event.events = EPOLLET | EPOLLIN | (rearm?EPOLLOUT:0);
	res = epoll_ctl(es->epfd, EPOLL_CTL_MOD, get_fd(conn), &event);
	if (res == -1)
	{
		SYSLOG_ERROR("epoll_ctl");
		return -1;
	} else {
		return 0;
	}
}

int get_epfd(struct epollserver *es)
{
	return es->epfd;
}

int get_sfd(struct epollserver *es)
{
	return es->fd;
}


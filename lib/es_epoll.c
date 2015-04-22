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
#include <sys/epoll.h>

#include <esvr.h>

#include "es_connmgr.h"
#include "es_conn.h"
#include "es_service.h"
#include "es_epoll.h"

#define MAX_SERVICE_NUM 16

int es_newepfd()
{
	int fd;

	fd = epoll_create1(0);
	if (fd == -1) {
		SYSLOG_ERROR("epoll_create");
		exit(1);
	}

	return fd;
}

int es_addservice(int epfd, struct es_service *s)
{
	struct epoll_event event;
	int res;

	assert(((long)s & 0x1) == 0 && "should be aligned");

	event.data.ptr = service_to_ptr(s);
	event.events = EPOLLIN | EPOLLET;
	res = epoll_ctl(epfd, EPOLL_CTL_ADD, get_service_fd(s), &event);
	if (res == -1)
	{
		SYSLOG_ERROR("epoll_ctl");
		return -1;
	}

	return 0;
}

int es_addconn(int epfd, struct es_conn *conn, int client_side)
{
	struct epoll_event event;
	int res;

	assert(((long)conn & 0x1) == 0 && "should be aligned");

	event.data.ptr = conn;
	event.events = EPOLLIN | EPOLLET;
	res = epoll_ctl(epfd, EPOLL_CTL_ADD, get_conn_fd(conn), &event);
	if (res == -1)
	{
		SYSLOG_ERROR("epoll_ctl");
		return -1;
	}

	set_conn_epfd(conn, epfd);

	return 0;
}

int rearm_out(int epfd, struct es_conn *conn, int rearm)
{
	struct epoll_event event;
	int res;

	LOG_CONN(LOG_DEBUG, "Rearm out %d", rearm);

	assert(((long)conn & 0x1) == 0 && "should be aligned");

	event.data.ptr = conn;
	event.events = EPOLLET | EPOLLIN | (rearm?EPOLLOUT:0);
	res = epoll_ctl(epfd, EPOLL_CTL_MOD, get_conn_fd(conn), &event);
	if (res == -1)
	{
		SYSLOG_ERROR("epoll_ctl");
		return -1;
	} else {
		return 0;
	}
}


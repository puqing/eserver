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
#include "es_poller.h"

#define MAX_SERVICE_NUM 16

struct es_poller
{
	int fd;
	struct es_service *services[MAX_SERVICE_NUM];
	unsigned int svr_num;
};

struct es_poller *es_newpoller()
{
	struct es_poller *p = malloc(sizeof(struct es_poller));
	p->fd = epoll_create1 (0);
	if (p->fd == -1)
	{
		SYSLOG_ERROR("epoll_create");
		abort ();
	}

	p->svr_num = 0;

	return p;
}

void es_addservice(struct es_poller *p, struct es_service *s)
{
	struct epoll_event event;

	p->services[p->svr_num++] = s;

	event.data.ptr = s;
	event.events = EPOLLIN | EPOLLET;
	int res = epoll_ctl(p->fd, EPOLL_CTL_ADD, get_service_fd(s), &event);
	if (res == -1)
	{
		SYSLOG_ERROR("epoll_ctl");
		abort ();
	}
}

/*
 * TODO: optimize search
 */
struct es_service *find_service(struct es_poller *p, void *s)
{
	int i;
	for (i = 0; i < p->svr_num; ++i) {
		if (p->services[i] == s) {
			return p->services[i];
		}
	}

	return NULL;
}

void es_addconn(struct es_poller *p, struct es_conn *conn)
{
	struct epoll_event event;

	event.data.ptr = conn;
	event.events = EPOLLIN | EPOLLET;
	int res = epoll_ctl(p->fd, EPOLL_CTL_ADD, get_conn_fd(conn), &event);
	if (res == -1)
	{
		SYSLOG_ERROR("epoll_ctl");
		abort ();
	}

	set_conn_poller(conn, p);
}

int get_poller_fd(struct es_poller *p)
{
	return p->fd;
}

int rearm_out(struct es_poller *p, struct es_conn *conn, int rearm)
{
	struct epoll_event event;
	int res;

	LOG_CONN(LOG_DEBUG, "Rearm out %d", rearm);
	event.data.ptr = conn;
	event.events = EPOLLET | EPOLLIN | (rearm?EPOLLOUT:0);
	res = epoll_ctl(p->fd, EPOLL_CTL_MOD, get_conn_fd(conn), &event);
	if (res == -1)
	{
		SYSLOG_ERROR("epoll_ctl");
		return -1;
	} else {
		return 0;
	}
}

int rearm_in(struct es_poller *p, struct es_conn *conn, int rearm)
{
	struct epoll_event event;
	int res;

	LOG_CONN(LOG_DEBUG, "Rearm in %d", rearm);
	event.data.ptr = conn;
	event.events = EPOLLET | EPOLLIN;
	if (rearm) {
		res = epoll_ctl(p->fd, EPOLL_CTL_ADD, get_conn_fd(conn), &event);
	} else {
		res = epoll_ctl(p->fd, EPOLL_CTL_DEL, get_conn_fd(conn), NULL);
	}
	if (res == -1)
	{
		SYSLOG_ERROR("epoll_ctl");
		return -1;
	} else {
		return 0;
	}
}

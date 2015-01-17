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

#include "connmgr.h"
#include "connection.h"
#include "service.h"
#include "poller.h"

#define MAX_SERVICE_NUM 16

struct poller
{
	int fd;
	struct service *services[MAX_SERVICE_NUM];
	unsigned int svr_num;
};

#define SYSLOG_ERROR(x) syslog(LOG_ERR, "[%s:%d]%s: %s", __FILE__, __LINE__, x, strerror(errno))

struct poller *create_poller()
{
	struct poller *p = malloc(sizeof(struct poller));
	p->fd = epoll_create1 (0);
	if (p->fd == -1)
	{
		SYSLOG_ERROR("epoll_create");
		abort ();
	}

	p->svr_num = 0;

	return p;
}

void add_service(struct poller *p, struct service *s)
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
struct service *find_service(struct poller *p, void *s)
{
	int i;
	for (i = 0; i < p->svr_num; ++i) {
		if (p->services[i] == s) {
			return p->services[i];
		}
	}

	return NULL;
}

void add_connection(struct poller *p, struct connection *conn)
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

int get_poller_fd(struct poller *p)
{
	return p->fd;
}

int rearm_out(struct poller *p, struct connection *conn, int rearm)
{
	struct epoll_event event;
	int res;

	syslog(LOG_INFO, "[%lx:%lx:%d:] Rearm out %d", (uint64_t)conn, (uint64_t)pthread_self(), get_conn_fd(conn), rearm);
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


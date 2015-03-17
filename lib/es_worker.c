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

#include <esvr.h>

#include "es_connmgr.h"
#include "es_conn.h"
#include "es_service.h"
#include "es_poller.h"

#define MAXEVENTS 64

static pthread_key_t g_key;
static pthread_once_t g_key_once = PTHREAD_ONCE_INIT;

struct es_worker {
	struct es_poller *p;
	pthread_t tid;
	void *data;
};

static void *work(void *data)
{
	struct epoll_event *events;

	struct es_worker *w = (struct es_worker*)data;

	int res = pthread_setspecific(g_key, w->data);
	assert(res == 0);

	int i;

	events = (struct epoll_event*)calloc(MAXEVENTS, sizeof *events);

	while(1)
	{
		syslog(LOG_DEBUG, "Begin read poll 0x%lx: %d:", pthread_self(), get_poller_fd(w->p));
		int n = epoll_wait(get_poller_fd(w->p), events, MAXEVENTS, -1);
		for (i = 0; i < n; ++i) {
			/*
			syslog(LOG_DEBUG, "Read poll %x: %d :%d: %d %d", (unsigned int)pthread_self(), i,
			(events[i].data.ptr==w->p)?get_sfd(w->p):get_fd((struct es_conn*)events[i].data.ptr),
					events[i].events & EPOLLIN, events[i].events & EPOLLOUT);*/ // TODO
		}
		for (i = 0; i < n; ++i)
		{
			if ((events[i].events & EPOLLERR) ||
				(events[i].events & EPOLLHUP) ||
				!((events[i].events & EPOLLIN) || (events[i].events & EPOLLOUT)))
			{
				if (find_service(w->p, events[i].data.ptr)) {
					syslog(LOG_ERR, "%s:%d: events = 0x%x", "epoll error on listening fd", get_service_fd((struct es_service*)events[i].data.ptr), events[i].events);
				} else {
					syslog(LOG_ERR, "%s:%d: events = 0x%x", "epoll error on working fd", get_conn_fd((struct es_conn*)events[i].data.ptr), events[i].events);
//					close_connection((struct es_conn*)events[i].data.ptr);
				}
			}
			if (find_service(w->p, events[i].data.ptr)) {
//				accept_all_connection(w->p);
				struct es_conn *conn;
				while (NULL != (conn = accept_connection((struct es_service*)events[i].data.ptr))) {
					es_addconn(w->p, conn);
				}
			} else if (events[i].events & EPOLLIN) {
				read_data((struct es_conn*)events[i].data.ptr);
			} else if (events[i].events & EPOLLOUT) {
				send_buffered_data(((struct es_conn*)events[i].data.ptr), 0);
			} else {
				syslog(LOG_INFO, "%s:%d: events = 0x%x", "epoll event neither IN nor OUT", get_conn_fd((struct es_conn*)events[i].data.ptr), events[i].events);
			}
		}
	}

	free(events);
	return NULL;
}

static void make_key()
{
	pthread_key_create(&g_key, NULL);
}

struct es_worker *es_newworker(struct es_poller *p, void *data)
{
	struct es_worker *w = malloc(sizeof(struct es_worker));
	w->p = p;
	w->data = data;
	pthread_once(&g_key_once, make_key);
	pthread_create(&w->tid, NULL, work, (void*)w);
	syslog(LOG_DEBUG, "thread 0x%x created\n", (unsigned int)w->tid);

	return w;
}

void *es_getworkerdata()
{
	void *res = pthread_getspecific(g_key);
	assert(res);
	return res;
}


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

#include "connection.h"
#include "service.h"
#include "poller.h"
#include "worker.h"

#define MAXEVENTS 64

//pthread_key_t g_thread_key;

struct worker {
	struct poller *p;
	void *handle;
	pthread_t tid;
};

static void *work(void *data)
{
	struct epoll_event *events;

	struct worker *w = (struct worker*)data;

//	pthread_setspecific(g_thread_key, w->handle);

	int i;

	events = (struct epoll_event*)calloc(MAXEVENTS, sizeof *events);

	while(1)
	{
		syslog(LOG_INFO, "Begin read poll 0x%x: %d:", (unsigned int)pthread_self(), get_poller_fd(w->p));
//		printf("%x ((( get_specific = %x\n", pthread_self(), pthread_getspecific(g_thread_key));
		int n = epoll_wait(get_poller_fd(w->p), events, MAXEVENTS, -1);
//		printf("%x ))) get_specific = %x\n", pthread_self(), pthread_getspecific(g_thread_key));
		for (i = 0; i < n; ++i) {
			/*
			syslog(LOG_INFO, "Read poll %x: %d :%d: %d %d", (unsigned int)pthread_self(), i,
			(events[i].data.ptr==w->p)?get_sfd(w->p):get_fd((struct connection*)events[i].data.ptr),
					events[i].events & EPOLLIN, events[i].events & EPOLLOUT);*/ // TODO
		}
		for (i = 0; i < n; ++i)
		{
			if ((events[i].events & EPOLLERR) ||
				(events[i].events & EPOLLHUP) ||
				!((events[i].events & EPOLLIN) || (events[i].events & EPOLLOUT)))
			{
				if (find_service(w->p, events[i].data.ptr)) {
					syslog(LOG_INFO, "%s:%d: events = 0x%x", "epoll error on listening fd", get_service_fd((struct service*)events[i].data.ptr), events[i].events);
				} else {
					syslog(LOG_INFO, "%s:%d: events = 0x%x", "epoll error on working fd", get_conn_fd((struct connection*)events[i].data.ptr), events[i].events);
//					close_connection((struct connection*)events[i].data.ptr);
				}
			}
			if (find_service(w->p, events[i].data.ptr)) {
//				accept_all_connection(w->p);
				struct connection *conn;
				while (NULL != (conn = accept_connection((struct service*)events[i].data.ptr))) {
					add_connection(w->p, conn);
				}
			} else if (events[i].events & EPOLLIN) {
				read_data((struct connection*)events[i].data.ptr, w->handle);
			} else {
				assert(events[i].events & EPOLLOUT);
				send_buffered_data(((struct connection*)events[i].data.ptr), 0);
			}
		}
	}

	free(events);
	return NULL;
}

struct worker *create_worker(struct poller *p, void *handle)
{
	struct worker *w = malloc(sizeof(struct worker));
	w->p = p;
	w->handle = handle;
	pthread_create(&w->tid, NULL, work, (void*)w);
//	pthread_key_create(&g_thread_key, NULL);
	syslog(LOG_INFO, "thread 0x%x created\n", (unsigned int)w->tid);

	return w;
}


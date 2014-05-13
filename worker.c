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

#include "worker.h"
#include "epollserver.h"
#include "connection.h"

#define MAXEVENTS 64

struct worker {
	struct epollserver *es;
	pthread_t tid;
};

static void *work(void *data)
{
	struct epoll_event *events;

	struct worker *w = (struct worker*)data;

	int i;

	events = (struct epoll_event*)calloc(MAXEVENTS, sizeof *events);

	while(1)
	{
		syslog(LOG_INFO, "Begin read poll %x: %d:", (unsigned int)pthread_self(), get_epfd(w->es));
		int n = epoll_wait(get_epfd(w->es), events, MAXEVENTS, -1);
		for (i = 0; i < n; ++i) {
			syslog(LOG_INFO, "Read poll %x: %d :%d: %d %d", (unsigned int)pthread_self(), i,
			(events[i].data.ptr==w->es)?get_sfd(w->es):get_fd((struct connection*)events[i].data.ptr),
					events[i].events & EPOLLIN, events[i].events & EPOLLOUT);
		}
		for (i = 0; i < n; ++i)
		{
			if ((events[i].events & EPOLLERR) ||
				(events[i].events & EPOLLHUP) ||
				!((events[i].events & EPOLLIN) || (events[i].events & EPOLLOUT)))
			{
				if (w->es == events[i].data.ptr) {
					syslog(LOG_INFO, "%s:%d: events = %x", "epoll error on listening fd", get_sfd(w->es), events[i].events);
				} else {
					syslog(LOG_INFO, "%s:%d: events = %x", "epoll error on working fd", get_fd((struct connection*)events[i].data.ptr), events[i].events);
//					close_connection((struct connection*)events[i].data.ptr);
				}
			}
			if (w->es == events[i].data.ptr) {
				accept_all_connection(w->es);
			} else if (events[i].events & EPOLLIN) {
				read_data((struct connection*)events[i].data.ptr);
			} else {
				assert(events[i].events & EPOLLOUT);
				send_buffered_data(((struct connection*)events[i].data.ptr), 0);
			}
		}
	}

	free(events);
	return NULL;
}

struct worker *create_worker(struct epollserver *es)
{
	struct worker *w = malloc(sizeof(struct worker));
	w->es = es;
	pthread_create(&w->tid, NULL, work, (void*)w);
	syslog(LOG_INFO, "thread %x created\n", (unsigned int)w->tid);

	return w;
}


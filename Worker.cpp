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

#include "Worker.h"
#include "EpollServer.h"
#include "ObjectQueue.h"
#include "Connection.h"

#define MAXEVENTS 64

void *Worker::run()
{
	struct epoll_event *events;

	events = (struct epoll_event*)calloc(MAXEVENTS, sizeof *events);

	while(1)
	{
		syslog(LOG_INFO, "Begin read poll %x: %d:", (unsigned int)pthread_self(), mES->mEPFD);
		int n = epoll_wait(mES->mEPFD, events, MAXEVENTS, -1);
		for ( int i = 0; i < n; ++i) {
			syslog(LOG_INFO, "Read poll %x: %d :%d: %d %d", (unsigned int)pthread_self(), i,
			(events[i].data.ptr==mES)?mES->mFD:((Connection*)events[i].data.ptr)->getFD(),
					events[i].events & EPOLLIN, events[i].events & EPOLLOUT);
		}
		for (int i = 0; i < n; ++i)
		{
			if ((events[i].events & EPOLLERR) ||
				(events[i].events & EPOLLHUP) ||
				!((events[i].events & EPOLLIN) || (events[i].events & EPOLLOUT)))
			{
				if (mES == events[i].data.ptr) {
					syslog(LOG_INFO, "%s:%d: events = %x", "epoll error on listening fd", mES->mFD, events[i].events);
				} else {
					syslog(LOG_INFO, "%s:%d: events = %x", "epoll error on working fd", ((Connection*)events[i].data.ptr)->getFD(), events[i].events);
					((Connection*)events[i].data.ptr)->closeConnection();
				}
			} else if (mES == events[i].data.ptr) {
				mES->acceptAllConnection();
			} else if (events[i].events & EPOLLIN) {
				((Connection*)events[i].data.ptr)->readData();
			} else {
				assert(events[i].events & EPOLLOUT);
				((Connection*)events[i].data.ptr)->sendBufferedData(false);
			}
		}
	}

	free(events);
	return NULL;
}

int Worker::start(EpollServer *es, int thread_number)
{
	pthread_t *tid;

	tid = (pthread_t*)malloc(thread_number * sizeof(*tid));

	for (int i = 0; i < thread_number; ++i) {
		Worker *w = new Worker(es);
		pthread_create(&tid[i], NULL, Worker::staticRun, (void*)w);
		syslog(LOG_INFO, "thread %x created\n", (unsigned int)tid[i]);
	}

	return 0;
}


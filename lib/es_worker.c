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
#include <signal.h>

#include <esvr.h>

#include "es_connmgr.h"
#include "es_conn.h"
#include "es_service.h"
#include "es_poller.h"

static unsigned int g_workingnum = 0;
static unsigned int g_syncnum = 0;

#define MAX_SYNC_HDLR_NUM 256

es_workerhandler *g_synchdlr[MAX_SYNC_HDLR_NUM];
static void *g_syncparam[MAX_SYNC_HDLR_NUM];
static unsigned int g_handler_counter = 0;

pthread_cond_t g_synccond;
pthread_mutex_t g_synclock;

int es_syncworkers(es_workerhandler *hdlr, void *data)
{
	if (g_handler_counter >= MAX_SYNC_HDLR_NUM) {
		return -1;
	}

	pthread_mutex_lock(&g_synclock);

	g_synchdlr[g_handler_counter] = hdlr;
	g_syncparam[g_handler_counter++] = data;

	g_syncnum = g_workingnum;

	pthread_mutex_unlock(&g_synclock);

	return 0;
}

static inline void checksync(void)
{
	int i;

	if (g_syncnum == 0) {
		return;
	}

	printf("g_syncnum, g_workingnum: %d, %d\n", g_syncnum, g_workingnum);

	pthread_mutex_lock(&g_synclock);
	--g_syncnum;
	--g_workingnum;
	while (g_syncnum > 0) {
		pthread_cond_wait(&g_synccond, &g_synclock);
	}
	++g_workingnum;
	for (i = 0; i < g_handler_counter; ++i) {
		(*g_synchdlr[i])(g_syncparam[i]);
	}
	g_handler_counter = 0;
	pthread_mutex_unlock(&g_synclock);

	if (g_workingnum == 1) {
		pthread_cond_broadcast(&g_synccond);
	}
}

static pthread_key_t g_key;
static pthread_once_t g_key_once = PTHREAD_ONCE_INIT;

#define MAXEVENTS 64

struct es_worker {
	struct es_poller *p;
	pthread_t tid;
	void *data;
};

static void *work(void *data)
{
	sigset_t set;
	sigfillset(&set);
	int res = pthread_sigmask(SIG_BLOCK, &set, NULL);
	assert(res == 0);

	struct es_worker *w = (struct es_worker*)data;

	res = pthread_setspecific(g_key, w->data);
	assert(res == 0);

	struct epoll_event *events;

	events = (struct epoll_event*)calloc(MAXEVENTS, sizeof *events);

	while(1)
	{
		int i;
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
			checksync();
		}
	}

	free(events);
	return NULL;
}

static void make_key()
{
	pthread_key_create(&g_key, NULL);
	pthread_cond_init(&g_synccond, NULL);
	pthread_mutex_init(&g_synclock, NULL);
}

/* g_synclock should have been initialized */
static inline void inc_workingnum()
{
	pthread_mutex_lock(&g_synclock);
	++g_workingnum;
	pthread_mutex_unlock(&g_synclock);
}

struct es_worker *es_newworker(struct es_poller *p, void *data)
{
	struct es_worker *w = malloc(sizeof(struct es_worker));
	w->p = p;
	w->data = data;
	pthread_once(&g_key_once, make_key);
	pthread_create(&w->tid, NULL, work, (void*)w);
	syslog(LOG_DEBUG, "thread 0x%x created\n", (unsigned int)w->tid);

	inc_workingnum();

	return w;
}

void *es_getworkerdata()
{
	void *res = pthread_getspecific(g_key);
	assert(res);
	return res;
}


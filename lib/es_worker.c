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

// Better be no less than MAXEVENTS * thread_num
#define EVENTSTACKSIZE 1024

struct epoll_event g_eventstack[EVENTSTACKSIZE];
struct epoll_event *g_eventend = g_eventstack;
pthread_mutex_t g_eventstacklock;

static void push_events(struct epoll_event *e, int num)
{
	assert(num >= 0);
	assert(g_eventend + num < g_eventstack + EVENTSTACKSIZE);

	pthread_mutex_lock(&g_eventstacklock);
	memcpy(g_eventend, e, num * sizeof(*e));
	g_eventend += num;
	pthread_mutex_unlock(&g_eventstacklock);
}

static int pop_events(struct epoll_event *e, int num)
{
	int n;

	assert(g_eventend >= g_eventstack);

	pthread_mutex_lock(&g_eventstacklock);
	n = g_eventend - g_eventstack;
	n = (n < num)?n:num;
	memcpy(e, g_eventend - n, n * sizeof(*e));
	g_eventend -= n;
	pthread_mutex_unlock(&g_eventstacklock);

	return n;
}

#define MAXEVENTS 64

struct es_worker {
	struct es_poller *p;
	pthread_t tid;
	void *data;

	struct epoll_event events[MAXEVENTS];
	struct epoll_event *e;
	int evnum;
};

static void unload_events(struct es_worker *w)
{
	push_events(w->e+1, w->events + w->evnum - w->e - 1);
	w->e = w->events;
	w->evnum = 0;
}

static void load_events(struct es_worker *w)
{
	// currently only load events for empty event sets
	assert(w->e == w->events);
	assert(w->evnum == 0);

	w->evnum += pop_events(w->e, MAXEVENTS - w->evnum);
}

static int g_workingnum = 0;
static int g_syncnum = 0;

pthread_cond_t g_synccond;
pthread_mutex_t g_synclock;

void es_syncworkers(int syncnum)
{
	int i, n;

	assert(g_workingnum >= 0);
	assert(syncnum >= 0);

	n = g_workingnum;

	g_syncnum = syncnum;

	for (i = n; i <syncnum; ++i) {
		pthread_cond_signal(&g_synccond);
	}
}

static inline void checksync(struct es_worker *w)
{
	assert(g_workingnum > 0);
	assert(g_syncnum >= 0);

	if (g_syncnum == g_workingnum) {
		return;
	}

	syslog(LOG_INFO, "g_syncnum, g_workingnum: %d, %d\n", g_syncnum, g_workingnum);

	pthread_mutex_lock(&g_synclock);
	if (g_workingnum  > g_syncnum) {
		unload_events(w);
		while (g_workingnum > g_syncnum) {
			--g_workingnum;
			pthread_cond_wait(&g_synccond, &g_synclock);
			++g_workingnum;
		}
	} else if (g_workingnum < g_syncnum) {
		int i, n;
		n = g_workingnum;
		for (i = n; i < g_syncnum; ++i) {
			pthread_cond_signal(&g_synccond);
		}
	} else {
		// in race condition this can be true
	}
	pthread_mutex_unlock(&g_synclock);
}

static pthread_key_t g_key;
static pthread_once_t g_key_once = PTHREAD_ONCE_INIT;

static void disable_signals()
{
	int res;
	sigset_t set;
	sigfillset(&set);
	res = pthread_sigmask(SIG_BLOCK, &set, NULL);
	assert(res == 0);
}

static void clear_events(struct es_worker *w)
{
	w->e = w->events;
	w->evnum = 0;
}

static void log_exceptional_events(uint32_t events, struct es_poller *p, void *ptr)
{
	if ((events & EPOLLERR) ||
		(events & EPOLLHUP) ||
		!((events & EPOLLIN) || (events & EPOLLOUT)))
	{
		if (find_service(p, ptr)) {
			syslog(LOG_ERR, "%s:%d: events = 0x%x", "epoll error on listening fd", get_service_fd((struct es_service*)ptr), events);
		} else {
			syslog(LOG_ERR, "%s:%d: events = 0x%x", "epoll error on working fd", get_conn_fd((struct es_conn*)ptr), events);
		}
	}
}

static void process_events(struct es_worker *w)
{
	for (w->e = &w->events[0]; w->e < &w->events[w->evnum]; ++w->e)
	{
		struct epoll_event *e;
		e = w->e;

		log_exceptional_events(w->e->events, w->p, e->data.ptr);

		if (find_service(w->p, e->data.ptr)) {
			struct es_conn *conn;
			while (NULL != (conn = accept_connection((struct es_service*)e->data.ptr))) {
				es_addconn(w->p, conn);
			}
		} else if (e->events & EPOLLIN) {
			read_data((struct es_conn*)e->data.ptr);
		} else if (e->events & EPOLLOUT) {
			send_buffered_data(((struct es_conn*)e->data.ptr), 0);
		} else {
			syslog(LOG_INFO, "%s:%d: events = 0x%x", "epoll event neither IN nor OUT", get_conn_fd((struct es_conn*)e->data.ptr), e->events);
		}

		checksync(w);
		if (w->evnum == 0) { // events unloaded
			break;
		}
	}

	clear_events(w);
}

static void *work(void *data)
{
	int res;
	struct es_worker *w;

	disable_signals();

	w = (struct es_worker*)data;

	res = pthread_setspecific(g_key, w);
	assert(res == 0);

	clear_events(w);

	while(1)
	{
		load_events(w);

		if (w->evnum == 0) {
			syslog(LOG_DEBUG, "Begin read poll 0x%lx: %d:",
					pthread_self(), get_poller_fd(w->p));
			w->evnum = epoll_wait(get_poller_fd(w->p), w->events, MAXEVENTS, -1);
			syslog(LOG_DEBUG, "%d events returned", w->evnum);
		}

		process_events(w);
	}

	return NULL;
}

static void make_key()
{
	pthread_key_create(&g_key, NULL);
	pthread_cond_init(&g_synccond, NULL);
	pthread_mutex_init(&g_synclock, NULL);
	pthread_mutex_init(&g_eventstacklock, NULL);
}

/* g_synclock should have been initialized */
static inline void inc_workernum()
{
	pthread_mutex_lock(&g_synclock);
	++g_workingnum;
	++g_syncnum;
	pthread_mutex_unlock(&g_synclock);
}

int es_getworkingnum(void)
{
	return g_workingnum;
}

struct es_worker *es_newworker(struct es_poller *p, void *data)
{
	struct es_worker *w = malloc(sizeof(struct es_worker));
	w->p = p;
	w->data = data;
	pthread_once(&g_key_once, make_key);
	pthread_create(&w->tid, NULL, work, (void*)w);
	syslog(LOG_DEBUG, "thread 0x%x created\n", (unsigned int)w->tid);

	inc_workernum();

	return w;
}

void *es_getworkerdata()
{
	struct es_worker *w = pthread_getspecific(g_key);
	assert(w);
	return w->data;
}


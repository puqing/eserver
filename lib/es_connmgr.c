#include <syslog.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

#include <esvr.h>

#include "es_connmgr.h"
#include "es_service.h"
#include "es_conn.h"

/***************************
*    Connection Manager    *
***************************/
struct es_connmgr
{
	struct es_conn **free_conn;
	unsigned int head;
	unsigned int tail;

	unsigned int mask;
	pthread_mutex_t lock;

	struct es_conn *all_conn;
//	size_t number;
	size_t size;
};

void push_conn(struct es_connmgr *cq, struct es_conn *conn)
{
	pthread_mutex_lock(&cq->lock);
	cq->free_conn[cq->tail & cq->mask]= conn;
	++cq->tail;
	pthread_mutex_unlock(&cq->lock);
}

struct es_conn *pop_conn(struct es_connmgr *cq)
{
	struct es_conn *conn;

	pthread_mutex_lock(&cq->lock);
	if (cq->head == cq->tail) {
		conn = NULL;
	} else {
		conn = cq->free_conn[cq->head & cq->mask];
		++cq->head;
	}
	pthread_mutex_unlock(&cq->lock);

	return conn;
}

struct es_conn *get_conn_set_fd(struct es_connmgr *cq, int fd)
{
	struct es_conn *conn;
	conn = pop_conn(cq);
	assert(conn != NULL);
	set_conn_fd(conn, fd);
	return conn;
}

struct es_connmgr *es_newconnmgr(int fd_base, size_t size, size_t read_buf_size, size_t write_buf_size)
{
	unsigned int cap;
	int i;

	struct es_connmgr *cq = malloc(sizeof(struct es_connmgr));

	cap = 1;
	while (cap <= size) cap <<= 1;
	cq->mask = cap - 1;
	cq->free_conn = malloc(sizeof(struct es_conn*) * cap);
	cq->head = cq->tail = 0;

	cq->size = size;
	cq->all_conn = allocate_connections(size);

	pthread_mutex_init(&cq->lock, NULL);

	for (i = 0; i < size; ++i)
	{
		struct es_conn *conn = get_conn(cq->all_conn, i);
		init_connection(conn, fd_base +i, read_buf_size, write_buf_size, cq);

		push_conn(cq, conn);
	}

	return cq;
}

size_t get_active_conn_num(struct es_connmgr *cq)
{
	return cq->size - (cq->tail - cq->head);
}

void es_logconnmgr(struct es_connmgr *cq)
{
	syslog(LOG_INFO, "Concurrent es_conn number = %ld\n", get_active_conn_num(cq));
}

#if 0
static void destroy_conn_queue(struct es_connmgr *cq)
{
	free(cq->free_conn);
	free(cq->all_conn);
	free(cq);
}
#endif



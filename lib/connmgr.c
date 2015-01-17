#include <syslog.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

#include <esvr.h>

#include "connmgr.h"
#include "service.h"
#include "connection.h"

/***************************
*    Connection Manager    *
***************************/
struct conn_queue
{
	struct connection **free_conn;
	unsigned int head;
	unsigned int tail;

	unsigned int mask;
	pthread_mutex_t lock;

	struct connection *all_conn;
//	size_t number;
	size_t size;
};

#define FD_BASE 1000

void push_conn(struct conn_queue *cq, struct connection *conn)
{
	pthread_mutex_lock(&cq->lock);
	cq->free_conn[cq->tail & cq->mask]= conn;
	++cq->tail;
	pthread_mutex_unlock(&cq->lock);
}

struct connection *pop_conn(struct conn_queue *cq)
{
	struct connection *conn;

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

struct conn_queue *create_conn_queue(size_t size, size_t read_buf_size, size_t write_buf_size)
{
	unsigned int cap;
	int i;

	struct conn_queue *cq = malloc(sizeof(struct conn_queue));

	cap = 1;
	while (cap <= size) cap <<= 1;
	cq->mask = cap - 1;
	cq->free_conn = malloc(sizeof(struct connection*) * cap);
	cq->head = cq->tail = 0;

	cq->size = size;
	cq->all_conn = allocate_connections(size);

	pthread_mutex_init(&cq->lock, NULL);

	for (i = 0; i < size; ++i)
	{
		struct connection *conn = get_conn(cq->all_conn, i);
		init_connection(conn, FD_BASE +i, read_buf_size, write_buf_size, cq);

		push_conn(cq, conn);
	}

	return cq;
}

size_t get_active_conn_num(struct conn_queue *cq)
{
	return cq->size - (cq->tail - cq->head);
}

#if 0
static void destroy_conn_queue(struct conn_queue *cq)
{
	free(cq->free_conn);
	free(cq->all_conn);
	free(cq);
}
#endif



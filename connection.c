#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <syslog.h>
#include <assert.h>

#include "epollserver.h"
#include "connection.h"

#define SYSLOG_ERROR(x) syslog(LOG_ERR, "[%s:%d]%s: %s", __FILE__, __LINE__, x, strerror(errno))

#define WRITE_BUFFER_SIZE (5*1024)
#define READ_BUFFER_SIZE 1024

struct connectionmanager *g_cm;

struct connection
{
	int fd;
	char *write_buf;
	char *write_buf_end;
	char *read_buf;
	char *read_buf_end;
	pthread_t reading;
	pthread_mutex_t read_lock;
	pthread_mutex_t write_buf_lock;
};

int get_fd(struct connection *conn)
{
	return conn->fd;
}

void init_connection(struct connection *conn)
{
	conn->fd = -1;
	conn->reading = 0;
	conn->write_buf_end = conn->write_buf = (char*)malloc(WRITE_BUFFER_SIZE);
	conn->read_buf_end = conn->read_buf = (char*)malloc(READ_BUFFER_SIZE);

	pthread_mutex_init(&conn->read_lock, NULL);
	pthread_mutex_init(&conn->write_buf_lock, NULL);
}

int send_data(struct connection *conn, const char *data, size_t num);

void process_message(struct connection *conn, const char *msg, size_t len)
{
	char buf[10000];
	char *p;
	int i;

	*(uint16_t*)buf = 30*len+3+3;
	p = buf + sizeof(uint16_t);

	p[0] = 0;
	strcpy(p, "^^^");
	p += 3;
	for (i=0; i<30; ++i) {
		memcpy(p, msg, len);
		p += len;
	}

	strcpy(p, "$$$");
	p += 3;

	send_data(conn, buf, p-buf);
}

char *process_data(struct connection *conn, char *buf, size_t size)
{
	uint16_t len;

	while (size >= sizeof(uint16_t)) {
		len = *(uint16_t*)buf;
		size -= sizeof(uint16_t);
		if (size < len) {
			break;
		}
		buf += sizeof(uint16_t);
		process_message(conn, buf, len);
		buf += len;
		size -= len;
	}

	return buf;
}

void read_data(struct connection *conn)
{
	ssize_t count;

	pthread_mutex_lock(&conn->read_lock);
	if (conn->reading != 0) {
		pthread_mutex_unlock(&conn->read_lock);
		return;
	}

	conn->reading = pthread_self();
	pthread_mutex_unlock(&conn->read_lock);

	do {
		pthread_mutex_lock(&conn->read_lock);
		count = read(conn->fd, conn->read_buf_end, READ_BUFFER_SIZE-(conn->read_buf_end-conn->read_buf));
		syslog(LOG_INFO, "[%x:%x:%d:] %d bytes read", (unsigned int)conn, (unsigned int)pthread_self(), conn->fd, count);

		if (count > 0) {
			pthread_mutex_unlock(&conn->read_lock);
			conn->read_buf_end += count;
			char *p = process_data(conn, conn->read_buf, conn->read_buf_end-conn->read_buf);
			assert(p >= conn->read_buf && p <= conn->read_buf_end);
			if (p == conn->read_buf_end) {
				conn->read_buf_end = conn->read_buf;
			} else if (p != conn->read_buf) {
				memcpy(conn->read_buf, p, conn->read_buf_end - p);
				conn->read_buf_end -= p-conn->read_buf;
			}
		} else if (count == 0) {
			syslog(LOG_INFO, "[%x:%x:%d:] Remote closed\n",
				(unsigned int)conn, (unsigned int)pthread_self(), conn->fd);
			close_connection(conn);
			pthread_mutex_unlock(&conn->read_lock);
		} else {
			assert(count == -1);
			conn->reading = 0;
			pthread_mutex_unlock(&conn->read_lock);
			SYSLOG_ERROR("read");
			if (errno == EBADF) {
				syslog(LOG_ERR, "[%x:%x:%d:] Reading a closed fd\n",
					(unsigned int)conn, (unsigned int)pthread_self(), conn->fd);
			} else if (errno != EAGAIN && errno != EWOULDBLOCK) {
				syslog(LOG_ERR, "[%x:%x:%d:] read: %s\n",
						(unsigned int)conn, (unsigned int)pthread_self(), conn->fd,
						strerror(errno));
				close_connection(conn);
			}
		}
	} while (count >0);
}

void recycle_connection(struct connection *conn);

/*
 * Note: conn->reading is intentionally untouched in this function.
 */
void close_connection(struct connection *conn)
{
	if (close(conn->fd) == -1) {
		syslog(LOG_INFO, "[%x:%x:%d:] close: %s",
			(unsigned int)conn, (unsigned int)pthread_self(), conn->fd, strerror(errno));
		return;
	}

	syslog(LOG_INFO, "[%x:%x:%d:] fd closed",
		(unsigned int)conn, (unsigned int)pthread_self(), conn->fd);
	recycle_connection(conn);
}

/*
 * direct_send: true, called by sendData(); false, called by EpollServer
 */
void send_buffered_data(struct connection *conn, int direct_send)
{
	int res = 0;
	int total = 0;

	do {
		pthread_mutex_lock(&conn->write_buf_lock);

		if (conn->write_buf == conn->write_buf_end) {
			if(!direct_send) rearm_out(g_es, conn, 0);
			pthread_mutex_unlock(&conn->write_buf_lock);
			break;
		}

		res = write(conn->fd, conn->write_buf, conn->write_buf_end - conn->write_buf);

		assert(res != 0);

		if (res > 0) {
			assert(res <= conn->write_buf_end - conn->write_buf);
			syslog(LOG_INFO, "[%x:%x:%d:] %d bytes sent",
				(unsigned int)conn, (unsigned int)pthread_self(), conn->fd, res);

			if (res < conn->write_buf_end-conn->write_buf) {
				memcpy(conn->write_buf, conn->write_buf+res, conn->write_buf_end-conn->write_buf-res);
			}
			conn->write_buf_end -= res;
			pthread_mutex_unlock(&conn->write_buf_lock);
			total += res;
		} else {
			assert(res == -1);
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				if (direct_send) rearm_out(g_es, conn, 1);
			}
			pthread_mutex_unlock(&conn->write_buf_lock);
			SYSLOG_ERROR("write");
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				syslog(LOG_ERR, "[%x:%x:%d:] Error other than EAGAIN encountered\n",
					(unsigned int)conn, (unsigned int)pthread_self(), conn->fd);
			}
		}
	} while (res > 0);

	syslog(LOG_INFO, "[%x:%x:%d:] total %d bytes sent",
		(unsigned int)conn, (unsigned int)pthread_self(), conn->fd, total);

}

int send_data(struct connection *conn, const char *data, size_t num)
{
	int required_size = conn->write_buf_end - conn->write_buf + num;
	if (required_size > WRITE_BUFFER_SIZE) {
		syslog(LOG_ERR, "write buffer overflow, size required: %d", required_size);
		return -1;
	}

	pthread_mutex_lock(&conn->write_buf_lock);
	memcpy(conn->write_buf_end, data, num);
	conn->write_buf_end += num;
	pthread_mutex_unlock(&conn->write_buf_lock);

	syslog(LOG_INFO, "[%x:%x:%d:] %d bytes put in write buffer",
		(unsigned int)conn, (unsigned int)pthread_self(), conn->fd, num);
	send_buffered_data(conn, 1);

	return 0;
}

struct connectionmanager
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

void push_conn(struct connectionmanager *cm, struct connection *conn);

void init_connectionmanager(size_t size)
{
	unsigned int cap;
	int i;

	g_cm = malloc(sizeof(struct connectionmanager));

	cap = 1;
	while (cap <= size) cap <<= 1;
	g_cm->mask = cap - 1;
	g_cm->free_conn = malloc(sizeof(struct connection*) * cap);
	g_cm->head = g_cm->tail = 0;

	g_cm->size = size;
	g_cm->all_conn = malloc(sizeof(struct connection) * size);

	pthread_mutex_init(&g_cm->lock, NULL);

	for (i = 0; i < size; ++i)
	{
		init_connection(&g_cm->all_conn[i]);
		g_cm->all_conn[i].fd = FD_BASE + i;
		push_conn(g_cm, &g_cm->all_conn[i]);
	}
}

void destroy_connectionmanager(struct connectionmanager *cm)
{
	free(cm->free_conn);
	free(cm->all_conn);
	free(cm);
}

struct connection *pop_conn(struct connectionmanager *cm);

struct connection *get_conn(struct connectionmanager *cm, int fd)
{
	struct connection *conn = pop_conn(cm);

	assert(conn != NULL);
	int res = dup2(fd, conn->fd);
	if (res == -1) {
		SYSLOG_ERROR("dup2");
		abort();
	}

	close(fd);

	conn->reading = 0;
	conn->write_buf_end = conn->write_buf;

	return conn;
}

void recycle_connection(struct connection *conn)
{
	push_conn(g_cm, conn);
}

size_t get_conn_num(struct connectionmanager *cm)
{
	return cm->size - (cm->tail - cm->head);
}

void push_conn(struct connectionmanager *cm, struct connection *conn)
{
	pthread_mutex_lock(&cm->lock);
	cm->free_conn[cm->tail & cm->mask]= conn;
	++cm->tail;
	pthread_mutex_unlock(&cm->lock);
}

struct connection *pop_conn(struct connectionmanager *cm)
{
	struct connection *conn;

	pthread_mutex_lock(&cm->lock);
	if (cm->head == cm->tail) {
		conn = NULL;
	} else {
		conn = cm->free_conn[cm->head & cm->mask];
		++cm->head;
	}
	pthread_mutex_unlock(&cm->lock);

	return conn;
}


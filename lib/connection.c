#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <syslog.h>
#include <assert.h>

#include "connection.h"
#include "service.h"
#include "poller.h"

#define SYSLOG_ERROR(x) syslog(LOG_ERR, "[%s:%d]%s: %s", __FILE__, __LINE__, x, strerror(errno))

#define WRITE_BUFFER_SIZE (5*1024)
#define READ_BUFFER_SIZE 1024

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
	struct poller *p;
	struct service *s;
};

int get_conn_fd(struct connection *conn)
{
	return conn->fd;
}

void init_connection(struct connection *conn, int fd, struct service *s)
{
	conn->fd = fd;
	conn->s = s;
	conn->reading = 0;
	conn->write_buf_end = conn->write_buf = (char*)malloc(WRITE_BUFFER_SIZE);
	conn->read_buf_end = conn->read_buf = (char*)malloc(READ_BUFFER_SIZE);

	pthread_mutex_init(&conn->read_lock, NULL);
	pthread_mutex_init(&conn->write_buf_lock, NULL);
}

static const char *process_data(struct connection *conn, const char *buf, size_t size)
{
	uint16_t len;

	while (size >= sizeof(uint16_t)) {
		len = *(uint16_t*)buf;
		size -= sizeof(uint16_t);
		if (size < len) {
			break;
		}
		buf += sizeof(uint16_t);
		(*get_handler(conn->s))(conn, buf, len);
		buf += len;
		size -= len;
	}

	return buf;
}

/*
 * Note: conn->reading is intentionally untouched in this function.
 */
static void close_connection(struct connection *conn)
{
	if (close(conn->fd) == -1) {
		syslog(LOG_INFO, "[%x:%x:%d:] close: %s",
			(unsigned int)conn, (unsigned int)pthread_self(), conn->fd, strerror(errno));
		return;
	}

	syslog(LOG_INFO, "[%x:%x:%d:] fd closed",
		(unsigned int)conn, (unsigned int)pthread_self(), conn->fd);
	recycle_connection(conn->s, conn);
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
			const char *p = process_data(conn, conn->read_buf, conn->read_buf_end-conn->read_buf);
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

/*
 * direct_send: true, called by sendData(); false, called by workers
 */
void send_buffered_data(struct connection *conn, int direct_send)
{
	int res = 0;
	int total = 0;

	do {
		pthread_mutex_lock(&conn->write_buf_lock);

		if (conn->write_buf == conn->write_buf_end) {
			if(!direct_send) rearm_out(conn->p, conn, 0);
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
				if (direct_send) rearm_out(conn->p, conn, 1);
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

static void clear_conn(struct connection *conn)
{
	conn->reading = 0;
	conn->write_buf_end = conn->write_buf;
}

void set_conn_fd(struct connection *conn, int fd)
{
	int res = dup2(fd, conn->fd);
	if (res == -1) {
		SYSLOG_ERROR("dup2");
		abort();
	}

	close(fd);

	clear_conn(conn);
}

struct connection *allocate_connections(size_t num)
{
	return malloc(sizeof(struct connection) * num);
}

struct connection *get_conn(struct connection *conn_array, size_t i)
{
	return &conn_array[i];
}

void set_conn_poller(struct connection *conn, struct poller *p)
{
	conn->p = p;
}


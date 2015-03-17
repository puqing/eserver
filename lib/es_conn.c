#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <syslog.h>
#include <assert.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include <esvr.h>

#include "es_connmgr.h"
#include "es_conn.h"
#include "es_service.h"
#include "es_poller.h"

struct es_conn
{
	int fd;
	char *write_buf;
	char *write_buf_end;
	char *read_buf;
	char *read_buf_end;
	pthread_t reading;
	pthread_mutex_t read_lock;
	pthread_mutex_t write_buf_lock;
	struct es_poller *p;
//	struct service *s;
	es_messagehandler *msg_handler;
	es_closehandler *close_handler;
	struct es_connmgr *cq;
	size_t read_buf_size;
	size_t write_buf_size;
	void *data;
};

int get_conn_fd(struct es_conn *conn)
{
	return conn->fd;
}

void init_connection(struct es_conn *conn, int fd, size_t read_buf_size, size_t write_buf_size, struct es_connmgr *cq)
{
	conn->fd = fd;
	conn->cq = cq;
	conn->msg_handler = NULL;
	conn->close_handler = NULL;
	conn->reading = 0;
	conn->read_buf_size = read_buf_size;
	conn->write_buf_size = write_buf_size;
	conn->write_buf_end = conn->write_buf = (char*)malloc(write_buf_size);
	conn->read_buf_end = conn->read_buf = (char*)malloc(read_buf_size);

	pthread_mutex_init(&conn->read_lock, NULL);
	pthread_mutex_init(&conn->write_buf_lock, NULL);
}

static const char *process_data(struct es_conn *conn, const char *buf, size_t size)
{
	uint32_t len;

	while (size >= sizeof(len)) {
		len = *(uint32_t*)buf;
		size -= sizeof(len);
		if (size < len) {
			break;
		}
		buf += sizeof(len);
//		(*get_handler(conn->s))(conn, buf, len);
		(*conn->msg_handler)(conn, buf, len);
		buf += len;
		size -= len;
	}

	return buf;
}

/*
 * Note: conn->reading is intentionally untouched in this function.
 * read/write buffers are cleaned when they are used again
 */
static void close_connection(struct es_conn *conn)
{
	if (close(conn->fd) == -1) {
		LOG_CONN(LOG_ERR, "close: %s", strerror(errno));
		return;
	}

	LOG_CONN(LOG_ERR, "fd closed");
	conn->close_handler(conn);
	push_conn(conn->cq, conn);
}

void read_data(struct es_conn *conn)
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
		count = read(conn->fd, conn->read_buf_end, conn->read_buf_size-(conn->read_buf_end-conn->read_buf));
		LOG_CONN(LOG_DEBUG, "%ld bytes read", count);

		if (count > 0) {
			pthread_mutex_unlock(&conn->read_lock);
			conn->read_buf_end += count;
			const char *p = process_data(conn, conn->read_buf, conn->read_buf_end-conn->read_buf);
			assert(p >= conn->read_buf && p <= conn->read_buf_end);
			if (p == conn->read_buf_end) {
				conn->read_buf_end = conn->read_buf;
			} else if (p != conn->read_buf) {
				// overlap! need more test.
				memcpy(conn->read_buf, p, conn->read_buf_end - p);
				conn->read_buf_end -= p-conn->read_buf;
			}
		} else if (count == 0) {
			LOG_CONN(LOG_DEBUG, "Remote closed");
			close_connection(conn);
			pthread_mutex_unlock(&conn->read_lock);
		} else {
			assert(count == -1);
			conn->reading = 0;
			pthread_mutex_unlock(&conn->read_lock);
			LOG_CONN(LOG_DEBUG, "read returns -1");
			if (errno == EBADF) {
				LOG_CONN(LOG_ERR, "Reading a closed fd");
				close_connection(conn);
			} else if (errno != EAGAIN && errno != EWOULDBLOCK) {
				LOG_CONN(LOG_ERR, "read: %s", strerror(errno));
				close_connection(conn);
			} // EAGAIN || EWOULDBLOCK, quit the loop.
		}
	} while (count >0);
}

/*
 * direct_send: true, called by sendData(); false, called by workers
 */
void send_buffered_data(struct es_conn *conn, int direct_send)
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
			LOG_CONN(LOG_DEBUG, "%d bytes sent", res);

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
				LOG_CONN(LOG_ERR, "Error other than EAGAIN encountered");
			}
		}
	} while (res > 0);

	LOG_CONN(LOG_DEBUG, "total %d bytes sent", total);

}

int es_send(struct es_conn *conn, const char *data, size_t num)
{
	size_t required_size = conn->write_buf_end - conn->write_buf + num;
	if (required_size > conn->write_buf_size) {
		LOG_CONN(LOG_ERR, "write buffer overflow, %ld > %ld", required_size, conn->write_buf_size);
		return -1;
	}

	pthread_mutex_lock(&conn->write_buf_lock);
	memcpy(conn->write_buf_end, data, num);
	conn->write_buf_end += num;
	pthread_mutex_unlock(&conn->write_buf_lock);

	LOG_CONN(LOG_DEBUG, "%ld bytes put in write buffer", num);
	send_buffered_data(conn, 1);

	return 0;
}

/*
 * for client side sockets, single-thread
 */
ssize_t es_recv(struct es_conn *conn, size_t num)
{
#if 0
	assert(conn->read_buf_end == conn->read_buf);

	*(uint32_t*)conn->read_buf_end = num;
	conn->read_buf_end += sizeof(uint32_t);

	rearm_in(conn->p, conn, 1);

	return 0;
#else
	size_t c;
	ssize_t r;

	c = 0;

	assert(conn->read_buf_end == conn->read_buf);

	*(uint32_t*)conn->read_buf_end = num;
	conn->read_buf_end += sizeof(uint32_t);

	do {
		r = read(conn->fd, conn->read_buf_end, num - c);
		LOG_CONN(LOG_DEBUG, "%ld bytes read", r);
		if (r > 0) {
			c += r;
			conn->read_buf_end += r;
		}
	} while (r > 0 && c < num);

	assert(c <= num);

	if (c == num) {
		printf("imm\n");
		conn->msg_handler(conn, conn->read_buf+sizeof(uint32_t), num);
		conn->read_buf_end = conn->read_buf;
		return num;
	} else if (r == 0) {
		LOG_CONN(LOG_DEBUG, "Remote closed");
		conn->close_handler(conn);
		close_connection(conn);
		return 0;
	} else {
		assert(r == -1);
		SYSLOG_ERROR("read");
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			rearm_in(conn->p, conn, 1);
		} else {
			LOG_CONN(LOG_ERR, "Error other than EAGAIN encountered");
		}
		return -1;
	}
#endif
}

static void clear_conn(struct es_conn *conn)
{
	conn->reading = 0;
	conn->read_buf_end = conn->read_buf;
	conn->write_buf_end = conn->write_buf;
}

void set_conn_fd(struct es_conn *conn, int fd)
{
	int res = dup2(fd, conn->fd);
	if (res == -1) {
		SYSLOG_ERROR("dup2");
		abort();
	}

	close(fd);

	LOG_CONN(LOG_DEBUG, "Connection asigned new fd");

	clear_conn(conn);
}

void es_sethandler(struct es_conn *conn,
	es_messagehandler *msg_handler,
	es_closehandler *close_handler)
{
	conn->msg_handler = msg_handler;
	conn->close_handler = close_handler;
}

struct es_conn *allocate_connections(size_t num)
{
	return malloc(sizeof(struct es_conn) * num);
}

struct es_conn *get_conn(struct es_conn *conn_array, size_t i)
{
	return &conn_array[i];
}

void set_conn_poller(struct es_conn *conn, struct es_poller *p)
{
	conn->p = p;
}

void es_setconndata(struct es_conn *conn, void *data)
{
	conn->data = data;
}

void *es_getconndata(struct es_conn *conn)
{
	return conn->data;
}

/*
 * client side connections
 */

static int make_socket_non_blocking(int sfd)
{
	int flags, res;

	flags = fcntl(sfd, F_GETFL, 0);
	if (flags == -1)
	{
		perror("fcntl");
		close(sfd);
		return -1;
	}

	flags |= O_NONBLOCK;
	res = fcntl (sfd, F_SETFL, flags);
	if (res == -1)
	{
		perror("fcntl");
		close(sfd);
		return -1;
	}

	return 0;
}

static int connect_server(const char *server, int port)
{
	int sfd;
	struct sockaddr_in addr;
	int res;

	sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sfd == -1) {
		perror("socket");
		return -1;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	inet_pton(AF_INET, server, &addr.sin_addr.s_addr);
	res = connect(sfd, (struct sockaddr*)&addr, sizeof addr);
	if (res < 0) {
		perror("connect");
		close(sfd);
		return -1;
	}

	make_socket_non_blocking(sfd);
	
	return sfd;
	
}

struct es_conn *es_newconn(char *ip, int port, struct es_connmgr *cq, es_connhandler *ch)
{
	int sfd;

	sfd = connect_server(ip, port);
	assert(sfd != -1);
	struct es_conn *conn = get_conn_set_fd(cq, sfd);
	assert(conn != NULL);
	(*ch)(conn);

	return conn;
}


#ifndef __ESVR_H__
#define __ESVR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

/*
 * Connections are pre-allocated in a container, the
 * `es_connmgr'.
 * Each es_conn object has a uniq and fixed fd number.
 * Newly established connections are copied to the object's fd
 * number using dup2(2), and its own fd will be close(2)d after
 * that.
 * When a connection sends out data, it firstly puts the data
 * into a buffer, then tries to send it. If sending fails with
 * EAGAIN, it will be delaied until the fd is available for
 * sending.
 * Each connection object has two call-back functions, one for
 * processing messages and another for clearing up resources.
 * Connection object can be asigned a user-data, which is
 * useful in the call-back functions.
 */
struct es_conn;

int es_send(struct es_conn *conn, const char *data, size_t num);
ssize_t es_recv(struct es_conn *conn, size_t num);

typedef void es_messagehandler(struct es_conn *conn,
		const char* msg, size_t len);
typedef void es_closehandler(struct es_conn *conn);
void es_sethandler(struct es_conn *conn,
	es_messagehandler *msg_handler,
	es_closehandler *close_handler);

void es_setconndata(struct es_conn *conn, void *data);
void *es_getconndata(struct es_conn *conn);

/*
 * The container of the connection objects.
 * The fd number range [fd_base, fd_base+size) * are
 * pre-allocated to the connection objects.
 * The container is an FIFO queue, so that an fd number that
 * is just closed will not be immediately reused.
 * All the connections have read-buffer and write-buffer of 
 * the same size.
 */
struct es_connmgr;
struct es_connmgr *es_newconnmgr(int fd_base,
		size_t size,
		size_t read_buf_size,
		size_t write_buf_size);
void es_logconnmgr(struct es_connmgr *cq);

typedef void es_connhandler(struct es_conn *conn);
struct es_conn *es_newconn(char *ip, int port, struct es_connmgr *cq, es_connhandler *ch);

/*
 * Service represents a listening socket, and manages the
 * working connections.
 * It has a call-back function for processing incoming 
 * connections
 */
struct es_service;
struct es_service *es_newservice(char *ip, int port,
		struct es_connmgr *cq,
		es_connhandler *ch);

/*
 * Poller represents epoll. It accepts both services and 
 * individual connections.
 */
struct es_poller;
struct es_poller *es_newpoller(void);
void es_addservice(struct es_poller *p, struct es_service *s);
void es_addconn(struct es_poller *p, struct es_conn *conn);
int rearm_in(struct es_poller *p, struct es_conn *conn, int rearm);

/*
 * A es_worker is a thread.
 * Each es_worker can have a user-data.
 */
struct es_worker;
struct es_worker *es_newworker(struct es_poller *p, void *data);
void *es_getworkerdata();
typedef void es_workerhandler(void *data);

/* Stop all the worker threads and run hdlr once, with data as its parameter */
void es_syncworkers(es_workerhandler *hdlr, void *data);

#ifdef __cplusplus
}
#endif

#endif // __ESVR_H__


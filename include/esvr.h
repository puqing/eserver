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

/* Return -1 will close the connection */
typedef int es_messagehandler(struct es_conn *conn,
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

typedef void es_connhandler(struct es_conn *conn);
struct es_conn *es_newconn(const char *ip, int port, struct es_connmgr *cq, es_connhandler *ch);

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
size_t es_getconnnum(struct es_service *s);

/*
 * Epoll functions
 */
int es_newepfd(void);
int es_addservice(int epfd, struct es_service *s);
int es_addconn(int epfd, struct es_conn *conn, int client_side);

/*
 * A es_worker is a thread.
 * Each es_worker can have a user-data.
 */
struct es_worker;
struct es_worker *es_newworker(int epfd, void *data);
void *es_getworkerdata();
typedef void es_workerhandler(void *data);

/* Stop all the worker threads and run hdlr once, with data as its parameter */
void es_syncworkers(int syncnum);
int es_getworkingnum(void);

#ifdef __cplusplus
}
#endif

#endif // __ESVR_H__


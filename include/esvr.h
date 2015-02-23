#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

/*
 * Connections are pre-allocated in a container, the
 * `conn_queue'.
 * Each connection object has a uniq and fixed fd number.
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
struct connection;
void set_conn_fd(struct connection *conn, int fd);
int get_conn_fd(struct connection *conn);

int sendout(struct connection *conn, const char *data, size_t num);

typedef void message_handler(struct connection *conn,
		const char* msg, size_t len);
typedef void connection_close_handler(struct connection *conn);
void set_conn_handlers(struct connection *conn,
	message_handler *msg_handler,
	connection_close_handler *close_handler);

void set_conn_data(struct connection *conn, void *data);
void *get_conn_data(struct connection *conn);

/*
 * The container of the connection objects.
 * The fd number range [fd_base, fd_base+size) * are
 * pre-allocated to the connection objects.
 * The container is an FIFO queue, so that an fd number that
 * is just closed will not be immediately reused.
 * All the connections have read-buffer and write-buffer of 
 * the same size.
 */
struct conn_queue;
struct conn_queue *create_conn_queue(int fd_base,
		size_t size,
		size_t read_buf_size,
		size_t write_buf_size);
struct connection *get_conn_set_fd(struct conn_queue *cq, int fd);
void log_conn_num(struct conn_queue *cq);

/*
 * Service represents a listening socket, and manages the
 * working connections.
 * It has a call-back function for processing incoming 
 * connections
 */
struct service;
typedef void connection_handler(struct connection *conn);
struct service *create_service(char *ip, int port,
		struct conn_queue *cq,
		connection_handler *ch);

/*
 * Poller represents epoll. It accepts both services and 
 * individual connections.
 */
struct poller;
struct poller *create_poller();
void add_service(struct poller *p, struct service *s);
void add_connection(struct poller *p, struct connection *conn);

/*
 * A worker is a thread.
 * Each worker can have a user-data.
 */
struct worker;
struct worker *create_worker(struct poller *p, void *data);
void *get_worker_data();

#ifdef __cplusplus
}
#endif


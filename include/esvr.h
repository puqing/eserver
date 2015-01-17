#ifdef __cplusplus
extern "C" {
#endif

struct connection;
typedef void message_handler(struct connection *conn,
		const char* msg, size_t len);
typedef void connection_handler(struct connection *conn);
typedef void connection_close_handler(struct connection *conn);
int send_data(struct connection *conn, const char *data, size_t num);
void set_conn_fd(struct connection *conn, int fd);
void set_conn_handlers(struct connection *conn,
	message_handler *msg_handler,
	connection_close_handler *close_handler);
int get_conn_fd(struct connection *conn);

void set_conn_data(struct connection *conn, void *data);
void *get_conn_data(struct connection *conn);

struct poller;
struct poller *create_poller();
void log_conn_num(struct poller *p);
void add_connection(struct poller *p, struct connection *conn);

struct service;
void add_service(struct poller *p, struct service *s);

struct service *create_service(char *ip, int port, size_t max_conn_num,
		size_t read_buf_size, size_t write_buf_size,
		message_handler *mh, connection_handler *ch, connection_close_handler *cch);

struct worker;
struct worker *create_worker(struct poller *p, void *data);
void *get_worker_data();

struct conn_queue;
struct conn_queue *create_conn_queue(size_t size, size_t read_buf_size, size_t write_buf_size);
struct connection *pop_conn(struct conn_queue *cq);
void push_conn(struct conn_queue *cq, struct connection *conn);

#ifdef __cplusplus
}
#endif


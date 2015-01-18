struct connection;

void read_data(struct connection *conn);
void send_buffered_data(struct connection *conn, int direct_send);

struct connection *allocate_connections(size_t num);
struct connection *get_conn(struct connection *conn_array, size_t i);

struct service;
void init_connection(struct connection *conn, int fd, size_t read_buf_size, size_t write_buf_size, struct conn_queue *cq);

struct poller;
void set_conn_poller(struct connection *conn, struct poller *p);

#define LOG_LIMIT LOG_INFO
#define syslog(a, ...) if (LOG_MASK(a) & LOG_UPTO(LOG_LIMIT)) syslog((a), __VA_ARGS__)
#define SYSLOG_ERROR(x) syslog(LOG_ERR, "[%s:%d]%s: %s", __FILE__, __LINE__, x, strerror(errno))
#define LOG_CONN(lvl, x, ...) syslog(lvl, "[%lx:%lx:%d] "x, (long unsigned int)conn, \
		pthread_self(), get_conn_fd(conn), ##__VA_ARGS__)


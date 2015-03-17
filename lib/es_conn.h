struct es_conn;

void set_conn_fd(struct es_conn *conn, int fd);
int get_conn_fd(struct es_conn *conn);

void read_data(struct es_conn *conn);
void send_buffered_data(struct es_conn *conn, int direct_send);

struct es_conn *allocate_connections(size_t num);
struct es_conn *get_conn(struct es_conn *conn_array, size_t i);

struct es_service;
void init_connection(struct es_conn *conn, int fd, size_t read_buf_size, size_t write_buf_size, struct es_connmgr *cq);

struct es_poller;
void set_conn_poller(struct es_conn *conn, struct es_poller *p);

// log level: LOG_DEBUG for more messages, or LOG_INFO for less
#define LOG_LIMIT LOG_INFO
#define syslog(a, ...) if (LOG_MASK(a) & LOG_UPTO(LOG_LIMIT)) syslog((a), __VA_ARGS__)
#define SYSLOG_ERROR(x) syslog(LOG_ERR, "[%s:%d]%s: %s", __FILE__, __LINE__, x, strerror(errno))
#define LOG_CONN(lvl, x, ...) syslog(lvl, "[%lx:%lx:%d] "x, (long unsigned int)conn, \
		pthread_self(), get_conn_fd(conn), ##__VA_ARGS__)


struct connection;

void read_data(struct connection *conn);
void send_buffered_data(struct connection *conn, int direct_send);

struct connection *allocate_connections(size_t num);
struct connection *get_conn(struct connection *conn_array, size_t i);

struct service;
void init_connection(struct connection *conn, int fd, size_t read_buf_size, size_t write_buf_size, struct conn_queue *cq);

struct poller;
void set_conn_poller(struct connection *conn, struct poller *p);

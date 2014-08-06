struct connection;

int get_conn_fd(struct connection *conn);

void read_data(struct connection *conn, void *handle);
void send_buffered_data(struct connection *conn, int direct_send);

struct connection *allocate_connections(size_t num);
struct connection *get_conn(struct connection *conn_array, size_t i);

struct service;
void init_connection(struct connection *conn, int fd, struct service *s, size_t recv_buf_size, size_t send_buf_size);
void set_conn_fd(struct connection *conn, int fd);

struct poller;
void set_conn_poller(struct connection *conn, struct poller *p);

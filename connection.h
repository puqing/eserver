struct connection;

void close_connection(struct connection *conn);

int get_fd(struct connection *conn);

extern struct connectionmanager *g_cm;
size_t get_conn_num(struct connectionmanager *cm);
void read_data(struct connection *conn);
void send_buffered_data(struct connection *conn, int direct_send);
void init_connectionmanager(size_t size);
struct connection *get_conn(struct connectionmanager *cm, int fd);

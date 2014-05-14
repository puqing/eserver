struct poller;

struct poller *create_poller();
void add_server(struct poller *p, struct server *s);

struct server *find_server(struct poller *p, void *s);
int get_poller_fd(struct poller *p);
void add_connection(struct poller *p, struct connection *conn);
int rearm_out(struct poller *p, struct connection *conn, int rearm);
void log_conn_num(struct poller *p);

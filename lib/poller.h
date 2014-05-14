struct poller;

struct poller *create_poller();
void add_service(struct poller *p, struct service *s);

struct service *find_service(struct poller *p, void *s);
int get_poller_fd(struct poller *p);
void add_connection(struct poller *p, struct connection *conn);
int rearm_out(struct poller *p, struct connection *conn, int rearm);
void log_conn_num(struct poller *p);

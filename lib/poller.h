struct poller;

struct service *find_service(struct poller *p, void *s);
int get_poller_fd(struct poller *p);
void add_connection(struct poller *p, struct connection *conn);
int rearm_out(struct poller *p, struct connection *conn, int rearm);

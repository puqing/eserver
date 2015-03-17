struct es_poller;

struct es_service *find_service(struct es_poller *p, void *s);
int get_poller_fd(struct es_poller *p);
int rearm_out(struct es_poller *p, struct es_conn *conn, int rearm);

struct es_poller *es_createpoller();


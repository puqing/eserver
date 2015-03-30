struct es_poller;
struct event_data;

struct es_service *event_service(struct event_data *evdata);
struct es_conn *event_server_conn(struct event_data *evdata);
struct es_conn *event_client_conn(struct event_data *evdata);
int get_poller_fd(struct es_poller *p);
int rearm_out(struct es_poller *p, struct es_conn *conn, int rearm);

struct es_poller *es_createpoller();


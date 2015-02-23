struct conn_queue;

size_t get_active_conn_num(struct conn_queue *cq);
struct connection *pop_conn(struct conn_queue *cq);
void push_conn(struct conn_queue *cq, struct connection *conn);

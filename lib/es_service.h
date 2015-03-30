struct es_service;

size_t get_conn_num(struct es_service *s);

int get_service_fd(const struct es_service *s);

struct es_conn *accept_connection(struct es_service *s);

es_messagehandler *get_handler(struct es_service *s);

void recycle_connection(struct es_service *s, struct es_conn *conn);


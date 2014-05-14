struct service;

size_t get_conn_num(struct service *s);

int get_service_fd(struct service *s);

struct connection *accept_connection(struct service *s);

service_handler *get_handler(struct service *s);

void recycle_connection(struct service *s, struct connection *conn);


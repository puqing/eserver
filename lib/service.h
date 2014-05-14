struct service;

size_t get_conn_num(struct service *s);

typedef void service_handler(struct connection *conn,
		const char* msg, size_t len);

struct service *create_service(char *ip, int port, size_t max_conn_num,
		size_t recv_buf_size, size_t send_buf_size,
		service_handler *sh);

int get_service_fd(struct service *s);

struct connection *accept_connection(struct service *s);

service_handler *get_handler(struct service *s);

void recycle_connection(struct service *s, struct connection *conn);


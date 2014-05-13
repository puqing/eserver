struct server;

size_t get_conn_num(struct server *s);

struct server *create_server(char *ip, int port, size_t max_conn_num,
		size_t recv_buf_size, size_t send_buf_size,
		server_handler *sh);

int get_server_fd(struct server *s);

struct connection *accept_connection(struct server *s);


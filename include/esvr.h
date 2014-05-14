struct connection;
int send_data(struct connection *conn, const char *data, size_t num);

struct poller;
struct service;

struct poller *create_poller();
void add_service(struct poller *p, struct service *s);
void log_conn_num(struct poller *p);

typedef void service_handler(struct connection *conn,
		const char* msg, size_t len);
struct service *create_service(char *ip, int port, size_t max_conn_num,
		size_t recv_buf_size, size_t send_buf_size,
		service_handler *sh);

struct worker;
struct worker *create_worker(struct poller *p);

struct epollserver *init_server(int port);

struct connection;

int rearm_out(struct epollserver *es, struct connection *conn, int rearm);

extern struct epollserver *g_es;

struct epollserver * init_server(int port);
void stop_server(struct epollserver *es);
int get_epfd(struct epollserver *es);
int get_sfd(struct epollserver *es);
void accept_all_connection(struct epollserver *es);

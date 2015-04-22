struct es_poller;
struct event_data;

int get_poller_fd(struct es_poller *p);
int rearm_out(struct es_poller *p, struct es_conn *conn, int rearm);

struct es_poller *es_createpoller();

inline static struct es_service *ptr_to_service(const void *ptr)
{
	if ((long)ptr & 0x1) {
		return (struct es_service*)((long)ptr &(~0x1));
	} else {
		return NULL;
	}
}

inline static void *service_to_ptr(const struct es_service *s)
{
	assert(((long)s & 0x1) == 0 && "should be aligned");
	return (void*)((long)s | 0x1);
}


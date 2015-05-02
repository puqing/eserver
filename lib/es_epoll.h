#ifndef __ES_EPOLL_H__
#define __ES_EPOLL_H__

#include <assert.h>

struct event_data;

int rearm_out(int epfd, struct es_conn *conn, int rearm);

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

#endif // __ES_EPOLL_H__


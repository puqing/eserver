#ifndef __ES_CONNMGR_H__
#define __ES_CONNMGR_H__

#include <stdlib.h>

struct es_connmgr;

size_t get_active_conn_num(struct es_connmgr *cq);
struct es_conn *pop_conn(struct es_connmgr *cq);
void push_conn(struct es_connmgr *cq, struct es_conn *conn);
struct es_conn *get_conn_set_fd(struct es_connmgr *cq, int fd);

#endif // __ES_CONNMGR_H__


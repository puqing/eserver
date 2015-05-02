#ifndef __ES_SERVICE_H__
#define __ES_SERVICE_H__

#include <esvr.h>

struct es_service;

int get_service_fd(const struct es_service *s);

struct es_conn *accept_connection(const struct es_service *s);

es_messagehandler *get_handler(struct es_service *s);

void recycle_connection(struct es_service *s, struct es_conn *conn);

#endif // __ES_SERVICE_H__


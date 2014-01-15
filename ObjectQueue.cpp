#include <stdlib.h>
#include <pthread.h>

#include "ObjectQueue.h"

ObjectQueue::ObjectQueue()
{
	tail = &head;

	pthread_mutex_init(&head_mutex, NULL);
	pthread_mutex_init(&tail_mutex, NULL);
}

void ObjectQueue::push(ObjectQueueItem *obj)
{
	pthread_mutex_lock(&tail_mutex);
	tail = tail->next = obj;
	pthread_mutex_unlock(&tail_mutex);
}

ObjectQueueItem *ObjectQueue::pop()
{
	ObjectQueueItem *obj;
	pthread_mutex_lock(&head_mutex);
	obj = head.next;
	if (obj != NULL) {
		pthread_mutex_lock(&tail_mutex);
		if (tail == obj) {
			tail = &head;
			head.next = NULL;
			pthread_mutex_unlock(&tail_mutex);
			pthread_mutex_unlock(&head_mutex);
		} else {
			pthread_mutex_unlock(&tail_mutex);
			head.next = obj->next;
			pthread_mutex_unlock(&head_mutex);
			obj->next = NULL;
		}
	} else {
		pthread_mutex_unlock(&head_mutex);
	}
	return obj;
}

//#include <stdlib.h>

/* Pushes from the tail and pop from the head */
class ObjectQueueItem
{
protected:
	ObjectQueueItem *next;
	ObjectQueueItem() { next = NULL; }
	friend class ObjectQueue;
};

class ObjectQueue
{

	ObjectQueueItem head;
	ObjectQueueItem *tail;

public:
	ObjectQueue();
	void push(ObjectQueueItem *obj);
	ObjectQueueItem *pop();

	pthread_mutex_t head_mutex;
	pthread_mutex_t tail_mutex;
};


//#include <stdlib.h>

class ObjectQueueItem
{
};

class ObjectQueue
{
	unsigned int mHead;
	unsigned int mTail;
	unsigned int mMask;
	ObjectQueueItem **mItems;
	pthread_mutex_t mLock;

public:
	ObjectQueue(size_t size);
	~ObjectQueue();

	void push(ObjectQueueItem *obj);
	ObjectQueueItem *pop();
	size_t getNumber() const;
};


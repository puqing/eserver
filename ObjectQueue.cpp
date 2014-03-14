#include <stdlib.h>
#include <pthread.h>
#include <syslog.h>

#include "ObjectQueue.h"

ObjectQueue::ObjectQueue(size_t size) : mHead(0), mTail(0)
{
	unsigned int cap = 1;
	while (cap <= size)
	{
		cap <<= 1;
	}
	mItems = new ObjectQueueItem*[cap];

	mMask = cap - 1;

	pthread_mutex_init(&mLock, NULL);
}

ObjectQueue::~ObjectQueue()
{
	delete [] mItems;
}

void ObjectQueue::push(ObjectQueueItem *obj)
{
	pthread_mutex_lock(&mLock);
	mItems[mTail & mMask]= obj;
	++mTail;
	pthread_mutex_unlock(&mLock);
}

ObjectQueueItem *ObjectQueue::pop()
{
	ObjectQueueItem *res;

	pthread_mutex_lock(&mLock);
	if (mHead == mTail) {
		res = NULL;
	} else {
		res = mItems[mHead & mMask];
		++mHead;
	}
	pthread_mutex_unlock(&mLock);

	return res;
}

size_t ObjectQueue::getNumber() const
{
	return mTail - mHead;
}


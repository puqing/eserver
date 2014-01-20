#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <syslog.h>
#include <assert.h>

#include "EpollServer.h"
#include "ObjectQueue.h"
#include "Connection.h"
#include "ConnectionManager.h"

#define SYSLOG_ERROR(x) syslog(LOG_ERR, "[%s:%d]%s: %s", __FILE__, __LINE__, x, strerror(errno))

#define WRITE_BUFFER_SIZE (64*1024)

extern EpollServer gEpollServer;

Connection::Connection()
{
	mFD = -1;
	mReading = 0;
	mWriteBufferEnd = mWriteBuffer = (char*)malloc(WRITE_BUFFER_SIZE);
	pthread_mutex_init(&mReadLock, NULL);
//	pthread_mutex_init(&mWriteLock, NULL);
	pthread_mutex_init(&mWriteBufferLock, NULL);
}

void Connection::readData()
{

	pthread_mutex_lock(&mReadLock);
	if (mReading) {
		pthread_mutex_unlock(&mReadLock);
		syslog(LOG_INFO, "[%x:%x:%d:] fd is being read by another thread %x", (unsigned int)this, (unsigned int)pthread_self(), mFD, mReading);
		return;
	}

	mReading = pthread_self();;
	pthread_mutex_unlock(&mReadLock);

	while (1)
	{
		ssize_t count;
		char buf[15];

		pthread_mutex_lock(&mReadLock);
		count = read(mFD, buf, sizeof buf);
		syslog(LOG_INFO, "[%x:%x:%d:] %d bytes read", (unsigned int)this, (unsigned int)pthread_self(), mFD, count);

		if (count > 0) {
			pthread_mutex_unlock(&mReadLock);
			sendData(buf, count);
			continue;
		} else if (count == 0) {
			/* The remote has closed the connection. */
			syslog(LOG_INFO, "[%x:%x:%d:] Remote closed\n",
				(unsigned int)this, (unsigned int)pthread_self(), mFD);
			mReading = 0;
			closeConnection();
			pthread_mutex_unlock(&mReadLock);
		} else {
			assert(count == -1);
			mReading = 0;
			pthread_mutex_unlock(&mReadLock);
			if (errno == EAGAIN)
			{
				syslog(LOG_INFO, "[%x:%x:%d:] EAGAIN while read", (unsigned int)this, (unsigned int)pthread_self(), mFD);
			} else {
				SYSLOG_ERROR("read");
				syslog(LOG_ERR, "[%x:%x:%d:] Error encountered\n",
					(unsigned int)this, (unsigned int)pthread_self(), mFD);
			}
		}
		break;
	}
}

extern ConnectionManager gConnectionManager;

void Connection::closeConnection()
{
	close(mFD);
	assert(mReading == 0);
	gConnectionManager.recycle(this);
}

int Connection::sendData(char *data, size_t num)
{
	int required_size = mWriteBufferEnd-mWriteBuffer+num;
	if (required_size > WRITE_BUFFER_SIZE) {
		syslog(LOG_ERR, "write buffer overflow, size required: %d", required_size);
		return -1;
	}

	pthread_mutex_lock(&mWriteBufferLock);
	memcpy(mWriteBufferEnd, data, num);
	mWriteBufferEnd += num;
	pthread_mutex_unlock(&mWriteBufferLock);

	sendBufferedData();

	return 0;

}

void Connection::sendBufferedData()
{
	int count;

	while (1) {
		pthread_mutex_lock(&mWriteBufferLock);

		if (mWriteBuffer == mWriteBufferEnd) {
			pthread_mutex_unlock(&mWriteBufferLock);
			break;
		}

		count = write(mFD, mWriteBuffer, mWriteBufferEnd-mWriteBuffer);
		assert(count != 0);

		if (count == mWriteBufferEnd-mWriteBuffer) {
			mWriteBufferEnd = mWriteBuffer;
			pthread_mutex_unlock(&mWriteBufferLock);
			break;
		} else if (count == -1) {
			pthread_mutex_unlock(&mWriteBufferLock);
			if (errno != EAGAIN) {
				SYSLOG_ERROR("write");
				syslog(LOG_ERR, "[%x:%x:%d:] Error encountered\n",
					(unsigned int)this, (unsigned int)pthread_self(), mFD);
			}
			break;
		} else {
			assert(count > 0 && count < mWriteBufferEnd-mWriteBuffer);
			memcpy(mWriteBuffer, mWriteBuffer+count, mWriteBufferEnd-mWriteBuffer-count);
			mWriteBufferEnd -= count;
			pthread_mutex_unlock(&mWriteBufferLock);
		}
	}
}


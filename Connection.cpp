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

void Connection::readAllData()
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
			/* End of file. The remote has closed the connection. */
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

#if 0
	int count;
	size_t total = num;

	assert(num != 0);

	pthread_mutex_lock(&mWriteLock);
	if (!mWriting) {
		mWriting = pthread_self();
		pthread_mutex_unlock(&mWriteLock);
		// sendBufferedData(); !!!
		// and check its return value,
		// if succeeded, we send our new data
		// if failed, we need to store new data in buffer and return
		while (1) {
			count = write(mFD, data, num);
			assert(count != 0);
			if (count > 0) {
				assert(count <= (signed)num);
				data += count;
				num -= count;
				if (0 == num) {
					// sendBufferedData(); !!!
					mWriting = 0;
					return total;
				}
			} else {
				assert(count == -1);
				if (errno != EAGAIN) {
					SYSLOG_ERROR("write");
					syslog(LOG_ERR, "[%x:%x:%d:] Error encountered\n",
						(unsigned int)this, (unsigned int)pthread_self(), mFD);
					return -1;
				}
				break;
			}
		}
	} else {
		pthread_mutex_unlock(&mWriteLock);
		syslog(LOG_INFO, "[%x:%x:%d:] fd is being written by another thread %x",
				(unsigned int)this, (unsigned int)pthread_self(), mFD, mReading);
	}

	assert(num != 0);

	// Another thread is writing the fd, or
	// this thread is writing the fd, but we have to send buffered old data firstly, and it still cannot be sent, or
	// buffer is empty, but new data cannot be totally sent out, so we got EAGAIN.
	// In these cases, we need to store the unsent new data into buffer

	int required_size;
	required_size = mWriteBufferEnd-mWriteBuffer+num;
	if (required_size > WRITE_BUFFER_SIZE) {
		syslog(LOG_ERR, "write buffer overflow, size required: %d", required_size);
		return -1;
	}

	pthread_mutex_lock(&mWriteBufferLock);
	memcpy(mWriteBufferEnd, data, num);
	mWriteBufferEnd += num;
	pthread_mutex_unlock(&mWriteBufferLock);

	return total-num;
#endif
}

void Connection::sendBufferedData()
{
	int count;

	if (mWriteBuffer == mWriteBufferEnd) {
		return;
	}

	while (1) {
		pthread_mutex_lock(&mWriteBufferLock);

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


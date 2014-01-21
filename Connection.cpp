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
#define READ_BUFFER_SIZE (1024)

extern EpollServer gEpollServer;

Connection::Connection()
{
	mFD = -1;
	mReading = 0;
	mWriteBufferEnd = mWriteBuffer = (char*)malloc(WRITE_BUFFER_SIZE);
	mReadBufferEnd = mReadBuffer = (char*)malloc(WRITE_BUFFER_SIZE);
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

		pthread_mutex_lock(&mReadLock);
		count = read(mFD, mReadBufferEnd, READ_BUFFER_SIZE-(mReadBufferEnd-mReadBuffer));
		syslog(LOG_INFO, "[%x:%x:%d:] %d bytes read", (unsigned int)this, (unsigned int)pthread_self(), mFD, count);

		if (count > 0) {
			pthread_mutex_unlock(&mReadLock);
			mReadBufferEnd += count;
			char *p = processData(mReadBuffer, mReadBufferEnd-mReadBuffer); //TODO: processData doesn't need these parameters
			assert(p >= mReadBuffer && p <= mReadBufferEnd);
			if (p == mReadBufferEnd) {
				mReadBufferEnd = mReadBuffer;
			} else if (p != mReadBuffer) {
				memcpy(mReadBuffer, p, mReadBufferEnd - p);
				mReadBufferEnd -= p-mReadBuffer;
			}
			continue;
		} else if (count == 0) {
			/* The remote has closed the connection. */
			syslog(LOG_INFO, "[%x:%x:%d:] Remote closed\n",
				(unsigned int)this, (unsigned int)pthread_self(), mFD);
//			mReading = 0;
			closeConnection();
			pthread_mutex_unlock(&mReadLock);
		} else {
			assert(count == -1);
			mReading = 0;
			pthread_mutex_unlock(&mReadLock);
			SYSLOG_ERROR("read");
			if (errno != EAGAIN) {
				syslog(LOG_ERR, "[%x:%x:%d:] Error other than EAGAIN encountered\n",
					(unsigned int)this, (unsigned int)pthread_self(), mFD);
			}
		}
		break;
	}
}

char *Connection::processData(char *buf, size_t size)
{
	uint16_t len;

	while (size >= sizeof(uint16_t)) {
		len = *(uint16_t*)buf;
		size -= sizeof(uint16_t);
		if (size < len) {
			break;
		}
		buf += sizeof(uint16_t);
		processMessage(buf, len);
		buf += len;
		size -= len;
	}

	return buf;
//	sendData(buf, size);
}

void Connection::processMessage(const char *msg, size_t len)
{
	char buf[1024];

	*(uint16_t*)buf = len+3+3;
	memcpy(buf+sizeof(uint16_t), "^^^", 3);
	memcpy(buf+sizeof(uint16_t)+3, msg, len);
	memcpy(buf+sizeof(uint16_t)+3+len, "$$$", 3);

	sendData(buf, sizeof(uint16_t)+3+len+3);

//	syslog(LOG_INFO, "[%x:%x:%d:] %d bytes sent", (unsigned int)this, (unsigned int)pthread_self(), mFD, sizeof(uint16_t)+3+len+3);
}

extern ConnectionManager gConnectionManager;

void Connection::closeConnection()
{
	close(mFD);
	syslog(LOG_INFO, "[%x:%x:%d:] fd closed",
		(unsigned int)this, (unsigned int)pthread_self(), mFD);
	assert(mReading != 0);
	assert(mWriteBufferEnd == mWriteBuffer);
	gConnectionManager.recycle(this);
}

int Connection::sendData(const char *data, size_t num)
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

	syslog(LOG_INFO, "[%x:%x:%d:] %d bytes put in write buffer",
		(unsigned int)this, (unsigned int)pthread_self(), mFD, num);
	sendBufferedData();

	return 0;

}

void Connection::sendBufferedData()
{
	int res = 0;
	int total = 0;

	while (1) {
		pthread_mutex_lock(&mWriteBufferLock);

		if (mWriteBuffer == mWriteBufferEnd) {
			pthread_mutex_unlock(&mWriteBufferLock);
			break;
		}

		res = write(mFD, mWriteBuffer, mWriteBufferEnd-mWriteBuffer);

		assert(res != 0);

		if (res == mWriteBufferEnd-mWriteBuffer) {
			mWriteBufferEnd = mWriteBuffer;
			pthread_mutex_unlock(&mWriteBufferLock);
			total += res;
			break;
		} else if (res == -1) {
			pthread_mutex_unlock(&mWriteBufferLock);
			SYSLOG_ERROR("write");
			if (errno != EAGAIN) {
				syslog(LOG_ERR, "[%x:%x:%d:] Error other than EAGAIN encountered\n",
					(unsigned int)this, (unsigned int)pthread_self(), mFD);
			}
			break;
		} else {
			assert(res > 0 && res < mWriteBufferEnd-mWriteBuffer);
			syslog(LOG_INFO, "[%x:%x:%d:]Less data than required sent (%d < %d)\n",
				(unsigned int)this, (unsigned int)pthread_self(), mFD, res, mWriteBufferEnd-mWriteBuffer);

			memcpy(mWriteBuffer, mWriteBuffer+res, mWriteBufferEnd-mWriteBuffer-res);
			mWriteBufferEnd -= res;
			pthread_mutex_unlock(&mWriteBufferLock);
			total += res;
		}
	}

	syslog(LOG_INFO, "[%x:%x:%d:] %d bytes sent",
		(unsigned int)this, (unsigned int)pthread_self(), mFD, total);
}


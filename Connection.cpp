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
	mWriteBufferEnd = mWriteBuffer = (char*)malloc(WRITE_BUFFER_SIZE);
	pthread_mutex_init(&mWriteBufferLock, NULL);
}

void Connection::readAllData()
{
	int done = 0;
	while (1)
	{
		ssize_t count;
		char buf[512];

		count = read(mFD, buf, sizeof buf);
		if (count == -1)
		{
			/* If errno == EAGAIN, that means we have read all
			   data. So go back to the main loop. */
			if (errno != EAGAIN)
			{
				SYSLOG_ERROR("read");
				done = 1;
			}
			break;
		}
		else if (count == 0)
		{
			/* End of file. The remote has closed the
			   connection. */
			done = 1;
			break;
		} else {
			/* Write the buffer back to the sender */
//			buf[count]='\0';
			sendData(buf, count);
/*			count = write(mFD, buf, count);
			if (count < 0) {
				if (errno != EAGAIN) {
					SYSLOG_ERROR("write");
					done = 1;
					break;
				}
				done = delaySending();
			}*/
//			printf("[%x:%x:%d] ", (unsigned int)this, (unsigned int)pthread_self(), mFD);
//			printf("%s\n", buf);
		}
	}

	if (done)
	{
		/* Closing the descriptor will make epoll remove it
		   from the set of descriptors which are monitored. */

		if (mFD != -1) {
			printf ("[%x:%x:%d] Closing connection\n",
				(unsigned int)this, (unsigned int)pthread_self(), mFD);
			closeConnection();
		}
	}
}

extern ConnectionManager gConnectionManager;

void Connection::closeConnection()
{
	closeFD();
//	delete this;
	gConnectionManager.recycle(this);
}

/*
 * return value:
 * 0: all right
 * 1: error occured
 */
/*int Connection::delaySending()
{
	pthread_mutex_lock(mWriteBufferLock);
	if (mWriteBufferEnd + count > mWriteBuffer + WRITE_BUFFER_SIZE) {
		syslog(LOG_ERR, "write buffer overflow, size required: %d",
				mWriteBufferEnd - mWriteBuffer + count);
		pthread_mutex_unlock(mWriteBufferLock);
		return 1;
	}
	memcpy(mWriteBufferEnd, buf, count);
	mWriteBufferEnd += count;
	pthread_mutex_unlock(mWriteBufferLock);

	return 0;
}*/

/*int Connection::haveDataToSend()
{
}*/

int Connection::sendData(char *data, size_t num)
{
	int count;

	count = write(mFD, data, num);
	if (count == (signed)num) {
		return count;
	}

	assert(count < (signed)num);

	if (errno != EAGAIN) {
		SYSLOG_ERROR("write");
		closeConnection();
		return -1;;
	}

	if (count > 0) {
		data += count;
		num -= count;
	}

	int required_size = 0;
	required_size = mWriteBufferEnd-mWriteBuffer+num;
	if (required_size > WRITE_BUFFER_SIZE) {
		syslog(LOG_ERR, "write buffer overflow, size required: %d", required_size);
		closeConnection();
		return -1;
	}

	pthread_mutex_lock(&mWriteBufferLock);
	memcpy(mWriteBufferEnd, data, num);
	mWriteBufferEnd += num;
	gEpollServer.pollSending(mFD, this);
	pthread_mutex_unlock(&mWriteBufferLock);

	return count;
}

int Connection::sendBufferedData()
{
	int count;

	pthread_mutex_lock(&mWriteBufferLock);

	count = write(mFD, mWriteBuffer, mWriteBufferEnd-mWriteBuffer);

	if (count == mWriteBufferEnd-mWriteBuffer) {
		mWriteBufferEnd = mWriteBuffer;
		gEpollServer.stopSending(mFD, this);
		pthread_mutex_unlock(&mWriteBufferLock);
		return 1;
	}

	if (errno == EAGAIN) {
		if (count > 0) {
			memcpy(mWriteBuffer, mWriteBuffer+count, mWriteBufferEnd-mWriteBuffer-count);
			mWriteBufferEnd -= count;
		}
		pthread_mutex_unlock(&mWriteBufferLock);
		return 0;
	} else {
		pthread_mutex_unlock(&mWriteBufferLock);
		assert(count < 0);
		SYSLOG_ERROR("write");
		closeConnection();
		return -1;
	}

}


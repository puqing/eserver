#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include "EpollServer.h"
#include "ObjectQueue.h"
#include "Connection.h"
#include "ConnectionManager.h"

#define SYSLOG_ERROR(x) syslog(LOG_ERR, "[%s:%d]%s: %s", __FILE__, __LINE__, x, strerror(errno))

#define FD_BASE 2000

ConnectionManager gConnectionManager(1024);

ConnectionManager::ConnectionManager(size_t size)
{
	mAllConnections = new Connection[size];
	for (unsigned int i = 0; i < size; ++i)
	{
		mAllConnections[i].mFD = FD_BASE + i;
		mFreeConnections.push(&mAllConnections[i]);
	}
}

ConnectionManager::~ConnectionManager()
{
	delete[] mAllConnections;
}

Connection *ConnectionManager::get(int fd)
{
	Connection *conn = static_cast<Connection*>(mFreeConnections.pop());
	assert(conn != NULL);
//	conn->mFD = fd;
	syslog(LOG_INFO, "[%x:%x:%d:] fd duplicating to :%d:", (unsigned int)this, (unsigned int)pthread_self(), fd, conn->mFD);
	int res = dup2(fd, conn->mFD);
	if (res == -1) {
		SYSLOG_ERROR("dup2");
		abort();
	}

	close(fd);

	return conn;
}

void ConnectionManager::recycle(Connection *conn)
{
	mFreeConnections.push(conn);
}


#include <stdlib.h>
#include <assert.h>

#include "EpollServer.h"
#include "ObjectQueue.h"
#include "Connection.h"
#include "ConnectionManager.h"

ConnectionManager::ConnectionManager(size_t size)
{
	mAllConnections = new Connection[size];
	for (unsigned int i = 0; i < size; ++i)
	{
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
	conn->mFD = fd;
	return conn;
}

void ConnectionManager::recycle(Connection *conn)
{
	mFreeConnections.push(conn);
}


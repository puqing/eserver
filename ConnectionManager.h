class ConnectionManager
{
	ObjectQueue mFreeConnections;
	Connection *mAllConnections;

public:
	ConnectionManager(size_t size);
	~ConnectionManager();

	Connection *get(int fd);
	void recycle(Connection *conn);
};


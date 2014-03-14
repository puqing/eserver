class ConnectionManager
{
	ObjectQueue mFreeConnections;
	Connection *mAllConnections;
	const size_t mMaximumConnectionNumber;

public:
	ConnectionManager(size_t max_num);
	~ConnectionManager();

	Connection *get(int fd);
	void recycle(Connection *conn);
	size_t getNumber() const;
};

extern ConnectionManager gConnectionManager;


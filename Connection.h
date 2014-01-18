class Connection : SocketFD, ObjectQueueItem
{
	char *mWriteBuffer;
	char *mWriteBufferEnd;
	pthread_mutex_t mWriteBufferLock;
public:
	Connection();
	Connection(int fd) { mFD = fd; }
	void readAllData();
	void closeConnection();
	int sendData(char *data, size_t num);
	int sendBufferedData();
	int getFD() { return mFD; }

	friend class ConnectionManager;
};


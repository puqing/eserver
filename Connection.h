class Connection : ObjectQueueItem
{
	int mFD;
	char *mWriteBuffer;
	char *mWriteBufferEnd;
	pthread_mutex_t mWriteBufferLock;
	unsigned int mReading;
	pthread_mutex_t mReadLock;
public:
	Connection();
	void readAllData();
	void closeConnection();
	int sendData(char *data, size_t num);
	int sendBufferedData();
	int getFD() { return mFD; }

	friend class ConnectionManager;
};


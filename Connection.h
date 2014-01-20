class Connection : ObjectQueueItem
{
	int mFD;
	char *mWriteBuffer;
	char *mWriteBufferEnd;
	unsigned int mReading;
	pthread_mutex_t mReadLock;
	unsigned int mWriting;
//	pthread_mutex_t mWriteLock;
	pthread_mutex_t mWriteBufferLock;
public:
	Connection();
	void readAllData();
	void closeConnection();
	int sendData(char *data, size_t num);
	void sendBufferedData();
	int getFD() { return mFD; }

	friend class ConnectionManager;
};


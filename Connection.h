class Connection : ObjectQueueItem
{
	int mFD;
	char *mWriteBuffer;
	char *mWriteBufferEnd;
	char *mReadBuffer;
	char *mReadBufferEnd;
	unsigned int mReading;
	pthread_mutex_t mReadLock;
	pthread_mutex_t mWriteBufferLock;
public:
	Connection();
	void readData();
	char *processData(char *buf, size_t size);
	virtual void processMessage(const char *msg, size_t len);
	virtual void closeConnection();
	int sendData(const char *data, size_t num);
	void sendBufferedData(bool direct_send);
	int getFD() { return mFD; }

	friend class ConnectionManager;
};


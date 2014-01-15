class Connection : SocketFD, ObjectQueueItem
{
public:
	Connection() { mFD = -1; }
	Connection(int fd) { mFD = fd; }
	void readAllData();
	void closeConnection();

	friend class ConnectionManager;
};


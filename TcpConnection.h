class TcpConnection : SocketFD
{
public:
	TcpConnection(int fd) { mFD = fd; }
	void readAllData();
	void closeConnection();
};


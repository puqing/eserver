class DBConnection : Connection
{
	MYSQL *mConnection;

public:
	void connectDB();
	virtual void processMessage(const char *msg, size_t len);
	void closeConnection();
};


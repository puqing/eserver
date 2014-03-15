class Connection;

class EpollServer
{

public:
	class EventHandler
	{
	};

private:
	EventHandler *mEventHandler;

public:
	int mFD;
	int mEPFD;

public:
	void setEventHandler(EventHandler *event_handler)
	{
		mEventHandler = event_handler;
	}

	void acceptAllConnection();
	int init(int port);
//	int start(int thread_number);
	int stop();
	int rearmOut(Connection *conn, bool poll);

//	virtual void *run();

//	virtual ~EpollServer() {};
};

extern EpollServer gEpollServer;


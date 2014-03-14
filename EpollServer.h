class EventLoop
{
protected:
	virtual void *run() = 0;
	static void *staticRun(void *data) {
		return ((EventLoop*)data)->run();
	}
	virtual ~EventLoop() { }
};

class Connection;

class EpollServer : EventLoop
{

public:
	class EventHandler
	{
	};

private:
	EventHandler *mEventHandler;
	int mFD;
	int mEPFD;

public:
	void setEventHandler(EventHandler *event_handler)
	{
		mEventHandler = event_handler;
	}

	void acceptAllConnection();
	int init(int port);
	int start(int thread_number);
	int stop();
	int rearmOut(Connection *conn, bool poll);

	virtual void *run();

	virtual ~EpollServer() {};
};

extern EpollServer gEpollServer;


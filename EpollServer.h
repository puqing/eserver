class SocketFD
{
protected:
	int mFD;
	int closeFD();
	virtual ~SocketFD() { closeFD();}
};

class EventLoop
{
protected:
	virtual void *runLoop() = 0;
	static void *staticRunLoop(void *data) {
		return ((EventLoop*)data)->runLoop();
	}
	virtual ~EventLoop() { }
};

class EpollServer : SocketFD, EventLoop
{

public:
	class EventHandler
	{
	};

private:
	EventHandler *mEventHandler;
	int mEPFD;

public:
	void setEventHandler(EventHandler *event_handler)
	{
		mEventHandler = event_handler;
	}

	void acceptAllConnection();
	int init(const char *port);
	int run(int thread_number);
	int stop();

	virtual void *runLoop();

	virtual ~EpollServer() {};
};


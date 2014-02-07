/*class SocketFD
{
protected:
	int mFD;
	int closeFD();
	virtual ~SocketFD() { }
};*/

class EventLoop
{
protected:
	virtual void *runLoop() = 0;
	static void *staticRunLoop(void *data) {
		return ((EventLoop*)data)->runLoop();
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
//	int mEPFDW;

public:
	void setEventHandler(EventHandler *event_handler)
	{
		mEventHandler = event_handler;
	}

	void acceptAllConnection();
	int init(int port);
	int run(int thread_number);
	int stop();
	int rearmOut(Connection *conn, bool poll);

	virtual void *runLoop();

	virtual ~EpollServer() {};
};


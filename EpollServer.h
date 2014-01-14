class SocketFD
{
protected:
	int mFD;
	int closeFD();
};

class EpollServer : SocketFD
{

public:
	class EventHandler
	{
	};

private:
	EventHandler *mEventHandler;
	int mEPFD;

	class EventLoop
	{
	public:
		void *run();
		static void *static_run(EventLoop *data) { data->run(); }
	};

public:
	void setEventHandler(EventHandler *event_handler)
	{
		mEventHandler = event_handler;
	}

	void acceptAllConnection();
	int init(const char *port);
	int run(int thread_number);
	int stop();
};


class EpollServer;

class Worker
{
	EpollServer *mES;

public:
	Worker(EpollServer *es) { mES = es; }
	inline static void *staticRun(void *w) {
		return ((Worker*)w)->run();
	}

	static int start(EpollServer *es, int thread_number);
protected:
	void *run();
};


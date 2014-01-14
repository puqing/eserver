#include "EpollServer.h"

int main(int argc, char *argv[])
{

	EpollServer *es;

	es = new EpollServer;

	es->init("8888");

	es->run(8);

	es->stop();

	delete es;

	return 0;
}


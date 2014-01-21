#include <syslog.h>

#include "EpollServer.h"

extern EpollServer gEpollServer;

int main(int argc, char *argv[])
{

	openlog("game_server", 0, LOG_USER);
	syslog(LOG_INFO, "server start");

	gEpollServer.init(8888);

	gEpollServer.run(8);

	gEpollServer.stop();

	syslog(LOG_INFO, "server stop");
	closelog();

	return 0;
}


#include <syslog.h>
#include <unistd.h>
#include <signal.h>

#include "EpollServer.h"

extern EpollServer gEpollServer;

int main(int argc, char *argv[])
{

	daemon(0, 0);

	signal(SIGPIPE, SIG_IGN);

	openlog("game_server", 0, LOG_USER);

	syslog(LOG_INFO, "server start");

	gEpollServer.init(80);

	gEpollServer.run(8);

	gEpollServer.stop();

	syslog(LOG_INFO, "server stop");

	closelog();

	return 0;
}


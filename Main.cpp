#include <syslog.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "ObjectQueue.h"
#include "Connection.h"
#include "ConnectionManager.h"
#include "EpollServer.h"
#include "Worker.h"

int main(int argc, char *argv[])
{

	int port = 0;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s port\n", argv[0]);
		abort();
	}

	sscanf(argv[1], "%d", &port);

	daemon(0, 0);

	signal(SIGPIPE, SIG_IGN);

	openlog("game_server", 0, LOG_USER);

	syslog(LOG_INFO, "server start");

	gEpollServer.init(port);

	Worker::start(&gEpollServer, 8);

	while (1) {
		syslog(LOG_INFO, "Concurrent connection number = %d\n", gConnectionManager.getNumber());
		sleep(1);
	}

	gEpollServer.stop();

	syslog(LOG_INFO, "server stop");

	closelog();

	return 0;
}


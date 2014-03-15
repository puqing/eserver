#include <syslog.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "connection.h"
#include "epollserver.h"
#include "worker.h"

int main(int argc, char *argv[])
{

	int port = 0;
	struct epollserver *es;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s port\n", argv[0]);
		abort();
	}

	sscanf(argv[1], "%d", &port);

//	daemon(0, 0);

	signal(SIGPIPE, SIG_IGN);

	openlog("gserver", 0, LOG_USER);

	syslog(LOG_INFO, "server start");

	es = init_server(port);
	init_connectionmanager(10000);

	start_workers(es, 8);

	while (1) {
		syslog(LOG_INFO, "Concurrent connection number = %d\n", get_conn_num(g_cm));
		sleep(1);
	}

	stop_server(es);

	syslog(LOG_INFO, "server stop");

	closelog();

	return 0;
}


#include <syslog.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "connection.h"
#include "service.h"
#include "poller.h"
#include "worker.h"

static void process_message(struct connection *conn, const char *msg, size_t len)
{
	char buf[10000];
	char *p;
	int i;

	*(uint16_t*)buf = 30*len+3+3;
	p = buf + sizeof(uint16_t);

	p[0] = 0;
	strcpy(p, "^^^");
	p += 3;
	for (i=0; i<30; ++i) {
		memcpy(p, msg, len);
		p += len;
	}

	strcpy(p, "$$$");
	p += 3;

	send_data(conn, buf, p-buf);
}

int main(int argc, char *argv[])
{

	int port = 0;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s port\n", argv[0]);
		abort();
	}

	sscanf(argv[1], "%d", &port);

//	daemon(0, 0);

	signal(SIGPIPE, SIG_IGN);

	openlog("gserver", 0, LOG_USER);

	syslog(LOG_INFO, "server start");

	struct poller *p = create_poller();

	struct service *s = create_service(NULL, port, 5000, 1024, 1024*5, &process_message);

	add_service(p, s);

	int i;
	for (i = 0; i < 8; ++i) {
		create_worker(p);
	}

	while (1) {
		log_conn_num(p);
		sleep(1);
	}

	/* TODO
	stop_poller(p);

	syslog(LOG_INFO, "server stop");

	closelog();
	*/

	return 0;
}


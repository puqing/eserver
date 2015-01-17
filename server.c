#include <syslog.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <esvr.h>

static void process_message(struct connection *conn, const char *msg, size_t len)
{
	char buf[10000];
	char *p;
	int i;

	*(uint32_t*)buf = 30*len+3+3;
	p = buf + sizeof(uint32_t);

	p[0] = 0;
	strcpy(p, "^^^");
	p += 3;
	for (i=0; i<30; ++i) {
		memcpy(p, msg, len);
		p += len;
	}

	strcpy(p, "$$$");
	p += 3;

	sendout(conn, buf, p-buf);
}

static void process_connection_close(struct connection *conn)
{
//	printf("%lx close\n", conn);
}

static void process_connection(struct connection *conn)
{
	set_conn_handlers(conn, process_message,
			process_connection_close);
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

	openlog(argv[0], LOG_PID, LOG_USER);

	syslog(LOG_INFO, "server start");

	struct poller *p = create_poller();

	struct conn_queue *cq = create_conn_queue(1000,
			20000, 1024, 1024);

	struct service *s = create_service(NULL, port, cq,
		&process_connection);

	add_service(p, s);

	long i;
	for (i = 0; i < 4; ++i) {
		create_worker(p, (void*)i);
	}

	while (1) {
		log_conn_num(cq);
		sleep(1);
	}

	/* TODO
	stop_poller(p);

	syslog(LOG_INFO, "server stop");

	closelog();
	*/

	return 0;
}


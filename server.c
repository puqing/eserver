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

	send_data(conn, buf, p-buf);
}

static void process_connection(struct connection *conn)
{
//	printf("%lx init\n", conn);
}

static void process_connection_close(struct connection *conn)
{
//	printf("%lx close\n", conn);
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

	struct service *s = create_service(NULL, port, 20000, 1*1024, 1024*1,
			&process_message, &process_connection, &process_connection_close);

	add_service(p, s);

//	s = create_service(NULL, 8889, 10000, 100*1024, 1024*5, &process_message,
//			&process_connection, &process_connection_close);

//	add_service(p, s);

	long i;
	for (i = 0; i < 4; ++i) {
		create_worker(p, (void*)i);
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


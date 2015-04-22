#include <syslog.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <esvr.h>

static int process_message(struct es_conn *conn, const char *msg, size_t len)
{
	char buf[256];
	char *p;
	int i;

	p = buf;

	strcpy(p, "^^^");
	p += 3;
	for (i=0; i<2; ++i) {
		memcpy(p, msg, len);
		p += len;
	}

	strcpy(p, "$$$");
	p += 3;

	es_send(conn, buf, p-buf);

	return 0;
}

static void process_connection_close(struct es_conn *conn)
{
//	printf("%lx close\n", conn);
}

static void process_connection(struct es_conn *conn)
{
	es_sethandler(conn, process_message,
			process_connection_close);
}

void signal_handler(int sig)
{
	static int i = 1;

	if (i) {
		i = 0;
		es_syncworkers(0);
	} else {
		i = 1;
		es_syncworkers(2);
	}
}

#define THREADNUM 4

int main(int argc, char *argv[])
{

	int port = 0;
	int epfd;
	struct es_connmgr *cq;
	struct es_service *s;
	long i;
	struct sigaction act;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s port\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	port = atoi(argv[1]);

//	daemon(0, 0);

	signal(SIGPIPE, SIG_IGN);

	act.sa_handler = &signal_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGHUP, &act, NULL);

	openlog(argv[0], LOG_PID, LOG_USER);

	syslog(LOG_INFO, "server start");

	epfd = es_newepfd();

	cq = es_newconnmgr(1000, 2000, 1024, 1024);

	s = es_newservice(NULL, port, cq, &process_connection);

	es_addservice(epfd, s);

	for (i = 0; i < THREADNUM; ++i) {
		es_newworker(epfd, (void*)i);
	}

	while (1) {
		syslog(LOG_INFO, "Connection number = %ld\n", \
				es_getconnnum(s));
		sleep(1);
	}

	/* TODO: close services
	syslog(LOG_INFO, "server stop");

	closelog();
	*/

	return 0;
}


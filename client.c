#include <netdb.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <time.h>

#include <esvr.h>

#define PERROR(s) printf("%s:%d: %s: %s\n", __FILE__, __LINE__, s, strerror(errno))

static unsigned long g_conn_total = 0;

static struct es_poller *g_p;

static int process_message(struct es_conn *conn, const char *msg, size_t len)
{
	write(1, msg, len);
	write(1, "\n", 1);
	fsync(1);
	__sync_add_and_fetch(&g_conn_total, 1);
//	++g_conn_total;
//	printf("%lu\n", g_conn_total);
//	fflush(stdout);

	return -1;
}

static void process_connection_close(struct es_conn *conn)
{
//	printf("%lx close\n", (unsigned long)conn);
}

static void process_connection(struct es_conn *conn)
{
	es_sethandler(conn, process_message,
			process_connection_close);
}

static int gStop = 0;
static time_t start_time;

void signal_handler(int sig)
{
	gStop = 1;
}

#define THREADNUM 4
#define CONN_NUM  1000

int main(int argc, char *argv[])
{
	char *ip_addr;
	short port;
	struct es_connmgr *cq;
	long i;
	struct es_poller *p;
	float avg_conn_per_sec;
	time_t end_time;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s ip_addr port\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	ip_addr = argv[1];
	port = atoi(argv[2]);

	openlog(argv[0], LOG_PID, LOG_USER);

	signal(SIGPIPE, SIG_IGN);

	signal(SIGINT, signal_handler);

	cq = es_newconnmgr(1000, 2000, 1024, 512);

	p = es_newpoller();

	for (i = 0; i < CONN_NUM; ++i) {
		struct es_conn *conn = es_newconn(ip_addr, port, cq, &process_connection);
		const char *data = "origin";
		uint32_t len;
		int r;

		es_addconn(p, conn);

		g_p = p;

		len = rand()*1.0/RAND_MAX*strlen(data);
		++len;
		es_send(conn, (char*)&len, sizeof(len));
		es_send(conn, data, len);

		r = es_recv(conn, len*2+6);
		printf("r = %d\n", r);
	}

	for (i = 0; i < THREADNUM; ++i) {
		es_newworker(p, (void*)i);
	}

	time(&start_time);

	while (gStop == 0) {
		if (g_conn_total == CONN_NUM) {
			break;
		}
//		es_logconnmgr(cq);
		usleep(1000);
	}

	avg_conn_per_sec = 0;
	time(&end_time);
	avg_conn_per_sec = 1.0*g_conn_total/(end_time-start_time);
	fprintf(stderr, "g_conn_total, avg_conn_per_sec = %lu, %f\n", g_conn_total, avg_conn_per_sec);

	return 0;
}


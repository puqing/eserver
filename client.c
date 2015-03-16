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

static struct poller *g_p;

static void process_message(struct connection *conn, const char *msg, size_t len)
{
	write(1, msg, len);
	write(1, "\n", 1);
	fsync(1);
	rearm_in(g_p, conn, 0);
	__sync_add_and_fetch(&g_conn_total, 1);
	printf("%lu\n", g_conn_total);
	fflush(stdout);
}

static void process_connection_close(struct connection *conn)
{
	printf("%lx close\n", (unsigned long)conn);
}

static int make_socket_non_blocking(int sfd)
{
	int flags, res;

	flags = fcntl(sfd, F_GETFL, 0);
	if (flags == -1)
	{
		perror("fcntl");
		close(sfd);
		return -1;
	}

	flags |= O_NONBLOCK;
	res = fcntl (sfd, F_SETFL, flags);
	if (res == -1)
	{
		perror("fcntl");
		close(sfd);
		return -1;
	}

	return 0;
}

static int connect_server(const char *server, int port)
{
	int sfd;
	struct sockaddr_in addr;
	int res;

	sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sfd == -1) {
		perror("socket");
		return -1;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	inet_pton(AF_INET, server, &addr.sin_addr.s_addr);
	res = connect(sfd, (struct sockaddr*)&addr, sizeof addr);
	if (res < 0) {
		perror("connect");
		close(sfd);
		return -1;
	}

	make_socket_non_blocking(sfd);
	
	return sfd;
	
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

	if (argc != 3) {
		fprintf(stderr, "Usage: %s ip_addr port\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	ip_addr = argv[1];
	port = atoi(argv[2]);

	openlog(argv[0], LOG_PID, LOG_USER);

	signal(SIGPIPE, SIG_IGN);

	signal(SIGINT, signal_handler);

	struct conn_queue *cq = create_conn_queue(1000,
			2000, 1024, 512);

	struct poller *p = create_poller();

	long i;
	for (i = 0; i < CONN_NUM; ++i) {
		int sfd = connect_server(ip_addr, port);
		assert(sfd != -1);

		struct connection *conn = get_conn_set_fd(cq, sfd);
		assert(conn != NULL);
		set_conn_handlers(conn, process_message, process_connection_close);

		add_connection(p, conn);
		rearm_in(p, conn, 0);

		g_p = p;

		char *data = "Hello";
		uint32_t len = rand()*1.0/RAND_MAX*strlen(data);
		if (len == 0) len = 1;
		sendout(conn, (char*)&len, sizeof(len));
		sendout(conn, data, len);

		int r = readin(conn, len*2+6);
		printf("r = %d\n", r);
	}

	for (i = 0; i < THREADNUM; ++i) {
		create_worker(p, (void*)i);
	}

	time(&start_time);

	while (gStop == 0) {
		log_conn_num(cq);
		sleep(1);
	}

	float avg_conn_per_sec = 0;
	time_t end_time;
	time(&end_time);
	avg_conn_per_sec = 1.0*g_conn_total/(end_time-start_time);
	fprintf(stderr, "g_conn_total, avg_conn_per_sec = %lu, %f\n", g_conn_total, avg_conn_per_sec);

	return 0;
}


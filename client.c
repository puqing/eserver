#include <netdb.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <signal.h>

#define PERROR(s) printf("%s:%d: %s: %s\n", __FILE__, __LINE__, s, strerror(errno))

static char *ip_addr = 0;

static int gStop = 0;

static unsigned int conn_total = 0;
float avg_conn_per_sec = 0;

static int shrink_socket_send_buffer(int sfd)
{
	int size;
	int res;

	size = 20;

	res = setsockopt(sfd, SOL_SOCKET, SO_SNDBUF, &size, sizeof size);
	if (res == -1) {
		perror("setsockopt");
		return -1;
	}

	res = setsockopt(sfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof size);
	if (res == -1) {
		perror("setsockopt");
		return -1;
	}

	return 0;
}

static int connect_server(const char *server, int port)
{
//	struct addrinfo hints;
//	struct addrinfo *result, *rp;
	int sfd;
	struct sockaddr_in addr;
	int res;
	int on = 1;

	sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sfd == -1) {
		perror("socket");
		return -1;
	}

	res = setsockopt(sfd, SOL_SOCKET,
			SO_REUSEADDR,
			(const char *) &on, sizeof(on));

	if (-1 == res)
	{
		perror("setsockopt(...,SO_REUSEADDR,...)");
	}

	struct linger v_linger;
	v_linger.l_onoff = 1;
	v_linger.l_linger = 0;
	res = setsockopt(sfd, SOL_SOCKET,
			SO_LINGER,
			(const char *) &v_linger, sizeof(v_linger));

	if (-1 == res)
	{
		perror("setsockopt(...,SO_LINGER,...)");
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
	
	shrink_socket_send_buffer(sfd);

	return sfd;
	
}

int send_data(int fd, char *data)
{
	int sr;
	uint16_t len = (uint16_t)(rand()*1.0/RAND_MAX*strlen(data));

	if (len == 0) len = 1;

	sr = send(fd, &len, sizeof len, 0);
	if (sr < 0) {
		PERROR("send");
	}

	sr = send(fd, data, len, 0);
	if (sr < 0) {
		PERROR("send");
	}

	return sr;
	
}

int recv_data(int fd, char *buf, size_t size)
{
	int count;
	uint16_t len;
	uint16_t data_len;

	data_len = 0;

	read(fd, &len, sizeof len);

	while (data_len < len) {
		count = read(fd, buf, len);
		if (count < 0) {
			perror("recv");
			abort();
		}
		data_len += count;
		buf += count;
	}

	assert(data_len == len);

	return data_len;
}

void print_local_port(int fd)
{
	struct sockaddr addr;
	socklen_t len;
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	int res;

	len = sizeof addr;

	res = getsockname(fd, &addr, &len);

	if (res == -1) {
		perror("getsockname");
		abort();
	}

	res = getnameinfo(&addr, len,
			hbuf, sizeof hbuf,
			sbuf, sizeof sbuf,
			NI_NUMERICHOST | NI_NUMERICSERV);
	if (res == 0) {
		printf("[%x:%d] connected"
				"(localhost=%s, port=%s)\n",
				 (unsigned int)pthread_self(), fd, hbuf, sbuf);
	} else {
		printf("Cannot print local host/port info, error returned: %d\n", res);
		abort();
	}
}

void *doit(void *str)
{
	int sfd, count;
	char buf[10240];

	while (gStop == 0) {
		sfd = connect_server(ip_addr, 8888);
		if (sfd == -1) {
			printf("Will connect again.\n");
//			sleep(rand()*1.0/RAND_MAX*30);
			sleep(10);
			continue;
		}

		__sync_add_and_fetch(&conn_total, 1);

		print_local_port(sfd);

		count = send_data(sfd, (char*)str);
		if (count < 0) {
			printf("Send error\n");
			close(sfd);
			sleep(30);
			continue;
		}

		printf("[%x:%d] %d bytes sent\n",
				(unsigned int)pthread_self(),
				sfd, count);

		count = recv_data(sfd, buf, sizeof(buf)-1);
		buf[count] = '\0';
		printf("[%x:%d] %d: %s\n", (unsigned int)pthread_self(),
				sfd, count, buf);

		close(sfd);

	}

	close(sfd);

	printf("[%x] thread end\n", (unsigned int)pthread_self());
	return NULL;
}

static time_t start_time;

void signal_handler(int sig)
{
	time_t end_time;

	time(&end_time);

	avg_conn_per_sec = 1.0*conn_total/(end_time-start_time);

	gStop = 1;
}

#define THREADNUM 8000

int main(int argc, char *argv[])
{
	int i;

	signal(SIGPIPE, SIG_IGN);

	signal(SIGINT, signal_handler);

	pthread_t tid[THREADNUM];

	if (argc != 3) {
		fprintf(stderr, "Usage: %s ip_addr message\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	time(&start_time);

	ip_addr = argv[1];

	for (i = 0; i < THREADNUM; ++i) {
		int res = pthread_create(&tid[i], NULL, &doit, argv[2]);
		if (res != 0) {
			printf("%d: %d\n", i, res);
			return res;
		}
	}

	for (i = 0; i < THREADNUM; ++i) {
		pthread_join(tid[i], NULL);
	}

	fprintf(stderr, "avg_conn_per_sec = %f\n", avg_conn_per_sec);

	return 0;
}


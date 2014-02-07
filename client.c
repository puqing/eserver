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

static int connect_server(char *server, char *port)
{
//	struct addrinfo hints;
//	struct addrinfo *result, *rp;
	int sfd;
	struct sockaddr_in addr;
	int res;

	sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sfd == -1) {
		perror("socket");
		abort();
	}

#if 0
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	s = getaddrinfo(server, port, &hints, &result);
	
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		close(sfd);
		return -1;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next)
	{
		int cr = connect(sfd, rp->ai_addr, rp->ai_addrlen);
		if (cr == 0) {
			break;
		} else if (cr == -1) {
			perror("connect");
			continue;
		}
	}
	
//	assert(rp == result);

	freeaddrinfo(result);

	if (rp == NULL) {
		fprintf(stderr, "Could not connect\n");
		close(sfd);
		return -1;
	}

#endif

	addr.sin_family = AF_INET;
	addr.sin_port = htons(8888);
	inet_pton(AF_INET, "10.0.2.15", &addr.sin_addr.s_addr);
	res = connect(sfd, (struct sockaddr*)&addr, sizeof addr);
	if (res < 0) {
		perror("connect");
		return -1;
	}
	
	shrink_socket_send_buffer(sfd);

	return sfd;
	
}

int send_data(int fd, char *data)
{
	int sr;
	uint16_t len = (uint16_t)(rand()*1.0/RAND_MAX*strlen(data));

	sr = send(fd, &len, sizeof len, 0);
	if (sr < 0) {
		perror("send");
	}

	sr = send(fd, data, len, 0);
	if (sr < 0) {
		perror("send");
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

	while (1) {
		sfd = connect_server("192.168.56.1", "8888");
		if (sfd == -1) {
			printf("Will connect again.\n");
			sleep(30);
			sfd = connect_server("192.168.56.1", "8888");
			if (sfd == -1) {
				printf("Still cannot connect\n");
				break;
			}
		}

		print_local_port(sfd);

		count = send_data(sfd, (char*)str);
		printf("[%x:%d] %d bytes sent\n",
				(unsigned int)pthread_self(),
				sfd, count);

		count = recv_data(sfd, buf, sizeof(buf)-1);
		buf[count] = '\0';
		printf("[%x:%d] %d: %s\n", (unsigned int)pthread_self(),
				sfd, count, buf);

		close(sfd);

	}

	printf("[%x] thread end\n", (unsigned int)pthread_self());
	return NULL;
}

#define THREADNUM 1015

int main(int argc, char *argv[])
{
	int i;

	pthread_t tid[THREADNUM];

	if (argc != 2) {
		fprintf(stderr, "Usage: %s message\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < THREADNUM; ++i) {
		pthread_create(&tid[i], NULL, &doit, argv[1]);
	}

	for (i = 0; i < THREADNUM; ++i) {
		pthread_join(tid[i], NULL);
	}

	return 0;
}


#include "EpollServer.h"
#include "TcpConnection.h"

#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

int SocketFD::closeFD()
{
	int fd = mFD;
	mFD = -1;
	return close(fd);
}

static int create_and_bind (const char *port)
{
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int res, sfd;

	memset(&hints, 0, sizeof (struct addrinfo));
	hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
	hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
	hints.ai_flags = AI_PASSIVE;     /* All interfaces */

	res = getaddrinfo(NULL, port, &hints, &result);
	if (res != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror (res));
		return -1;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next)
	{
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd == -1)
			continue;

		res = bind (sfd, rp->ai_addr, rp->ai_addrlen);
		if (res == 0)
		{
			/* We managed to bind successfully! */
			break;
		}

		close(sfd);
	}

	if (rp == NULL)
	{
		fprintf(stderr, "Could not bind\n");
		return -1;
	}

	freeaddrinfo(result);

	return sfd;
}

static int make_socket_non_blocking (int sfd)
{
	int flags, res;

	flags = fcntl(sfd, F_GETFL, 0);
	if (flags == -1)
	{
		perror ("fcntl");
		close(sfd);
		return -1;
	}

	flags |= O_NONBLOCK;
	res = fcntl (sfd, F_SETFL, flags);
	if (res == -1)
	{
		perror ("fcntl");
		close(sfd);
		return -1;
	}

	return 0;
}

static int accept_connection(int sfd)
{
	struct sockaddr in_addr;
	socklen_t in_len;
	int infd;
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	int res;

	in_len = sizeof in_addr;
	infd = accept(sfd, &in_addr, &in_len);
	if (infd == -1)
	{
		if ((errno == EAGAIN) ||
				(errno == EWOULDBLOCK))
		{
			/* We have processed all incoming
			   connections. */
			return -1;
		}
		else
		{
			perror ("accept");
			return -1;
		}
	}

	res = getnameinfo (&in_addr, in_len,
			hbuf, sizeof hbuf,
			sbuf, sizeof sbuf,
			NI_NUMERICHOST | NI_NUMERICSERV);
	if (res == 0)
	{
		printf("Accepted connection on descriptor %d "
				"(host=%s, port=%s, thread=%x)\n", infd, hbuf, sbuf, pthread_self());
	}

	return infd;
}

#define MAXEVENTS 64

void EpollServer::acceptAllConnection()
{
	struct epoll_event event;

	while (1) {
		int infd = accept_connection(mFD);
		if (infd == -1) {
			break;
		}
		
		int res = make_socket_non_blocking(infd);
		
		if (res == -1) {
			break;
		}

		event.data.ptr = (SocketFD*)new TcpConnection(infd);
		event.events = EPOLLIN | EPOLLET;
		res = epoll_ctl (mEPFD, EPOLL_CTL_ADD, infd, &event);
		if (res == -1)
		{
			perror ("epoll_ctl");
			abort ();
		}
	}
}

int EpollServer::init(const char *port)
{
	int sfd;
	int res;
	struct epoll_event event;

	sfd = create_and_bind(port);
	if (sfd == -1)
		abort ();

	res = make_socket_non_blocking(sfd);
	if (res == -1)
		abort ();

	res = listen(sfd, SOMAXCONN);
	if (res == -1)
	{
		perror ("listen");
		abort ();
	}

	mFD = sfd;

	mEPFD = epoll_create1 (0);
	if (mEPFD == -1)
	{
		perror ("epoll_create");
		abort ();
	}

	event.data.ptr = (SocketFD*)this;
	event.events = EPOLLIN | EPOLLET;
	res = epoll_ctl (mEPFD, EPOLL_CTL_ADD, sfd, &event);
	if (res == -1)
	{
		perror ("epoll_ctl");
		abort ();
	}

}

int EpollServer::run(int thread_number)
{
	pthread_t *tid;

	tid = (pthread_t*)malloc(thread_number * sizeof(*tid));

	for (int i = 0; i < thread_number; ++i) {
		pthread_create(&tid[i], NULL, &staticRunLoop, (EventLoop*)this);
		printf("thread %x created\n", tid[i]);
	}

	for (int i = 0; i < thread_number; ++i) {
		pthread_join(tid[i], NULL);
	}
}

int EpollServer::stop()
{
}

void *EpollServer::runLoop()
{
	struct epoll_event *events;

	events = (struct epoll_event*)calloc(MAXEVENTS, sizeof *events);

	while(1)
	{
		int n = epoll_wait(mEPFD, events, MAXEVENTS, -1);
		for (int i = 0; i < n; ++i)
		{
			if ((events[i].events & EPOLLERR) ||
				(events[i].events & EPOLLHUP) ||
				(!(events[i].events & EPOLLIN)))
			{
				if ((SocketFD*)this == events[i].data.ptr) {
					fprintf(stderr, "epoll error on listening fd\n");
				} else {
					fprintf(stderr, "epoll error on working fd\n");
					((TcpConnection*)(SocketFD*)events[i].data.ptr)->closeConnection();
				}
			} else if ((SocketFD*)this == events[i].data.ptr) {
				acceptAllConnection();
			} else {
				((TcpConnection*)(SocketFD*)events[i].data.ptr)->readAllData();
			}

		}
	}
	free(events);
	return NULL;
}


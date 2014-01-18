#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <syslog.h>
#include <assert.h>

#include "EpollServer.h"
#include "ObjectQueue.h"
#include "Connection.h"
#include "ConnectionManager.h"

#define SYSLOG_ERROR(x) syslog(LOG_ERR, "[%s:%d]%s: %s", __FILE__, __LINE__, x, strerror(errno))

ConnectionManager gConnectionManager(1024);

EpollServer gEpollServer;

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
//		syslog(LOG_ERROR, "getaddrinfo: %s\n", gai_strerror (res));
		SYSLOG_ERROR("getaddrinfo");
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
//		syslog(LOG_ERROR, "%s", "Could not bind\n");
		SYSLOG_ERROR("bind");
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
//		syslog(LOG_ERROR, "%s: %s", "fcntl", strerror(errno));
		SYSLOG_ERROR("fcntl");
		close(sfd);
		return -1;
	}

	flags |= O_NONBLOCK;
	res = fcntl (sfd, F_SETFL, flags);
	if (res == -1)
	{
//		syslog(LOG_ERROR, "%s: %s", "fcntl", strerror(errno));
		SYSLOG_ERROR("fcntl");
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
			SYSLOG_ERROR("accept");
			return -1;
		}
	}

	res = getnameinfo (&in_addr, in_len,
			hbuf, sizeof hbuf,
			sbuf, sizeof sbuf,
			NI_NUMERICHOST | NI_NUMERICSERV);
	if (res == 0)
	{
		printf("[%x:%d] Accepted connection"
				"(host=%s, port=%s)\n",
				 (unsigned int)pthread_self(), infd, hbuf, sbuf);
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

		event.data.ptr = (SocketFD*)gConnectionManager.get(infd);
		event.events = EPOLLIN | EPOLLET;
		res = epoll_ctl (mEPFD, EPOLL_CTL_ADD, infd, &event);
		if (res == -1)
		{
			SYSLOG_ERROR("epoll_ctl");
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
		SYSLOG_ERROR("listen");
		abort ();
	}

	mFD = sfd;

	mEPFD = epoll_create1 (0);
	if (mEPFD == -1)
	{
		SYSLOG_ERROR("epoll_create");
		abort ();
	}

	event.data.ptr = (SocketFD*)this;
	event.events = EPOLLIN | EPOLLET;
	res = epoll_ctl (mEPFD, EPOLL_CTL_ADD, sfd, &event);
	if (res == -1)
	{
		SYSLOG_ERROR("epoll_ctl");
		abort ();
	}

	return 0;
}

int EpollServer::run(int thread_number)
{
	pthread_t *tid;

	tid = (pthread_t*)malloc(thread_number * sizeof(*tid));

	for (int i = 0; i < thread_number; ++i) {
		pthread_create(&tid[i], NULL, &staticRunLoop, (EventLoop*)this);
		syslog(LOG_INFO, "thread %x created\n", (unsigned int)tid[i]);
	}

	for (int i = 0; i < thread_number; ++i) {
		pthread_join(tid[i], NULL);
	}

	return 0;
}

int EpollServer::stop()
{
	return 0;
}

void *EpollServer::runLoop()
{
	struct epoll_event *events;

	events = (struct epoll_event*)calloc(MAXEVENTS, sizeof *events);

	while(1)
	{
		int n = epoll_wait(mEPFD, events, MAXEVENTS, -1);
//		for ( int i = 0; i < n; ++i) {
//			syslog(LOG_INFO, "%d %d %d %d", i, ((Connection*)(SocketFD*)events[i].data.ptr)->getFD(),
//					events[i].events & EPOLLIN, events[i].events & EPOLLOUT);
//		}
		for (int i = 0; i < n; ++i)
		{
			if ((events[i].events & EPOLLERR) ||
				(events[i].events & EPOLLHUP) ||
				((!(events[i].events & EPOLLIN)) &&
				!(events[i].events & EPOLLOUT)))
			{
				if ((SocketFD*)this == events[i].data.ptr) {
					syslog(LOG_INFO, "%s", "epoll error on listening fd\n");
				} else {
					syslog(LOG_INFO, "%s", "epoll error on working fd\n");
					((Connection*)(SocketFD*)events[i].data.ptr)->closeConnection();
				}
			} else if ((SocketFD*)this == events[i].data.ptr) {
				acceptAllConnection();
			} else {
				assert((events[i].events & EPOLLOUT) || (events[i].events & EPOLLIN));
				if (events[i].events & EPOLLOUT) {
					((Connection*)(SocketFD*)events[i].data.ptr)->sendBufferedData();
				}
				if (events[i].events & EPOLLIN) {
					((Connection*)(SocketFD*)events[i].data.ptr)->readAllData();
				}
			}

		}
	}
	free(events);
	return NULL;
}

int EpollServer::pollSending(int fd, void *ptr)
{
	struct epoll_event event;
	int res;

	event.data.ptr = ptr;
	event.events = EPOLLOUT | EPOLLET | EPOLLIN;
	res = epoll_ctl(mEPFD, EPOLL_CTL_MOD, fd, &event);
	if (res == -1)
	{
		SYSLOG_ERROR("epoll_ctl");
		return -1;
	} else {
		return 0;
	}
}

int EpollServer::stopSending(int fd, void *ptr)
{
	struct epoll_event event;
	int res;

	event.data.ptr = ptr;
	event.events = EPOLLET | EPOLLIN;
	res = epoll_ctl(mEPFD, EPOLL_CTL_MOD, fd, &event);
	if (res == -1)
	{
		SYSLOG_ERROR("epoll_ctl");
		return -1;
	} else {
		return 0;
	}
}


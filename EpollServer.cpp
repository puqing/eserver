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

extern ConnectionManager gConnectionManager;

EpollServer gEpollServer;

/*int SocketFD::closeFD()
{
//	int fd = mFD;
//	mFD = -1;
//	return close(fd);
	return close(mFD);
}*/

static int create_and_bind (int port)
{
//	struct addrinfo hints;
//	struct addrinfo *result, *rp;
	int res, sfd;

#if 0
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
#endif

	struct sockaddr_in addr;

	sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sfd == -1) {
		SYSLOG_ERROR("socket");
		return -1;
	}

	memset(&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
//	inet_pton(AF_INET, "10.0.2.15", &addr.sin_addr.s_addr);
	res = bind(sfd, (struct sockaddr*)&addr, sizeof addr);
	if (res == -1) {
		SYSLOG_ERROR("bind");
		return -1;
	}

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
		syslog(LOG_INFO, "[%x:%d] Accepted connection"
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

		res = shrink_socket_send_buffer(infd);
		if (res == -1) {
			break;
		}

		event.data.ptr = (Connection*)gConnectionManager.get(infd);
		event.events = EPOLLIN | EPOLLET;
		res = epoll_ctl (mEPFD, EPOLL_CTL_ADD, ((Connection*)event.data.ptr)->getFD(), &event);
		if (res == -1)
		{
			SYSLOG_ERROR("epoll_ctl");
			abort ();
		}

/*		event.events = EPOLLOUT | EPOLLET;
		res = epoll_ctl (mEPFDW, EPOLL_CTL_ADD, ((Connection*)event.data.ptr)->getFD(), &event);
		if (res == -1)
		{
			SYSLOG_ERROR("epoll_ctl");
			abort ();
		}*/
	}
}

int EpollServer::init(int port)
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

/*	mEPFDW = epoll_create1 (0);
	if (mEPFDW == -1)
	{
		SYSLOG_ERROR("epoll_create");
		abort ();
	}*/

	event.data.ptr = this;
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
		syslog(LOG_INFO, "Begin read poll %x: %d:", (unsigned int)pthread_self(), mEPFD);
		int n = epoll_wait(mEPFD, events, MAXEVENTS, -1);
		for ( int i = 0; i < n; ++i) {
			syslog(LOG_INFO, "Read poll %x: %d :%d: %d %d", (unsigned int)pthread_self(), i, ((Connection*)events[i].data.ptr)->getFD(),
					events[i].events & EPOLLIN, events[i].events & EPOLLOUT);
		}
		for (int i = 0; i < n; ++i)
		{
			if ((events[i].events & EPOLLERR) ||
				(events[i].events & EPOLLHUP) ||
				!((events[i].events & EPOLLIN) || (events[i].events & EPOLLOUT)))
			{
				if (this == events[i].data.ptr) {
					syslog(LOG_INFO, "%s:%d: events = %x", "epoll error on listening fd", this->mFD, events[i].events);
				} else {
					perror("epoll_wait");
					syslog(LOG_INFO, "%s:%d: events = %x", "epoll error on working fd", ((Connection*)events[i].data.ptr)->getFD(), events[i].events);
					((Connection*)events[i].data.ptr)->closeConnection();
				}
			} else if (this == events[i].data.ptr) {
				acceptAllConnection();
			} else if (events[i].events & EPOLLIN) {
				((Connection*)events[i].data.ptr)->readData();
			} else {
				assert(events[i].events & EPOLLOUT);
				((Connection*)events[i].data.ptr)->sendBufferedData(false);
			}

		}

/*		syslog(LOG_INFO, "Begin write poll %x: %d:", (unsigned int)pthread_self(), mEPFDR);
		n = epoll_wait(mEPFDW, events, MAXEVENTS, -1);
		for ( int i = 0; i < n; ++i) {
			syslog(LOG_INFO, "Write poll %x: %d :%d: %d %d", (unsigned int)pthread_self(), i, ((Connection*)events[i].data.ptr)->getFD(),
					events[i].events & EPOLLIN, events[i].events & EPOLLOUT);
		}
		for (int i = 0; i < n; ++i)
		{
			assert(this != events[i].data.ptr);
			if ((events[i].events & EPOLLERR) ||
				(events[i].events & EPOLLHUP) ||
				!(events[i].events & EPOLLOUT))
			{
				syslog(LOG_INFO, "%s:%d: events = %x", "epoll error on working fd", ((Connection*)events[i].data.ptr)->getFD(), events[i].events);
				((Connection*)events[i].data.ptr)->closeConnection();
			} else {
				assert(events[i].events & EPOLLOUT);
				((Connection*)events[i].data.ptr)->sendBufferedData();
			}

		}*/
	}

	free(events);
	return NULL;
}

int EpollServer::rearmOut(Connection *conn, bool rearm) //int fd, void *ptr)
{
	struct epoll_event event;
	int res;

	syslog(LOG_INFO, "[%x:%x:%d:] Rearm out %d", (unsigned int)conn, (unsigned int)pthread_self(), conn->getFD(), rearm);
	event.data.ptr = conn;
	event.events = EPOLLET | EPOLLIN | (rearm?EPOLLOUT:0);
	res = epoll_ctl(mEPFD, EPOLL_CTL_MOD, conn->getFD(), &event);
	if (res == -1)
	{
		SYSLOG_ERROR("epoll_ctl");
		return -1;
	} else {
		return 0;
	}
}


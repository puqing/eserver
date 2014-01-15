#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include "EpollServer.h"
#include "ObjectQueue.h"
#include "Connection.h"
#include "ConnectionManager.h"

void Connection::readAllData()
{
	int done = 0;
	while (1)
	{
		ssize_t count;
		char buf[512];

		count = read(mFD, buf, sizeof buf);
		if (count == -1)
		{
			/* If errno == EAGAIN, that means we have read all
			   data. So go back to the main loop. */
			if (errno != EAGAIN)
			{
/*				printf("[%x:%x:%d] ",
						(unsigned int)this,
						(unsigned int)pthread_self(), mFD);*/
				perror ("read");
				done = 1;
			}
			break;
		}
		else if (count == 0)
		{
			/* End of file. The remote has closed the
			   connection. */
			done = 1;
			break;
		} else {
			/* Write the buffer to standard output */
			buf[count]='\0';
			printf("[%x:%x:%d] ", (unsigned int)this, (unsigned int)pthread_self(), mFD);
			printf("%s\n", buf);
		}
	}

	if (done)
	{
		/* Closing the descriptor will make epoll remove it
		   from the set of descriptors which are monitored. */

		if (mFD != -1) {
			printf ("[%x:%x:%d] Closing connection\n",
				(unsigned int)this, (unsigned int)pthread_self(), mFD);
			closeConnection();
		}
	}
}

extern ConnectionManager gConnectionManager;

void Connection::closeConnection()
{
	closeFD();
//	delete this;
	gConnectionManager.recycle(this);
}


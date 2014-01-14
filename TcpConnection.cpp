#include "EpollServer.h"
#include "TcpConnection.h"

#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

void TcpConnection::readAllData()
{
	int done = 0;
	int res;

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
		}

		/* Write the buffer to standard output */
		res = write(1, buf, count);
		if (res == -1)
		{
			perror ("write");
			abort ();
		}

		res = write(1, "\n", 1);
		if (res == -1)
		{
			perror ("newline");
			abort ();
		}

	}

	if (done)
	{
		printf ("Closed connection on descriptor %d\n", mFD);

		/* Closing the descriptor will make epoll remove it
		   from the set of descriptors which are monitored. */
		closeConnection();
	}
}

void TcpConnection::closeConnection()
{
	closeFD();
	delete this;
}


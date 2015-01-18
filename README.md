`eserver' is a server framework based on epoll and the edge-triggered (ET)
mode. It contains a library which handles the socket listening and connections,
as well as a server program and a client program for testing. The header file
`esvr.h' defines the interface of the library.

In the EPOLLET mode, the thread number should be equal to or a little more than
the number of CPU cores. Thread number can be configured in the server.c and
client.c files.

The code uses syslog for logging, the macro LOG_LIMIT is used for configuring
log level. Set it to LOG_INFO for performance testing and LOG_DEBUG for
debugging.

In order to run the testing server/client programs, the maximum number of
opening files need to be increased. It can be configured in the file
/etc/security/limits.conf:

username	soft	nofile		40000
username	hard	nofile		40000

And re-login is required for activating the configuration.


EServer is a multi-thread server based on epoll. It uses the
Edge-triggered(EPOLLET) mode, and launches multiple threads to do
epoll_wait(2). In this model, the number of threads should be set the same as or slightly over the CPU number.

Here are some issues I encountered and some considerations during the implementation of the server.

1. EPOLLET is not really edge-triggered, in multi-threading environment

While a thread has not finished reading the buffer of an fd, which has EPOLLET flag set, another thread could still get an EPOLLIN event on the fd from its own epoll_wait(2).

EPOLLONESHOT could be used to ensure that exactly one event will be returned. But it is not used in this code, as I didn't figured out how to make it consistent with the handling of EPOLLOUT events.  Instead, I find that it is not too difficult to handle the multiple-event problem by specifying user-space flags on each fd.

2. There should be no more than one thread reading an fd, and no more than one thread writing an fd at the same time.

If multiple threads are reading the same fd, and they share the same reading buffer, one reading operation will need to lock the buffer, and cause other threads to wait. As we are using asynchronous modle, during this period of time, the other threads are unable to handle the events on other fds too. This will become a performance issue.

If each thread uses its own buffer, the data of a single fd will be read into different buffers. Then the messages in the TCP data-stream is possible to be splitted. It will be a difficult problem to handle the splitted messages.

Writing operations on an fd cannot be put in different threads too. The write(2) system call can return a prositive number smaller than the size of the data specified to send. If this happens, we need to call write(2) again to send the rest data. If another thread is writing the fd before the second call, the messages would be interleaved.

3. Send(2) data first, and handle EPOLLOUT only if EAGAIN or EWOULDBLOCK is encountered.

When the file descriptors are added to epoll, they were not set with EPOLLOUT. If some data need to be sent, it will be sent directly with send(2). Only if -1 is returned, and errno is EAGAIN or EWOULDBLOCK, should the EPOLLOUT be setup, by calling epoll_ctl(2) and EPOLL_CTL_MOD.

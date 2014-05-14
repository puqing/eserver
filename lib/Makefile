TARGET=libesvr.a
CFLAGS=-pthread -g -Wall
ARFLAGS=rcs

$(TARGET): server.o poller.o connection.o worker.o
	ar $(ARFLAGS) $@ $^

.c.o:
	gcc -c $< -o $@ $(CFLAGS)

clean:
	rm -f *.o $(TARGET)


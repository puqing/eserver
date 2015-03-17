TARGET=libesvr.a
CFLAGS=-pthread -g -Wall -I../include
ARFLAGS=rcs

$(TARGET): es_service.o es_poller.o es_conn.o es_worker.o es_connmgr.o
	ar $(ARFLAGS) $@ $^

.c.o:
	gcc -c $< -o $@ $(CFLAGS)

clean:
	rm -f *.o $(TARGET)


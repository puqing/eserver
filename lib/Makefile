TARGET=libesvr.a
CFLAGS = -I ../include -Wall -Wno-unused-parameter -Werror=declaration-after-statement -O2 -march=x86-64 -pthread
ARFLAGS=rcs

$(TARGET): es_service.o es_poller.o es_conn.o es_worker.o es_connmgr.o
	ar $(ARFLAGS) $@ $^

.c.o:
	gcc -c $< -o $@ $(CFLAGS)

clean:
	rm -f *.o $(TARGET)


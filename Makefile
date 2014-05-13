EXE=server client
CFLAGS=-pthread -g -Wall
LDFLAGS=-pthread -static

all: $(EXE)

server: main.o server.o poller.o connection.o worker.o
	gcc $^ -o $@ $(LDFLAGS)

client: client.o
	gcc $^ -o $@ $(LDFLAGS)

.c.o:
	gcc -c $< -o $@ $(CFLAGS)

clean:
	rm -f *.o $(EXE)


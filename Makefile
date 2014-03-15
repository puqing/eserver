EXE=server client
CFLAGS=-pthread -g -Wall
LDFLAGS=-pthread -static

all: $(EXE)

server: epollserver.o connection.o main.o worker.o
	gcc $^ -o $@ $(LDFLAGS)

client: client.o
	gcc $^ -o $@ $(LDFLAGS)

.c.o:
	gcc -c $< -o $@ $(CFLAGS)

clean:
	rm -f *.o $(EXE)


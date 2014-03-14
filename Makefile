EXE=server client
CFLAGS=-pthread -g -Wall
LDFLAGS=-pthread -static

all: $(EXE)

server: EpollServer.o Connection.o ObjectQueue.o ConnectionManager.o Main.o
	g++ $^ -o $@ $(LDFLAGS)

client: client.o
	gcc $^ -o $@ $(LDFLAGS)

.cpp.o:
	g++ -c $< -o $@ $(CFLAGS)

.c.o:
	gcc -c $< -o $@ $(CFLAGS)

clean:
	rm -f *.o $(EXE)

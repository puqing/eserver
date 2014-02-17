EXE=server client
CFLAGS=-pthread
LDFLAGS=-pthread -static

all: $(EXE)

server: EpollServer.o Connection.o ObjectQueue.o ConnectionManager.o Main.o
	g++ $^ -o $@ $(LDFLAGS)

client: client.o
	gcc $^ -o $@ $(LDFLAGS)

.cpp.o:
	g++ -c $< -o $@ -Wall $(CFLAGS)

.c.o:
	gcc -c $< -o $@ -Wall -pthread

clean:
	rm -f *.o $(EXE)

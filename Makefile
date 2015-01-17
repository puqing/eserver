EXE=server client client1
LIB=lib/libesvr.a
CFLAGS=-pthread -g -Wall -I ./include
LDFLAGS=-L./lib -lesvr -pthread -static

all: $(EXE) $(LIB)

.PHONY: $(LIB)

$(LIB):
	(cd lib && make)

server: server.o $(LIB)
	gcc server.o -o $@ $(LDFLAGS)

client: client.o
	gcc $^ -o $@ $(LDFLAGS)

client1: client1.o
	gcc $^ -o $@ $(LDFLAGS)

.c.o:
	gcc -c $< -o $@ $(CFLAGS)

clean:
	rm -f *.o $(EXE)
	(cd lib && make clean)


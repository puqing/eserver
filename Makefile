EXE=server client
LIB=lib/libesvr.a
CFLAGS=-pthread -g -Wall -I ./lib
LDFLAGS=-L./lib -lesvr -pthread

all: $(EXE) $(LIB)

$(LIB):
	(cd lib && make)

server: server.o $(LIB)
	gcc $^ -o $@ $(LDFLAGS)

client: client.o
	gcc $^ -o $@ $(LDFLAGS)

.c.o:
	gcc -c $< -o $@ $(CFLAGS)

clean:
	rm -f *.o $(EXE)
	(cd lib && make clean)


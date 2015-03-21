EXE=server client
LIB=lib/libesvr.a
CFLAGS=-I ./include -Wall -Wno-unused-parameter -Werror=declaration-after-statement -O2 -march=x86-64 -g -pthread
LDFLAGS=-L./lib -lesvr -pthread -static

all: $(LIB) $(EXE)

.PHONY: $(LIB)

$(LIB):
	cd lib && make

server: server.o $(LIB)
	gcc server.o -o $@ $(LDFLAGS)

client: client.o $(LIB)
	gcc client.o -o $@ $(LDFLAGS)

.c.o:
	gcc -c $< -o $@ $(CFLAGS)

clean:
	rm -f *.o $(EXE)
	cd lib && make clean


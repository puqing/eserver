EXE=server client client_th
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

client_th: client_th.o
	gcc $^ -o $@ $(LDFLAGS)

.c.o:
	gcc -c $< -o $@ $(CFLAGS)

clean:
	rm -f *.o $(EXE)
	(cd lib && make clean)


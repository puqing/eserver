EXE=server client
CFLAGS=`mysql_config --cflags` -pthread
LDFLAGS=`mysql_config --libs_r` -pthread

server: EpollServer.o Connection.o DBConnection.o ObjectQueue.o ConnectionManager.o Main.o
	g++ $^ -o $@ $(LDFLAGS)

client: client.o
	gcc $^ -o $@ -pthread

.cpp.o:
	g++ -c $< -o $@ -Wall $(CFLAGS)

.c.o:
	gcc -c $< -o $@ -Wall -pthread

clean:
	rm -f *.o $(EXE)

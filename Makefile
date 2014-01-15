EXE=server

server: EpollServer.o Connection.o ObjectQueue.o ConnectionManager.o Main.o
	g++ $^ -o $@ -lpthread

.cpp.o:
	g++ -c $< -o $@ -Wall -g

clean:
	rm -f *.o $(EXE)

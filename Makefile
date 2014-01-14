EXE=server

server: EpollServer.o TcpConnection.o Main.o
	g++ $^ -o $@

.cpp.o:
	g++ -c $< -o $@

clean:
	rm -f *.o $(EXE)

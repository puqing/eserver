EXE=server
CFLAGS=`mysql_config --cflags`
LDFLAGS=`mysql_config --libs_r`

server: EpollServer.o Connection.o DBConnection.o ObjectQueue.o ConnectionManager.o Main.o
	g++ $^ -o $@ $(LDFLAGS)

.cpp.o:
	g++ -c $< -o $@ -Wall $(CFLAGS)

clean:
	rm -f *.o $(EXE)

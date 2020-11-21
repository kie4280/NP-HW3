all: server.out client.out

server.out: socket.h socket.cpp server/database.h server/database.cpp server/server.cpp
	g++ -g -pthread -o server.out socket.h socket.cpp server/database.h server/database.cpp server/server.cpp -lsqlite3

client.out: socket.h socket.cpp client/client.cpp
	g++ -g -pthread -o client.out socket.h socket.cpp client/client.cpp

test: test1.cpp socket.h socket.cpp
	g++ -g -pthread -o test1 test1.cpp socket.h socket.cpp
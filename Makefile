all: server.out client.out

server.out: socket.h socket.cpp server/database.h server/database.cpp server/server.cpp
	g++ -g -pthread -o server.out socket.h socket.cpp server/database.h server/database.cpp server/server.cpp -lsqlite3 -I .

client.out: socket.h socket.cpp client/client_part.cpp client/server_part.cpp client/client.cpp client/client.h
	g++ -g -pthread -o client.out socket.h socket.cpp client/client_part.cpp client/server_part.cpp client/client.cpp client/client.h -I .

test: test1.cpp socket.h socket.cpp
	g++ -g -pthread -o test1 test1.cpp socket.h socket.cpp
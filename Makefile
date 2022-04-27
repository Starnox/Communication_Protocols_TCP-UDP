CC = g++ 
CFLAGS = -Wall -Wextra -g -std=c++14 

# The port on which the server listens
PORT = 12345

# IP address of the server
IP_SERVER = 127.0.0.1 

all: server subscriber

.PHONY: clean run_server

server: server.o tcp_client.o helpers.o
	$(CC) $(CFLAGS) -o server server.o tcp_client.o helpers.o
 
server.o: server.cpp helpers.h tcp_client.h 
	$(CC) $(CFLAGS) -c server.cpp

subscriber: subscriber.o helpers.o
	$(CC) $(CFLAGS) -o subscriber subscriber.o helpers.o

subscriber.o : subscriber.cpp helpers.h
	$(CC) $(CFLAGS) -c subscriber.cpp
 
tcp_client.o: tcp_client.h tcp_client.cpp
	$(CC) $(CFLAGS) -c tcp_client.cpp

helpers.o: helpers.h helpers.cpp
	$(CC) $(CFLAGS) -c helpers.cpp

# Run sever
run_server:
	./server ${PORT}

clean:
	rm -f server server.o subscriber subscriber.o tcp_client tcp_client.o helpers helpers.o

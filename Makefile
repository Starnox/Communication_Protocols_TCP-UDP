CC = g++ 
CFLAGS = -Wall -g -std=c++14 

# The port on which the server listens
PORT = 12345

# IP address of the server
IP_SERVER = 127.0.0.1 

all: server subscriber

.PHONY: clean run_server run_subscriber

server: server.o tcp_client.o
	$(CC) $(CFLAGS) -o server server.o tcp_client.o
 
server.o: server.cpp helpers.h tcp_client.h 
	$(CC) $(CFLAGS) -c server.cpp

subscriber: subscriber.o
	$(CC) $(CFLAGS) -o subscriber subscriber.o

subscriber.o : subscriber.cpp helpers.h
	$(CC) $(CFLAGS) -c subscriber.cpp
 
tcp_client.o: tcp_client.h

# Run sever
run_server:
	./server ${PORT}

clean:
	rm -f server client tcp_client
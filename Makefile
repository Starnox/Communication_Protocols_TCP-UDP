CFLAGS = -Wall -g

# The port on which the server listens
PORT = 12345

# IP address of the server
IP_SERVER = 127.0.0.1

all: server subscriber

# compile server.c
server: server.c

# compile subscriber.c
subscriber: subscriber.c

.PHONY: clean run_server run_subscriber

# Run sever
run_server:
	./server ${PORT}

# Run client
run_subscriber:
	./subscriber ${IP_SERVER} ${PORT}

clean:
	rm -f server client

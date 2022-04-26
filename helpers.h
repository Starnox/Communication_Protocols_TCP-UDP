#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

/*
 * Macro de verificare a erorilor
 * Exemplu:
 *     int fd = open(file_name, O_RDONLY);
 *     DIE(fd == -1, "open failed");
 */

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#define BUFLEN		1600	// maximum buffer size
#define PAYLOAD_LEN 1500
#define TOPIC_LEN 50
#define MAX_CLIENTS	200	// the maximum number of clients that can be connected at any given time
#define ID_SIZE 10

// the packet that will be sent over tcp to the clients
#pragma pack(1)
typedef struct message {
	uint16_t len;
	struct in_addr ip;
	in_port_t port;
	char topic[TOPIC_LEN];
	uint8_t type;
	char payload[PAYLOAD_LEN];
}message;

void disable_nagle(int socket);
void pack(message *msg, char *buf, uint16_t size);
message unpack(int size, char *buf);
void sendall(int fd, char *buf, int len);
int recvall(int fd, char *buf) ;

#endif

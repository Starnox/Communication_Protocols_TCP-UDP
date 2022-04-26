#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cmath>
#include "helpers.h"

void usage(char *file)
{
	fprintf(stderr, "Usage: %s <ID_Client> <IP_Server> <Port_Server>\n", file);
	exit(0);
}

bool check_id(int sockfd, char id[ID_SIZE]) {
	int n = send(sockfd, id, strlen(id), 0);
	DIE(n < 0, "send id fail");
	
	char buffer[BUFLEN];
	n = recv(sockfd, buffer, BUFLEN, 0);
	if(strncmp(buffer, "Fail", 4) == 0)
		return false;

	return true;
	
}

void format_message(message &msg) {
	
	printf("%s:%d - %s - ", inet_ntoa(msg.ip), ntohs(msg.port), msg.topic);
	if(msg.type == 0) { 
		uint8_t sign = *msg.payload;
		uint32_t *num = ((uint32_t *) (msg.payload+1));
		*num = ntohl(*num);
		if(sign == 0)
			printf("INT - %d", *num);
		else
			printf("INT - -%d", *num);
	}
	else if(msg.type == 1) {
		uint16_t *num = ((uint16_t *) (msg.payload));
		*num = ntohs(*num);
 		printf("SHORT_REAL - %.2f", (*num * 1.0)/100);
	}
	else if(msg.type == 2) {
		uint8_t sign = *msg.payload;
		uint32_t *num = ((uint32_t *) (msg.payload+1));
		*num = ntohl(*num);
		uint8_t mod = *(msg.payload + 5);
		if(sign == 0)
			printf("FLOAT - %g", (*num * 1.0) * pow(10, -mod));
		else
			printf("FLOAT - -%g", (*num * 1.0) * pow(10, -mod));
		
	}
	else
		printf("STRING - %s", msg.payload);

	printf("\n");

}

int main(int argc, char *argv[])
{
	// disable output buffering
	setvbuf(stdout, NULL, _IONBF, BUFSIZ); 

	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];

	if (argc < 4) {
		usage(argv[0]);
	}
	
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3])); // set the port
	ret = inet_aton(argv[2], &serv_addr.sin_addr); // set the ip
	DIE(ret == 0, "inet_aton");

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

	// send the id to check if it is unique
	if (check_id(sockfd, argv[1]) == false) {
		// the id is already connected
		close(sockfd);
		return 0;
	}

	fd_set read_fds;	// set of descriptors we will use in select
	fd_set tmp_fds;		// temporary set of descriptors so we don't lose the original ones

	// clear the set of descriptors
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	// add stdin and the server socket to the set
	FD_SET(0, &read_fds);
	FD_SET(sockfd, &read_fds);

	disable_nagle(sockfd);
	int fdmax = sockfd;

	while (1) {
		tmp_fds = read_fds; 
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		// 0 is the file descriptor that represents console input
		// check if we have recieved input
		if(FD_ISSET(0, &tmp_fds)){
			// clear the buffer
			memset(buffer, 0, BUFLEN);
			
			// gets the data from the consoles
			fgets(buffer, BUFLEN - 1, stdin);
			if(buffer[strlen(buffer) - 1] == '\n')
				buffer[strlen(buffer) - 1] = '\0';

			// compare the first 4 characters with the word exit
			if (strncmp(buffer, "exit", 4) == 0) {
				break; // break out of the loop
			}

			// send the command to the server
			n = send(sockfd, buffer, strlen(buffer), 0);
			DIE(n < 0, "send");
		}
		// if we recieved information on the socket that is connected to the server
		if(FD_ISSET(sockfd, &tmp_fds)){
			// gets the data
			memset(buffer, 0, BUFLEN);
			int n = recvall(sockfd, buffer);
			DIE(n < 0, "recvall failed");
			if (n == 0) { // the server closed the connection
				break;
			}

			if(buffer[strlen(buffer) - 1] == '\n')
				buffer[strlen(buffer) - 1] = '\0';

			message msg = unpack(n, buffer); // unpack the message
			format_message(msg);

			// in case the server closed the connection, disconnect the subscriber
			
		}
	}

	close(sockfd);

	return 0;
}

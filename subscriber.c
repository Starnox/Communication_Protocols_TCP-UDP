#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helpers.h"

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_address server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	// disable output buffering
	setvbuf(stdout, NULL, _IONBF, BUFSIZ); 

	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];

	if (argc < 3) {
		usage(argv[0]);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[2]));
	ret = inet_aton(argv[1], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

	fd_set read_fds;	// set of descriptors we will use in select
	fd_set tmp_fds;		// temporary set of descriptors so we don't lose the original ones

	// clear the set of descriptors
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	// add stdin and the server socket to the set
	FD_SET(0, &read_fds);
	FD_SET(sockfd, &read_fds);
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
			n = recv(sockfd, buffer, BUFLEN, 0);

			// in case the server closed the connection, disconnect the subscriber
			if (n == 0) {
				break;
			}

			if(buffer[strlen(buffer) - 1] == '\n')
				buffer[strlen(buffer) - 1] = '\0';

			printf("Received: %s\n", buffer);
			DIE(n < 0, "recv_client");
		}
	}

	close(sockfd);

	return 0;
}

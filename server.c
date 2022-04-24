#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "helpers.h"

void print_usage_message(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	// disable output buffering
	setvbuf(stdout, NULL, _IONBF, BUFSIZ); 

	// declaration
	int listenfd, subscriberfd, portno;
	char buffer[BUFLEN];

	struct sockaddr_in serv_addr, cli_addr;
	int n, i, ret;

	socklen_t subscriber_len;

	fd_set read_fds;	// set of descriptors we will use in select
	fd_set tmp_fds;		// temporary set of descriptors so we don't lose the original ones
	int fdmax;			// valoare maxima fd din multimea read_fds

	// in case we don't recieve the port on which to start the server
	if (argc < 2) {
		print_usage_message(argv[0]);
	}

	// clear the set of descriptors
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	// initialise the listening socket
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(listenfd < 0, "socket");

	// get the port number from the argument
	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	// initialise information for the server
	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	// bind the socket to the server address
	// i.e assign a name to a socket
	ret = bind(listenfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	ret = listen(listenfd, MAX_CLIENTS);
	DIE(ret < 0, "listen");

	// add the listening socket to our set, from which we will select
	FD_SET(listenfd, &read_fds);

	// only one descriptor in the set
	fdmax = listenfd;

	while (1) {
		// save the original set because select modifies it
		tmp_fds = read_fds; 

		// the gist of our program -> multiplexing the connections
		// and the stream of data -> in and out our server
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		for (i = 0; i <= fdmax; i++) {
			// we iterate through each socket and check if something changed
			if (FD_ISSET(i, &tmp_fds)) {
				// if we recieved information on the listening socket
				// that means a new client wants to connect
				if (i == listenfd) {
					subscriber_len = sizeof(cli_addr);

					// accept the connection and create client socket
					subscriberfd = accept(listenfd, (struct sockaddr *) &cli_addr, &subscriber_len);
					DIE(subscriberfd < 0, "accept");

					// add the new socket to the set
					FD_SET(subscriberfd, &read_fds);
					if (subscriberfd > fdmax) { 
						// update the maximum value for the file descriptor
						fdmax = subscriberfd;
					}

					printf("New client %d connected from %s:%d.\n", 
							subscriberfd, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
				} else { // otherwise it means we recieved information from one of the clients
					
					// reset the buffer
					memset(buffer, 0, BUFLEN);

					// get the payload in the buffer
					n = recv(i, buffer, sizeof(buffer), 0);
					if(buffer[strlen(buffer) - 1] == '\n')
						buffer[strlen(buffer) - 1] = '\0';

					DIE(n < 0, "recv");

					if (n == 0) {
						// in case the connection was closed
						printf("Client %d disconnected.\n", i);
						close(i);
						
						// remove the fd from the set
						FD_CLR(i, &read_fds);
					} else {

						printf ("S-a primit de la clientul de pe socketul %d mesajul: %s\n", i, buffer);
						char mesaj[BUFLEN];
						int client;
						sscanf(buffer, "%d: %s", &client, mesaj);
						if(client <= 3){

							strcpy(buffer, "Invalid client\n");
							n = send(i, buffer, strlen(buffer) + 1, 0);
							DIE(n < 0, "sending_to_client");
						}
						else if(FD_ISSET(client, &read_fds)){
							n = send(client, mesaj, strlen(mesaj) + 1, 0);
							DIE(n < 0, "sending_to_client");
						}
					}
				}
			}
		}
	}

	close(listenfd);

	return 0;
}

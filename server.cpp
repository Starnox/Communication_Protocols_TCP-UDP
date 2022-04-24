#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>
#include <map>
#include "tcp_client.h"
#include "helpers.h"

void print_usage_message(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

/**
 * @brief check commands given to the server 
 * 
 * @return true the server continues to operates
 * @return false we recieved the exit command
 */
bool check_server_commands() {
	char buffer[BUFLEN];

	memset(buffer, 0, BUFLEN);
	// gets the data from the consoles
	fgets(buffer, BUFLEN - 1, stdin);
	if(buffer[strlen(buffer) - 1] == '\n')
		buffer[strlen(buffer) - 1] = '\0';
		
	// compare the first 4 characters with the word exit
	if (strncmp(buffer, "exit", 4) == 0) {
		return false;
	}

	return true;
}

/**
 * @brief recieve a connection from the listening tcp socket
 * if it is a valid one, add it to the list of clients
 * @param tcp_listenfd -> the socket that is listening for connections 
 * @param read_fds -> the set of sockets
 * @param clients -> the vector of clients
 */
void incoming_tcp_connection(int tcp_listenfd, int &fd_max,
								 fd_set *read_fds, std::vector<TCP_Client*> &subscribers,
								 std::map<int, TCP_Client*> &fd_to_client) {
	socklen_t subscriber_len;
	struct sockaddr_in cli_addr;

	subscriber_len = sizeof(cli_addr);
	char buffer[BUFLEN];
	int subscriberfd;

	// accept the connection and create client socket
	subscriberfd = accept(tcp_listenfd, (struct sockaddr *) &cli_addr, &subscriber_len); 
	DIE(subscriberfd < 0, "accept");

	// get the id of the tcp_client
	memset(buffer, 0, BUFLEN);

	// get the payload in the buffer
	int n = recv(subscriberfd, buffer, sizeof(buffer), 0);
	DIE(n < 0, "recv error");
	if(buffer[strlen(buffer) - 1] == '\n')
		buffer[strlen(buffer) - 1] = '\0';

	// if the id is bigger then 10
	if(strlen(buffer) > 10)
		return;

	bool exists = false;
	// go through the vector of subscribers and check if it is already connected
	for(TCP_Client* client : subscribers) {
		if(client->getId() == buffer) { // verify id
			exists = true;
			if(client->getConnected() == true) { // the client is already connected
				printf("Client %s already connected.\n", buffer);
				send(subscriberfd, "Fail", 4, 0);
				return;
			} 
			else {
				// connect the client
				client->setConnected(true);
				
				// remap the socket to the client
				fd_to_client[subscriberfd] = client;
				// add it to the set again
				FD_SET(subscriberfd, read_fds);
			}
		}
	}
	// the client is connecting for the first time
	if(!exists) {
		// create a new client and add it to the vector
		TCP_Client *new_client = new TCP_Client(buffer, subscriberfd, 0, true);
		subscribers.push_back(new_client);

		// map the socket to the client
		fd_to_client[subscriberfd] = new_client;

		// add it to the set
		FD_SET(subscriberfd, read_fds);

		// update fdmax
		if(subscriberfd > fd_max)
			fd_max = subscriberfd;
	}
	send(subscriberfd, "Success", 7, 0);
	printf("New client %s connected from %s:%d.\n", 
				buffer, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
}

int main(int argc, char *argv[])
{
	// disable output buffering
	setvbuf(stdout, NULL, _IONBF, BUFSIZ); 

	// declaration
	int tcp_listenfd, portno;
	char buffer[BUFLEN];

	std::vector<TCP_Client*> subscribers;
	std::map<int, TCP_Client*> fd_to_client;

	struct sockaddr_in serv_addr;
	int n, i, ret;

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
	tcp_listenfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(tcp_listenfd < 0, "socket");

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
	ret = bind(tcp_listenfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	ret = listen(tcp_listenfd, MAX_CLIENTS);
	DIE(ret < 0, "listen");

	// add the listening socket to our set, from which we will select
	FD_SET(tcp_listenfd, &read_fds);
	// add stdin to the set
	FD_SET(0, &read_fds);

	fdmax = tcp_listenfd;

	bool closed_connection = false;
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
				if (i == 0) { // console input
					if(check_server_commands() == false) {
						closed_connection = true;
						break;
					}
				}
				else if (i == tcp_listenfd) { // we have a new tcp client trying to connect
					incoming_tcp_connection(tcp_listenfd, fdmax, &read_fds, subscribers, fd_to_client);

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
						printf("Client %s disconnected.\n", fd_to_client[i]->getId().c_str());

						// disconnect the client
						fd_to_client[i]->setConnected(false);
						// remove the connection between the socket and the client
						fd_to_client.erase(i);
						close(i); // close the socket connection
						
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
		if(closed_connection)
			break;
	}

	close(tcp_listenfd);

	// free the memory
	for(TCP_Client* client : subscribers) {
		delete client;
	}

	return 0;
}

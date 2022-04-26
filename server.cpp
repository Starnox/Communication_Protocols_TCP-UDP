#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
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
	if(strlen(buffer) > 10) {
		fprintf(stderr, "ID is too long");
		return;
	}

	bool exists = false;
	// go through the vector of subscribers and check if it is already connected
	for(TCP_Client* client : subscribers) {
		if(client->getId() == buffer) { // verify id
			exists = true;
			if(client->getConnected() == true) { // the client is already connected
				printf("Client %s already connected.\n", buffer);
				send(subscriberfd, "Fail", 4, 0);
				close(subscriberfd); // close the connection
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
	// disable nagle for the socket
	disable_nagle(subscriberfd);
	send(subscriberfd, "Success", 7, 0);
	printf("New client %s connected from %s:%d.\n", 
				buffer, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
}

void incoming_udp_datagram(int udp_listenfd, struct sockaddr_in cli_udp,
							std::vector<TCP_Client*> &subscribers) {
	socklen_t udp_len = sizeof(cli_udp);

	// initialise the message
	message new_message;
	memset(&new_message, 0, sizeof(message));

	// the buffer in which we'll recieve the udp datagram
	char buffer[BUFLEN];
	memset(buffer, 0, sizeof(buffer));

	int ret = recvfrom(udp_listenfd, &buffer, BUFLEN, 0, (struct sockaddr *) &cli_udp, &udp_len);
	DIE(ret < 0, "Recv from udp client failed");

	// begin constructing the message from the buffer
	
	
	memcpy(&new_message.ip, &cli_udp.sin_addr, sizeof(struct in_addr)); // get the ip
	new_message.port = cli_udp.sin_port; // get the port
	memcpy(&new_message.topic, buffer, TOPIC_LEN); // get the topic
	new_message.type = buffer[TOPIC_LEN]; // get the type
	memcpy(&new_message.payload,
			buffer + TOPIC_LEN + sizeof(new_message.type), PAYLOAD_LEN); // get the payload
	new_message.len = sizeof(struct in_addr) + sizeof(in_port_t) + TOPIC_LEN + 1;
	if(new_message.type == 0) 
		new_message.len += 5;
	else if(new_message.type == 1) 
		new_message.len += 2;
	else if(new_message.type == 2) 
		new_message.len += 6;
	else
		new_message.len += strlen(new_message.payload);

	int aux = new_message.len;
	new_message.len = htons(new_message.len); // put in network byte order

	memset(buffer, 0, sizeof(buffer));
	pack(&new_message, buffer, aux+2);
	for (TCP_Client* client : subscribers) {
		if(client->getConnected()) {
			int fd = client->getFd(), len = aux+2;
			sendall(fd, buffer, len);
		}
	}
}

int main(int argc, char *argv[])
{
	// disable output buffering
	setvbuf(stdout, NULL, _IONBF, BUFSIZ); 

	// declaration
	int tcp_listenfd, udp_listenfd, portno;
	char buffer[BUFLEN];

	std::vector<TCP_Client*> subscribers; // list of subscribers
	std::map<int, TCP_Client*> fd_to_client; // map between fd and sub
	std::map<std::string, std::vector<TCP_Client *>> topic_subscribers;

	struct sockaddr_in serv_addr, cli_udp;
	int n, i, ret;

	fd_set read_fds;	// set of descriptors we will use in select
	fd_set tmp_fds;		// temporary set of descriptors so we don't lose the original ones
	int fdmax;

	// in case we don't recieve the port on which to start the server
	if (argc < 2) {
		print_usage_message(argv[0]);
	}

	// clear the set of descriptors
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	// initialise the tcp listening socket
	tcp_listenfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(tcp_listenfd < 0, "socket");

	// get the port number from the argument
	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	// initialise information for the tcp server
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

	disable_nagle(tcp_listenfd); // disable nagle on tcp_listening socket

	if(tcp_listenfd > fdmax)
		fdmax = tcp_listenfd;

	// initialise the udp listening socket
	udp_listenfd = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(udp_listenfd < 0, "udp listen socket");

	// reset the udp client
	memset((char *) &cli_udp, 0, sizeof(cli_udp));
	// set information
	cli_udp.sin_family = AF_INET;
	cli_udp.sin_port = htons(portno);
	cli_udp.sin_addr.s_addr = INADDR_ANY;

	ret = bind(udp_listenfd, (struct sockaddr *) &cli_udp, sizeof(struct sockaddr));
	DIE(ret < 0, "udp bind");

	if(udp_listenfd > fdmax)
		fdmax = udp_listenfd;

	
	// add the tcp socket to the set
	FD_SET(tcp_listenfd, &read_fds);
	// add the udp socket to the set
	FD_SET(udp_listenfd, &read_fds);
	// add stdin to the set
	FD_SET(0, &read_fds);


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
				}
				else if (i == udp_listenfd) {
					incoming_udp_datagram(udp_listenfd, cli_udp, subscribers);
				}
				 else { // otherwise it means we recieved information from one of the clients
					
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
						char mesaj[BUFLEN];
						int client;
						int result = sscanf(buffer, "%d: %s", &client, mesaj);					
						if(result == EOF || client <= 3){

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
	close(udp_listenfd);
	// free the memory
	for(TCP_Client* client : subscribers) {
		delete client;
	}

	return 0;
}

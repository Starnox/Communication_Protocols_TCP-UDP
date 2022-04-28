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
#include <set>
#include "tcp_client.h"
#include "helpers.h"

/**
 * @brief prints how to execute the program
 * 
 * @param file the name of the program
 */
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
	if(fgets(buffer, BUFLEN - 1, stdin) == NULL)
		return false;

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
			std::map<int, TCP_Client*> &fd_to_client,
			std::map<TCP_Client *, std::vector<char *>> &to_send) {
	socklen_t subscriber_len;
	struct sockaddr_in cli_addr;

	subscriber_len = sizeof(cli_addr);
	char buffer[ID_SIZE];
	int subscriberfd;

	// accept the connection and create client socket
	subscriberfd = accept(tcp_listenfd, (struct sockaddr *) &cli_addr, &subscriber_len); 
	DIE(subscriberfd < 0, "accept");

	// get the id of the tcp_client
	memset(buffer, 0, ID_SIZE);

	// get the payload in the buffer
	int n = recv(subscriberfd, buffer, ID_SIZE, 0);
	DIE(n < 0, "recv error");


	bool exists = false, has_messages = false;
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
				if(to_send[client].size() > 0)
					has_messages = true;
			}
		}
	}
	// the client is connecting for the first time
	
	if(!exists) {
		// create a new client and add it to the vector
		TCP_Client *new_client = new TCP_Client(buffer, subscriberfd, true);
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

	if(has_messages) {
		//send the client all the messages that he missed
		TCP_Client *client = fd_to_client[subscriberfd];
		for(char *msg : to_send[client]) {
			// the message is already packed, we just need to send it
			uint16_t *length = (uint16_t *) (msg);
			int n = sendall(client->getFd(), msg, ntohs(*length) +2);
			DIE(n < 0, "sendall failed");
			free(msg); // free memory
		}
		// clear the messages that have been sent
		to_send[client].clear();
	}
}

/**
 * @brief makes the connection with the udp client,
 * retrieves the data, decode it, create an appropriate structure,
 * pack the data in a buffer and send to all the appropiate tcp clients
 * @param udp_listenfd the udp socket
 * @param cli_udp information regarding the udp client
 * @param topic_subscribers map between topic and clients
 * @param has_sf map between topic, clients and store forward
 * @param toSend database of messages that need to be sent
 */
void incoming_udp_datagram(int udp_listenfd, struct sockaddr_in cli_udp,
			std::map<std::string, std::set<TCP_Client *>> &topic_subscribers,
			std::map<std::pair<TCP_Client*,std::string>,bool> &has_sf,
			std::map<TCP_Client *, std::vector<char *>> &toSend) {

	socklen_t udp_len = sizeof(cli_udp);

	// initialise the message
	message new_message{};
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
			buffer + TOPIC_LEN + sizeof(new_message.type),PAYLOAD_LEN); // get the payload
	new_message.payload[PAYLOAD_LEN-1] = '\0';
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

	// get the clients that are subscribed to that topic
	for (TCP_Client* client : topic_subscribers[new_message.topic]) {
		if(client->getConnected()) {
			int fd = client->getFd(), len = aux+2;
			int n = sendall(fd, (char *) &new_message, len);
			DIE(n < 0, "sendall failed");
			// if it has store forward
		} else if(has_sf[{client, new_message.topic}]){
			// store the message for the client
			char *msg_copy = (char *) malloc(BUFLEN);
			memcpy(msg_copy, (char *) &new_message, BUFLEN);
			toSend[client].push_back(msg_copy);
		}
	}
}

int main(int argc, char *argv[])
{
	// disable output buffering
	setvbuf(stdout, NULL, _IONBF, BUFSIZ); 

	// declaration
	int tcp_listenfd, udp_listenfd, portno;

	std::vector<TCP_Client*> subscribers; // list of subscribers
	std::map<int, TCP_Client*> fd_to_client; // map between fd and sub
	std::map<std::string, std::set<TCP_Client *>> topic_subscribers;
	std::map<std::pair<TCP_Client*,std::string>,bool> has_sf; // connection between topic,client and sf
	std::map<TCP_Client *, std::vector<char *>> to_send; // messages to send

	struct sockaddr_in serv_addr, cli_udp;
	int n, i, ret;

	fd_set read_fds;	// set of descriptors we will use in select
	fd_set tmp_fds;		// temporary set of descriptors so we don't lose the original ones
	int fdmax = 0;

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

	ret = listen(tcp_listenfd, BACKLOG);
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
					incoming_tcp_connection(tcp_listenfd, fdmax, &read_fds, subscribers, fd_to_client,
											to_send);
				}
				else if (i == udp_listenfd) {
					incoming_udp_datagram(udp_listenfd, cli_udp, topic_subscribers, has_sf, to_send);
				}
				else { // otherwise it means we recieved information from one of the clients
					// reset the buffer
					client_command cmd;
					char command[COMMAND_LEN] = "";

					memset(&cmd, 0, sizeof(client_command));
					memset(command, 0, COMMAND_LEN);

					// get the payload in the buffer
					n = recvall(i, command);
					DIE(n < 0, "recvall");

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
						DIE(n < 2, "command incomplete");
						char command_type;
						char topic[TOPIC_LEN];
						int sf, res;

						if(command[2] == 's') {
							res = sscanf(command+2, "%c %s %d", &command_type, topic, &sf);
							if(res != 3)
								continue;

							// add the client to that topic
							topic_subscribers[topic].insert(fd_to_client[i]);

							// make the connection beetween the topic,client and storeForward
							if(sf == 1) 
								has_sf[{fd_to_client[i], topic}] = true;
							else
								has_sf[{fd_to_client[i], topic}] = false;

						} else if(command[2] == 'u') {
							res = sscanf(command+2, "%c %s", &command_type, topic);
							if(res != 2)
								continue;
							// find the client and remove it
							for(auto client = topic_subscribers[topic].begin(); client != topic_subscribers[topic].end();) {
								if((*client)->getFd() == i)
									client = topic_subscribers[topic].erase(client);
								else
									++client;
							}
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
		// close connections
		if(client->getConnected())
			close(client->getFd());
		delete client;
	}

	return 0;
}

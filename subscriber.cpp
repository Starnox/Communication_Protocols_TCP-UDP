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

/**
 * @brief prints how to execute the program
 * 
 * @param file the name of the program
 */
void usage(char *file)
{
	fprintf(stderr, "Usage: %s <ID_Client> <IP_Server> <Port_Server>\n", file);
	exit(0);
}

/**
 * @brief checks if the client with the given id is connected
 * 
 * @param sockfd the socket that is connected to the server
 * @param id the id that this client want to connect with
 * @return true 
 * @return false 
 */
bool check_id(int sockfd, char id[ID_SIZE]) {
	id[ID_SIZE-1] = '\0';
	int n = send(sockfd, id, ID_SIZE, 0);
	DIE(n < 0, "send id fail");
	
	char buffer[ID_SIZE];
	n = recv(sockfd, buffer, ID_SIZE, 0);
	if(strncmp(buffer, "Fail", 4) == 0)
		return false;

	return true;
	
}

/**
 * @brief prints the message from the server 
 * using the correct format
 * @param msg 
 */
void format_message(message &msg) {
	char value[PAYLOAD_LEN+20];
	memset(value, 0 , PAYLOAD_LEN+20);

	if(msg.type == 0) {  // INT
		uint8_t sign = *msg.payload;
		uint32_t *num = ((uint32_t *) (msg.payload+1));
		*num = ntohl(*num);
		if(sign == 0)
			sprintf(value ,"INT - %d", *num);
		else
			sprintf(value, "INT - -%d", *num);
	}
	else if(msg.type == 1) { // SHORT_REAL
		uint16_t *num = ((uint16_t *) (msg.payload));
		*num = ntohs(*num);
 		sprintf(value,"SHORT_REAL - %.10g", (*num * 1.0)/100);
	}
	else if(msg.type == 2) { // FLOAT
		uint8_t sign = *msg.payload;
		uint32_t *num = ((uint32_t *) (msg.payload+1));
		*num = ntohl(*num);
		uint8_t mod = *(msg.payload + 5);
		if(sign == 0)
			sprintf(value,"FLOAT - %.10g", (*num * 1.0) * pow(10, -mod));
		else
			sprintf(value,"FLOAT - -%.10g", (*num * 1.0) * pow(10, -mod));	
	}
	else { // STRING
		int payload_size = ntohs(msg.len) - 57;
		msg.payload[payload_size] = '\0';
		sprintf(value,"STRING - %s", msg.payload);
	}

	printf("%s:%d - %s - %s\n", inet_ntoa(msg.ip), ntohs(msg.port), msg.topic, value);

}

int main(int argc, char *argv[])
{
	// disable output buffering
	setvbuf(stdout, NULL, _IONBF, BUFSIZ); 

	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];

	message msg;

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
			memset(buffer, 0, COMMAND_LEN);
			
			// gets the data from the consoles
			if(fgets(buffer, COMMAND_LEN - 1, stdin) == NULL)
				break;
			if(buffer[strlen(buffer) - 1] == '\n')
				buffer[strlen(buffer) - 1] = '\0';

			client_command cmd;
			char topic[TOPIC_LEN], command[COMMAND_LEN];

			memset(&cmd, 0, sizeof(client_command));
			memset(command,0, COMMAND_LEN);
			int type = 0;
			// compare the first 4 characters with the word exit
			if (strncmp(buffer, "exit", 4) == 0) {
				break; // break out of the loop
			}

			
			else if(strncmp(buffer, "subscribe", 9) == 0) {
				type = 1;
				int sf;
				// extract the info from the buffer
				int result = sscanf(buffer, "%s %s %d", command, topic, &sf);
				// input sanitization
				if(result != 3) 
					continue;
				if(sf != 0 && sf != 1)
					continue;
				else
					sprintf(command, "s %s %d", topic, sf); // create the command
			}
			else if(strncmp(buffer, "unsubscribe", 11) == 0) {
				type = 2;
				int result = sscanf(buffer, "%s %s", command, topic);
				if(result != 2) 
					continue;
				else 
					sprintf(command, "u %s", topic); // create the command
			}
			else {
				continue;;
			}
			
			int string_size = strlen(command);
			
			// construct payload and send
			cmd.len = htons(string_size);
			memcpy(&cmd.payload, command, string_size);
			memcpy(buffer, &cmd, string_size+2);

			int n = sendall(sockfd, (char *) &cmd, string_size + 2);
			DIE(n < 0, "sendall failed");

			if(type == 1) 
				printf("Subscribed to topic.\n");
			else
				printf("Unsubscribed from topic.\n");
				
		}
		// if we recieved information on the socket that is connected to the server
		if(FD_ISSET(sockfd, &tmp_fds)){
			// gets the data
			memset(buffer, 0, BUFLEN);
			n = recvall(sockfd, buffer);
			DIE(n < 0, "recvall failed");
			if (n == 0) { // the server closed the connectionc
				break;
			}

			msg = *((message *) buffer); // unpack the message
			format_message(msg);
		}
	}

	close(sockfd);

	return 0;
}

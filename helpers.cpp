#include "helpers.h"

void disable_nagle(int socket) {
    int flag = 1;
	int result = setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
    DIE(result < 0, "nagle disable failed");
}

void pack(message *msg, char *buf, uint16_t size) {
    /*
    memcpy(buf, &(msg->len), 2); // put the length
    memcpy(buf + 2, &(msg->ip), sizeof(struct in_addr)); // put the ip
    memcpy(buf + 6, &(msg->port), 2); // put the port
    memcpy(buf + 8, &(msg->topic), TOPIC_LEN); // put the topic
    memcpy(buf + 8 + TOPIC_LEN, &(msg->type), 1); // put the type
    memcpy(buf + 8 + TOPIC_LEN + 1, &(msg->payload), strlen(msg->payload)); // put the payload
    */
    memcpy(buf, msg, size); // the size of var length isn't included in var len
}

message unpack(int size, char *buf) {
    message new_message;
    /*
    memcpy(&new_message, 0, sizeof(message));
    new_message.len = *buf; // put the length
    memcpy(&new_message.ip, buf + 1, sizeof(struct in_addr));
    memcpy(&new_message.port, buf + 5, sizeof(in_port_t));
    */
    memcpy(&new_message, buf, size);
    return new_message;
}

// this function is taken from Beej's Guide to Network programming
void sendall(int fd, char *buf, int len) {
    int total = 0;
    int bytesleft = len;
    int n;

    while(total < len) {
        n = send(fd, buf + total, bytesleft, 0);
        DIE(n < 0, "tcp send error");
        total += n;
        bytesleft -= n;
    }

}

int recvall(int fd, char *buf) {
    // first of all recieve the length
    int n = recv(fd, buf, 2, 0);
    DIE(n < 0, "recieve length failed");

    // the connection was closed
    if(n == 0)
        return 0;

    int size;
    memcpy(&size, buf, 2);
    int remaining = ntohs(size), so_far = 2;

    while(remaining > 0) {
        n = recv(fd, buf + so_far, remaining, 0);
        DIE(n < 0, "recv failed");
        remaining -= n;
        so_far += n;
    }
    return so_far;
}
#include "helpers.h"

/**
 * @brief disable nagle algorithm for the socket
 * 
 * @param socket 
 */
void disable_nagle(int socket) {
    int flag = 1;
	int result = setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
    DIE(result < 0, "nagle disable failed");
}

/**
 * @brief use send multiple times
 * to make sure the whole message is transmitted
 * @param fd file descriptor
 * @param buf the stream of bytes
 * @param len the length of the stream
 * @return int 
 */
int sendall(int fd, char *buf, int len) {
    int total = 0;
    int bytesleft = len;
    int n;

    while(total < len) {
        n = send(fd, buf + total, bytesleft, 0);
        DIE(n < 0, "tcp send error");
        total += n;
        bytesleft -= n;
    }
    return (total == len ? total : -1);
}

/**
 * @brief firs recv the length of the packet
 * then as long as the message is not completly recieved use recv
 * @param fd 
 * @param buf 
 * @return int 
 */
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
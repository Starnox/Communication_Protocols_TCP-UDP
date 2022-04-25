#include "helpers.h"

void disable_nagle(int socket) {
    int flag = 1;
	int result = setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
    DIE(result < 0, "nagle disable failed");
}
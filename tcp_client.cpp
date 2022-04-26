#include "tcp_client.h"

// TCP_Client constructor
TCP_Client::TCP_Client(std::string c_id, int c_fd, bool c_connected)
{
    id = c_id;
    fd = c_fd;
    connected = c_connected;
}

void TCP_Client::setConnected(bool status) {
    connected = status;
}
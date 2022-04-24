#include "tcp_client.h"

// TCP_Client constructor
TCP_Client::TCP_Client(std::string c_id, int c_fd, int c_sf, bool c_connected)
{
    id = c_id;
    fd = c_fd;
    sf = c_sf;
    connected = c_connected;
}

void TCP_Client::setConnected(bool status) {
    connected = status;
}
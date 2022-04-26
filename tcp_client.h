#ifndef _TCP_CLIENT_H
#define _TCP_CLIENT_H 1

#include <string>
#include "helpers.h"

class TCP_Client
{
private:
    std::string id;
    int fd;
    bool connected;

public:
    TCP_Client(std::string c_id, int c_fd, bool c_connected);

    std::string getId() { return id; }
    int getFd() { return fd; }
    bool getConnected() { return connected; }

    void setConnected(bool status);
};


#endif
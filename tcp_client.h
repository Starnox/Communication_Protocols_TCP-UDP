#include <string>
#include "helpers.h"

#ifndef _TCP_CLIENT_H
#define _TCP_CLIENT_H 1

class TCP_Client
{
private:
    std::string id;
    int fd;
    int sf;
    bool connected;

public:
    TCP_Client(std::string c_id, int c_fd, int c_sf, bool c_connected);

    std::string getId() { return id; }
    int getFd() { return fd; }
    int getSf() { return sf; }
    bool getConnected() { return connected; }

    void setConnected(bool status);
};


#endif
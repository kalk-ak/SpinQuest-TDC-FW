#ifndef SERVER_PARSE_OPTIONS_H
#define SERVER_PARSE_OPTIONS_H

#include "CLI11.h"
#include <iostream>
#include <string>
#include <sys/socket.h>

struct server_parse_options
{
    // flag for logging and debugging output
    bool verbose = false;

    // default port set to 6767
    int SOCKET_PORT = 6767;

    // Have tcp connection as the default
    int network_type = SOCK_STREAM;
};

server_parse_options parse_args(int argc, char *argv[])
{
}

#endif // !#ifndef SERVER_PARSE_OPTIONS_H

#ifndef DEFINED_UTILITES_H
#define DEFINED_UTILITES_H

#include "UniqueFD.h"
#include <string>
#include <sys/socket.h>

// HACK: to increase arguments change server_parse_options
struct server_parse_options
{
    // flag for logging and debugging output
    bool verbose;

    // default port set to 6767
    int SOCKET_PORT;

    // Have tcp connection as the default
    int network_type;

    // TODO: Add Number of boards to be used for concuruncy

    // TODO: Add options for write back to buffer or file

    // TODO:
};

#endif

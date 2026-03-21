#ifndef DEFINED_UTILITES_H
#define DEFINED_UTILITES_H

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
};

/**
 * @brief Parses command line arguments for the server application.
 *
 * This function uses CLI11 to handle flags like --port and --verbose.
 * If the parsing fails or --help is requested, the program will exit.
 *
 * @param argc The number of command line arguments.
 * @param argv The array of command line argument strings.
 * @return server_parse_options A struct containing the parsed options.
 */
server_parse_options parse_args(int argc, char *argv[], std::string name);

#endif

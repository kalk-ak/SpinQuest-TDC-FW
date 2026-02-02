#ifndef SERVER_PARSE_OPTIONS_H
#define SERVER_PARSE_OPTIONS_H

#include "CLI/CLI.hpp"
#include <cstdlib>
#include <iostream>
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

server_parse_options parse_args(int argc, char *argv[], std::string name)
{
    CLI::App app{name};

    // ADD defaults
    bool verbose = false;
    int socket_port = 6767;
    std::string network_type_str = "tcp";

    app.add_flag("-v,--verbose", verbose, "Enable verbose logging output");
    app.add_option("-p,--port", socket_port, "Port number to listen on (default: 6767)");
    app.add_option("-n,--network", network_type_str, "Network type: tcp or udp (default: tcp)")
        ->check(CLI::IsMember({"tcp", "udp"}));

    try
    {
        app.parse(argc, argv);
    }
    catch (const CLI::ParseError &e)
    {
        std::cerr << "Invalid command line arguments: " << e.what() << "\n";
        exit(app.exit(e));
    }

    int network_type;
    if (network_type_str == "tcp")
    {
        network_type = SOCK_STREAM;
    }
    else if (network_type_str == "udp")
    {
        network_type = SOCK_DGRAM;
    }
    else
    {
        std::cerr << "Invalid network type specified. Use 'tcp' or 'udp'.\n";
        exit(EXIT_FAILURE);
    }

    server_parse_options options{
        .verbose = verbose, .SOCKET_PORT = socket_port, .network_type = network_type};

    return options;
}

#endif // !#ifndef SERVER_PARSE_OPTIONS_H

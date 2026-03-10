#include "CLI/CLI.hpp" // Used for parsing command line arguments
#include "defined_utilities.h"
#include <arpa/inet.h>
#include <cassert>
#include <iostream>
#include <netinet/in.h>
#include <spdlog/common.h>
#include <spdlog/spdlog.h>
#include <sys/un.h>

server_parse_options parse_args(int argc, char *argv[], std::string name)
{
    CLI::App app{name};

    // ADD defaults
    bool verbose = false;
    int socket_port = 6767;
    std::string network_type_str = "tcp";

    // Add the flags
    app.add_flag("-v,--verbose", verbose, "Enable verbose logging output");
    app.add_option("-p,--port", socket_port, "Port number to listen on (default: 6767)");

    // Check that the options for networking is TCP and UDP
    app.add_option("-n,--network", network_type_str, "Network type: tcp or udp (default: tcp)")
        ->check(CLI::IsMember({"tcp", "udp"}));

    try
    {
        app.parse(argc, argv);
    }
    catch (const CLI::ParseError &e)
    {
        spdlog::error("Invalid Command Line Arguments:", e.what());
        exit(app.exit(e));
    }

    // GET network type
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
        spdlog::error("Invalid network type specified. Use tcp or udp");
        exit(EXIT_FAILURE);
    }
    // NOTE: you can add functionality for adding more network types or BLUETOOTH in the future here

    server_parse_options options{
        .verbose = verbose, .SOCKET_PORT = socket_port, .network_type = network_type};

    // pass by value
    return options;
}

UniqueFD create_connection(const std::string &ip, const int port, const std::string &proto,
                           int internet_type, std::filesystem::path path)

{
    // assert statements before starting
    assert(proto == "tcp" or proto == "udp");
    assert(internet_type == AF_INET or internet_type == AF_INET6 or internet_type == AF_UNIX);

    // Create a raw file descriptor
    int raw_fd = -1;

    // Get Connection type
    if (proto == "udp")
        raw_fd = socket(internet_type, SOCK_DGRAM, 0);
    else
        raw_fd = socket(internet_type, SOCK_STREAM, 0);

    if (raw_fd < 0)
    {
        perror("Socket creation failed");
        std::exit(1);
    }

    UniqueFD socket_wrapper(raw_fd);

    // Prepare socket address and size for initiating a connection later
    // using a generic pointer and size variable to handle different socket types
    struct sockaddr *final_addr = nullptr;
    int final_addr_size = 0;

    // define the 3 types of socket address
    // NOTE: The reason why the socket address are defined here is because they need to be in scope
    // if defined in the switch statement, they will go out of scope
    struct sockaddr_in ip_addr4;
    struct sockaddr_in6 ip_addr6;
    struct sockaddr_un unix_addr;

    // NOTE: Handling different internet types
    switch (internet_type)
    {
    case AF_INET:
        // Handle IPv4
        memset(&ip_addr4, 0, sizeof(ip_addr4)); // Initiallize the structure to zero before using it
        ip_addr4.sin_family = AF_INET;
        ip_addr4.sin_port = htons(port);

        // log report
        spdlog::debug("Using ipv4 socket");

        // convert IP address to binary form
        if (inet_pton(internet_type, ip.c_str(), &ip_addr4.sin_addr) <= 0)
        {
            spdlog::error("Invalid address for ipv4 \n");
            std::exit(1);
        }

        // Update final pointer for connection
        final_addr_size = sizeof(ip_addr4);
        final_addr = (struct sockaddr *) &ip_addr4;

        break;

    case AF_INET6:
        // Handle IPv6
        memset(&ip_addr6, 0, sizeof(ip_addr6));
        ip_addr6.sin6_family = AF_INET6;
        ip_addr6.sin6_port =
            htons(port); // NOTE: Flip the port number to network byte order using htons

        spdlog::error("Using ipv6 socket.");

        // convert IP6 address to binary form
        if (inet_pton(internet_type, ip.c_str(), &ip_addr6.sin6_addr) <= 0)
        {
            spdlog::error("Invalid address for ipv6 \n");
            std::exit(1);
        }

        // Update final pointer for connection
        final_addr_size = sizeof(ip_addr6);
        final_addr = (struct sockaddr *) &ip_addr6;

        break;

    case AF_UNIX:
        // Handle Unix sockets
        memset(&unix_addr, 0, sizeof(unix_addr));
        unix_addr.sun_family = AF_UNIX;
        strncpy(unix_addr.sun_path, path.string().c_str(), sizeof(unix_addr.sun_path) - 1);
        spdlog::debug("Using UNIX socket.");

        // Update final pointer for connection
        final_addr_size = sizeof(unix_addr);
        final_addr = (struct sockaddr *) &unix_addr;

        break;

    default:
        throw std::invalid_argument("Unsupported internet type. IP4, IP6, and UNIX are supported.");
        break;
    }

    // Connect (For UDP this just sets the default destination)
    // TODO: For UDP Make sure that the server is running before sening
    if (connect(socket_wrapper.get(), final_addr, final_addr_size) < 0)
    {
        perror("Connection Failed");
        exit(1);
    }
    return socket_wrapper;
}

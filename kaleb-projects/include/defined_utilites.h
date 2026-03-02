#ifndef DEFINED_UTILITES_H
#define DEFINED_UTILITES_H

#include "UniqueFD.h"
#include <filesystem>
#include <string>
#include <sys/socket.h>

// ============================================================================
// Data Structures
// ============================================================================

/**
 * @brief Holds the parsed command-line configuration for the server.
 */
// NOTE: to increase arguments change server_parse_options
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

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Parses command-line arguments to configure the server.
 *
 * Supports the following flags:
 * -v, --verbose : Enable verbose logging output.
 * -p, --port    : Port number to listen on (default: 6767).
 * -n, --network : Network protocol, either 'tcp' or 'udp' (default: tcp).
 *
 * @param argc The number of command-line arguments.
 * @param argv The array of command-line argument strings.
 * @param name The name of the CLI application.
 * @return A server_parse_options struct containing the parsed settings.
 */
server_parse_options parse_args(int argc, char *argv[], std::string name);

/**
 * @brief Creates and connects a network or local domain socket.
 *
 * Initializes a socket descriptor and attempts to connect it to the specified
 * destination. Supports IPv4, IPv6, and UNIX domain sockets over TCP or UDP.
 *
 * @param ip            The IP address to connect to (e.g., "127.0.0.1").
 * @param port          The target port number.
 * @param proto         The transport protocol: "tcp" or "udp" (default: "tcp").
 * @param internet_type The address family: AF_INET, AF_INET6, or AF_UNIX (default: AF_INET).
 * @param path          The file path (used exclusively for UNIX domain sockets).
 * * @return A UniqueFD object managing the lifecycle of the created socket.
 * @throws std::invalid_argument If an unsupported internet_type is provided.
 */
UniqueFD create_connection(const std::string &ip, const int port, const std::string &proto = "tcp",
                           int internet_type = AF_INET,
                           std::filesystem::path path = std::filesystem::current_path());

#endif

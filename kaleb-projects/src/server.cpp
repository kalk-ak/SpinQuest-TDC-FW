#include "UniqueFD.h"
#include "defined_utilites.h"
#include <csignal>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

// --- Constants and Globals ---
const uint64_t START_PREAMBLE = 0xAAAAAAAAAAAAAAAA;
const uint64_t END_PREAMBLE = 0xBBBBBBBBBBBBBBBB;
const int PORT = 8080;

// Kept atomic to ensure safe access in signal handler
// declare as volatile to prevent compiler optimizations to cache its value
// forces the code to always read the variable from memory
volatile sig_atomic_t keep_running = 1;

// NOTE: The closure of the program is handled by sig handeler
// so that the program can be extended to handle more complex shutdown procedures
// like writing to memory
void sigint_handler(int sig)
{
    keep_running = 0;
}

int main(int argc, char *argv[])
{
    // TODO: in production (for the actual event handler) it is better to use more robust signals
    // and signal handlers
    signal(SIGINT, sigint_handler);

    // Parse command line options
    server_parse_options options = parse_args(argc, argv, "Server Application");

    // Create Socket using RAII
    UniqueFD server_fd(socket(AF_INET, SOCK_STREAM, 0));
    if (!server_fd.is_valid())
    {
        std::cerr << "socket creation failed" << std::endl;
        return EXIT_FAILURE;
    }

    int opt = 1;
    if (setsockopt(server_fd.get(), SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
    {
        perror("setsockopt");
        return EXIT_FAILURE;
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd.get(), (struct sockaddr *) &address, sizeof(address)) < 0)
    {
        perror("bind failed");
        return EXIT_FAILURE;
    }

    if (listen(server_fd.get(), 3) < 0)
    {
        perror("listen");
        return EXIT_FAILURE;
    }

    std::cout << "Server listening on port " << PORT << "..." << std::endl;

    // Accept connection using RAII
    int addrlen = sizeof(address);
    UniqueFD client_socket(
        accept(server_fd.get(), (struct sockaddr *) &address, (socklen_t *) &addrlen));

    if (!client_socket.is_valid())
    {
        perror("accept");
        return EXIT_FAILURE;
    }

    int iteration_count = 0;
    while (keep_running)
    {
        uint64_t buffer;
        int bytes_read = read(client_socket.get(), &buffer, sizeof(buffer));

        if (bytes_read > 0 && buffer == START_PREAMBLE)
        {
            iteration_count++;
            std::cout << "Iteration " << iteration_count << " started." << std::endl;

            // ofstream is also an RAII object! It closes when it goes out of scope.
            std::ofstream outfile("iteration_" + std::to_string(iteration_count) + ".txt");

            while (keep_running)
            {
                bytes_read = read(client_socket.get(), &buffer, sizeof(buffer));
                if (bytes_read > 0)
                {
                    if (buffer == END_PREAMBLE)
                    {
                        std::cout << "Iteration " << iteration_count << " ended." << std::endl;
                        break;
                    }
                    outfile << buffer << std::endl;
                }
                else
                {
                    keep_running = 0;
                    break;
                }
            }
        }

        if (bytes_read <= 0)
        {
            keep_running = 0;
        }
    }

    // No explicit close() calls needed here!
    // destructors for client_socket and server_fd handle it.
    std::cout << "Server shutting down cleanly." << std::endl;
    return 0;
}

#include "fake_fpga_client.h"
#include "UniqueFD.h"
#include <CLI/CLI.hpp>
#include <arpa/inet.h> // For sockets
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <netinet/in.h>
#include <random>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h> // For close()
#include <vector>

// HACK: BUFFER_SIZE is a hyperparameter that affects performance and timing accuracy
// can be hyer-tuned based on the target bandwidth and system/network capabilities
const int BUFFER_SIZE = 1024; // Send 1024 64-bit words at a time (8KB packets)

// --- HELPER: Socket Management ---
UniqueFD create_connection(const std::string &ip, int port, const std::string &proto = "tcp",
                           int internet_type = AF_INET,
                           std::filesystem::path path = std::filesystem::current_path())

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
        memset(&ip_addr4, 0, sizeof(ip_addr4));
        ip_addr4.sin_family = AF_INET;
        ip_addr4.sin_port = htons(port);

        // log report
        std::cout << "Using ipv4 socket." << std::endl;

        // convert IP address to binary form
        if (inet_pton(internet_type, ip.c_str(), &ip_addr4.sin_addr) <= 0)
        {
            perror("Invalid address for ipv4 \n");
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
        ip_addr6.sin6_port = htons(port);
        std::cout << "Using ipv6 socket." << std::endl;

        // convert IP6 address to binary form
        if (inet_pton(internet_type, ip.c_str(), &ip_addr4.sin_addr) <= 0)
        {
            perror("Invalid address for ipv6 \n");
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
        std::cout << "Using UNIX socket." << std::endl;

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
    if (connect(socket_wrapper.get(), final_addr, final_addr_size < 0))
    {
        perror("Connection Failed");
        exit(1);
    }
    return socket_wrapper;
}

// --- CORE: The Spill Simulation ---
void run_spill(int sockfd, double target_mbps)
{
    std::cout << ">>> STARTING SPILL (4.0 seconds) <<<\n";

    // Setup Random Number Generator (64-bit)
    std::mt19937_64 rng(std::random_device{}());

    // 1. Send Preamble (Start)
    send(sockfd, &PREAMBLE_START, sizeof(uint64_t), 0);

    // 2. Stream Data
    auto start_time = std::chrono::steady_clock::now();
    uint64_t total_bytes_sent = 0;
    std::vector<uint64_t> buffer(BUFFER_SIZE);

    // Calculate delay to match bandwidth
    // Bytes per second target
    double bytes_per_sec =
        target_mbps * 1024 * 1024 /
        8.0; // /8 because Mbps usually means bits, MBps is Bytes. Let's assume MB/s for simplicity?
    // Actually, usually BW is Megabits. Let's assume input is Megabytes per Second (MB/s) for
    // easier math. If target is 100 MB/s, and we send 8KB chunks. 8KB = 8192 bytes. Chunks per sec
    // = (Target * 10^6) / 8192. Delay between chunks = 1 / Chunks_per_sec.

    long delay_ns = 0;
    if (target_mbps > 0)
    {
        double chunks_per_sec = (target_mbps * 1000000.0) / (BUFFER_SIZE * 8); // 8 bytes per word
        delay_ns = (long) (1e9 / chunks_per_sec);
    }

    while (true)
    {
        // Check time
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = now - start_time;
        if (elapsed.count() >= SPILL_DURATION_SEC)
            break;

        // Fill buffer with random 64-bit words
        for (int i = 0; i < BUFFER_SIZE; ++i)
        {
            buffer[i] = rng();
        }

        // Send Buffer
        ssize_t sent = send(sockfd, buffer.data(), buffer.size() * sizeof(uint64_t), 0);
        if (sent < 0)
        {
            perror("Send failed");
            break;
        }
        total_bytes_sent += sent;

        // Rate Limit
        if (delay_ns > 0)
        {
            std::this_thread::sleep_for(std::chrono::nanoseconds(delay_ns));
        }
    }

    // 3. Send Preamble (End)
    send(sockfd, &PREAMBLE_END, sizeof(uint64_t), 0);

    std::cout << ">>> SPILL COMPLETE <<<\n";
    std::cout << "    Sent: " << (total_bytes_sent / 1024.0 / 1024.0) << " MB\n";
}

int main(int argc, char **argv)
{
    CLI::App app{"Fake FPGA Client - Particle Spill Simulator"};

    std::string ip = "127.0.0.1";
    int port = 6767;
    std::string proto = "tcp";
    double bandwidth_mbps = 0.0; // 0 = Unlimited

    app.add_option("-i,--ip", ip, "Server IP Address");
    app.add_option("-p,--port", port, "Server Port");
    app.add_option("-n,--network", proto, "Protocol (tcp/udp)")
        ->check(CLI::IsMember({"tcp", "udp"}));
    app.add_option("-b,--bw", bandwidth_mbps, "Bandwidth Limit in MB/s (0 = max)");

    CLI11_PARSE(app, argc, argv);

    std::cout << "Connecting to " << ip << ":" << port << " via " << proto << "...\n";
    int sockfd = create_connection(ip, port, proto);

    char cmd;
    while (true)
    {
        std::cout << "\nReady for spill? (y/n): ";
        std::cin >> cmd;
        if (cmd == 'n')
            break;
        if (cmd == 'y')
        {
            run_spill(sockfd, bandwidth_mbps);
        }
    }

    close(sockfd);
    return 0;
}

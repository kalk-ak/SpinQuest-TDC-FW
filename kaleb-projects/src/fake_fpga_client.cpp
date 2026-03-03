#include <CLI/CLI.hpp>
#include <arpa/inet.h> // For sockets
#include <cassert>
#include <netinet/in.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h> // For close()

#include "fake_fpga_client.h"

// Constructor
FakeFPG::FakeFPG(const std::string &ip, int port, const std::string &proto, int internet_type,
                 double spill_duration_sec, long long buffer_size)

    // Initializer list to initialize the const expressions
    : ip_(ip), port_(port), proto_(proto), internet_type_(internet_type),
      spill_duration_sec_(spill_duration_sec), buffer_size_(buffer_size)
{
    // Set up the connection with the Data Acquisition Server
    _setup_connection();
}

// Helper function to set up the connection
void FakeFPG::_setup_connection()
{
    // Create Socket
    int sockfd = socket(internet_type_, proto_ == "tcp" ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    sockfd_.reset(sockfd);

    // Connect to Server
    struct sockaddr_in server_addr;
    server_addr.sin_family = internet_type_;
    server_addr.sin_port = htons(port_);
    inet_pton(internet_type_, ip_.c_str(), &server_addr.sin_addr);

    if (connect(sockfd_.get(), (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    spdlog::info("Connected to {}:{}", ip_, port_);
}

// --- CORE: The Spill Simulation ---
FakeFPG::void run_spill(int sockfd, double target_mbps)
{
    spdlog::info(">>> STARTING SPILL (4.0 seconds) <<<");

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

    spdlog::info(">>> SPILL COMPLETE <<<");
    spdlog::info("    Sent: {} MB", total_bytes_sent / 1024.0 / 1024.0);
}

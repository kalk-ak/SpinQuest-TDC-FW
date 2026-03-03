#include <CLI/CLI.hpp>
#include <arpa/inet.h> // For sockets
#include <cassert>
#include <netinet/in.h>
#include <random>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h> // For close()

#include "defined_utilites.h"
#include "fake_fpga_client.h"

// Constructor
FakeFPG::FakeFPG(const int id, const std::string &ip, int port, const std::string &proto,
                 int internet_type, double spill_duration_sec, double frequency_Mhz,
                 long long buffer_size)

    // Initializer list to initialize the const expressions
    : id_(id), ip_(ip), port_(port), proto_(proto), internet_type_(internet_type),
      spill_duration_sec_(spill_duration_sec), frequency_Mhz_(frequency_Mhz),
      buffer_size_(buffer_size)
{
    spdlog::debug("Testing FPGA constructor with parameters:");
    // Fail-Fast: Reject physically impossible states immediately
    if (frequency_Mhz < 0.0)
    {
        throw std::invalid_argument("Error: FPGA frequency cannot be negative.");
    }

    if (id < 0)
    {
        throw std::invalid_argument("Error: FPGA ID cannot be negative.");
    }

    if (port < 0 || port > 65535)
    {
        throw std::invalid_argument("Error: Port number must be between 0 and 65535.");
    }

    if (internet_type != AF_INET && internet_type != AF_INET6 && internet_type != AF_UNIX)
    {
        throw std::invalid_argument(
            "Error: Invalid internet type. Must be AF_INET, AF_INET6, or AF_UNIX.");
    }

    if (spill_duration_sec <= 0.0)
    {
        throw std::invalid_argument("Error: Spill duration must be positive.");
    }

    if (frequency_Mhz <= 0.0)
    {
        throw std::invalid_argument("Error: Frequency must be positive.");
    }

    if (buffer_size < 0)
    {
        throw std::invalid_argument("Error: Buffer size cannot be negative.");
    }
    // Set up the connection with the Data Acquisition Server
    sockfd_ = create_connection(ip_, port_, proto_, internet_type_, path_);
    spdlog::debug("FPGA {}: Connected to {}:{}", id_, ip_, port_);
}

// --- CORE: The Spill Simulation ---
void FakeFPG::run_spill()
{
    spdlog::debug(">>> STARTING SPILL ({} seconds) <<<", spill_duration_sec_);

    // Setup Random Number Generator (64-bit)
    std::mt19937 rng(std::random_device{}());

    // Calculate the delay between sends to achieve the target bandwidth
    double target_mbps = frequency_Mhz_ * 1000.0;           // Convert MHz to Mbps
    double bytes_per_sec = target_mbps * 1024 * 1024 / 8.0; // Convert Mbps to Bytes per second

    // Log the time before starting the spill
    auto start_time = std::chrono::steady_clock::now();

    //
    // 1. Send Preamble (Start)
    send(sockfd_, &PREAMBLE_START, sizeof(uint64_t), 0);

    // 2. Stream Data
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

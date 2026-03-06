#include <CLI/CLI.hpp>
#include <arpa/inet.h> // For sockets
#include <cassert>
#include <chrono>
#include <cstdint>
#include <netinet/in.h>
#include <random>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h> // For close()

#include "defined_utilites.h"
#include "fake_fpga_client.h"

// Constructor
FakeFPG::FakeFPG(const std::string id, const std::string ip, int port, const std::string proto,
                 int internet_type, double spill_duration_sec, double frequency_Mhz,
                 long long buffer_size)

    : id_(id), ip_(ip), port_(port), proto_(proto), internet_type_(internet_type),
      spill_duration_sec_(spill_duration_sec), frequency_Mhz_(frequency_Mhz),
      buffer_size_(buffer_size)
{
    spdlog::debug("Testing FPGA constructor with parameters:");

    spdlog::debug("Testing FPGA constructor with parameters:");

    // Fast Fail: Reject physically impossible states immediately
    if (frequency_Mhz == 0.0)
    {
        throw std::invalid_argument("Error: FPGA frequency cannot be negative or zero.");
    }

    if (port < 0 || port > 65535)
    {
        throw std::invalid_argument("Error: Port number must be between 0 and 65535.");
    }

    if (internet_type != AF_INET && internet_type != AF_INET6 && internet_type != AF_UNIX)
    {
        throw std::invalid_argument(
            "Error: Invalid internet type. Only AF_INET, AF_INET6, or AF_UNIX supported.");
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
// TODO: Implement a feature for unlimited overclock (i.e. ignore the frequency parameter and just
// send as fast as possible).
void FakeFPG::run_spill()
{
    spdlog::debug(">>> STARTING SPILL FOR FPGA {} ({} seconds) <<<", id_, spill_duration_sec_);

    // ----- PRECOMPUTE PARAMETERS -----
    // PERF: Precompute the parameters so that we free the main loop from doning unncessary
    // computations

    // check if overclocking is set to unlimited
    bool is_unlimited = (frequency_Mhz_ == 0.0);
    long long delay_ns = 0;
    long long target_iteration = 0;

    if (not is_unlimited)
    {
        // Calculates the amount of data being sent each second
        double size_of_data = (double) sizeof(
            uint64_t); // NOTE: If you change the data you must change the parameter here

        double target_hz = frequency_Mhz_ * 1'000'000; // Convert MHz to Hz (cycles per second)

        // Gets the total bytes in one chunk (buffer)
        double bytes_per_chunk = buffer_size_ * size_of_data; // Total bytes in one chunk

        // get the total bytes sent in each second
        double total_bytes_per_sec = target_hz * size_of_data;

        // get how many chunk per second to send
        double chunks_per_sec =
            total_bytes_per_sec / bytes_per_chunk; // How many chunks we need to send per second to
                                                   // achieve the target bandwidth
        delay_ns = (long) (1e9 / chunks_per_sec);  // Delay in nanoseconds between sending each
                                                   // chunk to achieve the target bandwidth

        // get the target amount of iterations needed to simulate the spill duration
        // NOTE: Being static_cast to long long because std::chrono::nanoseconds takes a long long
        // as parameter and we want to avoid overflow
        long long target_iteration = static_cast<long long>(chunks_per_sec * spill_duration_sec_);
    }

    // pre fill the Buffer so that Zero RNG overhead in the tight loop
    std::vector<uint64_t> buffer(buffer_size_);
    std::mt19937_64 rng(std::random_device{}());
    for (int i = 0; i < buffer_size_; ++i)
    {
        buffer[i] = rng();
    }

    // Time the spill
    auto start_time = std::chrono::steady_clock::now();
    send(sockfd_.get(), &PREAMBLE_START, sizeof(uint64_t), 0);

    // Track the total bytes sent and the current iteration
    long long total_bytes_sent = 0;
    long long current_iteration = 0;
    long long failed_sends = 0;

    // 2 --- STREAM DATA ---
    while (true)
    {
        // --- LOOP BREAK CONDITIONS ---
        if (is_unlimited)
        {
            // OVERCLOCK MODE: Check the clock every 1000 sends to save CPU cycles
            if (current_iteration % 1000 == 0)
            {
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration<double>(now - start_time).count() >= spill_duration_sec_)
                {
                    break;
                }
            }
        }
        else
        {
            // LIMITED MODE: Just count iterations (Fastest)
            if (current_iteration >= target_iteration)
            {
                break;
            }
        }

        // send the data
        ssize_t sent = send(sockfd_.get(), buffer.data(), buffer.size() * sizeof(uint64_t), 0);
        if (sent < 0)
        {
            spdlog::error("Send failed at iteration {}: {}", current_iteration, strerror(errno));
            failed_sends += 1;
        }
        else
        {
            // track the successful send for debugging
            total_bytes_sent += sent;
        }

        // track the current iteration
        ++current_iteration;

        // Rate Limit of speed
        // NOTE: delay is set to zero for unlimited overclock mode
        if (delay_ns > 0)
        {
            std::this_thread::sleep_for(std::chrono::nanoseconds(delay_ns));
        }
    }
    // 3 ------ End Spill ------
    send(sockfd_.get(), &PREAMBLE_END, sizeof(uint64_t), 0);

    // Get the end time
    auto end_time = std::chrono::steady_clock::now();

    // LOGGING: Log the results of the spill
    spdlog::info(">>> SPILL COMPLETE FOR FPGA {} <<<", id_);
    spdlog::debug("\tSent: {:.2f} MB across {} iterations", total_bytes_sent / 1024.0 / 1024.0,
                  current_iteration);
    spdlog::debug("\tSkipped {} sends due to errors.", failed_sends);
    std::chrono::duration<double> elapsed_seconds = end_time - start_time;
    spdlog::info("\tElapsed Time: {:.2f} seconds", elapsed_seconds.count());
}

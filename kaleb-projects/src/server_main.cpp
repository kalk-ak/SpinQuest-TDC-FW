#include "daq_server.h"
#include <CLI/CLI.hpp>
#include <csignal>
#include <memory>
#include <spdlog/spdlog.h>

// Global pointer required so the signal handler can access the server instance
std::unique_ptr<DAQServer> global_server = nullptr;

// Traps OS signals (like Ctrl+C) to ensure a graceful shutdown
void signal_handler(int signum)
{
    spdlog::warn("\nInterrupt signal ({}) received. Initiating graceful shutdown...", signum);
    if (global_server)
    {
        // This will wake up all blocking recv() calls and join the threads
        global_server->stop();
    }
}

int main(int argc, char *argv[])
{
    // Register the signal handler for SIGINT (Ctrl+C) and SIGTERM (Kill command)
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    CLI::App app{"DAQ Server - High Throughput Particle Spill Receiver"};

    // ==========================================
    // SERVER CONFIGURATION DEFAULTS
    // ==========================================
    std::string ip = "666.6.6.7";
    int port = 6767;
    std::string output_dir = "./spill_data";
    std::string network_type_str = "tcp";
    std::string domain_type_str = "ipv4";
    size_t recv_buffer_size = 8192; // 8KB network chunks
    int num_udp_workers = 4;        // Number of SO_REUSEPORT threads
    bool verbose = false;

    // Add command-line options
    app.add_option("-i,--ip", ip, "Server IP Address to bind to (or file path for UNIX)");
    app.add_option("-p,--port", port, "Port number");
    app.add_option("-o,--out", output_dir, "Directory to save the binary .dat files");

    app.add_option("-n,--network", network_type_str, "Protocol (tcp/udp)")
        ->check(CLI::IsMember({"tcp", "udp"}));

    app.add_option("-d,--domain", domain_type_str, "Domain type (ipv4/ipv6/unix)")
        ->check(CLI::IsMember({"ipv4", "ipv6", "unix"}));

    app.add_option("-b,--buffer", recv_buffer_size, "Network receive buffer size in bytes");
    app.add_option("-w,--workers", num_udp_workers, "Number of concurrent UDP listeners");
    app.add_flag("-v,--verbose", verbose, "Enable verbose debug logging");

    // Parse the terminal arguments
    CLI11_PARSE(app, argc, argv);

    // Toggle debug logging if the verbose flag was passed
    if (verbose)
    {
        spdlog::set_level(spdlog::level::debug);
    }

    // Map the string inputs to the Linux socket macros
    int network_type = (network_type_str == "udp") ? SOCK_DGRAM : SOCK_STREAM;

    // Mapping internet Protocol type (or linux file system)
    int internet_type = AF_INET;
    if (domain_type_str == "ipv6")
    {
        internet_type = AF_INET6;
    }
    else if (domain_type_str == "unix")
    {
        internet_type = AF_UNIX;
    }

    try
    {
        spdlog::info("Booting up DAQ Server...");

        // Initialize the server using the smart pointer and the parsed arguments
        global_server = std::make_unique<DAQServer>(
            ip, port, output_dir, internet_type, network_type, recv_buffer_size, num_udp_workers);

        // start() blocks the main thread in an infinite loop.
        // It will only return when stop() is called by the signal handler.
        global_server->start();
    }
    catch (const std::exception &e)
    {
        spdlog::critical("Fatal Server Crash: {}", e.what());
        return EXIT_FAILURE;
    }

    spdlog::info("Main thread exiting. Goodbye!");
    return EXIT_SUCCESS;
}

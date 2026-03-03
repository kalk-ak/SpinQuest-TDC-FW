#ifndef FAKE_FPGA_CLIENT_H

#include "UniqueFD.h"
#include <arpa/inet.h> // For sockets
#include <filesystem>
#include <string>
#include <sys/socket.h>

// --- CONFIGURATION ---
// TODO: Make configurable via CLI

class FakeFPG
{
    // TODO: Add megahz bandwidth control
    // and speed (sleep time between sends)
  public:
    // constructor
    FakeFPG(const std::string &ip, int port, const std::string &proto = "tcp",
            int internet_type = AF_INET, double spill_duration_sec = 4.0,
            long long buffer_size = 1024);

    // delete because it has a filedescriptor and we don't want to accidentally copy it
    // also no need to have assignment operators as the sole purpose of this class
    // is to run a spill and then be destroyed. If you want to run multiple spills, just create
    // multiple instances of this class.
    FakeFPG(FakeFPG &&) = delete;
    FakeFPG(const FakeFPG &) = delete;

    // TODO: Add move and copy assignment operators
    FakeFPG &operator=(FakeFPG &&) = delete;
    FakeFPG &operator=(const FakeFPG &) = delete;

    // Deconstructor
    ~FakeFPG();

    // Main interface
    void run_spill();

  private:
    // NOTE: Setting everything as const because all that the FAKE FPGA client
    // needs to do is send data to a specific destination. It doesn't need to modify these
    // parameters after construction.
    const int id;
    const std::string &ip_;
    const int port_;
    const std::string &proto_;
    const int internet_type_;
    const std::filesystem::path path_ = "/tmp/fpga_socket"; // Default path for UNIX domain socket

    // HACK: Change preemble values here
    // captures and logs
    static constexpr uint64_t PREAMBLE_START = 0xAAAAAAAABBBBBBBB;
    static constexpr uint64_t PREAMBLE_END = 0xDEADBEEFDEADBEEF;

    // NOTE: Can be hyer-tuned based on the target bandwidth and system/network capabilities.
    // this is why i haven't set it to static constexpr
    const long long buffer_size_ = 1024; // Send 1024 64-bit words at a time (8KB packets)

    // Not setting it static constexpr because we want to be able to configure it via CLI in the
    // future. But for now, it's a constant.
    const double spill_duration_sec_ = 4.0;

    // Using the UniqueFD pointer defined using RAII
    UniqueFD sockfd_;

    // Private Utilities for setting up the connection
    void _setup_connection();
};

#endif

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
    /**
     * @brief Constructs a Fake FPGA board emulator and establishes its network connection.
     * * This constructor acts as the initialization phase for the FPGA. It stores the
     * networking parameters, calculates the required bandwidth delays, and actively
     * binds/connects the internal RAII socket (`UniqueFD`) to the target server.
     * * @param id The unique identifier for this simulated board (e.g., 1 to 100).
     * @param ip The target IP address or UNIX socket path of the DAQ server.
     * @param port The target port on the DAQ server.
     * @param proto The transport protocol to use ("tcp" or "udp"). Defaults to "tcp".
     * @param internet_type The address family (AF_INET, AF_INET6, or AF_UNIX). Defaults to AF_INET.
     * @param spill_duration_sec The length of the particle spill in seconds. Defaults to 4.0.
     * @param frequency_Mhz The target transmission rate in MHz. Defaults to 4.
     * @param buffer_size The number of 64-bit words to send in a single packet. Defaults to 1024.
     * * @throws std::runtime_error If the socket creation or connection fails.
     */
    FakeFPG(const int id, const std::string &ip, int port, const std::string &proto = "tcp",
            int internet_type = AF_INET, double spill_duration_sec = 4.0, double frequency_Mhz = 4,
            long long buffer_size = 1024);

    /**
     * @brief Executes a single particle accelerator spill cycle.
     * * This method simulates the high-throughput data generation of an FPGA during a beam spill.
     * It performs the following sequence:
     * 1. Transmits a predefined 64-bit PREAMBLE_START marker.
     * 2. Enters a high-speed loop, generating and transmitting buffers of random 64-bit integers.
     * 3. Uses the configured `frequency_Mhz_` to sleep between sends, rate-limiting the bandwidth.
     * 4. Exits the loop after `spill_duration_sec_` has elapsed.
     * 5. Transmits a predefined 64-bit PREAMBLE_END marker.
     */
    void run_spill();

    // delete because it has a file descriptor and we don't want to accidentally copy it
    // also no need to have assignment operators as the sole purpose of this class
    // is to run a spill and then be destroyed. If you want to run multiple spills, just create
    // multiple instances of this class.
    FakeFPG(FakeFPG &&) = delete;
    FakeFPG(const FakeFPG &) = delete;
    FakeFPG &operator=(FakeFPG &&) = delete;
    FakeFPG &operator=(const FakeFPG &) = delete;

    // Deconstructor
    ~FakeFPG();

    // Add getters and setters for debugging
    int get_id() const
    {
        return id_;
    }
    std::string get_ip() const
    {
        return ip_;
    }
    int get_port() const
    {
        return port_;
    }
    std::string get_proto() const
    {
        return proto_;
    }
    int get_internet_type() const
    {
        return internet_type_;
    }

  private:
    // NOTE: Setting everything as const because all that the FAKE FPGA client
    // needs to do is send data to a specific destination. It doesn't need to modify these
    // parameters after construction.
    const int id_;
    const double frequency_Mhz_; // Frequency in MHz, used to calculate the delay between sends to
                                 // achieve the target bandwidth

    const std::string ip_;
    const int port_;
    const std::string proto_;
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
};

#endif

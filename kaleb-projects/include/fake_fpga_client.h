#include <sys/socket.h>
#ifndef FAKE_FPGA_CLIENT_H

#include "UniqueFD.h"
#include <arpa/inet.h> // For sockets
#include <string>

// --- CONFIGURATION ---
// TODO: Make configurable via CLI

class FakeFPG
{
    // TODO: Add megahz bandwidth control
    // and speed (sleep time between sends)
  public:
    FakeFPG(const std::string &ip, int port, const std::string &proto = "tcp",
            int internet_type = AF_INET);
    FakeFPG(FakeFPG &&) = default;
    FakeFPG(const FakeFPG &) = default;
    FakeFPG &operator=(FakeFPG &&) = default;
    FakeFPG &operator=(const FakeFPG &) = default;
    ~FakeFPG();

  private:
    const std::string &ip_;
    int prot_;
    std::string &proto_;
    int internet_type_;
    uint64_t PREAMBLE_START = 0xAAAAAAAABBBBBBBB;
    uint64_t PREAMBLE_END = 0xDEADBEEFDEADBEEF;
    double SPILL_DURATION_SEC = 4.0;
};

FakeFPG::FakeFPG()
{
}

FakeFPG::~FakeFPG()
{
}
/**
 * @brief Creates and connects a client socket to a remote server.
 *
 *
 * @note This function terminates the program (exit 1) on any failure.
 * fixed usage of `struct sockaddr_in`.
 *
 * @param ip The target IP address string
 * @param port The target port number.
 * @param proto The protocol to use: "tcp" or "udp" (default: "tcp").
 * @param internet_type The address family (AF_INET, AF_UNIX, AF_INET6) (default: AF_INET).
 * @param path The filesystem path for UNIX sockets (default: current working directory).
 *
 * @return UniqueFD An RAII wrapper owning the connected socket file descriptor.
 */
UniqueFD create_connection(const std::string &ip, int port, const std::string &proto = "tcp",
                           int internet_type = AF_INET);

#endif // !#ifndef FAKE_FPGA_CLIENT_H

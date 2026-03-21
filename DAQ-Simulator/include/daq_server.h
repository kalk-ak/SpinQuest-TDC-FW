#ifndef DAQ_SERVER_H
#define DAQ_SERVER_H

#include "UniqueFD.h"
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unordered_map>
#include <vector>

class DAQServer
{
  public:
    /**
     * @brief Constructs the DAQ Server.
     * @param ip The IP address to bind to.
     * @param port The port to listen on.
     * @param output_directory The folder where the binary spill data will be saved.
     * @param internet_type AF_INET, AF_INET6, or AF_UNIX.
     * @param network_type SOCK_STREAM (TCP) or SOCK_DGRAM (UDP).
     * @param recv_buffer_size Hyperparameter: The size of the network read buffer in bytes.
     * @param num_udp_workers Hyperparameter: The number of concurrent listeners for UDP load
     * balancing.
     */
    DAQServer(const std::string &ip, int port, const std::string &output_directory = "./spill_data",
              int internet_type = AF_INET, int network_type = SOCK_STREAM,
              size_t recv_buffer_size = 8192, int num_udp_workers = 4);

    // Prevent copying because owns a listening socket and active threads
    DAQServer(const DAQServer &) = delete;
    DAQServer &operator=(const DAQServer &) = delete;

    ~DAQServer();

    /**
     * @brief Starts the server listening loop. Blocks the current thread.
     */
    void start();

    /**
     * @brief Gracefully shuts down the server, closes the port, and joins all client threads.
     */
    void stop();

  private:
    const std::string ip_;
    const int port_;
    const int internet_type_;
    const int network_type_;

    const std::filesystem::path unix_path_ =
        "/tmp/fpga_socket"; // Default path for UNIX domain socket

    // Path where the spill data files will be saved. Each file will be named "spill_board_X.dat"
    // where X is the client ID.
    const std::filesystem::path output_dir_;

    // Hyperparameter for tuning network throughput
    const size_t recv_buffer_size_;

    // Hyperparameter for UDP Thread Pool size
    const int num_udp_workers_;

    // RAII wrapper for the main listening socket
    UniqueFD server_socket_;

    // Concurrency management
    // the reason we need atomic for is_running_ is because it is accessed by both the main thread
    // (which calls start and stop) and the worker threads (which check it in their loops). Using
    // atomic ensures that changes to is_running_ are immediately visible across all threads.
    std::atomic<bool> is_running_{false};

    // pool of worker threads handling each connected FPGA client.
    std::vector<std::thread> client_threads_;

    // NEW: Tracks the raw socket IDs of the UDP listeners so stop() can wake them up
    std::vector<int> udp_worker_fds_;
    std::mutex udp_fds_mutex_;

    // Preambles to match the FakeFPGA client
    static constexpr uint64_t PREAMBLE_START = 0xAAAAAAAABBBBBBBB;
    static constexpr uint64_t PREAMBLE_END = 0xDEADBEEFDEADBEEF;

    // Maximum number of connection requests that would be queued by a server
    // NOTE: You can change this here to any number you like. For our research we are using 64
    // boards
    static constexpr int MAX_CONNECTION_WAITING_QUEUE = 64;

    /**
     * @brief Configures the socket, binds it to the port, and starts listening.
     */
    void setup_server_socket();

    /**
     * @brief The main server loop for handling UDP and UNIX filesystem clients.
     * Because UDP is connectionless, we use SO_REUSEPORT to spin up multiple listeners
     * and let the Linux kernel load-balance the packets across our CPU cores.
     */
    void run_datagram_server();
    void datagram_worker(int worker_id); // The concurrent port listeners

    /**
     * @brief The main server loop for handling TCP clients.
     */
    void run_stream_server();
    void handle_stream_client(UniqueFD client_socket, int client_id); // The spawned receiver
};

#endif

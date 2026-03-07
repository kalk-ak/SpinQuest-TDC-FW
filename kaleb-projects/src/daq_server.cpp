#include "daq_server.h"
#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>

DAQServer::DAQServer(const std::string &ip, int port, const std::string &output_directory,
                     int internet_type, int network_type, size_t recv_buffer_size,
                     int num_udp_workers)
    : ip_(ip), port_(port), output_dir_(output_directory), internet_type_(internet_type),
      network_type_(network_type), recv_buffer_size_(recv_buffer_size),
      num_udp_workers_(num_udp_workers)
{
    // Check if the directory is available and if not create it
    if (not std::filesystem::exists(output_dir_))
    {
        std::filesystem::create_directories(output_dir_);
    }

    // Helper function to set up the server
    setup_server_socket();
}

// Deconstructor
DAQServer::~DAQServer()
{
    stop();
}

void DAQServer::setup_server_socket()
{
    // Dynamically uses SOCK_STREAM, SOCK_DGRAM, or AF_UNIX based on user input
    int raw_fd = socket(internet_type_, network_type_, 0);
    if (raw_fd < 0)
    {
        throw std::runtime_error("Server socket creation failed");
    }

    // Wrap the socket ID into a RAII file descriptor manager to ensure it gets closed properly
    server_socket_.reset(raw_fd);

    // Set the socket to be reusable many times.
    // NOTE: Normally the kernel would reserve a port after being disconnected for a few mins but
    // setting SO_REUSEADDR to true we can run the simulation many times over without changing the
    // address. In short, this allows us to restart the server immediately after stopping it without
    // waiting for it.
    int true_opt = true;

    struct sockaddr *addr_ptr = nullptr;
    socklen_t addr_len = 0;
    struct sockaddr_in addr4;   // IPV4
    struct sockaddr_un addr_un; // UNIX domain socket address structure
    struct sockaddr_in6 addr6;  // Added support for ipv6 connections

    std::string net_str;

    // Set up connection
    if (internet_type_ == AF_INET)
    {
        // IPV4
        setsockopt(server_socket_.get(), SOL_SOCKET, SO_REUSEADDR, &true_opt, sizeof(true_opt));

        // If UDP, tell the kernel to allow multi-core port sharing
        if (network_type_ == SOCK_DGRAM)
        {
            setsockopt(server_socket_.get(), SOL_SOCKET, SO_REUSEPORT, &true_opt, sizeof(true_opt));
        }

        // Set up IPV4
        memset(&addr4, 0, sizeof(addr4));
        addr4.sin_family = AF_INET;
        addr4.sin_port = htons(port_);
        inet_pton(AF_INET, ip_.c_str(), &addr4.sin_addr);
        addr_ptr = (struct sockaddr *) &addr4;
        addr_len = sizeof(addr4);
        net_str = (network_type_ == SOCK_STREAM) ? "IPv4 TCP (Stream)" : "IPv4 UDP (Datagram)";
    }
    else if (internet_type_ == AF_INET6)
    {
        // IPv6
        setsockopt(server_socket_.get(), SOL_SOCKET, SO_REUSEADDR, &true_opt, sizeof(true_opt));

        // if UDP, tell the kernel to allow multi-core port sharing to maximize core utilization
        if (network_type_ == SOCK_DGRAM)
        {
            setsockopt(server_socket_.get(), SOL_SOCKET, SO_REUSEPORT, &true_opt, sizeof(true_opt));
        }

        // Set up IPV6
        memset(&addr6, 0, sizeof(addr6));
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port = htons(port_);
        inet_pton(AF_INET6, ip_.c_str(), &addr6.sin6_addr);

        addr_ptr = (struct sockaddr *) &addr6;
        addr_len = sizeof(addr6);
        net_str = (network_type_ == SOCK_STREAM) ? "IPv6 TCP (Stream)" : "IPv6 UDP (Datagram)";
    }
    else if (internet_type_ == AF_UNIX)
    {
        // UNIX File Socket (Does not support reuse port (SO_REUSEPORT))
        memset(&addr_un, 0, sizeof(addr_un));
        addr_un.sun_family = AF_UNIX;
        strncpy(addr_un.sun_path, ip_.c_str(), sizeof(addr_un.sun_path) - 1);
        unlink(ip_.c_str());
        addr_ptr = (struct sockaddr *) &addr_un;
        addr_len = sizeof(addr_un);
        net_str = "Unix Domain Socket";
    }
    else
    {
        throw std::runtime_error("Unsupported internet type. Use AF_INET, AF_INET6, or AF_UNIX.");
    }

    // Bind the socket to the specified IP and port (or file path for UNIX sockets)
    if (bind(server_socket_.get(), addr_ptr, addr_len) < 0)
    {
        throw std::runtime_error("Server bind failed: " + std::string(strerror(errno)));
    }

    // ONLY TCP uses listen(). UDP skips this entirely.
    if (network_type_ == SOCK_STREAM)
    {
        // Waits for clients to connect.
        // the queue has a MAX_CONNECTION_WAITING_QUEUE defined in the header file
        // default 64
        if (listen(server_socket_.get(), MAX_CONNECTION_WAITING_QUEUE) < 0)
        {
            throw std::runtime_error("Server listen failed");
        }
    }

    // End of setup, print some info about the server configuration
    spdlog::info("DAQ Server bound via {}. Tuning buffer set to {} bytes.", net_str,
                 recv_buffer_size_);
}

void DAQServer::start()
{
    is_running_ = true;
    spdlog::info(">>> DAQ SERVER ACTIVATED ON PORT {} <<<", port_);

    // Route to the correct architecture
    if (network_type_ == SOCK_STREAM)
    {
        // spawn a new thread for each client connection that comes in.
        run_stream_server();
    }
    else
    {
        // NOTE: Unix and UDP both use the datagram architecture since they don't have connections
        // to spawn threads for. Instead for UDP what we do is we spawn multiple threads that all
        // bind to the same port and then the kernel load balances
        run_datagram_server();
    }
}

void DAQServer::stop()
{
    if (not is_running_)
    {
        spdlog::warn("DAQ Server is already stopped.");
        return;
    }
    is_running_ = false;

    // HACK: by setting the server socket to -1,
    // we can wake up any blocking calls to recv() or accept() in the worker threads, allowing them
    // to check the is_running_ flag and exit gracefully.
    server_socket_.reset();

    // NOTE: Wake up all concurrent UDP listeners by shutting down their read channels
    for (int fd : udp_worker_fds_)
    {
        shutdown(fd, SHUT_RDWR);
    }

    // collect all the worker threads and join them to ensure a clean shutdown
    for (auto &t : client_threads_)
    {
        if (t.joinable())
            t.join();
    }
    spdlog::info("DAQ Server stopped cleanly.");
}

// ============================================================================
// ARCHITECTURE 1: TCP (Stream) - Spawn Receiver per Connection
// ============================================================================
void DAQServer::run_stream_server()
{
    // Because the server could accept 50+ connections in a
    // fraction of a second, we need a way to Track them
    int client_id_counter = 1;
    while (is_running_)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        // try initiating a connection. If the server socket is closed (e.g., by stop()), this will
        // return -1 and we can break out of the loop to allow the threads to exit gracefully.
        int client_fd = accept(server_socket_.get(), (struct sockaddr *) &client_addr, &client_len);
        if (client_fd < 0)
        {
            spdlog::error("Failed to accept client connection: {}", strerror(errno));
            break;
        }

        // Cast the file descriptor to a RAII wrapper to ensure it gets closed properly when the
        // thread finishes
        UniqueFD safe_client_fd(client_fd);

        // PERF: emplace_back is faster than push_back
        client_threads_.emplace_back(&DAQServer::handle_stream_client, this,
                                     std::move(safe_client_fd), client_id_counter++);

        // NOTE: std::move is used here to transfer ownership of the client socket's file descriptor
        // into the new thread's context. This allows the thread to manage the lifecycle. If we
        // haven't moved it here then when the while loop reaches the end of the current iteration,
        // the UniqueFD destructor would close the client socket leading to an error
    }
}

void DAQServer::handle_stream_client(UniqueFD client_socket, int client_id)
{
    // HACK:
    // First we start by creating the output file for this specific client connection. We name it
    // "tcp_spill_board_X.dat" where X is the client ID which is just a counter that increments for
    // every new connection. This way we can keep track of which file corresponds to which client
    // connection.
    std::filesystem::path file_path =
        output_dir_ / ("tcp_spill_board_" + std::to_string(client_id) + ".dat");

    // HACK: declaring the output file as binary is important to ensure that the data is written in
    // the correct format without any changes or corruption. If we don't specify std::ios::binary,
    // the ofstream might perform newline translations (e.g., converting "\n" to "\r\n" on Windows)
    // which would corrupt the binary data coming from the DAQ. By using std::ios::binary, we ensure
    // that the data is written exactly as it is received without any modifications, preserving the
    // integrity of the spill data.
    std::ofstream out_file(file_path, std::ios::binary);

    // NOTE: buffer size is a hyperparameter that can be tuned to optimize throughput. A larger
    // buffer size means we can write to disk less frequently which can improve performance, but it
    // also means we need more memory.
    std::vector<uint8_t> buffer(recv_buffer_size_);

    // recording is a state variable that tracks whether we are currently in the middle of a spill
    // set to true when preemble start is detected and set to false when preamble end is detected.
    bool recording = false;
    long long total_bytes = 0;

    // main loop: keep reading data from the client socket until the connection is closed or an
    // error occurs
    while (is_running_)
    {
        // NOTE: vector.data() returns a pointer to the start of the array
        ssize_t bytes_read = recv(client_socket.get(), buffer.data(), buffer.size(), 0);

        // check if connection is ok
        if (bytes_read < 0)
        {
            spdlog::error("Netowrk error encountered with {}", client_id);
            break;
        }
        else if (bytes_read == 0)
        {
            spdlog::error("Client {} disconnected unexpectedly.", client_id);
            break;
        }

        // Get the total number of words read and check if any of them have the start or end
        // preamble
        size_t words_read = bytes_read / sizeof(uint64_t);
        uint64_t *word_ptr = reinterpret_cast<uint64_t *>(buffer.data());
        size_t write_start_idx = 0;

        // NOTE: Have a for loop to check for the start and end preamble
        for (size_t i = 0; i < words_read; ++i)
        {
            if (not recording && word_ptr[i] == PREAMBLE_START)
            {
                recording = true;
                write_start_idx = i + 1; // start recording the data at the next 64bit word
            }
            else if (recording && word_ptr[i] == PREAMBLE_END)
            {
                size_t words_to_write = i - write_start_idx;

                // start writing from the start index to the end which equals num_words_to_write * 8
                out_file.write(reinterpret_cast<const char *>(&word_ptr[write_start_idx]),
                               words_to_write * 8);
                total_bytes += words_to_write * 8;
                recording = false;
                break;
            }
        }

        if (recording)
        {
            // write the received buffer to memory
            size_t words_to_write = words_read - write_start_idx;
            out_file.write(reinterpret_cast<const char *>(&word_ptr[write_start_idx]),
                           words_to_write * 8);
            total_bytes += words_to_write * 8;
        }

        if (not recording && total_bytes > 0)
        {
            spdlog::error("PREAMBLE Not sent in the first set of buffer for client {}", client_id);
            break;
        }
    }
    spdlog::info("TCP Worker {}: Saved {:.2f} MB.", client_id, total_bytes / (1024 * 1024));
}

// ============================================================================
// ARCHITECTURE 2: UDP (Datagram) - Multiple Listeners on the Same Port
// ============================================================================
void DAQServer::run_datagram_server()
{
    // WARN:: This is an unconventional approach. What I am doing is that I am creating multiple
    // listeners on the same port to utilize concurrency in UDP. There must be a way to re order the
    // data once we have collected them
    spdlog::info("Spawning {} concurrent UDP listeners on port {}...", num_udp_workers_, port_);

    // We don't use accept(). We just spin up multiple threads that all bind to the same port.
    for (int i = 0; i < num_udp_workers_; ++i)
    {
        client_threads_.emplace_back(&DAQServer::datagram_worker, this, i + 1);
    }

    // Keep the main thread alive while the listeners do the work
    while (is_running_)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

void DAQServer::datagram_worker(int worker_id)
{
    // Create a brand new socket for this specific thread
    int thread_fd = socket(internet_type_, network_type_, 0);
    if (thread_fd < 0)
        return;

    // Add the SO_REUSEPORT flag so it can share the port with the other threads
    // Add the SO_REUSEADDR so that there is no cool down in using the port
    int opt = 1;
    setsockopt(thread_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(thread_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    // Bind to the exact same address
    struct sockaddr_in addr4;
    memset(&addr4, 0, sizeof(addr4));
    addr4.sin_family = AF_INET;
    addr4.sin_port = htons(port_);
    inet_pton(AF_INET, ip_.c_str(), &addr4.sin_addr);

    // Error checker to see if we've binded correctly
    if (bind(thread_fd, (struct sockaddr *) &addr4, sizeof(addr4)) < 0)
    {
        close(thread_fd);
        return;
    }

    // Track it in a list so stop() can shut it down safely
    udp_worker_fds_.push_back(thread_fd);
    UniqueFD safe_thread_socket(thread_fd);

    spdlog::info("UDP Listener {} ready and bound.", worker_id);

    // Because UDP has no threads per connection, we multiplex by mapping IP:Port strings to
    // specific files
    struct UDPClientState
    {
        bool recording = false;
        long long total_bytes = 0;
        std::ofstream file;
    };

    // OPTIMIZE: Hashmaps can be slow. Perhaps embed the id inside the word and just dump into a big
    // binary
    std::unordered_map<std::string, UDPClientState> clients;

    std::vector<uint8_t> buffer(recv_buffer_size_);

    while (is_running_)
    {
        // store the sendsers address so we can create a unique file for each sender
        struct sockaddr_in sender_addr;
        socklen_t sender_len = sizeof(sender_addr);

        // recvfrom() grabs the packet AND tells us who sent it
        ssize_t bytes_read = recvfrom(safe_thread_socket.get(), buffer.data(), buffer.size(), 0,
                                      (struct sockaddr *) &sender_addr, &sender_len);

        // Check for errors
        if (bytes_read == 0)
        {
            spdlog::error("Connection closed for UDP Worker {}. Exiting thread.", worker_id);
            break;
        }
        else if (bytes_read < 0)
        {
            spdlog::error("Network error encountered in UDP Worker {}: {}", worker_id,
                          strerror(errno));
            break;
        }

        // Create a unique string ID for this specific board (e.g., "127.0.0.1:54321")
        // key = IP + PORT
        std::string client_id = std::string(inet_ntoa(sender_addr.sin_addr)) + ":" +
                                std::to_string(ntohs(sender_addr.sin_port));

        // Get or create the state for this client
        // NOTE: This variable is very important.
        UDPClientState &state = clients[client_id];

        // Calculate the number of 64-bit words read and get a pointer to the buffer as an array of
        // 64-bit
        size_t words_read = bytes_read / sizeof(uint64_t);
        uint64_t *word_ptr = reinterpret_cast<uint64_t *>(buffer.data());
        size_t write_start_idx = 0;

        for (size_t i = 0; i < words_read; ++i)
        {
            if (not state.recording && word_ptr[i] == PREAMBLE_START)
            {
                state.recording = true;
                write_start_idx = i + 1;

                // Open file the first time we see a start preamble from this board
                if (not state.file.is_open())
                {
                    // INFO:
                    // Because of the kernel level load balancing, we are guaranteed that only one
                    // thread will ever see packets from this specific board, so we can safely
                    // create a file for it without
                    std::filesystem::path fp =
                        output_dir_ / ("udp_spill_" + std::to_string(clients.size()) + ".dat");
                    state.file.open(fp, std::ios::binary);
                }
            }
            else if (state.recording && word_ptr[i] == PREAMBLE_END)
            {
                size_t words_to_write = i - write_start_idx;
                state.file.write(reinterpret_cast<const char *>(&word_ptr[write_start_idx]),
                                 words_to_write * 8);
                state.total_bytes += words_to_write * 8;
                state.recording = false;

                spdlog::info("UDP Worker {} -> Stream {}: Saved {:.2f} MB.", worker_id, client_id,
                             state.total_bytes / (1024 * 1024));
                state.file.close(); // Close the file when the spill ends
            }
        }

        if (state.recording && state.file.is_open())
        {
            size_t words_to_write = words_read - write_start_idx;
            state.file.write(reinterpret_cast<const char *>(&word_ptr[write_start_idx]),
                             words_to_write * 8);
            state.total_bytes += words_to_write * 8;
        }
    }

    // INFO: How Thread Safety is Guaranteed Without Mutexes
    // -------------------------------------------------------------------------
    // Because UDP is connectionless, you might wonder how we prevent two different
    // threads from grabbing packets from the same FPGA board and corrupting the
    // same output file simultaneously.
    //
    // INFO: That is because of the SO_REUSEPORT flag. When multiple threads bind to the exact
    // same port using this flag, the Linux kernel acts as a deterministic load balancer.
    //
    // INFO: 1. When a packet arrives, the kernel calculates a mathematical hash based on
    //    a 4-tuple: (Source IP, Source Port, Destination IP, Destination Port).
    // 2. Because an FPGA board's Source IP and Source Port never change during a
    //    continuous spill, the hash function always produces the exact same result.
    // 3. Therefore, 100% of the packets sent by a specific board (e.g., Board 5)
    //    will ALWAYS be routed to the exact same worker thread.
    //
    // INFO: This hardware level routing guarantees that our thread local `unordered_map`
    // and file streams are safe. No two threads will ever see data from
    // the same board, giving us lock-free, multi-core disk I/O performance.
}

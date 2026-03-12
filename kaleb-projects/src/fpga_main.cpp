#include "fake_fpga_client.h"
#include <CLI/CLI.hpp>
#include <fstream>
#include <iostream>
#include <memory>
#include <spdlog/spdlog.h>
#include <thread>
#include <unordered_set>
#include <vector>

int main(int argc, char **argv)
{
    CLI::App app{"Fake FPGA Client - Particle Spill Simulator"};
    bool verbose = false; // Set logging level to debug if verbose flag is passed

    std::string ip = "127.0.0.1";
    int port = 6767;
    std::string proto = "tcp";
    double frequency_mhz = 4.0;
    std::string domain = "ipv4";

    // New Concurrency Arguments
    int num_boards = 1;
    std::string names_file = "";

    app.add_option("-i,--ip", ip, "Server IP Address");
    app.add_option("-p,--port", port, "Server Port");
    app.add_option("-n,--network", proto, "Protocol (tcp/udp)")
        ->check(CLI::IsMember({"tcp", "udp"}));
    app.add_option("-f,--freq", frequency_mhz, "FPGA Clock Frequency in MHz (0 = unlimited)")
        ->check(CLI::NonNegativeNumber);
    app.add_option("-b,--boards", num_boards, "Number of concurrent boards to simulate");
    app.add_option("--names", names_file, "Path to text file containing board names");
    app.add_flag("-v,--verbose", verbose, "Enable verbose debug logging");
    app.add_option("-d,--domain", domain, "Domain Socket Type (unix/ipv4/ipv6)")
        ->check(CLI::IsMember({"unix", "ipv4", "ipv6"}));

    CLI11_PARSE(app, argc, argv);

    // Set logging level to debug if the verbose flag was passed
    if (verbose)
    {
        spdlog::set_level(spdlog::level::debug);
    }

    // get the network type
    int internet_type;
    if (domain == "ipv4")
    {
        spdlog::debug("Using IPv4 socket");
        internet_type = AF_INET;
    }
    else if (domain == "unix")
    {
        spdlog::debug("Using UNIX domain socket");
        internet_type = AF_UNIX;
    }
    else if (domain == "ipv6")
    {
        internet_type = AF_INET6;
        spdlog::debug("Using IPv6 socket");
    }

    try
    {
        // Parse the names file if provided
        std::vector<std::string> loaded_names;
        if (not names_file.empty())
        {
            // Read the files to get the names of the boards
            std::ifstream file(names_file);
            if (not file.is_open())
            {
                // check if the file is valid and can be opened
                throw std::runtime_error("Could not open names file: " + names_file);
            }

            // get as many names as possible from the file
            std::string line;
            while (std::getline(file, line))
            {
                // ignore empty lines in the file
                if (not line.empty())
                    loaded_names.push_back(line);
            }
        }

        // Assign unique IDs to each board
        std::unordered_set<std::string>
            used_ids;                       // used to track assigned IDs and ensure uniqueness
                                            //
        std::vector<std::string> final_ids; // list of final assigned IDs for each board (in order)

        int name_idx = 0;         // pointer to track the current index in the loaded_names vector
        int numeric_fallback = 1; // if there aren't en

        for (int i = 0; i < num_boards; ++i)
        {
            std::string assigned_id = "";

            // Try to pull an unused name from the file.
            // We loop through the loaded names until we find one that hasn't been used yet.
            while (name_idx < loaded_names.size() && assigned_id.empty())
            {
                std::string candidate = loaded_names[name_idx];
                if (used_ids.find(candidate) == used_ids.end())
                {
                    assigned_id = candidate;
                }
                ++name_idx;
            }

            // Fallback: Generate an unused numeric string (1, 2, 3...)
            while (assigned_id.empty())
            {
                std::string candidate = std::to_string(numeric_fallback);
                if (used_ids.find(candidate) == used_ids.end())
                {
                    assigned_id = candidate;
                }
                ++numeric_fallback;
            }

            // store the assigned_id as used and in the final list
            used_ids.insert(assigned_id);
            final_ids.push_back(assigned_id);
        }

        // Instantiate the boards (Using unique_ptr because FakeFPG cannot be copied)
        std::vector<std::unique_ptr<FakeFPG>> fpga_cluster;

        for (const std::string &board_id : final_ids)
        {

            // Crate a new FakeFPG instance for each board and add it to the cluster
            fpga_cluster.push_back(std::make_unique<FakeFPG>(
                board_id, ip, port, proto, internet_type, 4.0, frequency_mhz, 1024LL));
        }

        // Wait for user trigger
        char cmd;
        std::cout << "\nSuccessfully initialized " << num_boards << " boards.\n";
        std::cout << "Ready to trigger concurrent spill? (y/n): ";
        std::cin >> cmd;

        if (cmd == 'y')
        {
            spdlog::info(">>> FIRING CONCURRENT SPILL FOR {} BOARDS <<<", num_boards);

            // Launch all boards on separate threads simultaneously
            std::vector<std::thread> threads;
            for (auto &board : fpga_cluster)
            {
                threads.emplace_back(&FakeFPG::run_spill, board.get());
            }

            // Wait for all threads to finish their spill
            for (auto &t : threads)
            {
                if (t.joinable())
                {
                    t.join();
                }
            }

            spdlog::info(">>> ALL BOARDS FINISHED <<<");
        }
    }
    catch (const std::exception &e)
    {
        spdlog::error("Fatal Error: {}", e.what());
        return 1;
    }

    return 0;
}

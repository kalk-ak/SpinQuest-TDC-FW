#include <iostream>
#include <vector>
#include <cstdint>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <csignal>
#include <random>

const uint64_t START_PREAMBLE = 0xAAAAAAAAAAAAAAAA;
const uint64_t END_PREAMBLE = 0xBBBBBBBBBBBBBBBB;
const int PORT = 8080;
const char* SERVER_IP = "127.0.0.1";
const int NUM_WORDS = 1000000;

volatile sig_atomic_t keep_running = 1;

void sigint_handler(int sig) {
    keep_running = 0;
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }

    signal(SIGINT, sigint_handler);

    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;

    while (keep_running) {
        send(sock, &START_PREAMBLE, sizeof(START_PREAMBLE), 0);
        for (int i = 0; i < NUM_WORDS; ++i) {
            uint64_t data = dis(gen);
            send(sock, &data, sizeof(data), 0);
        }
        send(sock, &END_PREAMBLE, sizeof(END_PREAMBLE), 0);
        std::cout << "Sent one iteration of data." << std::endl;
    }

    close(sock);
    std::cout << "Sender shutting down." << std::endl;
    return 0;
}
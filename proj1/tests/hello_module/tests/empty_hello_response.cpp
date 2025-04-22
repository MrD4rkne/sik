#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <chrono>

#include "../../common.h"

#ifdef LOG
constexpr bool DEBUG = true;
#else
constexpr bool DEBUG = false;
#endif

using namespace std;

void run(const args_t &args, int socket_fd, int expected) {
    const char *message = "\x1";
    auto start_time = std::chrono::high_resolution_clock::now();

    ssize_t sent_bytes = send_message(socket_fd, args.peer_address, args.peer_port, message, strlen(message));
    if (sent_bytes < 0) {
        throw std::runtime_error("Failed to send message");
    }

    // Expect empty response
    std::string received_message = wait_and_read_bytes(socket_fd);
    cout << "Received message size: " << received_message.size() << endl;
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    std::string hex_received_message = to_hex(received_message);
    
    if constexpr (DEBUG) {
        cerr << "Received message: " << hex_received_message << endl;
        cerr << "Response time: " << duration << " ms" << endl;
    }

    if(received_message.size() < 2) {
        throw std::runtime_error("Received too short message");
    }

    uint8_t type = received_message[0];
    if (type != 0x2) {
        throw std::runtime_error("Type is not 0x2");
    }

    uint16_t count = ntohs(*(uint16_t*)(received_message.data() + 1));
    if constexpr (DEBUG) {
        cerr << "Count: " << count << endl;
    }
    if (count != expected) {
        throw std::runtime_error("Count does not match expected value");
    }

    int peer_count = 0;

    for (size_t i = 3; i < received_message.size();) {
        ++peer_count;

        uint8_t length = received_message[i];
        ++i;

        if (length != 4) {
            throw std::runtime_error("Length is not four");
        }

        uint32_t adress_network_order = *(uint32_t *)(received_message.data() + i);
        struct in_addr ip_addr;
        ip_addr.s_addr = adress_network_order;
        i += length;

        if(i + 2 > received_message.size()) {
            throw std::runtime_error("Missing port");
        }

        uint16_t port = ntohs(*(uint16_t *)(received_message.data() + i));
        i += 2;

        if constexpr (DEBUG) {
            cout << inet_ntoa(ip_addr) << ":" << port << endl;
        }
    }

    if (peer_count != expected) {
        throw std::runtime_error("Parsed peers count does not match expected value");
    }
}

int main(int argc, char *argv[]) {
    argc-=1;
    argv+=1;

    args_t args;
    try {
        parse(argc, argv, args);
    } catch (const std::invalid_argument &e) {
        cerr << "Error when parsing args: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    argc-=4;
    argv+=4;


    if (argc == 0) {
        return EXIT_FAILURE;
    }

    argc--;
    int expected_count = stoi(argv[0]);

    cout << "Parsed arguments:" << endl;
    cout << args << endl;
    cout << "Expected count: " << expected_count << endl;

    // Create and bind a socket
    int sockfd = create_and_bind_socket(args.address, args.port);
    if (sockfd < 0) {
        cerr << "Error creating socket" << endl;
        return EXIT_FAILURE;
    }

    try{
        run(args, sockfd, expected_count);
    } catch (const std::exception &e) {
        cerr << "Run failure: " << e.what() << endl;
        close(sockfd);
        return EXIT_FAILURE;
    }

    close(sockfd);
}
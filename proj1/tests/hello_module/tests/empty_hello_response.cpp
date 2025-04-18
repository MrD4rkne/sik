#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../../common.h"

using namespace std;

void run(const args_t &args, int socket_fd, int expected) {
    const char *message = "\x1";

    ssize_t sent_bytes = send_message(socket_fd, args.peer_address, args.peer_port, message, strlen(message));
    if (sent_bytes < 0) {
        throw std::runtime_error("Failed to send message");
    }

    // Expect empty response
    std::string received_message = wait_and_read_bytes(socket_fd);
    std::string hex_received_message = to_hex(received_message);
    cerr << "Received message: " << hex_received_message << endl;
    cerr << "Received message size: " << received_message.size() << endl;

    if(received_message.size() < 2) {
        throw std::runtime_error("Received too short message");
    }

    uint8_t type = received_message[0];
    if (type != 0x2) {
        throw std::runtime_error("Type is not 0x2");
    }

    uint16_t count = ntohs(*(uint16_t*)(received_message.data() + 1));
    cerr << "Count: " << count << endl;
    if (count != expected) {
        throw std::runtime_error("Count does not match expected value");
    }

    int peer_count = 0;

    for (size_t i = 3; i < received_message.size();) {
        ++peer_count;

        uint8_t length = received_message[i];
        ++i;

        cerr << "Length: " << (int)length << endl;
        if (length != 4) {
            throw std::runtime_error("Length is not four");
        }

        uint32_t value = ntohl(*(uint32_t *)(received_message.data() + i));
        struct in_addr ip_addr;
        ip_addr.s_addr = value;
        i += length;

        if(i + 2 > received_message.size()) {
            throw std::runtime_error("Missing port");
        }

        uint16_t port = ntohs(*(uint16_t *)(received_message.data() + i));
        i += 2;

        cout << inet_ntoa(ip_addr) << ":" << port << endl;
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
        cerr << "Error: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    argc-=4;
    argv+=4;


    if (argc == 0) {
        return EXIT_FAILURE;
    }

    argc--;
    int expected_count = stoi(argv[0]);

    cerr << "Parsed arguments:" << endl;
    cerr << args << endl;
    cerr << "Expected count: " << expected_count << endl;

    // Create and bind a socket
    int sockfd = create_and_bind_socket(args.address, args.port);
    if (sockfd < 0) {
        cerr << "Error creating socket" << endl;
        return EXIT_FAILURE;
    }

    try{
        run(args, sockfd, expected_count);
    } catch (const std::exception &e) {
        cerr << "Error: " << e.what() << endl;
        close(sockfd);
        return EXIT_FAILURE;
    }

    close(sockfd);
}
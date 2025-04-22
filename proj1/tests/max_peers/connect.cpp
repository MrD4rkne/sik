#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <chrono>

#include "../common.h"

using namespace std;

void run(const args_t &args, int socket_fd) {
    const char *message = "\x3";

    cout << "Sending message: " << to_hex(message) << endl;
    ssize_t sent_bytes = send_message(socket_fd, args.peer_address, args.peer_port, message, strlen(message));
    if (sent_bytes < 0) {
        throw std::runtime_error("Failed to send message");
    }

    cout << "Sent " << sent_bytes << " bytes." << endl;

    // Expect connect_ack
    std::string received_message = wait_and_read_bytes(socket_fd);
    cout << "Received message size: " << received_message.size() << endl;
    
    std::string hex_received_message = to_hex(received_message);
    cout << "Received message: " << hex_received_message << endl;
    
    if(received_message.size() != 1 || received_message[0] != 0x4) {
        throw std::runtime_error("Received message is not connect_ack");
    }
    
    cout << "Successfully received connect_ack." << endl;
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

    cout << "Parsed arguments:" << endl;
    cout << args << endl;

    // Create and bind a socket
    int sockfd = create_and_bind_socket(args.address, args.port);
    if (sockfd < 0) {
        cerr << "Error creating socket" << endl;
        return EXIT_FAILURE;
    }

    cout << "Socket created and bound to " << args.address << ":" << args.port << endl;

    try{
        run(args, sockfd);
    } catch (const std::exception &e) {
        cerr << "Run failure: " << e.what() << endl;
        close(sockfd);
        return EXIT_FAILURE;
    }

    close(sockfd);
}
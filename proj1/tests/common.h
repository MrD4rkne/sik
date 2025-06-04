#ifndef COMMON_H
#define COMMON_H

#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdexcept>

struct args_t {
    std::string address;
    uint16_t port;
    std::string peer_address;
    uint16_t peer_port;
};

std::ostream& operator<<(std::ostream& os, const args_t& args) {
    os << "Address: " << args.address << ":" << args.port
       << ", Peer: " << args.peer_address << ":" << args.peer_port;
    return os;
}

// Parse IPv4 address and port from command line arguments
bool parse_address_port(int argc, char *argv[], int index, std::string &address, uint16_t &port) {
    if (index + 1 >= argc) {
        return false;
    }
    
    address = argv[index];
    
    try {
        port = static_cast<uint16_t>(std::stoi(argv[index + 1]));
    } catch (const std::exception &e) {
        return false;
    }
    
    return true;
}

bool is_valid_address(const std::string &address) {
    struct sockaddr_in sa;
    return inet_pton(AF_INET, address.c_str(), &(sa.sin_addr)) != 0;
}

void parse(int argC, char *argV[], args_t &out) {
    if (argC < 4) {
        throw std::invalid_argument("Too few arguments");
    }

    if(! parse_address_port(argC, argV, 0, out.address, out.port)) {
        throw std::invalid_argument("Invalid address or port");
    }

    if(! parse_address_port(argC, argV, 2, out.peer_address, out.peer_port)) {
        throw std::invalid_argument("Invalid peer address or port");
    }
}

// Create and bind a UDP socket
int create_and_bind_socket(const std::string &address, uint16_t port) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        throw std::runtime_error("Socket creation failed");
    }
    
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    
    if (inet_pton(AF_INET, address.c_str(), &(server_address.sin_addr)) <= 0) {
        close(sockfd);
        throw std::runtime_error("Invalid address");
    }
    
    if (bind(sockfd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        close(sockfd);
        throw std::runtime_error("Bind failed");
    }
    
    return sockfd;
}

// Send a message to a specific address and port
ssize_t send_message(int sockfd, const std::string &address, uint16_t port, const void *buffer, size_t length) {
    struct sockaddr_in dest_address;
    memset(&dest_address, 0, sizeof(dest_address));
    dest_address.sin_family = AF_INET;
    dest_address.sin_port = htons(port);
    
    if (inet_pton(AF_INET, address.c_str(), &(dest_address.sin_addr)) <= 0) {
        throw std::runtime_error("Invalid address");
    }
    
    size_t sent_bytes = 0;
    while (sent_bytes < length) {
        ssize_t bytes = sendto(sockfd, (const char *)buffer + sent_bytes, length - sent_bytes, 0, (struct sockaddr *)&dest_address, sizeof(dest_address));
        if (bytes < 0) {
            return -1;
        }

        if (bytes == 0) {
            break; // No more data to send
        }
        sent_bytes += bytes;
    }

    return sent_bytes;
}

std::string wait_and_read_bytes(int sockfd) {
    char buffer[70000];
    memset(buffer, 0, sizeof(buffer));

    ssize_t bytes_received = recv(sockfd, buffer, sizeof(buffer), 0);
    if (bytes_received < 0) {
        throw std::runtime_error("Receive failed");
    }
    
    return std::string(buffer, bytes_received);
}

// Helper function to get string representation of sender's address
std::string get_sender_address(struct sockaddr_in *src_addr) {
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(src_addr->sin_addr), ip_str, INET_ADDRSTRLEN);
    return std::string(ip_str) + ":" + std::to_string(ntohs(src_addr->sin_port));
}

std::string to_hex(const std::string &str) {
    std::string hex_str;
    char hex_buf[6];
    for (unsigned char c : str) {
        snprintf(hex_buf, sizeof(hex_buf), "0x%02x ", c);
        hex_str += hex_buf;
    }
    return hex_str;
}

#endif // COMMON_H
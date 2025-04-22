#include <cerrno>
#include <cinttypes>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>

#include "common.h"

uint16_t read_port(const std::string& string) {
    char* endptr;
    errno = 0;
    unsigned long port = std::strtoul(string.c_str(), &endptr, 10);
    if (errno != 0 || *endptr != '\0' || port > UINT16_MAX) {
        throw std::invalid_argument("Invalid port number: " + string);
    }
    return static_cast<uint16_t>(port);
}

size_t read_size(const std::string& string) {
    char* endptr;
    errno = 0;
    unsigned long long number = std::strtoull(string.c_str(), &endptr, 10);
    if (errno != 0 || *endptr != '\0' || number > SIZE_MAX) {
        throw std::invalid_argument("Invalid number: " + string);
    }
    return static_cast<size_t>(number);
}

sockaddr_in get_server_address(const std::string& host, uint16_t port) {
    addrinfo hints{};
    hints.ai_family = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_DGRAM; // UDP
    hints.ai_protocol = IPPROTO_UDP;

    addrinfo* address_result;
    int errcode = getaddrinfo(host.c_str(), nullptr, &hints, &address_result);
    if (errcode != 0) {
        throw std::runtime_error("getaddrinfo(): " + std::string(gai_strerror(errcode)));
    }

    sockaddr_in send_address{};
    send_address.sin_family = AF_INET; // IPv4
    send_address.sin_addr.s_addr =     // IP address
        reinterpret_cast<sockaddr_in*>(address_result->ai_addr)
            ->sin_addr.s_addr;
    send_address.sin_port = htons(port); // port from the command line

    freeaddrinfo(address_result);

    return send_address;
}

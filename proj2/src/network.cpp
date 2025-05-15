#include <algorithm>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string>

#include "network.h"

namespace network {

static inline const std::string END_OF_MESSAGE = "\r\n";

std::vector<std::string> MessageConcater::put_data(const std::string& data) {
    buffer += data;
    std::vector<std::string> messages;

    size_t pos = 0;
    while ((pos = buffer.find(network::END_OF_MESSAGE)) != std::string::npos) {
        messages.push_back(buffer.substr(0, pos));
        buffer.erase(0, pos + network::END_OF_MESSAGE.size());
    }

    return messages;
}

void MessageBuffer::add_message(
    const std::string& message,
    const std::function<void(const std::string&)>& callback) {
    Message msg;
    msg.data = message;
    msg.callback = callback;
    buffer.push(msg);
    std::cout << "Buffer size: " << buffer.size() << std::endl;
}

bool MessageBuffer::has_message() const {
    std::cout << "Buffer size: " << buffer.size() << std::endl;
    return !buffer.empty();
}

const std::string& MessageBuffer::get_buffer() const {
    if (!has_message()) {
        throw std::runtime_error("Buffer is empty");
    }

    auto& message = buffer.front();
    return message.data;
}

void MessageBuffer::mark_sent(size_t bytes) {
    if (!has_message()) {
        throw std::runtime_error("Buffer is empty");
    }

    auto& message = buffer.front();
    message.data.erase(0, bytes);
    if (message.data.empty()) {
        buffer.pop();
    }
}

static inline ip::IPAddress addr_to_ip(struct sockaddr* addr) {
    if (addr->sa_family == AF_INET) {
        auto* ipv4 = reinterpret_cast<sockaddr_in*>(addr);
        std::array<uint8_t, 4> bytes;
        uint32_t ip = ntohl(ipv4->sin_addr.s_addr);
        for (size_t i = 0; i < 4; ++i) {
            bytes[3 - i] = static_cast<uint8_t>(ip & 0xFF);
            ip >>= 8;
        }

        uint16_t port = ntohs(ipv4->sin_port);
        return ip::IPAddress(bytes, port);
    } else if (addr->sa_family == AF_INET6) {
        auto* ipv6 = reinterpret_cast<sockaddr_in6*>(addr);
        std::array<uint8_t, 16> bytes;
        std::memcpy(bytes.data(), &(ipv6->sin6_addr), 16);
        uint16_t port = ntohs(ipv6->sin6_port);
        return ip::IPAddress(bytes, port);
    }

    throw std::invalid_argument("Unsupported address family");
}

static inline ip::IPAddress addr_to_ip(const addrinfo* addr,
                                       const ip::port_t port) {
    if (addr->ai_family == AF_INET) {
        auto* ipv4 = reinterpret_cast<sockaddr_in*>(addr->ai_addr);
        std::array<uint8_t, 4> bytes;
        uint32_t ip = ntohl(ipv4->sin_addr.s_addr);
        for (size_t i = 0; i < 4; ++i) {
            bytes[3 - i] = static_cast<uint8_t>(ip & 0xFF);
            ip >>= 8;
        }
        return ip::IPAddress(bytes, port);
    } else if (addr->ai_family == AF_INET6) {
        auto* ipv6 = reinterpret_cast<sockaddr_in6*>(addr->ai_addr);
        std::array<uint8_t, 16> bytes;
        std::memcpy(bytes.data(), &(ipv6->sin6_addr), 16);
        return ip::IPAddress(bytes, port);
    }

    throw std::invalid_argument("Unsupported address family");
}

ip::IPAddress IpParser::parse(const std::string& ip_str, const ip::port_t port,
                              ip::IPAddress::Type type) {
    addrinfo hints{};
    switch (type) {
    case ip::IPAddress::Type::IPv4:
        hints.ai_family = AF_INET; // IPv4
        break;
    case ip::IPAddress::Type::IPv6:
        hints.ai_family = AF_INET6; // IPv6
        break;
    default:
        hints.ai_family = AF_UNSPEC; // Any address family
        break;
    }

    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_protocol = IPPROTO_TCP; // TCP

    addrinfo* address_result;
    int errcode = getaddrinfo(ip_str.c_str(), nullptr, &hints, &address_result);
    if (errcode != 0) {
        throw std::runtime_error("getaddrinfo(): " +
                                 std::string(gai_strerror(errcode)));
    }

    auto address = addr_to_ip(address_result, port);
    freeaddrinfo(address_result);
    return address;
}

std::pair<std::unique_ptr<sockaddr>, socklen_t>
IpParser::to_addr(const ip::IPAddress& ip_address) {
    if (ip_address.get_type() == ip::IPAddress::Type::IPv4) {
        auto ipv4 = std::make_unique<sockaddr_in>();
        ipv4->sin_family = AF_INET;
        ipv4->sin_port = htons(ip_address.get_port());

        auto addr_bytes = std::get<std::array<uint8_t, ip::IPV4_SIZE>>(
            ip_address.get_adress());

        std::array<uint8_t, 4> bytes;
        for (size_t i = 0; i < 4; ++i) {
            bytes[i] = addr_bytes[i];
        }
        std::memcpy(&(ipv4->sin_addr), bytes.data(), 4);

        return std::make_pair(std::unique_ptr<sockaddr>(
                                  reinterpret_cast<sockaddr*>(ipv4.release())),
                              sizeof(sockaddr_in));
    } else if (ip_address.get_type() == ip::IPAddress::Type::IPv6) {
        auto ipv6 = std::make_unique<sockaddr_in6>();
        ipv6->sin6_family = AF_INET6;
        ipv6->sin6_port = htons(ip_address.get_port());

        auto addr_bytes = std::get<std::array<uint8_t, ip::IPV6_SIZE>>(
            ip_address.get_adress());
        std::memcpy(&(ipv6->sin6_addr), addr_bytes.data(), 16);

        return std::make_pair(std::unique_ptr<sockaddr>(
                                  reinterpret_cast<sockaddr*>(ipv6.release())),
                              sizeof(sockaddr_in6));
    } else {
        throw std::invalid_argument("Unsupported address type");
    }
}

} // namespace network
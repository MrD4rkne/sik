#include <arpa/inet.h>
#include <array>
#include <cstring>
#include <iomanip>
#include <netdb.h>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <variant>

#include "ip.h"

namespace ip {

using port_t = uint16_t;
using Type = ip::IPAddress::Type;

IPAddress::IPAddress(const std::array<uint8_t, IPV4_SIZE>& ipv4_address,
                     port_t port)
    : port(port), type(Type::IPv4), address(ipv4_address) {
}

IPAddress::IPAddress(const std::array<uint8_t, IPV6_SIZE>& ipv6_address,
                     port_t port)
    : port(port), type(Type::IPv6), address(ipv6_address) {
}

Type IPAddress::get_type() const noexcept {
    return type;
}

bool IPAddress::operator==(const IPAddress& other) const {
    return port == other.port && type == other.type && address == other.address;
}

bool IPAddress::operator!=(const IPAddress& other) const {
    return !(*this == other);
}

bool IPAddress::operator<(const IPAddress& other) const {
    if (port != other.port) {
        return port < other.port;
    }
    if (type != other.type) {
        return type < other.type;
    }

    if (type == Type::IPv4) {
        return std::get<std::array<uint8_t, IPV4_SIZE>>(address) <
               std::get<std::array<uint8_t, IPV4_SIZE>>(other.address);
    } else {
        return std::get<std::array<uint8_t, IPV6_SIZE>>(address) <
               std::get<std::array<uint8_t, IPV6_SIZE>>(other.address);
    }
}

std::string IPAddress::to_string() const {
    std::ostringstream oss;
    oss << '[';
    if (type == Type::IPv4) {
        const auto& addr = std::get<std::array<uint8_t, IPV4_SIZE>>(address);
        for (size_t i = 0; i < IPV4_SIZE; ++i) {
            if (i > 0)
                oss << ".";
            oss << static_cast<int>(addr[i]);
        }
    } else {
        const auto& addr = std::get<std::array<uint8_t, IPV6_SIZE>>(address);
        for (size_t i = 0; i < IPV6_SIZE; i += 2) {
            if (i > 0)
                oss << ":";
            oss << std::hex << std::setw(2) << std::setfill('0')
                << (static_cast<int>(addr[i]) << 8 |
                    static_cast<int>(addr[i + 1]));
        }
    }

    oss << "]:" << port;
    return oss.str();
}

std::ostream& operator<<(std::ostream& os, const ip::IPAddress& ip) {
    return os << ip.to_string();
}

static inline IPAddress addr_to_ip(const addrinfo* addr, const port_t port) {
    if (addr->ai_family == AF_INET) {
        auto* ipv4 = reinterpret_cast<sockaddr_in*>(addr->ai_addr);
        std::array<uint8_t, 4> bytes;
        uint32_t ip = ntohl(ipv4->sin_addr.s_addr);
        for (size_t i = 0; i < 4; ++i) {
            bytes[3 - i] = static_cast<uint8_t>(ip & 0xFF);
            ip >>= 8;
        }
        return IPAddress(bytes, port);
    } else if (addr->ai_family == AF_INET6) {
        auto* ipv6 = reinterpret_cast<sockaddr_in6*>(addr->ai_addr);
        std::array<uint8_t, 16> bytes;
        std::memcpy(bytes.data(), &(ipv6->sin6_addr), 16);
        return IPAddress(bytes, port);
    }

    throw std::invalid_argument("Unsupported address family");
}

IPAddress IpParser::parse(const std::string& ip_str, const port_t port,
                          IPAddress::Type type) {
    addrinfo hints{};
    switch (type) {
    case IPAddress::Type::IPv4:
        hints.ai_family = AF_INET; // IPv4
        break;
    case IPAddress::Type::IPv6:
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

    return addr_to_ip(address_result, port);
}

} // namespace ip
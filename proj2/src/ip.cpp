#include <array>
#include <cstring>
#include <iomanip>
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

port_t IPAddress::get_port() const noexcept {
    return port;
}

std::variant<std::array<uint8_t, IPV4_SIZE>, std::array<uint8_t, IPV6_SIZE>>
IPAddress::get_adress() const noexcept {
    return address;
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

        oss << std::dec;
    }

    oss << "]:" << port;
    return oss.str();
}

std::ostream& operator<<(std::ostream& os, const ip::IPAddress& ip) {
    return os << ip.to_string();
}

} // namespace ip

namespace std {
size_t hash<ip::IPAddress>::operator()(const ip::IPAddress& k) const {
    size_t h = 0;
    if (k.get_type() == ip::IPAddress::Type::IPv4) {
        const auto addr = std::get<std::array<uint8_t, ip::IPV4_SIZE>>(k.get_adress());
        for (const auto& byte : addr) {
            h ^= std::hash<uint8_t>()(byte);
        }
    } else {
        const auto addr = std::get<std::array<uint8_t, ip::IPV6_SIZE>>(k.get_adress());
        for (const auto& byte : addr) {
            h ^= std::hash<uint8_t>()(byte);
        }
    }
    return h ^ std::hash<ip::port_t>()(k.get_port());
}

} // namespace std
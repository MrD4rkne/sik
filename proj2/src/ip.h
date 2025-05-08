#ifndef IP_H
#define IP_H

#include <array>
#include <cstring>
#include <sstream>
#include <string>
#include <variant>

namespace ip {

inline constexpr size_t IPV4_SIZE = 4;
inline constexpr size_t IPV6_SIZE = 16;

using port_t = uint16_t;

/// @brief A structure to represent an IP address (IPv4 or IPv6).
class IPAddress {
  public:
    enum class Type { IPv4, IPv6, None };

    IPAddress(const std::array<uint8_t, IPV4_SIZE>& ipv4_address, port_t port);

    IPAddress(const std::array<uint8_t, IPV6_SIZE>& ipv6_address, port_t port);

    bool operator==(const IPAddress& other) const;

    bool operator!=(const IPAddress& other) const;

    bool operator<(const IPAddress& other) const;

    Type get_type() const noexcept;

    port_t get_port() const noexcept;

    std::variant<std::array<uint8_t, IPV4_SIZE>, std::array<uint8_t, IPV6_SIZE>> get_adress() const noexcept;

    std::string to_string() const;

  private:
    port_t port;
    Type type;
    std::variant<std::array<uint8_t, IPV4_SIZE>, std::array<uint8_t, IPV6_SIZE>>
        address;
};

std::ostream& operator<<(std::ostream& os, const ip::IPAddress& ip);
} // namespace ip

#endif // IP_H
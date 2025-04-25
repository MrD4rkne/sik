#include "peers.h"

#include <arpa/inet.h>
#include <array>
#include <cstdint>
#include <ifaddrs.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace peers {

bool peer::operator<(const peer& other) const noexcept {
    if (this->get_port() != other.get_port()) {
        return this->get_port() < other.get_port();
    }

    std::array<uint8_t, 4> this_address = this->get_address();
    std::array<uint8_t, 4> other_address = other.get_address();
    return std::lexicographical_compare(
        this_address.begin(), this_address.end(), other_address.begin(),
        other_address.end());
}

bool peer::operator==(const peer& other) const noexcept {
    return this->get_port() == other.get_port() &&
           this->get_address() == other.get_address();
}

bool peer::operator!=(const peer& other) const noexcept {
    return !(*this == other);
}

std::string peer::to_string() const {
    std::ostringstream oss;
    for (size_t i = 0; i < this->get_address().size(); ++i) {
        oss << static_cast<int>(this->get_address()[i]);
        if (i < this->get_address().size() - 1) {
            oss << ".";
        }
    }
    oss << ":" << this->get_port();
    return oss.str();
}

std::ostream& operator<<(std::ostream& os, const peers::peer& p) {
    return os << p.to_string();
}

const std::set<peer>
all_ipv4_interfaces_host_peer_provider::get_my_addresses() const {
    std::set<peer> my_addresses;
    struct ifaddrs* ptr_ifaddrs = nullptr;

    auto result = getifaddrs(&ptr_ifaddrs);
    if (result != 0) {
        throw std::runtime_error("getifaddrs failed");
    }

    uint16_t host_port = ntohs(this->port);

    for (struct ifaddrs* ifa = ptr_ifaddrs; ifa != nullptr;
         ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        struct sockaddr_in* addr =
            reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);

        // Extract IP address bytes
        in_addr_t ip_addr = addr->sin_addr.s_addr;
        std::array<uint8_t, 4> address = {
            static_cast<uint8_t>(ip_addr & 0xFF),
            static_cast<uint8_t>((ip_addr >> 8) & 0xFF),
            static_cast<uint8_t>((ip_addr >> 16) & 0xFF),
            static_cast<uint8_t>((ip_addr >> 24) & 0xFF)};

        my_addresses.insert(peer(host_port, address));
    }

    freeifaddrs(ptr_ifaddrs);

    for (const auto& addr : my_addresses) {
        std::cout << "Found address: " << addr.to_string() << std::endl;
    }

    return my_addresses;
}

const std::set<peer> ipv4_host_peer_provider::get_my_addresses() const {
    return std::set<peers::peer>{this->peer};
}

} // namespace peers
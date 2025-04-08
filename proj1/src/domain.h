#ifndef DOMAIN_H
#define DOMAIN_H

#include <cstdint>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace domain {

using message_type_t = uint8_t;
using count_t = uint16_t;
using peer_address_length_t = uint8_t;
using port_t = uint16_t;
using timestamp_t = uint64_t;
using synchronized_t = uint8_t;

class peer {
  public:
    peer(uint16_t port, const std::vector<uint8_t>& address)
        : peer_port(port), peer_address(address) {
    }

    port_t get_port() const noexcept {
        return peer_port;
    }

    const std::vector<uint8_t> get_address() const noexcept {
        return peer_address;
    }

    peer_address_length_t get_address_length() const noexcept {
        return peer_address.size();
    }

    bool operator<(const peer& other) const noexcept {
        if (peer_address != other.peer_address) {
            return peer_address < other.peer_address;
        }
        return peer_port < other.peer_port;
    }

    bool operator==(const peer& other) {
        return peer_address == other.peer_address &&
               peer_port == other.peer_port;
    }

    ~peer() = default;

    std::string to_string() const {
        std::stringstream ss;
        for (size_t i = 0; i < peer_address.size(); ++i) {
            ss << static_cast<int>(peer_address[i]);
            if (i < peer_address.size() - 1) {
                ss << ".";
            }
        }

        ss << ":" << peer_port;
        return ss.str();
    }

  private:
    port_t peer_port;
    std::vector<uint8_t> peer_address;
};

inline std::ostream& operator<<(std::ostream& os, const domain::peer& p) {
    return os << p.to_string();
}

class Node {
  public:
    Node() : peers{} {
    }

    void add_peer(domain::peer peer) {
        peers.insert(peer);
    }

    const std::set<domain::peer>& get_peers() const {
        return peers;
    }

  private:
    std::set<domain::peer> peers;
};

} // namespace domain

#endif
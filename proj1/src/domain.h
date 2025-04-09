#ifndef DOMAIN_H
#define DOMAIN_H

#include <cstdint>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <time.h>

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

class Clock{
    public:
        Clock() : timestamp(static_cast<timestamp_t>(clock())) {
        }

        timestamp_t get_timestamp() const {
            timestamp_t current_time = static_cast<timestamp_t>(clock());
            return current_time - timestamp;
        }
    
    private:
        timestamp_t timestamp;
    };

struct peer_status_t {};

class Node {
  public:
    Node() : peers{}, waiting_for_connect_ack{}, waiting_for_hello_rsp{}, clock{} {
    }

    void add_peer(const domain::peer& peer) {
        peers.insert({peer, peer_status_t{}});
    }

    void add_range(const std::vector<domain::peer>& new_peers) {
        for (const auto& peer : new_peers) {
            peers.insert({peer, peer_status_t{}});
        }
    }

    bool has_peer(const domain::peer& peer) const {
        return peers.find(peer) != peers.end();
    }

    void add_waiting_for_connect_ack(const domain::peer& peer) {
        waiting_for_connect_ack.insert(peer);
    }

    void add_waiting_for_hello_rsp(const domain::peer& peer) {
        waiting_for_hello_rsp.insert(peer);
    }

    void acknowledge_connect(const domain::peer& peer) {
        auto it = waiting_for_connect_ack.find(peer);
        if (it == waiting_for_connect_ack.end()) {
            throw std::runtime_error(
                "Peer not found in waiting_for_connect_ack");
        }
        waiting_for_connect_ack.erase(it);
        add_peer(peer);
    }

    void acknowledge_hello_rsp(const domain::peer& peer) {
        auto it = waiting_for_hello_rsp.find(peer);
        if (it == waiting_for_hello_rsp.end()) {
            throw std::runtime_error("Peer not found in waiting_for_hello_rsp");
        }

        waiting_for_hello_rsp.erase(it);
        add_peer(peer);
    }

    peer_status_t& get_peer_status(const domain::peer& peer) {
        auto it = peers.find(peer);
        if (it != peers.end()) {
            return it->second;
        }

        throw std::runtime_error("Peer not found");
    }

    std::vector<peer> get_peers() const {
        std::vector<peer> peer_vector;
        peer_vector.reserve(peers.size());
        for (const auto& pair : peers) {
            peer_vector.push_back(pair.first);
        }
        return peer_vector;
    }

    timestamp_t get_time() const {
        return clock.get_timestamp();
    }

  private:
    std::map<domain::peer, peer_status_t> peers;
    std::set<domain::peer> waiting_for_connect_ack;
    std::set<domain::peer> waiting_for_hello_rsp;
    Clock clock;
};

} // namespace domain

#endif
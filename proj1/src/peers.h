#ifndef PEERS_H
#define PEERS_H

#include <algorithm>
#include <array>
#include <iterator>
#include <set>
#include <string>

namespace peers {

/// @brief Class representing a peer in the network.
class peer {
  public:
    peer(uint16_t port, const std::array<uint8_t, 4>& address) noexcept
        : address(address), port(port) {
    }

    bool operator<(const peer& other) const noexcept;

    bool operator==(const peer& other) const noexcept;

    bool operator!=(const peer& other) const noexcept;

    uint16_t get_port() const noexcept {
        return port;
    }

    const std::array<uint8_t, 4>& get_address() const noexcept {
        return address;
    }

    std::string to_string() const;

  private:
    std::array<uint8_t, 4> address;
    uint16_t port;
};

std::ostream& operator<<(std::ostream& os, const peers::peer& p);

/// Function template that operates on a range of peers
template<typename Iterator>
void process_peer_range(Iterator begin, Iterator end) {
    for (auto it = begin; it != end; ++it) {
        // Process each peer in the range
        // Example implementation would use *it to access the peer
    }
}

class host_peer_provider {
  public:
    host_peer_provider() = default;

    template<typename Iterator>
    bool check_if_on_list(Iterator begin, Iterator end) const {
        auto get_my_addresses = this->get_my_addresses();
        return std::any_of(begin, end, [&get_my_addresses](const peer& p) {
            return std::find(get_my_addresses.begin(), get_my_addresses.end(),
                             p) != get_my_addresses.end();
        });
    }

    virtual ~host_peer_provider() = default;

  protected:
    virtual const std::set<peer> get_my_addresses() const = 0;
};

class all_ipv4_interfaces_host_peer_provider : public host_peer_provider {
  public:
    all_ipv4_interfaces_host_peer_provider(uint16_t port) noexcept
        : port(port) {
    }

    ~all_ipv4_interfaces_host_peer_provider() = default;

  protected:
    const std::set<peer> get_my_addresses() const override;

  private:
    uint16_t port;
};

class ipv4_host_peer_provider : public host_peer_provider {
  public:
    ipv4_host_peer_provider(const peer& peer) noexcept : peer(peer) {
    }

    ~ipv4_host_peer_provider() = default;

  protected:
    const std::set<peer> get_my_addresses() const override;

  private:
    peer peer;
};

} // namespace peers

#endif // PEERS_H
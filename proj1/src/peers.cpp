#include "peer.h"

#include <string>
#include <array>
#include <stdexcept>
#include <cstdint>

/// @brief Class representing a peer in the network.
class ip_peer : public peers::peer {
    public:
    ip_peer(uint16_t port, const std::array<uint8_t, 4>& address)
          : peer_port(port), peer_address(address) {
          if (address.size() != 4) {
              throw std::invalid_argument("Invalid address size.");
          }
      }
  
      uint16_t get_port() const noexcept;
  
      const std::array<uint8_t, 4> get_address() const noexcept;
  
      size get_address_length() const noexcept;
  
      bool operator<(const peer& other) const noexcept;
  
      bool operator==(const peer& other) const;
  
      bool operator!=(const peer& other) const;
  
      std::string to_string() const;
  
    private:
      port_t peer_port;
      std::array<uint8_t, 4> peer_address;
};
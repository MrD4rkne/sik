#ifndef PEER_H
#define PEER_H

#include <string>

namespace peers{

/// @brief Class representing a peer.
class peer {
  
      virtual bool operator<(const peer& other) const = 0;
  
      virtual bool operator==(const peer& other) const noexcept = 0;
  
      virtual bool operator!=(const peer& other) const = 0;
  
      virtual std::string to_string() const = 0;
};

std::ostream& operator<<(std::ostream& os, const peers::peer& p);

} // namespace peers

#endif // PEER_H
#ifndef NETWORK_H
#define NETWORK_H

#include <array>
#include <iostream>
#include <string>
#include <variant>

namespace network {

class MessageSender {
  public:
    MessageSender(int socket_fd) : socket_fd(socket_fd) {
    }

    /// @brief Send a message to a target peer.
    /// @param message The message to send.
    /// @throws std::runtime_error if the address preparation fails.
    /// @throws std::runtime_error if the sendto operation fails.
    void send_message(const std::string& message);

  private:
    int socket_fd;
};

} // namespace network

#endif // NETWORK_H
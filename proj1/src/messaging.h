#ifndef MESSAGES_H
#define MESSAGES_H

#include <cstdint>
#include <map>
#include <memory>
#include <netinet/in.h>
#include <string>

#include "domain.h"
#include "logging.h"
#include "packets.h"

namespace messaging {

class MessageSender {
  public:
    MessageSender(int socket_fd, logging::Logger& logger)
        : socket_fd(socket_fd), logger(logger) {
    }

    /// @brief Send a message to a target peer.
    /// @param target The target peer to send the message to.
    /// @param message The message to send.
    /// @throws std::invalid_argument if the message size exceeds the maximum size.
    /// @throws std::runtime_error if the address preparation fails.
    /// @throws std::runtime_error if the sendto operation fails.
    template<typename T>
    void send_message(domain::peer target, T& message) {
        logger.logDebug("Sending message: ", message, " to target: ", target);
        auto serialized = packets::mappers<T>::serialize_packet(message);
        send_message(target, serialized.c_str(), serialized.size());
    }

    /// @brief Send a message to a target peer.
    /// @param target The target peer to send the message to.
    /// @param buffer The buffer containing the message to send.
    /// @param bytes_to_send The number of bytes to send from the buffer.
    /// @throws std::invalid_argument if the message size exceeds the maximum size.
    /// @throws std::runtime_error if the address preparation fails.
    /// @throws std::runtime_error if the sendto operation fails.
    void send_message(domain::peer target, const char* buffer,
                      size_t bytes_to_send);

  private:
    int socket_fd;
    logging::Logger& logger;
};

} // namespace messaging
#endif
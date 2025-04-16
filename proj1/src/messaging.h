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

    template<typename T>
    bool send_message(domain::peer target, T& message) {
        auto serialized = packets::mappers<T>::serialize_packet(message);
        return send_message(target, serialized.c_str(), serialized.size());
    }

    bool send_message(domain::peer target, const char* buffer,
                      size_t bytes_to_send);

  private:
    int socket_fd;
    logging::Logger& logger;
};

} // namespace messaging
#endif
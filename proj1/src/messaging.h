#ifndef MESSAGES_H
#define MESSAGES_H

#include <cstdint>
#include <map>
#include <memory>
#include <netinet/in.h>
#include <string>

#include "domain.h"
#include "packets.h"

namespace messaging {

using namespace domain;

class MessageSender {
  public:
    MessageSender(int socket_fd, logging::Logger& logger)
        : socket_fd(socket_fd), logger(logger) {
    }

    bool send_message(domain::peer target, const char* buffer,
                      size_t bytes_to_send);

  private:
    int socket_fd;
    logging::Logger& logger;
};

class MessageHandler {
  public:
    virtual Result handle(Node& node, logging::Logger& logger,
                        const domain::peer& peer, size_t read_bytes,
                        char* buffer, MessageSender& message_sender) = 0;

    virtual ~MessageHandler() = default;
};

template<typename T>
class MessageMediator {
  public:
    MessageMediator() = default;
    ~MessageMediator() = default;

    void register_handler(T message_type,
                          std::shared_ptr<MessageHandler> handler) {
        handlers[message_type] = std::move(handler);
    }

    Result handle_message(domain::Node& node, logging::Logger& logger,
                        const domain::peer& peer, T message_type,
                        size_t read_bytes, char* buffer,
                        MessageSender& message_sender) const {
        auto it = handlers.find(message_type);
        if (it == handlers.end()) {
            return Result::Failure("Handler not found for message type.");
        }

        return it->second->handle(node, logger, peer, read_bytes,
          buffer, message_sender);
    }

  private:
    std::map<T, std::shared_ptr<MessageHandler>> handlers;
};

} // namespace messaging
#endif
#ifndef HANDLERS_H
#define HANDLERS_H

#include "domain.h"
#include "logging.h"
#include "messaging.h"
#include "packets.h"

namespace handlers {

class MessageHandler {
  public:
    virtual results::Result
    handle(domain::Node& node, logging::Logger& logger,
           const domain::peer& peer, size_t read_bytes, char* buffer,
           messaging::MessageSender& message_sender) = 0;

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

    results::Result
    handle_message(domain::Node& node, logging::Logger& logger,
                   const domain::peer& peer, T message_type, size_t read_bytes,
                   char* buffer,
                   messaging::MessageSender& message_sender) const {
        auto it = handlers.find(message_type);
        if (it == handlers.end()) {
            return results::Result::Failure(
                "Handler not found for message type.");
        }

        return it->second->handle(node, logger, peer, read_bytes, buffer,
                                  message_sender);
    }

  private:
    std::map<T, std::shared_ptr<MessageHandler>> handlers;
};

class hello_message_handler : public MessageHandler {
  public:
    results::Result handle(domain::Node& node, logging::Logger& logger,
                           const domain::peer& peer, size_t read_bytes,
                           char* buffer,
                           messaging::MessageSender& message_sender) override;
};

class hello_response_handler : public MessageHandler {
  public:
    results::Result handle(domain::Node& node, logging::Logger& logger,
                           const domain::peer& peer, size_t read_bytes,
                           char* buffer,
                           messaging::MessageSender& message_sender) override;
};

class connect_handler : public MessageHandler {
  public:
    results::Result handle(domain::Node& node, logging::Logger& logger,
                           const domain::peer& peer, size_t read_bytes,
                           char* buffer,
                           messaging::MessageSender& message_sender) override;
};

class ack_connect_handler : public MessageHandler {
  public:
    results::Result handle(domain::Node& node, logging::Logger& logger,
                           const domain::peer& peer, size_t read_bytes,
                           char* buffer,
                           messaging::MessageSender& message_sender) override;
};

class leader_handler : public MessageHandler {
  public:
    results::Result handle(domain::Node& node, logging::Logger& logger,
                           const domain::peer& peer, size_t read_bytes,
                           char* buffer,
                           messaging::MessageSender& message_sender) override;
};

class sync_start_handler : public MessageHandler {
  public:
    results::Result handle(domain::Node& node, logging::Logger& logger,
                           const domain::peer& peer, size_t read_bytes,
                           char* buffer,
                           messaging::MessageSender& message_sender) override;
};

class delay_request_handler : public MessageHandler {
  public:
    results::Result handle(domain::Node& node, logging::Logger& logger,
                           const domain::peer& peer, size_t read_bytes,
                           char* buffer,
                           messaging::MessageSender& message_sender) override;
};

class delay_response_handler : public MessageHandler {
  public:
    results::Result handle(domain::Node& node, logging::Logger& logger,
                           const domain::peer& peer, size_t read_bytes,
                           char* buffer,
                           messaging::MessageSender& message_sender) override;
};

void start_synchronization(domain::Node& node, logging::Logger& logger,
                           messaging::MessageSender& message_sender);

} // namespace handlers

#endif
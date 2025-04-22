#ifndef HANDLERS_H
#define HANDLERS_H

#include "domain.h"
#include "logging.h"
#include "messaging.h"
#include "packets.h"

namespace handlers {

/// @brief Abstract base class for message handlers.
class MessageHandler {
  public:
    /// @brief Handle a message.
    /// @param node The node that received the message.
    /// @param logger The logger to use for logging.
    /// @param peer The peer that sent the message.
    /// @param read_bytes The number of bytes read from the socket.
    /// @param buffer The buffer containing the message.
    /// @param message_sender The message sender to use for sending messages.
    /// @return A Result indicating success or failure.
    /// @throws std::invalid_argument if the message was of invalid structure.
    virtual results::Result
    handle(domain::Node& node, logging::Logger& logger,
           const domain::peer& peer, size_t read_bytes, char* buffer,
           messaging::MessageSender& message_sender) = 0;

    virtual ~MessageHandler() = default;
};

/// @brief Message mediator class that handles message dispatching.
template<typename T>
class MessageMediator {
  public:
    MessageMediator() = default;
    ~MessageMediator() = default;

    /// @brief Register a message handler for a specific message type.
    /// @param message_type The message type to register the handler for.
    /// @param handler The message handler to register.
    /// @throws std::invalid_argument if the message type is already registered.
    void register_handler(T message_type,
                          std::shared_ptr<MessageHandler> handler) {
                            if (handlers.find(message_type) != handlers.end()) {
                                throw std::invalid_argument(
                                    "Handler already registered for this message type.");
                            }
        handlers[message_type] = std::move(handler);
    }

    /// @brief Handle a message.
    /// @param node The node that received the message.
    /// @param logger The logger to use for logging.
    /// @param peer The peer that sent the message.
    /// @param message_type The message type.
    /// @param read_bytes The number of bytes read from the socket.
    /// @param buffer The buffer containing the message.
    /// @param message_sender The message sender to use for sending messages.
    /// @return A Result indicating success or failure. If the handler is not
    /// registered, it returns a failure result also.
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

class get_time_handler : public MessageHandler {
  public:
    results::Result handle(domain::Node& node, logging::Logger& logger,
                           const domain::peer& peer, size_t read_bytes,
                           char* buffer,
                           messaging::MessageSender& message_sender) override;
};

/// @brief Send a hello message to a peer.
/// @param node current node.
/// @param logger logger to use.
/// @param peer peer to send the message to.
/// @param message_sender message sender to use.
void send_hello(domain::Node& node, logging::Logger& logger,
                const domain::peer& peer,
                messaging::MessageSender& message_sender);

/// @brief Try to send sync_start message to all peers.
/// @param node current node.
/// @param logger logger to use.
/// @param message_sender message sender to use.
void start_synchronization(domain::Node& node, logging::Logger& logger,
                           messaging::MessageSender& message_sender);

} // namespace handlers

#endif
#ifndef HANDLERS_H
#define HANDLERS_H

#include <functional>
#include <stdexcept>
#include <unordered_map>

#include "ip.h"
#include "logging.h"
#include "messages.h"
#include "results.h"

namespace handlers {

/// @brief A message handler that processes messages of type T.
/// It allows registering handlers for specific message types and handles
/// messages by invoking the appropriate handler based on the message type.
/// @tparam T The type of the state that the handler operates on.
/// It can be any type that contains the necessary state for processing
/// messages.
template<typename T>
class message_handler {
  public:
    using delegate_t = std::function<results::Result(
        const ip::IPAddress, messages::MessageSender&, T&, logging::Logger&,
        const std::string& message)>;

    /// @brief Register a handler for a specific message type.
    void register_handler(const std::string& message_type,
                          delegate_t delegate) {
        handlers[message_type] = std::move(delegate);
    }

    /// @brief Dispatch a message to the appropriate handler based on its type.
    /// @param message_type The type of the message to handle.
    /// @param ip_addr The IP address of the sender.
    /// @param sender The message sender to use for sending responses.
    /// @param state The state of type T that the handler operates on.
    /// @param logger The logger to use for logging messages.
    /// @param message The message content to handle.
    /// @return A Result indicating the success or failure of the handling.
    /// @throws std::invalid_argument if no handler is registered for the
    /// message type.
    /// @note It does not catch exceptions thrown by the handler.
    results::Result handle(const std::string& message_type,
                           const ip::IPAddress ip_addr,
                           messages::MessageSender& sender, T& state,
                           logging::Logger& logger,
                           const std::string& message) const {
        auto it = handlers.find(message_type);
        if (it != handlers.end()) {
            return it->second(ip_addr, sender, state, logger, message);
        } else {
            throw std::invalid_argument("Handler not found for message type: " +
                                        message_type);
        }
    }

  private:
    std::unordered_map<std::string, delegate_t> handlers;
};

} // namespace handlers

#endif
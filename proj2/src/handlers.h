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

template<typename T>
class message_handler {
  public:
    using delegate_t = std::function<results::Result(
        const ip::IPAddress, messages::MessageSender&, T&, logging::Logger&,
        const std::string& message)>;

    void register_handler(const std::string& message_type,
                          delegate_t delegate) {
        handlers[message_type] = std::move(delegate);
    }

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
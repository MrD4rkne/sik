#ifndef NETWORK_H
#define NETWORK_H

#include <arpa/inet.h>
#include <array>
#include <iostream>
#include <memory>
#include <poll.h>
#include <string>
#include <unordered_map>
#include <variant>

#include "ip.h"
#include "logging.h"
#include "messages.h"
#include <functional>
#include <netinet/in.h>
#include <queue>
#include "fd.h"

namespace network {

class MessageConcater {
  public:
    std::vector<std::string> put_data(const std::string& data);

  private:
    std::string buffer;
};

class MessageBuffer {

  public:
    void add_message(const std::string& message,
                     const std::function<void(const std::string&)>& callback);

    bool has_message() const;

    const std::string& get_buffer() const;

    void mark_sent(size_t bytes);

  private:
    struct Message {
        std::string data;
        std::function<void(const std::string&)> callback;
    };

    std::queue<Message> buffer;
};

class MessageSender {
  public:
    virtual void
    send_message(const ip::IPAddress& ip_address, const std::string& message,
                 const std::function<void(const std::string&)>& callback) = 0;

    virtual void send_message(const ip::IPAddress& ip_address,
                              const std::string& message){
                                static const auto EMPTY = [](const std::string&) {};
                                send_message(ip_address, message, EMPTY);
                              }

    template<typename T>
    void send_message_serialized(const ip::IPAddress& ip_address, const T& message,
                      const std::function<void(const std::string&)>& callback) {
        send_message(ip_address, messages::serialize_message(message),
                     callback);
    }

    virtual ~MessageSender() = default;
};

class IpParser {
  public:
    /// @brief Parse an IP address string and port number.
    /// @param ip_str The IP address string to parse.
    /// @param port The port number.
    /// @param type The type of IP address (IPv4 or IPv6).
    /// @return An IPAddress object representing the parsed IP address.
    static ip::IPAddress parse(const std::string& ip_str, const ip::port_t port,
                               ip::IPAddress::Type type);

    static std::pair<std::unique_ptr<sockaddr>, socklen_t>
    to_addr(const ip::IPAddress& ip_address);
};

} // namespace network

#endif // NETWORK_H
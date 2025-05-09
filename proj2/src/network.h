#ifndef NETWORK_H
#define NETWORK_H

#include <array>
#include <iostream>
#include <string>
#include <variant>
#include <memory>
#include <arpa/inet.h>

#include "logging.h"
#include "ip.h"
#include "messages.h"

namespace network {

class MessageSender {
  public:
    MessageSender(int socket_fd) : socket_fd(socket_fd), logger() {
    }

    MessageSender(int socket_fd, const logging::Logger& logger)
        : socket_fd(socket_fd), logger(logger) {
    }

    template<typename T>
    void send_message(const T& message){
        std::string msg = messages::serialize_message(message);
        send_message(msg);
    }

    /// @brief Send a message to a target peer.
    /// @param message The message to send.
    /// @throws std::runtime_error if the address preparation fails.
    /// @throws std::runtime_error if the sendto operation fails.
    template<>
    void send_message(const std::string& message);

  private:
    int socket_fd;
    logging::Logger logger;
};

class MessageReceiver {
  public:
    MessageReceiver(int socket_fd) : socket_fd(socket_fd), logger() {
    }

    MessageReceiver(int socket_fd, const logging::Logger& logger)
        : socket_fd(socket_fd), logger(logger) {
    }

    std::string receive_message();

  private:
    int socket_fd;
    logging::Logger logger;
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

    static std::pair<std::unique_ptr<sockaddr>, socklen_t> to_addr(const ip::IPAddress& ip_address);
};

} // namespace network

#endif // NETWORK_H
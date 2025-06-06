#ifndef NETWORK_H
#define NETWORK_H

#include <arpa/inet.h>
#include <array>
#include <iostream>
#include <memory>
#include <netdb.h>
#include <poll.h>
#include <string>
#include <unordered_map>
#include <variant>

#include "concaters.h"
#include "fd.h"
#include "ip.h"
#include "logging.h"
#include "messages.h"
#include <functional>
#include <netinet/in.h>
#include <queue>

/// @brief Stuff related to network operations, including IP parsing and socket
/// handling. This header file defines classes for parsing IP addresses,
/// handling single and multiple sockets, and sending messages over the network.
/// It also includes utility functions for converting IP addresses to and from
/// sockaddr structures, and for managing socket events using file descriptors.
namespace network {

/// @brief A utility class for parsing IP addresses and ports.
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

    static ip::IPAddress addr_to_ip(struct sockaddr* addr);

    static ip::IPAddress addr_to_ip(const addrinfo* addr,
                                    const ip::port_t port);
};

/// @brief A handler for a single socket connection that can send and receive
/// messages.
class SingleSocketHandler : public fd::FDHandler,
                            public messages::MessageSender {
  public:
    SingleSocketHandler(
        logging::Logger logger, const ip::IPAddress& ip,
        const std::function<void(const ip::IPAddress ip,
                                 const std::string& msg)>& on_message_received,
        const std::function<void(const ip::IPAddress)>& on_client_disconnect)
        : logger(logger), socket_fd(DEFAULT_SOCKET_FD), ip_address(ip),
          message_buffer(), message_concater(),
          on_message_received(on_message_received),
          on_client_disconnect(on_client_disconnect) {
    }

    SingleSocketHandler(
        int socket, logging::Logger logger, const ip::IPAddress& ip,
        const std::function<void(const ip::IPAddress ip,
                                 const std::string& msg)>& on_message_received,
        const std::function<void(const ip::IPAddress)>& on_client_disconnect)
        : logger(logger), socket_fd(socket), ip_address(ip), message_buffer(),
          message_concater(), on_message_received(on_message_received),
          on_client_disconnect(on_client_disconnect) {
    }

    /// @brief Connect to a given IP address.
    /// @param ip_address The IP address to connect to.
    /// @return The real IP address of the connected peer.
    ip::IPAddress connect_to(ip::IPAddress ip_address);

    /// @brief Force flush the message buffer, sending any pending messages
    /// immediately. Then disconnect.
    void force_flush();

    /// @brief Disconnect the socket and clean up resources.
    void disconnect();

    /// @brief Handle events on the descriptor.
    /// @param fd The file descriptor of the socket.
    /// @param events The events that occurred on the socket (e.g., POLLIN,
    /// POLLOUT).
    /// @throws std::invalid_argument if the file descriptor is invalid.
    void handle(int fd, short events) override;

    /// @brief Get the timeout for polling the socket.
    int get_event_change_time(int fd) const override;

    /// @brief Get the events to listen for on the socket.
    short get_events(int fd) const override;

    /// @brief Send a message to the socket.
    /// @param ip The IP address to send the message to.
    /// @param message The message to send.
    /// @param callback A callback function to be called when the message is
    /// sent.
    /// @param delay The delay in milliseconds before sending the message.
    void send_message(const ip::IPAddress, const std::string& message,
                      const std::function<void(const std::string&)>& callback,
                      uint64_t delay) override;

    /// @brief Send a message to the socket.
    /// @param ip The IP address to send the message to.
    /// @param message The message to send.
    /// @param delay The delay in milliseconds before sending the message.
    int get_socket_fd() const;

  private:
    constexpr static int DEFAULT_SOCKET_FD = -1;

    logging::Logger logger;
    int socket_fd;
    const ip::IPAddress ip_address;
    concaters::MessageBuffer message_buffer;
    concaters::MessageConcater message_concater;
    const std::function<void(const ip::IPAddress ip, const std::string& msg)>
        on_message_received;
    const std::function<void(const ip::IPAddress)> on_client_disconnect;
};

/// @brief A handler for multiple socket connections that listens for incoming
/// connections and manages client connections. It can accept new clients, send
/// messages to clients, and handle disconnections.
class SocketHandler : public fd::FDHandler, public messages::MessageSender {
  public:
    SocketHandler(
        int socket, std::shared_ptr<fd::FDPoller> fdPoller,
        const std::function<void(const ip::IPAddress ip,
                                 const std::string& msg)>& on_message_received,
        const std::function<void(const ip::IPAddress)>& on_client_connect,
        const std::function<void(const ip::IPAddress)>& on_client_disconnect)
        : listen_fd(socket), fdPoller(fdPoller), logger(),
          on_message_received(on_message_received),
          on_client_connect(on_client_connect),
          on_client_disconnect(on_client_disconnect) {
    }

    /// @brief Handle events on the socket.
    /// @param socket_fd The file descriptor of the socket to handle.
    /// @param events The events that occurred on the socket (e.g., POLLIN,
    /// POLLOUT).
    /// @throws std::invalid_argument if the file descriptor is invalid.
    /// @note This method will accept new clients if the socket is listening.
    void handle(int socket_fd, short events) override;

    /// @brief Disconnect a client.
    /// @param ip The IP address of the client to disconnect.
    void disconnect(const ip::IPAddress ip);

    /// @brief Force flush the message buffer for a specific client IP address
    /// and disconnect.
    /// @param ip The IP address of the client to force flush and disconnect.
    void force_flush(const ip::IPAddress ip);

    /// @brief Get the timeout for polling the socket.
    int get_event_change_time(int fd) const override;

    /// @brief Get the events to listen for on the socket.
    short get_events(int socket_fd) const override;

    /// @brief Send a message to a specific client IP address.
    void send_message(const ip::IPAddress ip, const std::string& message,
                      const std::function<void(const std::string&)>& callback,
                      uint64_t delay) override;

  private:
    /// @brief Accept a new client connection.
    void accept_client();

    int listen_fd;
    std::unordered_map<ip::IPAddress, int> fds;
    std::unordered_map<int, std::shared_ptr<SingleSocketHandler>> clients;
    std::shared_ptr<fd::FDPoller> fdPoller;
    logging::Logger logger;
    const std::function<void(const ip::IPAddress ip, const std::string& msg)>
        on_message_received;
    const std::function<void(const ip::IPAddress)> on_client_connect;
    const std::function<void(const ip::IPAddress)> on_client_disconnect;
};

} // namespace network

#endif // NETWORK_H
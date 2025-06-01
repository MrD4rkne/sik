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

namespace network {

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

    void connect_to(ip::IPAddress ip_address);

    void force_flush();

    void disconnect();

    void handle(int socket_fd, short events) override;

    int get_event_change_time(int fd) const override;

    short get_events(int socket_fd) const override;

    void send_message(const ip::IPAddress, const std::string& message,
                      const std::function<void(const std::string&)>& callback,
                      uint64_t delay) override;

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

class SocketHandler : public fd::FDHandler, public messages::MessageSender {
  public:
    SocketHandler(
        int socket, fd::FDPoller& fdPoller,
        const std::function<void(const ip::IPAddress ip,
                                 const std::string& msg)>& on_message_received,
        const std::function<void(const ip::IPAddress)>& on_client_connect,
        const std::function<void(const ip::IPAddress)>& on_client_disconnect)
        : listen_fd(socket), fdPoller(fdPoller),
          on_message_received(on_message_received),
          on_client_connect(on_client_connect),
          on_client_disconnect(on_client_disconnect) {
    }

    void handle(int socket_fd, short events) override;

    void disconnect(const ip::IPAddress ip);

    void force_flush(const ip::IPAddress ip);

    int get_event_change_time(int fd) const override;

    short get_events(int socket_fd) const override;

    void send_message(const ip::IPAddress ip, const std::string& message,
                      const std::function<void(const std::string&)>& callback,
                      uint64_t delay) override;

  private:
    void accept_client();

    int listen_fd;
    std::unordered_map<ip::IPAddress, int> fds;
    std::unordered_map<int, std::shared_ptr<SingleSocketHandler>> clients;
    fd::FDPoller& fdPoller;
    const std::function<void(const ip::IPAddress ip, const std::string& msg)>
        on_message_received;
    const std::function<void(const ip::IPAddress)> on_client_connect;
    const std::function<void(const ip::IPAddress)> on_client_disconnect;
};

} // namespace network

#endif // NETWORK_H
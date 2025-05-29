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

    void connect_to(ip::IPAddress ip_address) {
        if (socket_fd != DEFAULT_SOCKET_FD) {
            throw std::runtime_error("Socket already connected: " +
                                     std::to_string(socket_fd));
        }

        auto addr = network::IpParser::to_addr(ip_address);
        socket_fd = socket(addr.first->sa_family, SOCK_STREAM, 0);
        if (socket_fd < 0) {
            throw std::runtime_error("Failed to create socket");
        }

        if (connect(socket_fd, addr.first.get(), addr.second) < 0) {
            close(socket_fd);
            throw std::runtime_error("Failed to connect to server");
        }
    }

    void disconnect() {
        if (socket_fd == DEFAULT_SOCKET_FD) {
            throw std::runtime_error("Socket not connected: " +
                                     std::to_string(socket_fd));
        }

        close(socket_fd);
        socket_fd = DEFAULT_SOCKET_FD;
        on_client_disconnect(ip_address);
    }

    void force_flush() {
        if (socket_fd == DEFAULT_SOCKET_FD) {
            throw std::runtime_error("Socket not connected: " +
                                     std::to_string(socket_fd));
        }

        logger.log_debug("Forcing flush for socket: " + std::to_string(socket_fd));

        while(message_buffer.has_message()) {
            const std::string& message = message_buffer.get_buffer();
            logger.log_debug("Sending message: " + message);
            ssize_t bytes_sent =
                send(socket_fd, message.c_str(), message.size(), 0);
            if (bytes_sent > 0) {
                message_buffer.mark_sent(bytes_sent);
            } else if (bytes_sent < 0) {
                logger.log_error("Failed to send message: " +
                                 std::string(strerror(errno)));
                break;
            }
            else{
                logger.log_error("Connection closed by peer: ");
            }
        }

        close(socket_fd);
    }

    void handle(int socket_fd, short events) override {
        if (socket_fd != this->socket_fd) {
            throw std::runtime_error("Socket fd mismatch");
        }

        if (events & POLLIN) {
            logger.log_debug("Handling POLLIN event for socket: " +
                             std::to_string(socket_fd));

            char buffer[1024];
            ssize_t bytes_read = recv(socket_fd, buffer, sizeof(buffer), 0);
            if (bytes_read > 0) {
                std::string data(buffer, bytes_read);
                logger.log_debug("Received data: " + data);

                auto new_messages = message_concater.put_data(data);
                for (const auto& message : new_messages) {
                    on_message_received(ip_address, message);
                }
            } else if (bytes_read == 0) {
                // Handle disconnection
                logger.log_debug("Client disconnected: " +
                                 std::to_string(socket_fd));
                close(socket_fd);
                socket_fd = DEFAULT_SOCKET_FD;
                on_client_disconnect(ip_address);
            } else if (bytes_read < 0) {
                logger.log_error("Failed to read from socket: " +
                                 std::string(strerror(errno)));
            }
        }

        if (events & POLLOUT && message_buffer.has_message()) {
            logger.log_debug("Handling POLLOUT event for socket: " +
                             std::to_string(socket_fd));

            const std::string& message = message_buffer.get_buffer();
            logger.log_debug("Sending message: " + message);
            ssize_t bytes_sent =
                send(socket_fd, message.c_str(), message.size(), 0);
            if (bytes_sent > 0) {
                message_buffer.mark_sent(bytes_sent);
            } else if (bytes_sent < 0) {
                logger.log_error("Failed to send message" +
                                 std::string(strerror(errno)));
            }

            logger.log_debug("Sent ", bytes_sent,
                             " bytes to socket: " + std::to_string(socket_fd));
        }
    }

    int get_event_change_time(int fd) const override {
        if (fd != socket_fd) {
            throw std::runtime_error("Socket fd mismatch");
        }

        return message_buffer.next_message_time();
    }

    short get_events(int socket_fd) const override {
        if (socket_fd != this->socket_fd) {
            throw std::runtime_error("Socket fd mismatch");
        }

        short events = POLLIN;
        if (message_buffer.has_message()) {
            events |= POLLOUT;
        }
        return events;
    }

    void send_message(const ip::IPAddress, const std::string& message,
                      const std::function<void(const std::string&)>& callback,
                      uint64_t delay) override {
        logger.log_debug("Sending message: " + message);
        message_buffer.add_message(message + "\r\n", callback, delay);
    }

    int get_socket_fd() const {
        return socket_fd;
    }

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

    void handle(int socket_fd, short events) override {
        if (socket_fd != listen_fd) {
            throw std::runtime_error("Fd is not the listenning one.");
        }

        if (events != POLLIN) {
            // We ignore not connect/disconnect events.
            return;
        }

        accept_client();
    }

    void disconnect(const ip::IPAddress ip) {
        auto it = fds.find(ip);
        if (it == fds.end()) {
            throw std::runtime_error("Client not found in open connections.");
        }

        int fd = it->second;
        fds.erase(it);
        clients[fd]->disconnect();
        clients.erase(fd);
        fdPoller.remove_socket(fd);
    }

    void force_flush(const ip::IPAddress ip) {
        auto it = fds.find(ip);
        if (it == fds.end()) {
            throw std::runtime_error("Client not found in open connections.");
        }

        int fd = it->second;
        clients[fd]->force_flush();
        fdPoller.remove_socket(fd);
        clients.erase(fd);
        fds.erase(it);
        on_client_disconnect(ip);
    }

    int get_event_change_time(int fd) const override {
        if (fd != listen_fd) {
            throw std::runtime_error("Fd is not the listenning one.");
        }

        return -1;
    }

    short get_events(int socket_fd) const override {
        if (socket_fd != listen_fd) {
            throw std::runtime_error("Fd is not the listenning one.");
        }

        // Return the connect / disconnect events.
        return POLLIN;
    }

    void send_message(const ip::IPAddress ip, const std::string& message,
                      const std::function<void(const std::string&)>& callback,
                      uint64_t delay) override {
        auto it = fds.find(ip);
        if (it == fds.end()) {
            throw std::runtime_error("Client not found in open connections.");
        }

        int fd = it->second;
        clients[fd]->send_message(ip, message, callback, delay);
    }

  private:
    void accept_client() {
        // Accept a new client connection
        sockaddr client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, &client_addr, &addr_len);
        if (client_fd < 0) {
            // TODO: Handle error
            return;
        }

        auto on_disconnect = [&, client_fd](const ip::IPAddress ip) {
            clients.erase(client_fd);
            fds.erase(ip);
            fdPoller.remove_socket(client_fd);
            on_client_disconnect(ip);
        };

        auto ip = network::IpParser::addr_to_ip(&client_addr);
        auto ptr = std::make_shared<SingleSocketHandler>(
            client_fd, logging::LoggerFactory::create_logger(ip), ip,
            on_message_received, on_disconnect);

        fdPoller.add_socket(client_fd, ptr);
        on_client_connect(ip);

        clients[client_fd] = ptr;
        fds[ip] = client_fd;
    }

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
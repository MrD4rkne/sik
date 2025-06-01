#include <algorithm>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string>

#include "network.h"

namespace network {

ip::IPAddress IpParser::parse(const std::string& ip_str, const ip::port_t port,
                              ip::IPAddress::Type type) {
    addrinfo hints{};
    switch (type) {
    case ip::IPAddress::Type::IPv4:
        hints.ai_family = AF_INET; // IPv4
        break;
    case ip::IPAddress::Type::IPv6:
        hints.ai_family = AF_INET6; // IPv6
        break;
    default:
        hints.ai_family = AF_UNSPEC; // Any address family
        break;
    }

    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_protocol = IPPROTO_TCP; // TCP

    addrinfo* address_result;
    int errcode = getaddrinfo(ip_str.c_str(), nullptr, &hints, &address_result);
    if (errcode != 0) {
        throw std::runtime_error("getaddrinfo(): " +
                                 std::string(gai_strerror(errcode)));
    }

    auto address = IpParser::addr_to_ip(address_result, port);
    freeaddrinfo(address_result);
    return address;
}

std::pair<std::unique_ptr<sockaddr>, socklen_t>
IpParser::to_addr(const ip::IPAddress& ip_address) {
    if (ip_address.get_type() == ip::IPAddress::Type::IPv4) {
        auto ipv4 = std::make_unique<sockaddr_in>();
        ipv4->sin_family = AF_INET;
        ipv4->sin_port = htons(ip_address.get_port());

        auto addr_bytes = std::get<std::array<uint8_t, ip::IPV4_SIZE>>(
            ip_address.get_adress());

        std::array<uint8_t, 4> bytes;
        for (size_t i = 0; i < 4; ++i) {
            bytes[i] = addr_bytes[i];
        }
        std::memcpy(&(ipv4->sin_addr), bytes.data(), 4);

        return std::make_pair(std::unique_ptr<sockaddr>(
                                  reinterpret_cast<sockaddr*>(ipv4.release())),
                              sizeof(sockaddr_in));
    } else if (ip_address.get_type() == ip::IPAddress::Type::IPv6) {
        auto ipv6 = std::make_unique<sockaddr_in6>();
        ipv6->sin6_family = AF_INET6;
        ipv6->sin6_port = htons(ip_address.get_port());

        auto addr_bytes = std::get<std::array<uint8_t, ip::IPV6_SIZE>>(
            ip_address.get_adress());
        std::memcpy(&(ipv6->sin6_addr), addr_bytes.data(), 16);

        return std::make_pair(std::unique_ptr<sockaddr>(
                                  reinterpret_cast<sockaddr*>(ipv6.release())),
                              sizeof(sockaddr_in6));
    } else {
        throw std::invalid_argument("Unsupported address type");
    }
}

ip::IPAddress IpParser::addr_to_ip(struct sockaddr* addr) {
    if (addr->sa_family == AF_INET) {
        // Standard IPv4 address
        auto* ipv4 = reinterpret_cast<sockaddr_in*>(addr);
        std::array<uint8_t, 4> bytes;
        uint32_t ip = ntohl(ipv4->sin_addr.s_addr);
        for (size_t i = 0; i < 4; ++i) {
            bytes[3 - i] = static_cast<uint8_t>(ip & 0xFF);
            ip >>= 8;
        }

        uint16_t port = ntohs(ipv4->sin_port);
        return ip::IPAddress(bytes, port);
    } else if (addr->sa_family == AF_INET6) {
        auto* ipv6 = reinterpret_cast<sockaddr_in6*>(addr);
        uint16_t port = ntohs(ipv6->sin6_port);

        // Check if this is an IPv4-mapped IPv6 address
        if (IN6_IS_ADDR_V4MAPPED(&ipv6->sin6_addr)) {
            // Extract the IPv4 part from the IPv4-mapped IPv6 address
            // The last 4 bytes of the IPv6 address contain the IPv4 address
            std::array<uint8_t, 4> ipv4_bytes;
            memcpy(ipv4_bytes.data(), &ipv6->sin6_addr.s6_addr[12], 4);
            return ip::IPAddress(ipv4_bytes, port);
        } else {
            // Regular IPv6 address
            std::array<uint8_t, 16> bytes;
            std::memcpy(bytes.data(), &(ipv6->sin6_addr), 16);
            return ip::IPAddress(bytes, port);
        }
    }

    throw std::invalid_argument("Unsupported address family");
}

ip::IPAddress IpParser::addr_to_ip(const addrinfo* addr,
                                   const ip::port_t port) {
    if (addr->ai_family == AF_INET) {
        auto* ipv4 = reinterpret_cast<sockaddr_in*>(addr->ai_addr);
        std::array<uint8_t, 4> bytes;
        uint32_t ip = ntohl(ipv4->sin_addr.s_addr);
        for (size_t i = 0; i < 4; ++i) {
            bytes[3 - i] = static_cast<uint8_t>(ip & 0xFF);
            ip >>= 8;
        }
        return ip::IPAddress(bytes, port);
    } else if (addr->ai_family == AF_INET6) {
        auto* ipv6 = reinterpret_cast<sockaddr_in6*>(addr->ai_addr);
        std::array<uint8_t, 16> bytes;
        std::memcpy(bytes.data(), &(ipv6->sin6_addr), 16);
        return ip::IPAddress(bytes, port);
    }

    throw std::invalid_argument("Unsupported address family");
}

void SingleSocketHandler::connect_to(ip::IPAddress ip) {
    if (socket_fd != DEFAULT_SOCKET_FD) {
        throw std::runtime_error("Socket already connected: " +
                                 std::to_string(socket_fd));
    }

    auto addr = network::IpParser::to_addr(ip);
    socket_fd = socket(addr.first->sa_family, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    if (connect(socket_fd, addr.first.get(), addr.second) < 0) {
        close(socket_fd);
        throw std::runtime_error("Failed to connect to server");
    }
}

void SingleSocketHandler::disconnect() {
    if (socket_fd == DEFAULT_SOCKET_FD) {
        throw std::runtime_error("Socket not connected: " +
                                 std::to_string(socket_fd));
    }

    close(socket_fd);
    socket_fd = DEFAULT_SOCKET_FD;
    on_client_disconnect(ip_address);
}

void SingleSocketHandler::force_flush() {
    if (socket_fd == DEFAULT_SOCKET_FD) {
        throw std::runtime_error("Socket not connected: " +
                                 std::to_string(socket_fd));
    }

    logger.log_debug("Forcing flush for socket: " + std::to_string(socket_fd));

    while (message_buffer.has_message()) {
        const std::string& message = message_buffer.get_buffer();
        logger.log_debug("Sending message: " + message);
        ssize_t bytes_sent =
            send(socket_fd, message.c_str(), message.size(), 0);
        if (bytes_sent > 0) {
            message_buffer.mark_sent((size_t)bytes_sent);
        } else if (bytes_sent < 0) {
            logger.log_error("Failed to send message: " +
                             std::string(strerror(errno)));
            break;
        } else {
            logger.log_error("Connection closed by peer: ");
        }
    }

    disconnect();
}

void SingleSocketHandler::handle(int fd, short events) {
    if (fd != this->socket_fd) {
        throw std::runtime_error("Socket fd mismatch");
    }

    if (events & POLLIN) {
        logger.log_debug("Handling POLLIN event for socket: " +
                         std::to_string(socket_fd));

        char buffer[1024];
        ssize_t bytes_read = recv(socket_fd, buffer, sizeof(buffer), 0);
        if (bytes_read > 0) {
            std::string data(buffer, (size_t)bytes_read);
            logger.log_debug("Received data: " + data);

            auto new_messages = message_concater.put_data(data);
            for (const auto& message : new_messages) {
                on_message_received(ip_address, message);
            }

            return;
        }

        if (bytes_read == 0) {
            // Handle disconnection
            logger.log_debug("Client disconnected: " +
                             std::to_string(socket_fd));
            disconnect();
            return;
        }

        // Handle read error
        logger.log_error("Failed to read from socket: " +
                         std::string(strerror(errno)));
        // If the error is EAGAIN or EWOULDBLOCK, we can ignore it and retry later.
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            return;
        }

        logger.log_info("Socket read error: " + std::string(strerror(errno)) +
                        ", closing socket: " + std::to_string(socket_fd));
        disconnect();
        return;
    }

    if (events & POLLOUT && message_buffer.has_message()) {
        logger.log_debug("Handling POLLOUT event for socket: " +
                         std::to_string(socket_fd));

        const std::string& message = message_buffer.get_buffer();
        logger.log_debug("Sending message: " + message);
        ssize_t bytes_sent =
            send(socket_fd, message.c_str(), message.size(), 0);
        if (bytes_sent > 0) {
            message_buffer.mark_sent((size_t)bytes_sent);
            logger.log_debug("Sent ", bytes_sent,
                             " bytes to socket: " + std::to_string(socket_fd));
            return;
        }

        if (bytes_sent == 0) {
            logger.log_info("Connection closed by peer: " +
                            std::to_string(socket_fd));
            disconnect();
            return;
        }

        // If the error is EAGAIN or EWOULDBLOCK, we can ignore it and retry later.
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            logger.log_debug("Socket send would block, retrying later");
            return; // Retry later
        }

        logger.log_error("Failed to send message: " +
                         std::string(strerror(errno)));
        disconnect();
        return;
    }
}

int SingleSocketHandler::get_event_change_time(int fd) const {
    if (fd != socket_fd) {
        throw std::runtime_error("Socket fd mismatch");
    }

    return message_buffer.next_message_time();
}

short SingleSocketHandler::get_events(int fd) const {
    if (fd != this->socket_fd) {
        throw std::runtime_error("Socket fd mismatch");
    }

    short events = POLLIN;
    if (message_buffer.has_message()) {
        events |= POLLOUT;
    }
    return events;
}

void SingleSocketHandler::send_message(
    const ip::IPAddress, const std::string& message,
    const std::function<void(const std::string&)>& callback, uint64_t delay) {
    logger.log_debug("Sending message: " + message);
    message_buffer.add_message(message + "\r\n", callback, delay);
}

int SingleSocketHandler::get_socket_fd() const {
    return socket_fd;
}

void SocketHandler::handle(int socket_fd, short events) {
    if (socket_fd != listen_fd) {
        throw std::runtime_error("Fd is not the listenning one.");
    }

    if (events != POLLIN) {
        // We ignore not connect/disconnect events.
        return;
    }

    accept_client();
}

void SocketHandler::disconnect(const ip::IPAddress ip) {
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

void SocketHandler::force_flush(const ip::IPAddress ip) {
    auto it = fds.find(ip);
    if (it == fds.end()) {
        throw std::runtime_error("Client not found in open connections.");
    }

    int fd = it->second;
    clients[fd]->force_flush();
}

int SocketHandler::get_event_change_time(int fd) const {
    if (fd != listen_fd) {
        throw std::runtime_error("Fd is not the listenning one.");
    }

    return -1;
}

short SocketHandler::get_events(int socket_fd) const {
    if (socket_fd != listen_fd) {
        throw std::runtime_error("Fd is not the listenning one.");
    }

    // Return the connect / disconnect events.
    return POLLIN;
}

void SocketHandler::send_message(
    const ip::IPAddress ip, const std::string& message,
    const std::function<void(const std::string&)>& callback, uint64_t delay) {
    auto it = fds.find(ip);
    if (it == fds.end()) {
        throw std::runtime_error("Client not found in open connections.");
    }

    int fd = it->second;
    clients[fd]->send_message(ip, message, callback, delay);
}

void SocketHandler::accept_client() {
    // Accept a new client connection
    sockaddr_storage client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd = accept(listen_fd, (sockaddr*)&client_addr, &addr_len);
    if (client_fd < 0) {
        logger.log_error("Failed to accept client connection: " +
                         std::string(strerror(errno)));

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }

        throw std::system_error(errno, std::generic_category(),
                                "Failed to accept client connection");
    }

    auto on_disconnect = [&, client_fd](const ip::IPAddress ip) {
        clients.erase(client_fd);
        fds.erase(ip);
        fdPoller.remove_socket(client_fd);
        on_client_disconnect(ip);
    };

    auto ip = network::IpParser::addr_to_ip((sockaddr*)&client_addr);
    auto ptr = std::make_shared<SingleSocketHandler>(
        client_fd, logging::Logger(), ip,
        on_message_received, on_disconnect);

    fdPoller.add_socket(client_fd, ptr);
    on_client_connect(ip);

    clients[client_fd] = ptr;
    fds[ip] = client_fd;
}

} // namespace network
#include <algorithm>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string>

#include "network.h"

namespace network {

static inline const std::string END_OF_MESSAGE = "\r\n";

std::vector<std::string> MessageConcater::put_data(const std::string& data) {
    buffer += data;
    std::vector<std::string> messages;

    size_t pos = 0;
    while ((pos = buffer.find(network::END_OF_MESSAGE)) != std::string::npos) {
        messages.push_back(buffer.substr(0, pos));
        buffer.erase(0, pos + network::END_OF_MESSAGE.size());
    }

    return messages;
}

void MessageBuffer::add_message(
    const std::string& message,
    const std::function<void(const std::string&)>& callback) {
    Message msg;
    msg.data = message;
    msg.callback = callback;
    buffer.push(msg);
}

bool MessageBuffer::has_message() const {
    return !buffer.empty();
}

const std::string& MessageBuffer::get_buffer() const {
    if (!has_message()) {
        throw std::runtime_error("Buffer is empty");
    }

    auto& message = buffer.front();
    return message.data;
}

void MessageBuffer::mark_sent(size_t bytes) {
    if (!has_message()) {
        throw std::runtime_error("Buffer is empty");
    }

    auto& message = buffer.front();
    message.data.erase(0, bytes);
    if (message.data.empty()) {
        buffer.pop();
    }
}

static inline ip::IPAddress addr_to_ip(struct sockaddr* addr) {
    if (addr->sa_family == AF_INET) {
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
        std::array<uint8_t, 16> bytes;
        std::memcpy(bytes.data(), &(ipv6->sin6_addr), 16);
        uint16_t port = ntohs(ipv6->sin6_port);
        return ip::IPAddress(bytes, port);
    }

    throw std::invalid_argument("Unsupported address family");
}

void SocketPoller::add_socket(int fd, short events) {
    pollfd pfd;
    pfd.fd = fd;
    pfd.events = events;
    pfd.revents = 0;
    poll_fds.push_back(pfd);
    fd_to_index[fd] = poll_fds.size() - 1;
}

void SocketPoller::set_events(int fd, short events) {
    auto it = fd_to_index.find(fd);
    if (it != fd_to_index.end()) {
        poll_fds[it->second].events = events;
    } else {
        throw std::invalid_argument("Socket not found in poller");
    }
}

void SocketPoller::remove_socket(int fd) {
    if (fd_to_index.find(fd) != fd_to_index.end()) {
        size_t idx = fd_to_index[fd];

        // Swap with the last element and pop
        if (idx != poll_fds.size() - 1) {
            std::swap(poll_fds[idx], poll_fds.back());
            fd_to_index[poll_fds[idx].fd] = idx;
        }

        poll_fds.pop_back();
        fd_to_index.erase(fd);
    }
}

int SocketPoller::poll_sockets(int timeout_ms) {
    // Reset revents before polling
    for (auto& pfd : poll_fds) {
        pfd.revents = 0;
    }
    return poll(poll_fds.data(), poll_fds.size(), timeout_ms);
}

bool SocketPoller::has_events(int fd, short event_mask) {
    auto it = fd_to_index.find(fd);
    if (it == fd_to_index.end()) {
        throw std::invalid_argument("Socket not found in poller");
    }

    size_t idx = it->second;
    return poll_fds[idx].revents & event_mask;
}

std::vector<int> SocketPoller::get_ready_sockets(short event_mask) {
    std::vector<int> ready;
    for (const auto& pfd : poll_fds) {
        if (pfd.revents & event_mask) {
            ready.push_back(pfd.fd);
        }
    }
    return ready;
}

Server::Server(const std::function<void(const ip::IPAddress&)>& on_connect,
               const std::function<void(const ip::IPAddress&)>& on_disconnect)
    : listen_fd(-1), connections{}, on_connect(on_connect),
      on_disconnect(on_disconnect), messages{}, poller() {
}

Server::Server(int listen_fd,
               const std::function<void(const ip::IPAddress&)>& on_connect,
               const std::function<void(const ip::IPAddress&)>& on_disconnect)
    : listen_fd(listen_fd), connections{}, on_connect(on_connect),
      on_disconnect(on_disconnect), messages{}, poller() {
}

void Server::connect_to(const ip::IPAddress& ip_address) {
    auto addr = network::IpParser::to_addr(ip_address);

    int socket_fd = socket(addr.first->sa_family, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    if (connect(socket_fd, addr.first.get(), addr.second) < 0) {
        close(socket_fd);
        throw std::runtime_error("Failed to connect to server");
    }

    poller.add_socket(socket_fd);
    connections[ip_address] = {socket_fd, MessageConcater(), MessageBuffer()};
    on_connect(ip_address);
}

void Server::disconnect(const ip::IPAddress ip_address) {
    auto it = connections.find(ip_address);
    if (it == connections.end()) {
        throw std::runtime_error("Socket not found");
    }

    auto& connection = it->second;
    close(connection.socket_fd);
    poller.remove_socket(connection.socket_fd);

    on_disconnect(ip_address);
    connections.erase(it);
}

void Server::disconnect_all() {
    for (const auto& pair : connections) {
        close(pair.second.socket_fd);
    }
    connections.clear();
}

size_t Server::get_num_connections() const {
    return connections.size();
}

void Server::send_message(
    const ip::IPAddress& ip_address, const std::string& message,
    const std::function<void(const std::string&)>& callback) {
    auto it = connections.find(ip_address);
    if (it == connections.end()) {
        throw std::runtime_error("Socket not found");
    }

    it->second.message_buffer.add_message(message + END_OF_MESSAGE, callback);
}

void Server::send_message(const ip::IPAddress& ip_address,
                          const std::string& message) {
    static const auto DO_NOTHING = [](const std::string&) {};
    send_message(ip_address, message, DO_NOTHING);
}

bool Server::has_messages() const {
    return !messages.empty();
}

std::pair<ip::IPAddress, std::string> Server::get_message() {
    if (!has_messages()) {
        throw std::runtime_error("No messages available");
    }

    auto message = messages.front();
    messages.pop();
    return message;
}

void Server::process() {
    for (const auto& pair : connections) {
        auto& connection = pair.second;
        short events = POLLIN;
        if (connection.message_buffer.has_message()) {
            events |= POLLOUT;
        }

        poller.set_events(connection.socket_fd, events);
    }

    int poll_result = poller.poll_sockets(-1);
    if (poll_result < 0) {
        throw std::runtime_error("Poll failed");
    }

    try_read();
    try_write();
}

void Server::try_read() {
    auto get_sockets_with_data = poller.get_ready_sockets(POLLIN);
    for (const auto& fd : get_sockets_with_data) {
        auto it = std::find_if(
            connections.begin(), connections.end(),
            [fd](const auto& pair) { return pair.second.socket_fd == fd; });

        if (it == connections.end()) {
            // No connection, so this is listening socket.
            // Accept new connection.
            struct sockaddr client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int new_fd = accept(listen_fd, &client_addr, &addr_len);
            if (new_fd < 0) {
                throw std::runtime_error("Failed to accept new connection");
            }

            auto ip_address = addr_to_ip(&client_addr);
            connections[ip_address] = {new_fd, MessageConcater(),
                                       MessageBuffer()};
            poller.add_socket(new_fd);
        } else {
            auto& connection = it->second;
            char buffer[1024];
            ssize_t bytes_read = recvfrom(fd, buffer, sizeof(buffer),
                                          MSG_DONTWAIT, nullptr, nullptr);
            if (bytes_read > 0) {
                std::string data(buffer, bytes_read);
                auto new_messages = connection.message_concater.put_data(data);
                for (const auto& message : new_messages) {
                    messages.push(std::make_pair(it->first, message));
                }
            } else if (bytes_read == 0) {
                disconnect(it->first);
            } else if (bytes_read < 0) {
                // TODO: handle errno
                throw std::runtime_error("Failed to read from socket");
            }
        }
    }
}

void Server::try_write() {
    auto get_sockets_with_data = poller.get_ready_sockets(POLLOUT);
    for (const auto& fd : get_sockets_with_data) {
        auto it = std::find_if(
            connections.begin(), connections.end(),
            [fd](const auto& pair) { return pair.second.socket_fd == fd; });

        if (it != connections.end()) {
            auto& connection = it->second;
            if (connection.message_buffer.has_message()) {
                const std::string& message =
                    connection.message_buffer.get_buffer();
                ssize_t bytes_sent =
                    send(fd, message.c_str(), message.size(), MSG_DONTWAIT);
                if (bytes_sent > 0) {
                    connection.message_buffer.mark_sent(bytes_sent);
                } else if (bytes_sent < 0) {
                    // TODO: handle errno
                    throw std::runtime_error("Failed to send message");
                }
            }
        }
    }
}

static inline ip::IPAddress addr_to_ip(const addrinfo* addr,
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

    auto address = addr_to_ip(address_result, port);
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

} // namespace network
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

class SocketPoller {
  public:
    // Add a socket to be monitored for read events
    void add_socket(int fd, short events = POLLIN);

    void set_events(int fd, short events);

    // Remove a socket from polling
    void remove_socket(int fd);

    // Poll for events with timeout in milliseconds
    int poll_sockets(int timeout_ms = -1);

    // Check if a specific socket has events
    bool has_events(int fd, short event_mask = POLLIN);

    // Get all ready socket file descriptors
    std::vector<int> get_ready_sockets(short event_mask = POLLIN);

  private:
    std::vector<pollfd> poll_fds;
    std::unordered_map<int, size_t> fd_to_index; // Maps fd to index in poll_fds
};

class MessageSender {
  public:
    virtual void
    send_message(const ip::IPAddress& ip_address, const std::string& message,
                 const std::function<void(const std::string&)>& callback) = 0;

    virtual void send_message(const ip::IPAddress& ip_address,
                              const std::string& message) = 0;

    template<typename T>
    void send_message(const ip::IPAddress& ip_address, const T& message,
                      const std::function<void(const std::string&)>& callback) {
        send_message(ip_address, messages::serialize_message(message),
                     callback);
    }

    virtual ~MessageSender() = default;
};

class Server : public MessageSender {
  public:
    Server(const std::function<void(const ip::IPAddress&)>& on_connect,
           const std::function<void(const ip::IPAddress&)>& on_disconnect);

    Server(int listen_fd,
           const std::function<void(const ip::IPAddress&)>& on_connect,
           const std::function<void(const ip::IPAddress&)>& on_disconnect);

    void connect_to(const ip::IPAddress& ip_address);

    void disconnect(const ip::IPAddress ip_address);

    void disconnect_all();

    size_t get_num_connections() const;

    void send_message(const ip::IPAddress& ip_address,
                      const std::string& message,
                      const std::function<void(const std::string&)>& callback);

    void send_message(const ip::IPAddress& ip_address,
                      const std::string& message);

    bool has_messages() const;

    std::pair<ip::IPAddress, std::string> get_message();

    void process();

  private:
    struct connection_t {
        int socket_fd;
        MessageConcater message_concater;
        MessageBuffer message_buffer;
    };

    int listen_fd;
    std::unordered_map<ip::IPAddress, connection_t> connections;
    const std::function<void(const ip::IPAddress&)> on_connect;
    const std::function<void(const ip::IPAddress&)> on_disconnect;
    std::queue<std::pair<ip::IPAddress, std::string>> messages;
    SocketPoller poller;

    void try_read();
    void try_write();
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
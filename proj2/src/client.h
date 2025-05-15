#ifndef CLIENT_H
#define CLIENT_H

#include "handlers.h"
#include "ip.h"
#include "logging.h"
#include "messages.h"
#include "network.h"
#include "results.h"
#include <algorithm>
#include <memory>

namespace client {

class strategy {
    // Define the strategy class here
};

class client_state {
    enum class state {
        WAITING_FOR_FIRST_MESSAGE,
        WRONG_FIRST_MESSAGE,
        RUNNING,
        WAITING_FOR_PUT_RESPONSE,
        ERROR,
        STOPPED
    };

  public:
    client_state()
        : current_state(state::WAITING_FOR_FIRST_MESSAGE), coeffs(nullptr) {
    }

    void mark_wrong_message() {
        if (current_state != state::WAITING_FOR_FIRST_MESSAGE) {
            return;
        }

        current_state = state::WRONG_FIRST_MESSAGE;
    }

    results::Result
    mark_coeffs_received(const std::vector<messages::rational_t>& coeffs) {
        if (current_state != state::WAITING_FOR_FIRST_MESSAGE) {
            return results::Result::Failure(
                "Wrong state for coeffs: " +
                std::to_string(static_cast<int>(current_state)));
        }

        current_state = state::RUNNING;
        this->coeffs =
            std::make_unique<std::vector<messages::rational_t>>(coeffs);
        return results::Result::Success();
    }

    state get_state() const {
        return current_state;
    }

    bool should_be_running() const {
        constexpr state not_running_states[] = {state::WRONG_FIRST_MESSAGE,
                                                state::ERROR, state::STOPPED};

        return std::find(std::begin(not_running_states),
                         std::end(not_running_states),
                         current_state) == std::end(not_running_states);
    }

    bool should_exit_with_error() const {
        constexpr state error_states[] = {state::WRONG_FIRST_MESSAGE,
                                          state::ERROR};
        return std::find(std::begin(error_states), std::end(error_states),
                         current_state) == std::end(error_states);
    }

  private:
    state current_state;
    std::unique_ptr<std::vector<messages::rational_t>> coeffs;
};

class client {
  public:
    client(const std::string& player_id, const ip::IPAddress& server_address,
           const strategy& strat, logging::Logger& logger)
        : player_id(player_id), strat(std::make_unique<strategy>(strat)),
          logger(logger), ip_address(server_address), state{},
          message_handler{} {
    }

    void init();

    void run();

    void set_error();

    void handle_message(const std::string message,
                        network::MessageSender& message_sender);

    void forget_client() {
    }

  private:
    std::string player_id;
    std::unique_ptr<strategy> strat;
    logging::Logger& logger;
    ip::IPAddress ip_address;
    client_state state;
    handlers::message_handler<client_state> message_handler;
};

class SingleSocketHandler : public fd::FDHandler,
                            public network::MessageSender {
  public:
    SingleSocketHandler(client& client_instance, logging::Logger& logger)
        : client_instance(client_instance), logger(logger),
          socket_fd(DEFAULT_SOCKET_FD), message_buffer(), message_concater() {
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
                auto new_messages = message_concater.put_data(data);
                for (const auto& message : new_messages) {
                    client_instance.handle_message(message, *this);
                }
            } else if (bytes_read == 0) {
                // Handle disconnection
                close(socket_fd);
                logger.log_error("Disconnected from server");
                client_instance.set_error();
            } else if (bytes_read < 0) {
                logger.log_error("Failed to read from socket" +
                                 std::string(strerror(errno)));
            }
        }

        if (events & POLLOUT && message_buffer.has_message()) {
            logger.log_debug("Handling POLLOUT event for socket: " +
                             std::to_string(socket_fd));

            const std::string& message = message_buffer.get_buffer();
            ssize_t bytes_sent =
                send(socket_fd, message.c_str(), message.size(), 0);
            if (bytes_sent > 0) {
                message_buffer.mark_sent(bytes_sent);
            } else if (bytes_sent < 0) {
                logger.log_error("Failed to send message" +
                                 std::string(strerror(errno)));
            }
        }
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

    void send_message(
        const ip::IPAddress&, const std::string& message,
        const std::function<void(const std::string&)>& callback) override {
        logger.log_debug("Sending message: " + message);
        message_buffer.add_message(message + "\r\n", callback);
    }

    int get_socket_fd() const {
        return socket_fd;
    }

  private:
    constexpr static int DEFAULT_SOCKET_FD = -1;

    client& client_instance;
    logging::Logger& logger;
    int socket_fd;
    network::MessageBuffer message_buffer;
    network::MessageConcater message_concater;
};

} // namespace client

#endif
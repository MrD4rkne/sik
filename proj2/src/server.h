#ifndef SERVER_H
#define SERVER_H

#include "fd.h"
#include "file.h"
#include "handlers.h"
#include "ip.h"
#include "network.h"

#include <algorithm>
#include <chrono>
#include <queue>
#include <unordered_map>

namespace server {

constexpr uint64_t DELAY_BEFORE_COEFF = 1000; // 10 seconds

class Server;

results::Result handler_hello(const ip::IPAddress sender,
                              network::MessageSender&, Server& state,
                              logging::Logger& logger,
                              const std::string& message);

class Server {
  public:
    Server(const std::string filePath)
        : logger(), message_sender{nullptr},
          file_handler(
              std::make_shared<file::FileHandler>(filePath, on_coeff_read)),
          msg_handler{} {
        msg_handler.register_handler(messages::HELLO_MESSAGE, handler_hello);
    }

    void run(int socket_fd) {
        auto on_connect = [&](const ip::IPAddress ip) { add_client(ip); };
        auto on_disconnect = [&](const ip::IPAddress ip) { forget(ip); };
        auto on_message = [&](const ip::IPAddress ip,
                              const std::string message) {
            handle_message(ip, message);
        };

        fd::FDPoller poller;
        message_sender = std::make_shared<network::SocketHandler>(
            socket_fd, poller, on_message, on_connect, on_disconnect);
        poller.add_socket(socket_fd, message_sender);

        try {
            file_handler->open_file();
            poller.add_socket(file_handler->get_fd(), file_handler);
        } catch (const std::exception& ex) {
            logger.log_error("Exception when opening file: ", ex.what());
            throw;
        }

        while (true) {
            int result = poller.poll_sockets();
            if (result < 0) {
                // TODO: handle error
                logger.log_error("Poll error: " + std::string(strerror(errno)));
                break;
            }

            poller.handle();

            dispatch_coeffs();
        }
    }

    void dispatch_coeffs() {
        while (!read_coeffs.empty() && !waiting_for_coeffs.empty()) {
            auto [player, timepoint] = waiting_for_coeffs.front();
            auto msg = read_coeffs.front();
            waiting_for_coeffs.pop_front();
            read_coeffs.pop();

            uint64_t delay = 0;
            auto now = std::chrono::system_clock::now();
            if (timepoint > now) {
                auto remaining = std::chrono::system_clock::now() - timepoint;
                delay = std::chrono::duration_cast<std::chrono::milliseconds>(
                            remaining)
                            .count();
            }

            message_sender->send_message(
                player, msg, [](const std::string&) {}, delay);
        }

        if (!waiting_for_coeffs.empty() &&
            !file_handler->is_waiting_for_line()) {
            file_handler->request_line();
        }
    }

    void add_client(const ip::IPAddress ip_address) {
        // Add client to the server
        logger.log_debug("New player: ", ip_address);

        players[ip_address] = {};
    }

    void forget(const ip::IPAddress ip_address) {
        // Remove client from the server
        logger.log_debug("Forgetting player: ", ip_address);
        players.erase(ip_address);

        auto it = std::remove_if(
            waiting_for_coeffs.begin(), waiting_for_coeffs.end(),
            [&](const auto& pair) { return pair.first == ip_address; });
        if (it == waiting_for_coeffs.end()) {
            return;
        }

        waiting_for_coeffs.erase(it, waiting_for_coeffs.end());
    }

    void handle_message(const ip::IPAddress sender, const std::string message) {
        auto player_id = players[sender].id;

        bool was_ok = true;
        try {
            std::string type = messages::get_type(message);
            logging::Logger local_logger =
                logging::LoggerFactory::create_logger(sender);
            auto result = msg_handler.handle(type, sender, *message_sender,
                                             *this, local_logger, message);
            if (!result.is_success()) {
                was_ok = false;
                logger.log_debug("Wrong message: " +
                                 result.get_error_message());
            }

        } catch (const std::invalid_argument& e) {
            logger.log_debug("Invalid message: " + std::string(e.what()));
            was_ok = false;
        }

        if (was_ok) {
            logger.log_debug("Message handled successfully");
        } else {
            logger.log_bad_message(sender, player_id, message);
        }
    }

    results::Result mark_hello(const ip::IPAddress sender,
                               const std::string player_id) {
        auto& player = players[sender];
        if (player.has_sent_hello) {
            return results::Result::Failure("Already sent hello.");
        }

        player.id = player_id;
        player.has_sent_hello = true;
        waiting_for_coeffs.push_back(std::make_pair(
            sender, std::chrono::system_clock::now() +
                        std::chrono::milliseconds(DELAY_BEFORE_COEFF)));

        return results::Result::Success();
    }

  private:
    struct player {
        std::string id = "UNKNOWN";
        bool has_sent_hello = false;
    };

    std::function<void(const std::string)> on_coeff_read =
        [&](const std::string msg) {
            logger.log_debug("Read new coef: ", msg);
            read_coeffs.push(msg);
        };

    logging::Logger logger;
    std::shared_ptr<network::SocketHandler> message_sender;
    std::shared_ptr<file::FileHandler> file_handler;
    handlers::message_handler<server::Server> msg_handler;
    std::unordered_map<ip::IPAddress, player> players;
    std::queue<std::string> read_coeffs;
    std::deque<std::pair<ip::IPAddress,
                         std::chrono::time_point<std::chrono::system_clock>>>
        waiting_for_coeffs;
};

results::Result handler_hello(const ip::IPAddress sender,
                              network::MessageSender&, Server& state,
                              logging::Logger& logger,
                              const std::string& message) {
    logger.log_debug("Handling HELLO message: " + message);

    // TODO: disconnect on wrong message if first
    messages::hello_message_t hello_message =
        messages::deserialize_message<messages::hello_message_t>(message);
    logger.log_debug("HELLO received: ", hello_message);

    return state.mark_hello(sender, hello_message.player_id);
}

} // namespace server

#endif
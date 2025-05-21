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

constexpr static uint64_t DELAY_BEFORE_COEFF = 1000; // milliseconds
constexpr static uint64_t MAX_DELAY_BETWEEN_CONNECT_AND_HELLO =
    3000; // milliseconds
const static std::string UNKNOWN_ID = "UNKNOWN";
constexpr static messages::rational_t POINT_DEFAULT = 0.0f;
constexpr static uint64_t PENALTY_ON_PUT_BEFORE_RESPONSE = 20;
constexpr static uint64_t PENALTY_ON_BAD_PUT = 10;
constexpr static messages::rational_t MIN_VALUE = -5.0f;
constexpr static messages::rational_t MAX_VALUE = 5.0f;
constexpr static uint64_t DELAY_AFTER_BAD_PUT = 1000;
constexpr static uint64_t PENALTY_DELAY = 0;

static inline uint64_t count_small_letters(const std::string& str) {
    return std::count_if(str.begin(), str.end(),
                         [](unsigned char c) { return std::islower(c); });
}

class Server;

results::Result handler_hello(const ip::IPAddress sender,
                              network::MessageSender&, Server& state,
                              logging::Logger& logger,
                              const std::string& message);

results::Result handler_put(const ip::IPAddress sender, network::MessageSender&,
                            Server& state, logging::Logger& logger,
                            const std::string& message);

static uint64_t
get_diff_time(const std::chrono::time_point<std::chrono::system_clock>& start,
              const std::chrono::time_point<std::chrono::system_clock>& end) {
    auto diff = end - start;
    if (diff < std::chrono::milliseconds(0)) {
        return 0;
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();
}

class Server {
  public:
    Server(const std::string filePath, uint16_t k, uint8_t n, uint32_t m)
        : logger(), message_sender{nullptr},
          file_handler(
              std::make_shared<file::FileHandler>(filePath, on_coeff_read)),
          msg_handler{}, k{k}, n{n}, m{m} {
        msg_handler.register_handler(messages::HELLO_MESSAGE, handler_hello);
        msg_handler.register_handler(messages::PUT_MESSAGE, handler_put);
    }

    results::Result should_be_proccessed(const ip::IPAddress ip_address) {
        auto it = players.find(ip_address);
        if (it == players.end()) {
            return results::Result::Failure(
                "Player not found in the list of players.");
        }

        return it->second.has_sent_hello
                   ? results::Result::Success()
                   : results::Result::Failure("Player has not sent hello yet.");
    }

    void mark_message_from(const ip::IPAddress client) {
        auto it = players.find(client);
        if (it == players.end()) {
            throw std::runtime_error(
                "Player not found in the list of players.");
        }

        if (it->second.has_sent_hello) {
            return;
        }

        logger.log_info(it->second.id,
                        " has sent sth, which is not a valid HELLO "
                        "message. Disconnecting.");
        message_sender->disconnect(client);
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
            uint64_t timeout = UINT64_MAX;
            if (players_before_hello > 0) {
                for (const auto& pair : players) {
                    if (!pair.second.has_sent_hello) {
                        auto now = std::chrono::system_clock::now();
                        auto remaining = get_diff_time(
                            now, pair.second.connect_time +
                                     std::chrono::milliseconds(
                                         MAX_DELAY_BETWEEN_CONNECT_AND_HELLO));
                        timeout = std::min(timeout, remaining);
                    }
                }
            }

            int result = poller.poll_sockets(timeout);
            if (result < 0) {
                // TODO: handle error
                logger.log_error("Poll error: " + std::string(strerror(errno)));
                break;
            }

            if (players_before_hello > 0) {
                close_timeout_newbies();
            }

            poller.handle();

            dispatch_coeffs();
        }
    }

    void close_timeout_newbies() {
        auto now = std::chrono::system_clock::now();
        std::vector<ip::IPAddress> to_disconnect;

        for (const auto& pair : players) {
            if (!pair.second.has_sent_hello &&
                (pair.second.connect_time +
                 std::chrono::milliseconds(
                     MAX_DELAY_BETWEEN_CONNECT_AND_HELLO)) < now) {
                to_disconnect.push_back(pair.first);
            }
        }

        for (const auto& ip : to_disconnect) {
            logger.log_info(
                UNKNOWN_ID, "(", ip, ") didn't send HELLO in required time (",
                MAX_DELAY_BETWEEN_CONNECT_AND_HELLO, " ms). Disconnecting.");
            message_sender->disconnect(ip);
        }
    }

    void dispatch_coeffs() {
        while (!read_coeffs.empty() && !waiting_for_coeffs.empty()) {
            auto [player, timepoint] = waiting_for_coeffs.front();
            auto msg = read_coeffs.front();
            waiting_for_coeffs.pop_front();
            read_coeffs.pop();

            auto it = players.find(player);
            if (it == players.end()) {
                throw std::runtime_error(
                    "Player not found in the list of players.");
            }
            logger.log_info(it->second.id, " gets coefficients: ", msg, ".");

            uint64_t delay = 0;
            auto now = std::chrono::system_clock::now();
            if (timepoint > now) {
                auto remaining = timepoint - std::chrono::system_clock::now();
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
        logger.log_info("New client ", ip_address, ".");

        players[ip_address] = {
            .id = UNKNOWN_ID,
            .connect_time = std::chrono::system_clock::now(),
            .has_sent_hello = false,
            .put_responses_to_be_sent = 0,
            .penalty = 0,
            .points = std::vector<messages::rational_t>(k, POINT_DEFAULT)};

        ++players_before_hello;
    }

    void forget(const ip::IPAddress ip_address) {
        {
            const auto it = players.find(ip_address);
            if (it == players.end()) {
                throw std::runtime_error(
                    "Player not found in the list of players.");
            }

            logger.log_info(it->second.id, " disconnected.");
            players.erase(it);
        }

        auto it = std::remove_if(
            waiting_for_coeffs.begin(), waiting_for_coeffs.end(),
            [&](const auto& pair) { return pair.first == ip_address; });
        if (it == waiting_for_coeffs.end()) {
            return;
        }

        --players_before_hello;
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

        mark_message_from(sender);
    }

    results::Result mark_hello(const ip::IPAddress sender,
                               const std::string player_id) {
        auto& player = players[sender];
        if (player.has_sent_hello) {
            return results::Result::Failure("Already sent hello.");
        }

        --players_before_hello;
        player.id = player_id;
        player.has_sent_hello = true;
        logger.log_info(sender, " is now known as ", player_id, ".");

        waiting_for_coeffs.push_back(std::make_pair(
            sender, std::chrono::system_clock::now() +
                        std::chrono::milliseconds(DELAY_BEFORE_COEFF)));

        return results::Result::Success();
    }

    results::Result process_put(const ip::IPAddress sender,
                                const uint16_t point,
                                const messages::rational_t value) {
        auto& player = players[sender];
        if (!player.has_sent_hello) {
            return results::Result::Failure("Player has not sent hello yet.");
        }

        if (player.put_responses_to_be_sent > 0) {
            messages::penalty_message_t penalty_message;
            penalty_message.point = point;
            penalty_message.value = value;
            message_sender->send_message(
                sender,
                messages::serialize_message<messages::penalty_message_t>(
                    penalty_message),
                [&, ip = sender, id = player.id](const std::string msg) {
                    logger.log_info(id, " was sent a penalty message: ", msg);
                    this->mark_put_response_sent(ip);
                },
                PENALTY_DELAY);
            player.penalty += PENALTY_ON_PUT_BEFORE_RESPONSE;
            ++player.put_responses_to_be_sent;
            return results::Result::Failure(
                "Player hasn't been sent all PUT responses yet.");
        }

        if (point > k || value < MIN_VALUE || value > MAX_VALUE) {
            messages::bad_put_message_t bad_put_message;
            bad_put_message.point = point;
            bad_put_message.value = value;
            message_sender->send_message(
                sender,
                messages::serialize_message<messages::bad_put_message_t>(
                    bad_put_message),
                [&, ip = sender, id = player.id](const std::string msg) {
                    logger.log_info(id, " was sent a bad_put: ", msg);
                    this->mark_put_response_sent(ip);
                },
                DELAY_AFTER_BAD_PUT);
            player.penalty += PENALTY_ON_BAD_PUT;
            ++player.put_responses_to_be_sent;
            return results::Result::Failure("Point is out of range.");
        }

        player.points[point] += value;

        messages::state_message_t state_message;
        state_message.coeffs = player.points;

        uint64_t delay = 1000 * (count_small_letters(player.id));
        message_sender->send_message(
            sender,
            messages::serialize_message<messages::state_message_t>(
                state_message),
            [&, ip = sender, id = player.id](const std::string msg) {
                logger.log_info(id, " was sent a state message: ", msg);
                this->mark_put_response_sent(ip);
            },
            delay);
        player.put_responses_to_be_sent++;

        return results::Result::Success();
    }

    void mark_put_response_sent(const ip::IPAddress sender) {
        auto it = players.find(sender);
        if (it == players.end()) {
            logger.log_warning("Player not found in the list of players.");
            return;
        }

        if (it->second.put_responses_to_be_sent == 0) {
            throw std::runtime_error("Player has no PUT responses to be sent.");
        }

        --it->second.put_responses_to_be_sent;
    }

  private:
    struct player {
        std::string id;
        std::chrono::time_point<std::chrono::system_clock> connect_time;
        bool has_sent_hello;
        uint64_t put_responses_to_be_sent;
        uint64_t penalty;
        std::vector<messages::rational_t> points;
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

    uint16_t k;
    uint8_t n;
    uint32_t m;

    uint64_t players_before_hello = 0;

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

results::Result handler_put(const ip::IPAddress sender, network::MessageSender&,
                            Server& state, logging::Logger& logger,
                            const std::string& message) {
    logger.log_debug("Handling PUT message: " + message);

    // TODO: disconnect on wrong message if first
    messages::put_message_t put_message =
        messages::deserialize_message<messages::put_message_t>(message);
    logger.log_debug("PUT received: ", put_message);

    return state.process_put(sender, put_message.point, put_message.value);
}

} // namespace server

#endif
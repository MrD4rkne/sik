#include "client.h"
#include "fd.h"
#include "messages.h"
#include "network.h"
#include <functional>

namespace client {

static const auto EMPTY = [](const std::string&) {};

void client_state::mark_wrong_message() {
    if (current_state != state::WAITING_FOR_FIRST_MESSAGE) {
        return;
    }

    current_state = state::WRONG_FIRST_MESSAGE;
}

void client_state::try_send_put(const ip::IPAddress ip,
                                messages::MessageSender& messages) {
    if (current_state != state::RUNNING) {
        return;
    }

    if (!strat->has_put_pending()) {
        return;
    }

    auto put = strat->get_put_pending();
    messages::put_message_t put_message;
    put_message.point = put.first;
    put_message.value = put.second;

    logger.log_info("Putting ", put_message.value, " in ", put_message.point,
                    ".");

    messages.send_message_serialized(
        ip, put_message,
        [&, point = put.first, value = put.second](const std::string&) {
            strat->mark_put_sent(point, value);
        });
}

results::Result client_state::mark_coeffs_received(
    const std::vector<types::rational_t>& received_coeffs) {
    if (current_state != state::WAITING_FOR_FIRST_MESSAGE) {
        return results::Result::Failure(
            "Wrong state for coeffs: " +
            std::to_string(static_cast<int>(current_state)));
    }

    current_state = state::RUNNING;
    strat->add_coeffs(received_coeffs);
    return results::Result::Success();
}

client_state::state client_state::get_state() const {
    return current_state;
}

bool client_state::should_be_running() const {
    constexpr state not_running_states[] = {state::WRONG_FIRST_MESSAGE,
                                            state::ERROR, state::STOPPED};

    return std::find(std::begin(not_running_states),
                     std::end(not_running_states),
                     current_state) == std::end(not_running_states);
}

bool client_state::should_exit_with_error() const {
    constexpr state error_states[] = {state::WRONG_FIRST_MESSAGE, state::ERROR};
    return std::find(std::begin(error_states), std::end(error_states),
                     current_state) != std::end(error_states);
}

results::Result
client_state::mark_state(const std::vector<types::rational_t>& current_points) {
    if (current_state != state::RUNNING) {
        return results::Result::Failure(
            "Wrong state for coeffs: " +
            std::to_string(static_cast<int>(current_state)));
    }

    return strat->add_state_response(current_points);
}

results::Result client_state::mark_bad_put(const types::k_t point,
                                           const types::rational_t value) {
    if (current_state != state::RUNNING) {
        return results::Result::Failure(
            "Wrong state for bad put: " +
            std::to_string(static_cast<int>(current_state)));
    }

    return strat->add_bad_put_response(point, value);
}

results::Result client_state::mark_penalty(const types::k_t point,
                                           const types::rational_t value) {
    if (current_state != state::RUNNING) {
        return results::Result::Failure(
            "Wrong state for penalty: " +
            std::to_string(static_cast<int>(current_state)));
    }

    return strat->add_penalty_response(point, value);
}

results::Result client_state::mark_scoring(
    const std::vector<std::pair<std::string, types::rational_t>>& scores) {
    if (current_state != state::RUNNING) {
        return results::Result::Failure(
            "Wrong state for scoring: " +
            std::to_string(static_cast<int>(current_state)));
    }

    logger.log_info("Scoring received: ");
    for (const auto& score : scores) {
        logger.log_info("Player: " + score.first +
                        ", Score: " + std::to_string(score.second));
    }

    current_state = state::STOPPED;

    return results::Result::Success();
}

void client::handle_message(const std::string message,
                            messages::MessageSender& message_sender) {
    bool was_ok = true;
    try {
        logger.log_info("Server sent message: " + message);

        std::string type = messages::get_type(message);
        logging::Logger local_logger;
        auto result = message_handler.handle(type, ip_address, message_sender,
                                             state, local_logger, message);
        if (!result.is_success()) {
            was_ok = false;
            logger.log_debug("Wrong message: " + result.get_error_message());
        }

    } catch (const std::invalid_argument& e) {
        logger.log_debug("Invalid message: " + std::string(e.what()));
        was_ok = false;
    }

    if (was_ok) {
        logger.log_debug("Message handled successfully");
    } else {
        logger.log_bad_message(ip_address, "UNKNOWN", message);
        state.mark_wrong_message();
    }
}

void client::run() {
    logger.log_debug("Client started with player ID: " + player_id);

    std::shared_ptr<network::SingleSocketHandler> server_ptr = nullptr;

    auto on_disconnect = [&](const ip::IPAddress ip) {
        if (ip != ip_address) {
            throw std::runtime_error("Disconnected from unexpected IP: " +
                                     ip.to_string());
        }

        int fd = server_ptr->get_socket_fd();
        poller->remove_socket(fd);
        throw std::runtime_error("unexpected server disconnect");
    };

    auto on_message_received = [&](const ip::IPAddress ip,
                                   const std::string& msg) {
        if (ip != ip_address) {
            throw std::runtime_error("Received message from unexpected IP: " +
                                     ip.to_string());
        }
        handle_message(msg, *server_ptr);
    };

    server_ptr = std::make_shared<network::SingleSocketHandler>(
        logger, ip_address, on_message_received, on_disconnect);

    logger.log_info("Connecting to server at " + ip_address.to_string());
    server_ptr->connect_to(ip_address);
    logger.log_info("Connected to server at " + ip_address.to_string());

    poller->add_socket(server_ptr->get_socket_fd(), server_ptr);

    logger.log_info("Sending HELLO message to server.");
    messages::hello_message_t hello_message{.player_id = player_id};
    server_ptr->send_message_serialized(ip_address, hello_message, EMPTY);

    while (state.should_be_running()) {
        int result = poller->poll_sockets();
        if (result < 0) {
            logger.log_error("Poll error: " + std::string(strerror(errno)));
            if (errno == EINTR) {
                logger.log_info("Poll interrupted, continuing...");
                continue;
            }

            break;
        }

        while (state.should_be_running() && poller->has_ready_socket()) {
            poller->process_next();
        }

        poller->clear_round();

        if (state.should_be_running()) {
            state.try_send_put(ip_address, *server_ptr);
        }
    }

    if (this->state.should_exit_with_error()) {
        throw std::runtime_error("Client exiting with error.");
    } else {
        return;
    }
}

results::Result handler_coeff(const ip::IPAddress, messages::MessageSender&,
                              client_state& state, logging::Logger& logger,
                              const std::string& message) {
    logger.log_debug("Handling COEFF message: " + message);

    messages::coeff_message_t coeff_message =
        messages::deserialize_message<messages::coeff_message_t>(message);
    logger.log_info("Coefficients received: ", coeff_message);

    return state.mark_coeffs_received(coeff_message.coeffs);
}

results::Result handler_state(const ip::IPAddress, messages::MessageSender&,
                              client_state& state, logging::Logger& logger,
                              const std::string& message) {
    logger.log_debug("Handling STATE message: " + message);

    messages::state_message_t state_message =
        messages::deserialize_message<messages::state_message_t>(message);
    logger.log_info("State message received: ", state_message);

    return state.mark_state(state_message.coeffs);
}

results::Result handler_bad_put(const ip::IPAddress, messages::MessageSender&,
                                client_state& state, logging::Logger& logger,
                                const std::string& message) {
    logger.log_debug("Handling BAD_PUT message: " + message);

    messages::bad_put_message_t bad_put_message =
        messages::deserialize_message<messages::bad_put_message_t>(message);
    logger.log_debug("Bad PUT message received: ", bad_put_message);

    if(bad_put_message.point < 0 || 
       bad_put_message.value < messages::MIN_OFFSET ||
       bad_put_message.value > messages::MAX_OFFSET) {
        return results::Result::Failure("Invalid BAD_PUT message values.");
    }

    return state.mark_bad_put(bad_put_message.point, bad_put_message.value);
}

results::Result handler_penalty(const ip::IPAddress, messages::MessageSender&,
                                client_state& state, logging::Logger& logger,
                                const std::string& message) {
    logger.log_debug("Handling PENALTY message: " + message);

    messages::penalty_message_t penalty_message =
        messages::deserialize_message<messages::penalty_message_t>(message);
    logger.log_info("Penalty message received: ", penalty_message);

    if(penalty_message.point < 0 || 
       penalty_message.value < messages::MIN_OFFSET ||
       penalty_message.value > messages::MAX_OFFSET) {
        return results::Result::Failure("Invalid PENALTY message values.");
    }
    return state.mark_penalty(penalty_message.point, penalty_message.value);
}

results::Result handler_scoring(const ip::IPAddress, messages::MessageSender&,
                                client_state& state, logging::Logger& logger,
                                const std::string& message) {
    logger.log_debug("Handling SCORING message: " + message);

    messages::scoring_message_t scoring_message =
        messages::deserialize_message<messages::scoring_message_t>(message);
    logger.log_info("Scoring message received: ", scoring_message);

    return state.mark_scoring(scoring_message.scores);
}

void client::init() {
    message_handler.register_handler(messages::COEFF_MESSAGE, handler_coeff);
    message_handler.register_handler(messages::STATE_MESSAGE, handler_state);
    message_handler.register_handler(messages::BAD_PUT_MESSAGE,
                                     handler_bad_put);
    message_handler.register_handler(messages::PENALTY_MESSAGE,
                                     handler_penalty);
    message_handler.register_handler(messages::SCORING_MESSAGE,
                                     handler_scoring);
}

} // namespace client
#include "client.h"
#include "fd.h"
#include "messages.h"
#include <functional>

namespace client {

static const auto EMPTY = [](const std::string&) {};

void client::handle_message(const std::string message,
                            messages::MessageSender& message_sender) {
    bool was_ok = true;
    try {
        logger.log_info("Server sent message: " + message);

        std::string type = messages::get_type(message);
        logging::Logger local_logger =
            logging::LoggerFactory::create_logger(ip_address);
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
        int fd = server_ptr->get_socket_fd();
        poller->remove_socket(fd);
        throw std::runtime_error("Server disconnected.");
    };

    auto on_message_received = [&](const ip::IPAddress ip,
                                   const std::string& msg) {
        handle_message(msg, *server_ptr);
    };

    server_ptr = std::make_shared<network::SingleSocketHandler>(
        logger, ip_address, on_message_received, on_disconnect);
    server_ptr->connect_to(ip_address);

    poller->add_socket(server_ptr->get_socket_fd(), server_ptr);

    messages::hello_message_t hello_message{.player_id = player_id};
    server_ptr->send_message_serialized(ip_address, hello_message, EMPTY);

    while (state.should_be_running()) {
        int result = poller->poll_sockets();
        if (result < 0) {
            // TODO: handle error
            logger.log_error("Poll error: " + std::string(strerror(errno)));
            break;
        }

        while (state.should_be_running() && poller->has_ready_socket()) {
            poller->process_next();
        }

        poller->clear_round();

        if(state.should_be_running()){
            state.try_send_put(ip_address, *server_ptr);
        }
    }

    if (this->state.should_exit_with_error()) {
        throw std::runtime_error("Client exiting with error.");
    } else {
        return;
    }
}

void client::set_error() {
    logger.log_debug("Client error state set");
    // TODO: handle error
}

results::Result handler_coeff(const ip::IPAddress, messages::MessageSender&,
                              client_state& state, logging::Logger& logger,
                              const std::string& message) {
    logger.log_debug("Handling COEFF message: " + message);

    messages::coeff_message_t coeff_message =
        messages::deserialize_message<messages::coeff_message_t>(message);
    logger.log_debug("Coefficients received: ", coeff_message);

    return state.mark_coeffs_received(coeff_message.coeffs);
}

results::Result handler_state(const ip::IPAddress, messages::MessageSender&,
                              client_state& state, logging::Logger& logger,
                              const std::string& message) {
    logger.log_debug("Handling STATE message: " + message);

    messages::state_message_t state_message =
        messages::deserialize_message<messages::state_message_t>(message);
    logger.log_debug("State message received: ", state_message);

    return state.mark_state(state_message.coeffs);
}

results::Result handler_bad_put(const ip::IPAddress, messages::MessageSender&,
                              client_state& state, logging::Logger& logger,
                              const std::string& message) {
    logger.log_debug("Handling BAD_PUT message: " + message);

    messages::bad_put_message_t bad_put_message =
        messages::deserialize_message<messages::bad_put_message_t>(message);
    logger.log_debug("Bad PUT message received: ", bad_put_message);

    return state.mark_bad_put(bad_put_message.point,
                             bad_put_message.value);
}

results::Result handler_penalty(const ip::IPAddress, messages::MessageSender&,
                              client_state& state, logging::Logger& logger,
                              const std::string& message) {
    logger.log_debug("Handling PENALTY message: " + message);

    messages::penalty_message_t penalty_message =
        messages::deserialize_message<messages::penalty_message_t>(message);
    logger.log_debug("Penalty message received: ", penalty_message);
    return state.mark_penalty(penalty_message.point,
                             penalty_message.value);
}

results::Result handler_scoring(const ip::IPAddress, messages::MessageSender&,
                              client_state& state, logging::Logger& logger,
                              const std::string& message) {
    logger.log_debug("Handling SCORING message: " + message);

    messages::scoring_message_t scoring_message =
        messages::deserialize_message<messages::scoring_message_t>(message);
    logger.log_debug("Scoring message received: ", scoring_message);

    return state.mark_scoring(scoring_message.scores);
}

void client::init() {
    message_handler.register_handler(messages::COEFF_MESSAGE, handler_coeff);
    message_handler.register_handler(messages::STATE_MESSAGE, handler_state);
    message_handler.register_handler(messages::BAD_PUT_MESSAGE, handler_bad_put);
    message_handler.register_handler(messages::PENALTY_MESSAGE, handler_penalty);
    message_handler.register_handler(messages::SCORING_MESSAGE, handler_scoring);
}

} // namespace client
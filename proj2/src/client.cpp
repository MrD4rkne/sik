#include "client.h"
#include "messages.h"
#include <functional>

namespace client {

static const auto EMPTY = [](const std::string&) {};

void client::handle_message(const std::string message,
                            network::MessageSender& message_sender) {
    bool was_ok = true;
    try {
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
        logger.log_bad_message(ip_address, player_id, message);
        state.mark_wrong_message();
    }
}

void client::run() {
    logger.log_debug("Client started with player ID: " + player_id);

    network::Server server(
        [this](ip::IPAddress ip_address) {
            logger.log_debug("Connected to server: " + ip_address.to_string());
        },
        [this](ip::IPAddress ip_address) {
            logger.log_debug("Disconnected from server: " +
                             ip_address.to_string());
        });

    network::MessageSender& message_sender = server;

    server.connect_to(ip_address);

    messages::hello_message_t hello_message{.player_id = player_id};
    message_sender.send_message(ip_address, hello_message, EMPTY);

    while (this->state.should_be_running()) {
        server.process();

        if (server.has_messages()) {
            auto message = server.get_message();
            handle_message(message.second, message_sender);
        }

        if (server.get_num_connections() == 0) {
            // TODO: handle disconnection
            throw std::runtime_error("Server disconnected");
        }
    }

    if (this->state.should_exit_with_error()) {
        throw std::runtime_error("Client exiting with error.");
    } else {
        return;
    }
}

results::Result handler_coeff(const ip::IPAddress&, network::MessageSender&,
                              client_state& state, logging::Logger& logger,
                              const std::string& message) {
    logger.log_debug("Handling COEFF message: " + message);

    messages::coeff_message_t coeff_message =
        messages::deserialize_message<messages::coeff_message_t>(message);
    logger.log_debug("Coefficients received: ", coeff_message);

    return state.mark_coeffs_received(coeff_message.coeffs);
}

void client::init() {
    message_handler.register_handler(messages::COEFF_MESSAGE, handler_coeff);
}

} // namespace client
#include "client.h"
#include "fd.h"
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

    std::shared_ptr<network::SingleSocketHandler> server_ptr = nullptr;
    fd::FDPoller poller;

    auto on_disconnect = [&](const ip::IPAddress ip) {
        int fd = server_ptr->get_socket_fd();
        poller.remove_socket(fd);
        throw std::runtime_error("Server disconnected.");
    };

    auto on_message_received = [&](const ip::IPAddress ip,
                                   const std::string& msg) {
        handle_message(msg, *server_ptr);
    };

    server_ptr = std::make_shared<network::SingleSocketHandler>(
        logger, ip_address, on_message_received, on_disconnect);
    server_ptr->connect_to(ip_address);

    poller.add_socket(server_ptr->get_socket_fd(), server_ptr);

    messages::hello_message_t hello_message{.player_id = player_id};
    server_ptr->send_message_serialized(ip_address, hello_message, EMPTY);

    while (state.should_be_running()) {
        int result = poller.poll_sockets();
        if (result < 0) {
            // TODO: handle error
            logger.log_error("Poll error: " + std::string(strerror(errno)));
            break;
        }

        poller.handle();
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

results::Result handler_coeff(const ip::IPAddress, network::MessageSender&,
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
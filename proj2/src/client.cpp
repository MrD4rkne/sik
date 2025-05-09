#include "client.h"
#include "messages.h"
#include <functional>

namespace client{

void client::handle_message(const std::string message){
    try {
        std::string type = messages::get_type(message);
        logging::Logger local_logger = logging::LoggerFactory::create_logger(ip_address);
        message_handler.handle(type, ip_address, message_sender, state, local_logger, message);
    } catch (const std::invalid_argument& e) {
        logger.log_bad_message(ip_address, player_id, message);
        state.mark_wrong_message();
    }
}

void client::run() {
    logger.log_debug("Client started with player ID: " + player_id);

    messages::hello_message_t hello_message{
        .player_id = player_id
    };
    message_sender.send_message(hello_message);

    while (this->state.should_be_running()) {
        std::string message = message_receiver.receive_message();
        handle_message(message);
    }

    if (this->state.should_exit_with_error()) {
        throw std::runtime_error("Client exiting with error.");
    } else {
        return;
    }
}

void handler_coeff(const ip::IPAddress& ip_address, network::MessageSender& message_sender, client_state& state, logging::Logger& logger, const std::string& message) {
    logger.log_debug("Handling COEFF message: " + message);
    messages::coeff_message_t coeff_message = messages::deserialize_message<messages::coeff_message_t>(message);
    state.mark_coeffs_received(coeff_message.coeffs);
    logger.log_debug("Coefficients received: " + coeff_message.to_string());
}

void client::init(){
    message_handler.register_handler(messages::COEFF_MESSAGE, handler_coeff);
}

} // namespace client
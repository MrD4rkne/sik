#ifndef CLIENT_H
#define CLIENT_H

#include <memory>
#include <algorithm>
#include "ip.h"
#include "network.h"
#include "logging.h"
#include "handlers.h"

namespace client{

class strategy{
    // Define the strategy class here
};

class client_state{
    enum class state{
        WAITING_FOR_FIRST_MESSAGE,
        WRONG_FIRST_MESSAGE,
        RUNNING,
        WAITING_FOR_PUT_RESPONSE,
        ERROR,
        STOPPED
    };

    public:
    client_state() : current_state(state::WAITING_FOR_FIRST_MESSAGE) {}

    void mark_wrong_message(){
        if (current_state != state::WAITING_FOR_FIRST_MESSAGE) {
            return;
        }

        current_state = state::WRONG_FIRST_MESSAGE;
    }

    state get_state() const {
        return current_state;
    }

    bool should_be_running() const {
        constexpr state not_running_states[] = {
            state::WRONG_FIRST_MESSAGE,
            state::ERROR,
            state::STOPPED
        };

        return std::find(std::begin(not_running_states), std::end(not_running_states), current_state) == std::end(not_running_states);
    }

    bool should_exit_with_error() const {
        constexpr state error_states[] = {
            state::WRONG_FIRST_MESSAGE,
            state::ERROR
        };
        return std::find(std::begin(error_states), std::end(error_states), current_state) == std::end(error_states);
    }

    private:
    state current_state;
};

class client{
    public:
    client(const std::string& player_id, const ip::IPAddress& server_address,
           const strategy& strat, network::MessageSender& message_sender,
           network::MessageReceiver& message_receiver,
            logging::Logger& logger)
        : player_id(player_id), strat(std::make_unique<strategy>(strat)),
        message_sender(message_sender), message_receiver(message_receiver), 
        logger(logger), ip_address(server_address),
        state{}, message_handler{} {}

    void init();

    void run();

    private:
        void handle_message(const std::string message);

    std::string player_id;
    std::unique_ptr<strategy> strat;
    network::MessageSender& message_sender;
    network::MessageReceiver& message_receiver;
    logging::Logger& logger;
    ip::IPAddress ip_address;
    client_state state;
    handlers::message_handler<client_state> message_handler;
};

} // namespace client

#endif
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
          logger(logger),
          ip_address(server_address), state{}, message_handler{} {
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
} // namespace client

#endif
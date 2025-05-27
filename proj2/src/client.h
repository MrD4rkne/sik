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
  public:
    strategy() {
    }

    virtual void add_coeffs(const std::vector<messages::rational_t>& new_coeffs) = 0;

    virtual bool has_put_pending() = 0;

    virtual std::pair<messages::k_t, messages::offset_t>
    get_put_pending() = 0;

    virtual results::Result add_bad_put_response(
        const messages::k_t point, const messages::offset_t value) = 0;

    virtual results::Result add_penalty_response(
        const messages::k_t point, const messages::offset_t value) = 0;

    virtual results::Result add_state_response(
        const std::vector<messages::rational_t>& points) = 0;

    virtual ~strategy() = default;
};

class client_state {
    enum class state {
        WAITING_FOR_FIRST_MESSAGE,
        WRONG_FIRST_MESSAGE,
        RUNNING,
        ERROR,
        STOPPED
    };

  public:
    client_state(std::shared_ptr<strategy> strat)
        : current_state(state::WAITING_FOR_FIRST_MESSAGE), coeffs(nullptr),
          strat(strat) {
    }

    void mark_wrong_message() {
        if (current_state != state::WAITING_FOR_FIRST_MESSAGE) {
            return;
        }

        current_state = state::WRONG_FIRST_MESSAGE;
    }

    void try_send_put(const ip::IPAddress ip,messages::MessageSender& messages) {
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

        messages.send_message_serialized(ip,put_message, [](const std::string&){});
    }

    results::Result
    mark_coeffs_received(const std::vector<messages::rational_t>& coeffs) {
        if (current_state != state::WAITING_FOR_FIRST_MESSAGE) {
            return results::Result::Failure(
                "Wrong state for coeffs: " +
                std::to_string(static_cast<int>(current_state)));
        }

        current_state = state::RUNNING;
        strat->add_coeffs(coeffs);
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
                         current_state) != std::end(error_states);
    }

    results::Result mark_state(const std::vector<messages::rational_t>& coeffs) {
        if (current_state != state::RUNNING) {
            return results::Result::Failure(
                "Wrong state for coeffs: " +
                std::to_string(static_cast<int>(current_state)));
                // TODO: what if we buffe rmultiple messages
        }

        return strat->add_state_response(coeffs);
    }

    results::Result mark_bad_put(const messages::k_t point,
                                 const messages::offset_t value) {
        if (current_state != state::RUNNING) {
            return results::Result::Failure(
                "Wrong state for bad put: " +
                std::to_string(static_cast<int>(current_state)));
        }

        return strat->add_bad_put_response(point, value);
    }

    results::Result mark_penalty(const messages::k_t point,
                                 const messages::offset_t value) {
        if (current_state != state::RUNNING) {
            return results::Result::Failure(
                "Wrong state for penalty: " +
                std::to_string(static_cast<int>(current_state)));
        }

        return strat->add_penalty_response(point, value);
    }

    results::Result mark_scoring(const std::vector<std::pair<std::string,messages::rational_t>>& scores) {
        if (current_state != state::RUNNING) {
            return results::Result::Failure(
                "Wrong state for scoring: " +
                std::to_string(static_cast<int>(current_state)));
        }

        current_state = state::STOPPED;
        return results::Result::Success();
    }

  private:
    state current_state;
    std::unique_ptr<std::vector<messages::rational_t>> coeffs;
    std::shared_ptr<strategy> strat;
};

class client {
  public:
    client(const std::string& player_id, const ip::IPAddress server_address,
           logging::Logger& logger, std::shared_ptr<strategy> strat,
            std::shared_ptr<fd::FDPoller> poller)
        : player_id(player_id), logger(logger), ip_address(server_address),
          state{strat}, message_handler{}, poller(poller) {
    }

    void init();

    void run();

    void set_error();

    void handle_message(const std::string message,
                        messages::MessageSender& message_sender);

  private:
    std::string player_id;
    logging::Logger& logger;
    ip::IPAddress ip_address;
    client_state state;
    handlers::message_handler<client_state> message_handler;
    std::shared_ptr<fd::FDPoller> poller;
};
} // namespace client

#endif
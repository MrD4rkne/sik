#ifndef CLIENT_H
#define CLIENT_H

#include "fd.h"
#include "handlers.h"
#include "ip.h"
#include "logging.h"
#include "messages.h"
#include "results.h"
#include <memory>

namespace client {

class strategy {
  public:
    strategy() {
    }

    virtual void
    add_coeffs(const std::vector<types::rational_t>& new_coeffs) = 0;

    virtual bool has_put_pending() = 0;

    virtual void mark_put_sent(const size_t point,
                               const types::rational_t value) = 0;

    virtual std::pair<types::k_t, types::rational_t> get_put_pending() = 0;

    virtual results::Result
    add_bad_put_response(const size_t point, const types::rational_t value) = 0;

    virtual results::Result
    add_penalty_response(const size_t point, const types::rational_t value) = 0;

    virtual results::Result
    add_state_response(const std::vector<types::rational_t>& points) = 0;

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
          strat(strat), logger() {
    }

    void mark_wrong_message();

    bool has_pending_put();

    void try_send_put(const ip::IPAddress ip,
                      messages::MessageSender& messages);

    results::Result
    mark_coeffs_received(const std::vector<types::rational_t>& coeffs);

    state get_state() const;

    bool should_be_running() const;

    bool should_exit_with_error() const;

    results::Result mark_state(const std::vector<types::rational_t>& coeffs);

    results::Result mark_bad_put(const types::k_t point,
                                 const types::rational_t value);

    results::Result mark_penalty(const types::k_t point,
                                 const types::rational_t value);

    results::Result mark_scoring(
        const std::vector<std::pair<std::string, types::rational_t>>& scores);

  private:
    state current_state;
    std::unique_ptr<std::vector<types::rational_t>> coeffs;
    std::shared_ptr<strategy> strat;
    logging::Logger logger;
};

class client {
  public:
    client(const std::string& player_id, const ip::IPAddress server_address,
           logging::Logger& logger, std::shared_ptr<strategy> strat,
           std::shared_ptr<fd::FDPoller> poller)
        : player_id(player_id), logger(logger),
          ip_address(server_address), state{strat}, message_handler{},
          poller(poller) {
    }

    void init();

    void run();

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
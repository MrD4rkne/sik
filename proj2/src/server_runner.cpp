#include "server_runner.h"
#include "logging.h"
#include "network.h"

namespace server_runner {

static void
handle_message(server::Server& server, const ip::IPAddress sender,
               const std::string message, logging::Logger& logger,
               handlers::message_handler<server::Server>& msg_handler,
               server::PlayersManager& message_sender) {
    if (!server.known_player(sender)) {
        logger.log_debug("Unknown player: ", sender.to_string());
        return;
    }

    auto player_id = server.get_player_id(sender);

    bool was_ok = true;
    try {
        logger.log_info(player_id, " sent: ", message);

        std::string type = messages::get_type(message);
        logging::Logger local_logger;
        auto result = msg_handler.handle(type, sender, message_sender, server,
                                         local_logger, message);
        if (!result.is_success()) {
            was_ok = false;
            logger.log_info(result.get_error_message());
        }

    } catch (const std::invalid_argument& e) {
        logger.log_info("Invalid message: " + std::string(e.what()));
        was_ok = false;
    }

    if (was_ok) {
        logger.log_debug("Message handled successfully");
    } else {
        logger.log_bad_message(sender, player_id, message);
    }

    auto result = server.mark_message_from(sender);
    if (!result.is_success()) {
        logger.log_info(player_id, " has not sent hello yet. Disconnecting.");
        message_sender.disconnect(sender);
    }
}

runner::runner(int socket_fd, const server_args_t& args,
               handlers::message_handler<server::Server>& msg_handler,
               std::shared_ptr<file::COEFFFromFileProvider> coeff_provider) {

    logging::Logger logger;
    logger.log_info("");
    logger.log_info("Creating new game with parameters: k=", args.k,
                    ", n=", (int)args.n, ", m=", args.m,
                    ", file=", args.file_name);

    game = std::make_shared<game_t>();
    game->server =
        std::make_shared<server::Server>(args.k + 1, args.m, coeff_provider);

    auto on_connect = [=](const ip::IPAddress ip) {
        game->server->add_client(ip);
    };
    auto on_disconnect = [&](const ip::IPAddress ip) {
        auto result = game->server->forget(ip);
        if (!result.is_success()) {
            logging::Logger logger;
            logger.log_warning("Failed to forget player. Probably left over "
                               "from previous game: ",
                               result.get_error_message());
        }
    };
    auto on_message = [&, game = game](const ip::IPAddress ip,
                                       const std::string message) {
        logging::Logger logger;
        logger.log_debug("Message from ", ip, ": ", message);
        if (!game->server->is_game_ongoing()) {
            logger.log_info("Game is not ongoing. Message was ignored: ", ip);
        }
        handle_message(*game->server, ip, message, logger, msg_handler,
                       *game->player);
    };

    game->poller = std::make_shared<fd::FDPoller>();
    auto sh = std::make_shared<network::SocketHandler>(
        socket_fd, game->poller, on_message, on_connect, on_disconnect);
    game->player = std::make_shared<Manager>(sh);
    game->poller->add_socket(socket_fd, sh);
    game->poller->add_socket(coeff_provider->get_fd_handler()->get_fd(),
                             coeff_provider->get_fd_handler());
}

static void disconnect_timedout_players(std::shared_ptr<game_t> game,
                                        logging::Logger& logger) {
    auto timedout_newbies = game->server->get_timedout_newbies();
    for (const auto& ip : timedout_newbies) {
        game->player->disconnect(ip);
    }
}

static void dispatch_coeffs(std::shared_ptr<game_t> game,
                            logging::Logger& logger) {
    auto coeff_result = game->server->dispatch_coeffs();
    if (coeff_result.is_success()) {
        auto [ip, delay, coeffs] = coeff_result.get_value();
        auto msg = messages::coeff_message_t{coeffs};
        game->player->send_message_serialized(
            ip, msg,
            [&, ip = ip, msg = msg](const std::string&) {
                logger.log_info(game->server->get_player_id(ip),
                                " was sent coeffs: ", msg);
                game->server->mark_coeff_sent(ip);
            },
            delay);
    }
}

void run_round(std::shared_ptr<game_t> game, logging::Logger& logger) {
    int result = game->poller->poll_sockets(game->server->get_max_timeout());
    if (result < 0) {
        logger.log_error("Error during poll: ", strerror(errno));
        return;
    }

    disconnect_timedout_players(game, logger);

    while (game->server->is_game_ongoing() &&
           game->poller->has_ready_socket()) {
        try {
            game->poller->process_next();
        } catch (const std::exception& e) {
            logger.log_error("Error during processing socket: ", e.what());
        }
    }

    if (game->server->is_game_ongoing()) {
        dispatch_coeffs(game, logger);
    }

    game->poller->clear_round();
}

void runner::run() {
    while (game->server->is_game_ongoing()) {
        run_round(game, logger);
    }

    logger.log_info("Game ended. Disconnecting players.");
    auto results = game->server->get_scorings();
    for (const auto& [player_id, score] : results) {
        logger.log_info(player_id, " score: ", score);
    }

    for (const auto& ip : game->server->get_players()) {
        game->player->send_message_serialized(
            ip, messages::scoring_message{results},
            [&, id = game->server->get_player_id(ip)](const std::string&) {
                logger.log_info(id, " was sent scoring message.");
            });
        game->player->force_flush(ip);
    }
}

static inline uint64_t count_small_letters(const std::string& str) {
    return std::count_if(str.begin(), str.end(),
                         [](unsigned char c) { return std::islower(c); });
}

results::Result handler_hello(const ip::IPAddress sender,
                              messages::MessageSender&, server::Server& state,
                              logging::Logger& logger,
                              const std::string& message) {
    logger.log_debug("Handling HELLO message: " + message);

    messages::hello_message_t hello_message =
        messages::deserialize_message<messages::hello_message_t>(message);

    logger.log_info(sender, " sent HELLO: ", hello_message.player_id);
    return state.mark_hello(sender, hello_message.player_id);
}

results::Result handler_put(const ip::IPAddress sender,
                            messages::MessageSender& msg_sender,
                            server::Server& state, logging::Logger& logger,
                            const std::string& message) {
    logger.log_debug("Handling PUT message: " + message);

    messages::put_message_t put_message =
        messages::deserialize_message<messages::put_message_t>(message);
    logger.log_debug("PUT received: ", put_message);

    auto player_id = state.get_player_id(sender);

    auto can_send_put_result = state.can_send_put(sender);
    if (!can_send_put_result.is_success()) {
        messages::penalty_message_t penalty_message = {
            .point = put_message.point, .value = put_message.value};

        msg_sender.send_message_serialized(
            sender, penalty_message,
            [&, ip = sender, id = player_id,
             msg = penalty_message](const std::string&) {
                logging::Logger local_logger;
                local_logger.log_info(id, " was sent penalty: ", msg);
                state.mark_put_response_sent(ip);
            },
            server::PENALTY_DELAY);
    }

    auto validation_result =
        state.validate_put(sender, put_message.point, put_message.value);
    if (!validation_result.is_success()) {
        messages::bad_put_message_t bad_put_message = {
            .point = put_message.point, .value = put_message.value};
        msg_sender.send_message_serialized(
            sender, bad_put_message,
            [&, ip = sender, id = player_id,
             msg = bad_put_message](const std::string&) {
                logging::Logger local_logger;
                local_logger.log_info(id, " was sent bad PUT: ", msg);
                state.mark_put_response_sent(ip);
            },
            server::DELAY_AFTER_BAD_PUT);
    }

    if (!can_send_put_result.is_success() || !validation_result.is_success()) {
        logger.log_info(player_id, " sent PUT: ", put_message,
                        " but it was not valid.");
        std::string msg;
        if (!can_send_put_result.is_success()) {
            msg += can_send_put_result.get_error_message();
        }

        if (!validation_result.is_success()) {
            if (!msg.empty()) {
                msg += " and ";
            }
            msg += validation_result.get_error_message();
        }

        return results::Result::Failure(msg);
    }

    logger.log_info(player_id, " puts: ", put_message);

    auto player_state =
        state.process_put(sender, put_message.point, put_message.value);

    uint64_t delay = 1000 * count_small_letters(state.get_player_id(sender));
    logger.log_info(player_id,
                    " PUT processed. Delay before response: ", delay);

    messages::state_message_t state_message = {.coeffs = player_state};
    msg_sender.send_message_serialized(
        sender, state_message,
        [&, ip = sender, id = player_id,
         msg = state_message](const std::string&) {
            logging::Logger local_logger;
            local_logger.log_info(id, " was sent state: ", msg);
            state.mark_put_response_sent(ip);
        },
        delay);
    return results::Result::Success();
}

} // namespace server_runner
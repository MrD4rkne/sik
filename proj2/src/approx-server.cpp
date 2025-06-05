#include <iostream>

#include "input.h"
#include "ip.h"
#include "logging.h"
#include "network.h"
#include "server.h"
#include <csignal>
#include <thread>

static inline std::string PORT_NUMBER_ARG = "-p";
static inline std::string K_ARG = "-k";
static inline std::string N_ARG = "-n";
static inline std::string M_ARG = "-m";
static inline std::string FILE_ARG = "-f";

using ip::port_t;

static inline uint64_t count_small_letters(const std::string& str) {
    return std::count_if(str.begin(), str.end(),
                         [](unsigned char c) { return std::islower(c); });
}

int bind_ipv6(port_t port_number, logging::Logger& logger) {
    logger.log_info("Opening IPv6 socket");

    int listen_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        if (errno == EAFNOSUPPORT) {
            throw std::system_error(EAFNOSUPPORT, std::system_category());
        }
        throw std::runtime_error("Failed to create socket: " +
                                 std::string(strerror(errno)));
    }

    struct sockaddr_in6 server_address;
    server_address.sin6_family = AF_INET6; // IPv6
    server_address.sin6_flowinfo = 0;
    server_address.sin6_addr = in6addr_any; // Listening on all interfaces.
    server_address.sin6_port = htons(port_number);
    server_address.sin6_scope_id = 0;

    logger.log_info("Socket created: ", listen_fd);

    logger.log_info("Binding socket to port: ", port_number);
    {
        int result =
            bind(listen_fd, reinterpret_cast<sockaddr*>(&server_address),
                 sizeof(server_address));
        if (result < 0) {
            close(listen_fd);
            throw std::runtime_error("Failed to bind socket: " +
                                     std::string(strerror(errno)));
        }
    }

    logger.log_info("Enabling SO_REUSEADDR.");
    int on = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        close(listen_fd);
        throw std::runtime_error("Failed to set socket options: " +
                                 std::string(strerror(errno)));
    }

    logger.log_info("Disabling IPV6_V6ONLY.");
    int off = 0;
    if (setsockopt(listen_fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off)) <
        0) {
        if (errno != EINVAL) { // Ignore if not supported
            close(listen_fd);
            throw std::runtime_error("Failed to set IPV6_V6ONLY option: " +
                                     std::string(strerror(errno)));
        }

        logger.log_info(
            "IPV6_V6ONLY option not supported, continuing without it.");
    }

    logger.log_info("Setting socket to listen");
    {
        int result = listen(listen_fd, SOMAXCONN);
        if (result < 0) {
            close(listen_fd);
            throw std::runtime_error("Failed to listen on socket: " +
                                     std::string(strerror(errno)));
        }
    }

    // Get the port number assigned by the kernel
    socklen_t addr_len = sizeof(server_address);
    if (getsockname(listen_fd, reinterpret_cast<sockaddr*>(&server_address),
                    &addr_len) < 0) {
        close(listen_fd);
        throw std::runtime_error("Failed to get socket name");
    }

    port_number = ntohs(server_address.sin6_port);
    logger.log_info("Listening on port: ", port_number);

    return listen_fd;
}

int bind_ipv4(port_t port_number, logging::Logger& logger) {
    logger.log_info("Opening socket");

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;         // IPv4
    server_address.sin_addr.s_addr = INADDR_ANY; // Listening on all interfaces.
    server_address.sin_port = htons(port_number);

    logger.log_info("Socket created: ", listen_fd);

    logger.log_info("Binding socket to port: ", port_number);
    {
        int result =
            bind(listen_fd, reinterpret_cast<sockaddr*>(&server_address),
                 sizeof(server_address));
        if (result < 0) {
            close(listen_fd);
            throw std::runtime_error("Failed to bind socket: " +
                                     std::string(strerror(errno)));
        }
    }

    logger.log_info("Enabling SO_REUSEADDR.");
    int on = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        close(listen_fd);
        throw std::runtime_error("Failed to set socket options: " +
                                 std::string(strerror(errno)));
    }

    logger.log_info("Setting socket to listen");
    {
        int result = listen(listen_fd, SOMAXCONN);
        if (result < 0) {
            close(listen_fd);
            throw std::runtime_error("Failed to listen on socket: " +
                                     std::string(strerror(errno)));
        }
    }

    // Get the port number assigned by the kernel
    socklen_t addr_len = sizeof(server_address);
    if (getsockname(listen_fd, reinterpret_cast<sockaddr*>(&server_address),
                    &addr_len) < 0) {
        close(listen_fd);
        throw std::runtime_error("Failed to get socket name");
    }

    port_number = ntohs(server_address.sin_port);
    logger.log_info("Listening on port: ", port_number);

    return listen_fd;
}

int open_listen(port_t port_number, logging::Logger& logger) {
    try {
        return bind_ipv6(port_number, logger);
    } catch (const std::system_error& e) {
        logger.log_error("Could not create an IPV6 socket");
    }

    return bind_ipv4(port_number, logger);
}

class Manager : public server::PlayersManager {
  public:
    Manager(std::shared_ptr<network::SocketHandler> message_sender)
        : message_sender(message_sender) {
    }

    void send_message(const ip::IPAddress ip, const std::string& message,
                      const std::function<void(const std::string&)>& callback,
                      uint64_t delay) override {
        message_sender->send_message(ip, message, callback, delay);
    }

    void disconnect(const ip::IPAddress ip) override {
        message_sender->disconnect(ip);
    }

    void force_flush(const ip::IPAddress ip) {
        message_sender->force_flush(ip);
    }

  private:
    std::shared_ptr<network::SocketHandler> message_sender;
};

class COEFFFromFileProvider : public server::COEFFProvider {
  public:
    COEFFFromFileProvider(const std::string& file_name)
        : file_handler(
              std::make_shared<file::FileHandler>(file_name, on_new_line)) {
        file_handler->open_file();
    }

    size_t get_available_coeffs_count() const {
        return coeffs.size();
    }

    bool has_coeffs() const {
        return !coeffs.empty();
    }

    void request_coeffs() {
        if (file_handler->is_waiting_for_line()) {
            return;
        }
        file_handler->request_line();
    }

    const std::string& get_coeffs() {
        if (coeffs.empty()) {
            throw std::runtime_error("No coefficients available.");
        }

        return coeffs.front();
    }

    void pop_coeffs() {
        if (coeffs.empty()) {
            throw std::runtime_error("No coefficients available.");
        }
        coeffs.pop_front();
    }

    std::shared_ptr<file::FileHandler> get_fd_handler() {
        return file_handler;
    }

  private:
    std::function<void(const std::string msg)> on_new_line =
        [&](const std::string& line) { coeffs.push_back(line); };

    std::deque<std::string> coeffs;
    std::shared_ptr<file::FileHandler> file_handler;
};

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

struct game {
    std::shared_ptr<server::Server> server;
    std::shared_ptr<fd::FDPoller> poller;
    std::shared_ptr<Manager> player;
};

struct program_args {
    port_t port_number;
    uint16_t k;
    uint8_t n;
    uint32_t m;
    std::string file_name;
};

static std::shared_ptr<game>
init(int socket_fd, const program_args& args,
     handlers::message_handler<server::Server>& msg_handler,
     std::shared_ptr<COEFFFromFileProvider> coeff_provider) {
    auto game = std::make_shared<struct game>();

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

    return game;
}

static struct program_args parse_args(int argc, char* argv[]) {
    std::unordered_map<std::string, input::arg_t> allowed_args = {
        input::arg_t::get_arg(PORT_NUMBER_ARG, false, "0"),
        input::arg_t::get_arg(K_ARG, false, "100"),
        input::arg_t::get_arg(N_ARG, false, "4"),
        input::arg_t::get_arg(M_ARG, false, "131"),
        input::arg_t::get_arg(FILE_ARG, true)};

    input::args_parses_t args_map(argc, argv, allowed_args);
    logging::Logger logger;

    program_args args;

    args.port_number = input::parse_input<port_t>(
        PORT_NUMBER_ARG, args_map.get_value(PORT_NUMBER_ARG), 0, 65535);
    logger.log_debug("Port number: ", args.port_number);
    args.k = input::parse_input<uint16_t>(K_ARG, args_map.get_value(K_ARG), 1,
                                          10000);
    logger.log_debug("K: ", args.k);
    args.n =
        input::parse_input<uint8_t>(N_ARG, args_map.get_value(N_ARG), 1, 8);
    logger.log_debug("N: ", (int)args.n);
    args.m = input::parse_input<uint32_t>(M_ARG, args_map.get_value(M_ARG), 1,
                                          12341234);
    logger.log_debug("M: ", args.m);
    args.file_name = args_map.get_value(FILE_ARG);
    logger.log_debug("File: ", args.file_name);

    return args;
}

void run_game(int listen_fd, const program_args& args,
              handlers::message_handler<server::Server>& msg_handler,
              std::shared_ptr<COEFFFromFileProvider> coeff_provider) {
    logging::Logger logger;
    logger.log_info("");
    logger.log_info("Starting new game with parameters: k=", args.k,
                    ", n=", (int)args.n, ", m=", args.m,
                    ", file=", args.file_name);

    auto game = init(listen_fd, args, msg_handler, coeff_provider);

    while (game->server->is_game_ongoing()) {
        int result =
            game->poller->poll_sockets(game->server->get_max_timeout());
        if (result < 0) {
            logger.log_error("Error during poll: ", strerror(errno));
            continue;
        }

        // Disconnect people
        auto timedout_newbies = game->server->get_timedout_newbies();
        for (const auto& ip : timedout_newbies) {
            game->player->disconnect(ip);
        }

        while (game->server->is_game_ongoing() &&
               game->poller->has_ready_socket()) {
            try {
                game->poller->process_next();
            } catch (const std::exception& e) {
                logger.log_error("Error during processing socket: ", e.what());
            }
        }

        if (game->server->is_game_ongoing()) {
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

        game->poller->clear_round();
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

int main(int argc, char* argv[]) {
    logging::Logger logger;
    handlers::message_handler<server::Server> msg_handler;
    std::shared_ptr<COEFFFromFileProvider> coeff_provider;
    program_args args;
    int listen_fd = -1;

    try {
        msg_handler.register_handler(messages::HELLO_MESSAGE, handler_hello);
        msg_handler.register_handler(messages::PUT_MESSAGE, handler_put);

        args = parse_args(argc, argv);
        listen_fd = open_listen(args.port_number, logger);
        coeff_provider =
            std::make_shared<COEFFFromFileProvider>(args.file_name);
    } catch (const std::exception& e) {
        logger.log_error(e.what());

        if (listen_fd >= 0) {
            close(listen_fd);
            listen_fd = -1;
        }

        return 1;
    }

    while (true) {
        try {
            run_game(listen_fd, args, msg_handler, coeff_provider);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        } catch (const std::exception& e) {
            logger.log_error("Error during game: ", e.what());
            close(listen_fd);
            listen_fd = -1;
            return 1;
        }
    }

    return 0;
}
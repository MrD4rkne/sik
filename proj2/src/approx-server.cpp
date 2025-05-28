#include <iostream>

#include "input.h"
#include "ip.h"
#include "logging.h"
#include "network.h"
#include "server.h"

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

int open_listen(port_t port_number, logging::Logger& logger) {
    logger.log_debug("Openning socket");

    int listen_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    struct sockaddr_in6 server_address;
    server_address.sin6_family = AF_INET6; // IPv6
    server_address.sin6_flowinfo = 0;
    server_address.sin6_addr = in6addr_any; // Listening on all interfaces.
    server_address.sin6_port = htons(port_number);
    server_address.sin6_scope_id = 0;

    logger.log_debug("Socket created: ", listen_fd);

    // TODO: handle IPV6

    logger.log_debug("Binding socket to port: ", port_number);
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

    logger.log_debug("Setting socket to listen");
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
    logger.log_debug("Listening on port: ", port_number);

    return listen_fd;
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

    void mark_disconnected(const ip::IPAddress ip) {
        message_sender->mark_disconnected(ip);
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

static std::shared_ptr<server::Server> server_instance = nullptr;

results::Result handler_hello(const ip::IPAddress sender,
                              messages::MessageSender&, server::Server& state,
                              logging::Logger& logger,
                              const std::string& message) {
    logger.log_debug("Handling HELLO message: " + message);

    messages::hello_message_t hello_message =
        messages::deserialize_message<messages::hello_message_t>(message);
    logger.log_debug("HELLO received: ", hello_message);

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

    auto can_send_put_result = state.can_send_put(sender);
    if (!can_send_put_result.is_success()) {
        messages::penalty_message_t penalty_message = {
            .point = put_message.point, .value = put_message.value};

        msg_sender.send_message_serialized(
            sender, penalty_message,
            [&, ip = sender](const std::string&) {
                server_instance->mark_put_response_sent(ip);
            },
            server::PENALTY_DELAY);

        return can_send_put_result;
    }

    auto result =
        state.process_put(sender, put_message.point, put_message.value);
    if (!result.is_success()) {
        messages::bad_put_message_t bad_put_message = {
            .point = put_message.point, .value = put_message.value};
        msg_sender.send_message_serialized(
            sender, bad_put_message,
            [&, ip = sender](const std::string&) {
                server_instance->mark_put_response_sent(ip);
            },
            server::DELAY_AFTER_BAD_PUT);
        return results::Result::Failure("Invalid PUT parameters: " +
                                        result.get_error_message());
    }

    uint64_t delay = 1000 * count_small_letters(state.get_player_id(sender));

    messages::state_message_t state_message = {.coeffs = result.get_value()};
    msg_sender.send_message_serialized(
        sender, state_message,
        [&, ip = sender](const std::string&) {
            server_instance->mark_put_response_sent(ip);
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

    bool was_ok = true;
    try {
        std::string type = messages::get_type(message);
        logging::Logger local_logger =
            logging::LoggerFactory::create_logger(sender);
        auto result = msg_handler.handle(type, sender, message_sender, server,
                                         local_logger, message);
        if (!result.is_success()) {
            was_ok = false;
            logger.log_debug("Wrong message: " + result.get_error_message());
        }

    } catch (const std::invalid_argument& e) {
        logger.log_debug("Invalid message: " + std::string(e.what()));
        was_ok = false;
    }

    auto player_id = server.get_player_id(sender);

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

int main(int argc, char* argv[]) {
    std::unordered_map<std::string, input::arg_t> allowed_args = {
        input::arg_t::get_arg(PORT_NUMBER_ARG, false, "0"),
        input::arg_t::get_arg(K_ARG, false, "100"),
        input::arg_t::get_arg(N_ARG, false, "4"),
        input::arg_t::get_arg(M_ARG, false, "131"),
        input::arg_t::get_arg(FILE_ARG, true)};

    logging::Logger logger;

    std::shared_ptr<fd::FDPoller> poller = nullptr;
    std::shared_ptr<COEFFFromFileProvider> coeff_provider = nullptr;
    std::shared_ptr<Manager> player = nullptr;

    handlers::message_handler<server::Server> msg_handler;
    msg_handler.register_handler(messages::HELLO_MESSAGE, handler_hello);
    msg_handler.register_handler(messages::PUT_MESSAGE, handler_put);

    auto on_connect = [&](const ip::IPAddress ip) {
        server_instance->add_client(ip);
    };
    auto on_disconnect = [&](const ip::IPAddress ip) {
        auto result = server_instance->forget(ip);
        if (!result.is_success()) {
            logger.log_warning("Failed to forget player. Probably left over "
                               "from previous game: ",
                               result.get_error_message());
        }
    };
    auto on_message = [&](const ip::IPAddress ip, const std::string message) {
        logger.log_debug("Message from ", ip, ": ", message);
        if (!server_instance->is_game_ongoing()) {
            logger.log_info("Game is not ongoing. Message was ignored: ", ip);
        }
        handle_message(*server_instance, ip, message, logger, msg_handler,
                       *player);
    };

    int listen_fd = -1;

    std::string file_name;
    port_t port_number = 0;
    uint16_t k = 0;
    uint8_t n = 0;
    uint32_t m = 0;

    try {
        // Parse the command line arguments
        input::args_parses_t args_map(argc, argv, allowed_args);

        port_number = input::parse_input<port_t>(
            PORT_NUMBER_ARG, args_map.get_value(PORT_NUMBER_ARG), 0, 65535);
        logger.log_debug("Port number: ", port_number);
        k = input::parse_input<uint16_t>(K_ARG, args_map.get_value(K_ARG), 1,
                                         100);
        logger.log_debug("K: ", k);
        n = input::parse_input<uint8_t>(N_ARG, args_map.get_value(N_ARG), 1,
                                        255);
        logger.log_debug("N: ", n);
        m = input::parse_input<uint32_t>(M_ARG, args_map.get_value(M_ARG), 1,
                                         1000000);
        logger.log_debug("M: ", m);
        file_name = args_map.get_value(FILE_ARG);
        logger.log_debug("File: ", file_name);

        listen_fd = open_listen(port_number, logger);

        poller = std::make_shared<fd::FDPoller>();
        auto sh = std::make_shared<network::SocketHandler>(
            listen_fd, *poller, on_message, on_connect, on_disconnect);
        player = std::make_shared<Manager>(sh);
        poller->add_socket(listen_fd, sh);

        coeff_provider = std::make_shared<COEFFFromFileProvider>(file_name);
        poller->add_socket(coeff_provider->get_fd_handler()->get_fd(),
                           coeff_provider->get_fd_handler());

    } catch (const std::exception& e) {
        std::cerr << "Error during setup: " << e.what() << std::endl;
        return 1;
    }

    while (true) {
        try {
            server_instance =
                std::make_shared<server::Server>(k, n, m, coeff_provider);

            while (server_instance->is_game_ongoing()) {
                int result =
                    poller->poll_sockets(server_instance->get_max_timeout());
                if (result < 0) {
                    logger.log_error("Error during poll: ", strerror(errno));
                    continue;
                }

                // Disconnect people
                auto timedout_newbies = server_instance->get_timedout_newbies();
                for (const auto& ip : timedout_newbies) {
                    player->disconnect(ip);
                }

                while (server_instance->is_game_ongoing() &&
                       poller->has_ready_socket()) {
                    try {
                        poller->process_next();
                    } catch (const std::exception& e) {
                        logger.log_error("Error during processing socket: ",
                                         e.what());
                    }
                }

                if (server_instance->is_game_ongoing()) {
                    auto coeff_result = server_instance->dispatch_coeffs();
                    if (coeff_result.is_success()) {
                        auto [ip, delay, coeffs] = coeff_result.get_value();
                        player->send_message_serialized(
                            ip, messages::coeff_message_t{coeffs},
                            [&, ip = ip](const std::string&) {
                                server_instance->mark_coeff_sent(ip);
                            },
                            delay);
                    }
                }

                poller->clear_round();
            }

            logger.log_info("Game ended. Disconnecting players.");
            auto results = server_instance->get_scorings();
            for (const auto& [player_id, score] : results) {
                logger.log_info(player_id, " score: ", score);
            }

            for (const auto& ip : server_instance->get_players()) {
                player->send_message_serialized(
                    ip, messages::scoring_message{results},
                    [&, id = server_instance->get_player_id(ip)](
                        const std::string&) {
                        logger.log_info(id, " was sent scoring message.");
                    });
                player->mark_disconnected(ip);
            }
        } catch (const std::exception& e) {
            std::cerr << "Error during setup: " << e.what() << std::endl;
            return 1;
        }
    }

    return 0;
}
#include <iostream>

#include "input.h"
#include "ip.h"
#include "logging.h"
#include "network.h"
#include "server.h"
#include "server_runner.h"
#include <thread>

static inline std::string PORT_NUMBER_ARG = "-p";
static inline std::string K_ARG = "-k";
static inline std::string N_ARG = "-n";
static inline std::string M_ARG = "-m";
static inline std::string FILE_ARG = "-f";

using ip::port_t;

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

static server_runner::server_args_t parse_args(int argc, char* argv[]) {
    std::unordered_map<std::string, input::arg_t> allowed_args = {
        input::arg_t::get_arg(PORT_NUMBER_ARG, false, "0"),
        input::arg_t::get_arg(K_ARG, false, "100"),
        input::arg_t::get_arg(N_ARG, false, "4"),
        input::arg_t::get_arg(M_ARG, false, "131"),
        input::arg_t::get_arg(FILE_ARG, true)};

    input::args_parses_t args_map(argc, argv, allowed_args);
    logging::Logger logger;

    server_runner::server_args_t args;

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

int main(int argc, char* argv[]) {
    logging::Logger logger;
    handlers::message_handler<server::Server> msg_handler;
    std::shared_ptr<file::COEFFFromFileProvider> coeff_provider;
    server_runner::server_args_t args;
    int listen_fd = -1;

    try {
        msg_handler.register_handler(messages::HELLO_MESSAGE,
                                     server_runner::handler_hello);
        msg_handler.register_handler(messages::PUT_MESSAGE,
                                     server_runner::handler_put);

        args = parse_args(argc, argv);
        listen_fd = open_listen(args.port_number, logger);
        coeff_provider =
            std::make_shared<file::COEFFFromFileProvider>(args.file_name);
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
            server_runner::runner game(listen_fd, args, msg_handler,
                                       coeff_provider);
            game.run();
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
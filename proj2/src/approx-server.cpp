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

int open_listen(port_t port_number, logging::Logger& logger) {
    logger.log_debug("Openning socket");

    int listen_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    struct sockaddr_in6 server_address;
    server_address.sin6_family = AF_INET6;  // IPv6
    server_address.sin6_flowinfo = 0;
    server_address.sin6_addr = in6addr_any; // Listening on all interfaces.
    server_address.sin6_port = htons(port_number);
    server_address.sin6_scope_id = 0;

    logger.log_debug("Socket created: ", listen_fd);

    // TODO: handle IPV6

    logger.log_debug("Binding socket to port: ", port_number);
    {
        int result = bind(listen_fd, reinterpret_cast<sockaddr*>(&server_address),
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
    if (getsockname(listen_fd, reinterpret_cast<sockaddr*>(&server_address), &addr_len) < 0) {
        close(listen_fd);
        throw std::runtime_error("Failed to get socket name");
    }

    port_number = ntohs(server_address.sin6_port);
    logger.log_debug("Listening on port: ", port_number);

    return listen_fd;
}

int main(int argc, char* argv[]) {
    std::unordered_map<std::string, input::arg_t> allowed_args = {
        input::arg_t::get_arg(PORT_NUMBER_ARG, false, "0"),
        input::arg_t::get_arg(K_ARG, false, "100"),
        input::arg_t::get_arg(N_ARG, false, "4"),
        input::arg_t::get_arg(M_ARG, false, "131"),
        input::arg_t::get_arg(FILE_ARG, true)};

    logging::Logger logger;

    try {
        // Parse the command line arguments
        input::args_parses_t args_map(argc, argv, allowed_args);

        port_t port_number = input::parse_input<port_t>(
            PORT_NUMBER_ARG, args_map.get_value(PORT_NUMBER_ARG), 0, 65535);
        logger.log_debug("Port number: ", port_number);
        uint16_t k = input::parse_input<uint16_t>(
            K_ARG, args_map.get_value(K_ARG), 1, 100);
        logger.log_debug("K: ", k);
        uint8_t n = input::parse_input<uint8_t>(
            N_ARG, args_map.get_value(N_ARG), 1, 255);
        logger.log_debug("N: ", n);
        uint32_t m = input::parse_input<uint32_t>(
            M_ARG, args_map.get_value(M_ARG), 1, 1000000);
        logger.log_debug("M: ", m);
        std::string file_name = args_map.get_value(FILE_ARG);
        logger.log_debug("File: ", file_name);

        // Initialize the server
        int listen_fd = open_listen(port_number, logger);
        server::Server server;
        server.run(listen_fd);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
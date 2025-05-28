#include <iostream>

#include "cin.h"
#include "client.h"
#include "input.h"
#include "ip.h"
#include "logging.h"
#include "network.h"
#include "polynomial.h"
#include "strategies.h"
#include <arpa/inet.h>
#include <iomanip>
#include <netinet/in.h>
#include <sstream>
#include <unistd.h>

static inline std::string PLAYER_ID_ARG = "-u";
static inline std::string PORT_NUMBER_ARG = "-p";
static inline std::string SERVER_ARG = "-s";
static inline std::string IPV4_FLAG = "-4";
static inline std::string IPV6_FLAG = "-6";
static inline std::string STRATEGY_FLAG = "-a";

using port_t = ip::port_t;

int main(int argc, char* argv[]) {
    std::unordered_map<std::string, input::arg_t> allowed_args = {
        input::arg_t::get_arg(PLAYER_ID_ARG, true),
        input::arg_t::get_arg(PORT_NUMBER_ARG, true),
        input::arg_t::get_arg(SERVER_ARG, true),
        input::arg_t::get_flag(IPV4_FLAG),
        input::arg_t::get_flag(IPV6_FLAG),
        input::arg_t::get_flag(STRATEGY_FLAG)};

    logging::Logger logger;

    try {
        input::args_parses_t args_map(argc, argv, allowed_args);

        std::string player_id = args_map.get_value(PLAYER_ID_ARG);
        logger.log_debug("Player ID: ", player_id);
        port_t port_number = input::parse_input<port_t>(
            PORT_NUMBER_ARG, args_map.get_value(PORT_NUMBER_ARG), 1, 65535);
        logger.log_debug("Server Port number: ", port_number);
        std::string server_address = args_map.get_value(SERVER_ARG);
        logger.log_debug("Server address: ", server_address);

        bool ipv4_flag = args_map.has_flag(IPV4_FLAG);
        logger.log_debug("IPv4 flag: ", ipv4_flag);
        bool ipv6_flag = args_map.has_flag(IPV6_FLAG);
        logger.log_debug("IPv6 flag: ", ipv6_flag);

        ip::IPAddress::Type ip_type = ip::IPAddress::Type::None;
        if (ipv4_flag != ipv6_flag) {
            ip_type = ipv4_flag ? ip::IPAddress::Type::IPv4
                                : ip::IPAddress::Type::IPv6;
        }

        ip::IPAddress ip_adress =
            network::IpParser::parse(server_address, port_number, ip_type);
        logger.log_debug("Parsed IP address: ", ip_adress.to_string());

        auto poller = std::make_shared<fd::FDPoller>();

        std::shared_ptr<client::strategy> strategy = nullptr;

        if (args_map.has_flag(STRATEGY_FLAG)) {
            logger.log_debug("Using auto strategy");
            strategy = std::make_shared<strategies::AutoStrategy>();
        } else {
            logger.log_debug("Using user strategy");
            auto cin_handler = std::make_shared<cin_fd_handler>();
            strategy = std::make_shared<strategies::UserStrategy>(cin_handler);
            poller->add_socket(STDIN_FILENO, cin_handler);
        }

        client::client client(player_id, ip_adress, logger, strategy, poller);
        client.init();
        client.run();
    } catch (const std::exception& e) {
        logger.log_error(e.what());
        return 1;
    }

    logger.log_info("Client finished successfully.");
    return 0;
}
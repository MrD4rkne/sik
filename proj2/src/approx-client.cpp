#include <iostream>

#include "input.h"
#include "network.h"
#include <iomanip>
#include <sstream>
#include "ip.h"
#include "logging.h"
#include "client.h"
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

static inline std::string PLAYER_ID_ARG = "-u";
static inline std::string PORT_NUMBER_ARG = "-p";
static inline std::string SERVER_ARG = "-s";
static inline std::string IPV4_FLAG = "-4";
static inline std::string IPV6_FLAG = "-6";
static inline std::string STRATEGY_FLAG = "-a";

using port_t = ip::port_t;

static inline int open_socket(const ip::IPAddress& ip_address) {
    sockaddr* adrr;
    socklen_t addr_len;
    network::IpParser::to_adrr(ip_address, &adrr, &addr_len);

    int socket_fd = socket(adrr->sa_family, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        throw std::runtime_error("Failed to create socket");
    }

    if(connect(socket_fd, adrr, addr_len) < 0) {
        close(socket_fd);
        free(adrr);
        throw std::runtime_error("Failed to connect to server");
    }

    free(adrr);

    return socket_fd;
}

int main(int argc, char* argv[]) {
    std::unordered_map<std::string, input::arg_t> allowed_args = {
        input::arg_t::get_arg(PLAYER_ID_ARG, true),
        input::arg_t::get_arg(PORT_NUMBER_ARG, true),
        input::arg_t::get_arg(SERVER_ARG, true),
        input::arg_t::get_flag(IPV4_FLAG),
        input::arg_t::get_flag(IPV6_FLAG),
        input::arg_t::get_flag(STRATEGY_FLAG)};

    logging::Logger logger;

    int socket_fd=-1;

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

        socket_fd = open_socket(ip_adress);
        network::MessageSender message_sender(socket_fd, logger);

        client::client client(player_id, ip_adress, client::strategy(), message_sender, logger);
        client.run();
    } catch (const std::exception& e) {
        logger.log_error(e.what());
        return 1;
    }

    if (socket_fd != -1) {
        close(socket_fd);
    }

    return 0;
}
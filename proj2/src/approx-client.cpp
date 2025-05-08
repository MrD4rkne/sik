#include <iostream>

#include "input.h"
#include "network.h"
#include <iomanip>
#include <sstream>
#include "ip.h"

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

    try {
        // Parse the command line arguments
        input::args_parses_t args_map(argc, argv, allowed_args);

        std::string player_id = args_map.get_value(PLAYER_ID_ARG);
        std::cout << "Player ID: " << player_id << std::endl;

        port_t port_number = input::parse_input<port_t>(
            PORT_NUMBER_ARG, args_map.get_value(PORT_NUMBER_ARG), 1, 65535);
        std::cout << "Port number: " << port_number << std::endl;

        std::string server_address = args_map.get_value(SERVER_ARG);
        std::cout << "Server address: " << server_address << std::endl;

        bool ipv4_flag = args_map.has_flag(IPV4_FLAG);
        bool ipv6_flag = args_map.has_flag(IPV6_FLAG);

        ip::IPAddress::Type ip_type = ip::IPAddress::Type::None;
        if (ipv4_flag != ipv6_flag) {
            ip_type = ipv4_flag ? ip::IPAddress::Type::IPv4
                                : ip::IPAddress::Type::IPv6;
        }

        std::cout << "IP Type: "
                  << (ip_type == ip::IPAddress::Type::IPv4
                          ? "IPv4"
                          : (ip_type == ip::IPAddress::Type::IPv6
                                 ? "IPv6"
                                 : "None"))
                  << std::endl;

        ip::IPAddress ip_adress =
            ip::IpParser::parse(server_address, port_number, ip_type);

        std::cout << "IP Address: " << ip_adress << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
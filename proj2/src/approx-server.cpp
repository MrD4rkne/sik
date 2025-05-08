#include <iostream>

#include "input.h"
#include "network.h"

static inline std::string PORT_NUMBER_ARG = "-p";
static inline std::string K_ARG = "-k";
static inline std::string N_ARG = "-n";
static inline std::string M_ARG = "-m";
static inline std::string FILE_ARG = "-f";

using network::port_t;

int main(int argc, char* argv[]) {
    std::unordered_map<std::string, input::arg_t> allowed_args = {
        input::arg_t::get_arg(PORT_NUMBER_ARG, false, "0"),
        input::arg_t::get_arg(K_ARG, false, "100"),
        input::arg_t::get_arg(N_ARG, false, "4"),
        input::arg_t::get_arg(M_ARG, false, "131"),
        input::arg_t::get_arg(FILE_ARG, true)};

    try {
        // Parse the command line arguments
        input::args_parses_t args_map(argc, argv, allowed_args);

        port_t port_number = input::parse_input<port_t>(
            PORT_NUMBER_ARG, args_map.get_value(PORT_NUMBER_ARG), 0, 65535);
        std::cout << "Port number: " << port_number << std::endl;
        uint16_t k = input::parse_input<uint16_t>(
            K_ARG, args_map.get_value(K_ARG), 1, 100);
        std::cout << "K: " << k << std::endl;
        uint8_t n = input::parse_input<uint8_t>(
            N_ARG, args_map.get_value(N_ARG), 1, 255);
        std::cout << "N: " << static_cast<int>(n) << std::endl;
        uint32_t m = input::parse_input<uint32_t>(
            M_ARG, args_map.get_value(M_ARG), 1, 1000000);
        std::cout << "M: " << m << std::endl;
        std::string file_name = args_map.get_value(FILE_ARG);

        std::cout << "File: " << file_name << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
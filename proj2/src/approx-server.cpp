#include <iostream>

#include "input.h"
#include "network.h"

int main(int argc, char* argv[]) {
    std::unordered_map<std::string, input::arg_t> allowed_args = {
        input::arg_t::get_arg("-p", false, "0"),   // Port number
        input::arg_t::get_arg("-k", false, "100"), // constant K
        input::arg_t::get_arg("-n", false, "4"),   // constant N
        input::arg_t::get_arg("-m", false, "131"), // constant M
        input::arg_t::get_arg("-f", true)          // File
    };

    try {
        // Parse the command line arguments
        input::args_parses_t args_map(argc, argv, allowed_args);

        // Print the parsed arguments
        for (const auto& arg : allowed_args) {
            if (arg.second.is_flag) {
                if (args_map.has_flag(arg.first)) {
                    std::cout << arg.first << " is set" << std::endl;
                } else {
                    std::cout << arg.first << " is not set" << std::endl;
                }

                continue;
            }

            std::cout << arg.first << ": " << args_map.get_value(arg.first)
                      << std::endl;
        }
    } catch (const std::invalid_argument& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
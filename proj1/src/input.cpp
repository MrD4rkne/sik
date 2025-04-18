#include <cstring>
#include <string>
#include <unordered_map>

#include "common.h"
#include "err.h"
#include "input.h"

static const std::string ADRESS_ARG = "-b";
static const std::string PORT_ARG = "-p";
static const std::string PEER_ARG = "-a";
static const std::string PEER_PORT_ARG = "-r";

static const std::string DEFAULT_SERVER_ADRESS = "0.0.0.0";
static const uint16_t DEFAULT_SERVER_PORT = 0;

node_parameters_t parse_args(int argc, char* argv[]) {
    std::unordered_map<std::string, std::string> args_map;

    // Process command line arguments
    for (int i = 1; i < argc; i++) {
        std::string current_arg = argv[i];

        // Check if it's a recognized argument
        if (current_arg == ADRESS_ARG || current_arg == PORT_ARG ||
            current_arg == PEER_ARG || current_arg == PEER_PORT_ARG) {

            // Check if this flag already has a value
            if (args_map.find(current_arg) != args_map.end()) {
                fatal("Duplicate argument: %s", current_arg.c_str());
            }

            // Check if there's a value following the flag
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                args_map[current_arg] = argv[i + 1];
                i++; // Skip the next argument as it's the value
            } else {
                fatal("Missing value for argument: %s", current_arg.c_str());
            }
        } else {
            fatal("Unknown argument: %s", current_arg.c_str());
        }
    }

    // Prepare parameters
    const char* server_adress = args_map.count(ADRESS_ARG)
                                    ? args_map[ADRESS_ARG].c_str()
                                    : DEFAULT_SERVER_ADRESS.c_str();

    uint16_t server_port = DEFAULT_SERVER_PORT;
    if (args_map.count(PORT_ARG)) {
        server_port = read_port(args_map[PORT_ARG].c_str());
    }

    bool has_peer_address = args_map.count(PEER_ARG);
    bool has_peer_port = args_map.count(PEER_PORT_ARG);

    // Validate peer arguments
    if (has_peer_address != has_peer_port) {
        fatal("Peer address and port must be set together");
    }

    node_parameters_t node_params;
    node_params.server_address = get_server_address(server_adress, server_port);

    if (has_peer_address && has_peer_port) {
        const char* peer_adress = args_map[PEER_ARG].c_str();
        uint16_t peer_port = read_port(args_map[PEER_PORT_ARG].c_str());

        if (peer_port == 0) {
            fatal("Peer port must be in range [1, 65535]");
        }

        node_params.peer_address_set = true;
        node_params.peer_address = get_server_address(peer_adress, peer_port);
    } else {
        node_params.peer_address_set = false;
        node_params.peer_address = {};
    }

    return node_params;
}
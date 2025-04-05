#include <cstring>

#include "input.h"
#include "common.h"
#include "err.h"

static const std::string ADRESS_ARG="-b";
static const std::string PORT_ARG="-p";
static const std::string PEER_ARG="-a";
static const std::string PEER_PORT_ARG="-r";

static const std::string DEFAULT_SERVER_ADRESS = "0.0.0.0";
static const uint16_t DEFAULT_SERVER_PORT = 0;

node_parameters_t parse_args(int argc, char* argv[]){
    const char* server_adress = nullptr;
    uint16_t server_port = DEFAULT_SERVER_PORT;

    const char* peer_adress = nullptr;
    uint16_t peer_port = 0;

    // TODO: match better, fix if uknown argument is provided or they stack, like -a -r ...

    for (int i = 1; i < argc; i++) {
        if(i == argc - 1){
            fatal("Missing argument for %s", argv[i]);
        }

        if (std::strcmp(argv[i], ADRESS_ARG.c_str()) == 0) {
            server_adress = argv[++i];
        } else if (std::strcmp(argv[i], PORT_ARG.c_str()) == 0) {
            server_port = read_port(argv[++i]);
        } else if (std::strcmp(argv[i], PEER_ARG.c_str()) == 0) {
            peer_adress = argv[++i];
        } else if (std::strcmp(argv[i], PEER_PORT_ARG.c_str()) == 0) {
            peer_port = read_port(argv[++i]);
            if(peer_port == 0){
                fatal("Peer port must be in rage [1, 65535]");
            }
        }else {
            fatal("Unknown argument: %s", argv[i]);
        }
    }

    // Validate the arguments.
    if((peer_port == 0) != (peer_adress == nullptr)){
        fatal("Peer address and port must be set together");
    }

    if (server_adress == nullptr) {
        server_adress = DEFAULT_SERVER_ADRESS.c_str();
    }

    node_parameters_t node_params;
    node_params.server_address = get_server_address(server_adress, server_port);
    if(peer_adress != nullptr){
        node_params.peer_address_set = true;
        node_params.peer_address = get_server_address(peer_adress, peer_port);
    }
    else{
        node_params.peer_address_set = false;
        node_params.peer_address = {};
    }

    return node_params;
}
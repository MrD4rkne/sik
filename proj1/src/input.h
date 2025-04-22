#ifndef INPUT_H
#define INPUT_H

#include <memory>
#include <netinet/in.h>

typedef struct node_parameters {
    sockaddr_in server_address;
    sockaddr_in peer_address;
    bool peer_address_set = false;
} node_parameters_t;

/// @brief Parse command line arguments.
/// @param argc The number of arguments.
/// @param argv The array of arguments.
/// @return A node_parameters_t structure containing the parsed parameters.
/// @throws std::invalid_argument if the arguments are invalid
node_parameters_t parse_args(int argc, char* argv[]);

#endif
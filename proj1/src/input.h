#ifndef INPUT_H
#define INPUT_H

#include <memory>
#include <netinet/in.h>

typedef struct node_parameters{
    sockaddr_in server_address;
    sockaddr_in peer_address;
    bool peer_address_set = false;
} node_parameters_t;

std::unique_ptr<node_parameters_t> parse_args(int argc, char* argv[]);

#endif
#include <iostream>
#include <netinet/in.h>
#include <string.h>
#include <cstdlib>
#include <cstdint>
#include <memory>

#include "err.h"
#include "common.h"
#include "input.h"

using namespace std;

int init_server(sockaddr_in* server_address) {
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        syserr("cannot create a socket");
        return -1;
    }

    if (bind(socket_fd, (struct sockaddr *) server_address, (socklen_t) sizeof(*server_address)) < 0) {
        syserr("bind");
    }

    return socket_fd;
}

int main(int argc, char* argv[]) {
    unique_ptr<node_parameters_t> node_params = parse_args(argc, argv);

    int server_socket = init_server(&node_params->server_address);

    int a;
    cin >> a;

    close(server_socket);

    return 0;
}
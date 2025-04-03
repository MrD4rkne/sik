#include <iostream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <cstdlib>
#include <cstdint>
#include <memory>

#include "err.h"
#include "common.h"
#include "input.h"
#include "logging.h"

using namespace std;

static const int SOCK_TIMEOUT = 4;

static inline int init_server(sockaddr_in* server_address) {
    logging::logDebug("Initializing socket...");

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        syserr("cannot create a socket");
    }

    logging::logDebug("Socket created successfully.");
    logging::logDebug("Binding socket...");

    if (bind(socket_fd, (struct sockaddr *) server_address, (socklen_t) sizeof(*server_address)) < 0) {
        syserr("bind");
    }

    logging::logDebug("Socket bound successfully.");

    if constexpr (logging::LOG_DEBUG) {
        sockaddr_in bound_address;
        socklen_t bound_address_len = sizeof(bound_address);
        if (getsockname(socket_fd, (struct sockaddr *)&bound_address, &bound_address_len) < 0) {
            syserr("getsockname");
        }
        logging::logDebug("Socket bound on IP: ", inet_ntoa(bound_address.sin_addr), 
                        ", Port: ", ntohs(bound_address.sin_port));
    }

    // TODO: Set socket timeout

    return socket_fd;
}

static inline void process_client(size_t bytes_received, const char* buffer, char const *client_ip, uint16_t client_port) {
    logging::connection::logDebug(client_ip, client_port, "Received ", bytes_received, " bytes from client.");
    logging::log_bad_message(bytes_received, buffer);
}

static inline void run_server(int socket_fd){
    const static size_t BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];
    for (;;) {
        sockaddr_in client_address;
        socklen_t client_address_len = sizeof(client_address);

        logging::logDebug("Waiting for a message...");

        memset(buffer, 0, BUFFER_SIZE);
        ssize_t bytes_received = recvfrom(socket_fd, buffer, BUFFER_SIZE, 0,
                                          (struct sockaddr *) &client_address, &client_address_len);
        if (bytes_received < 0) {
            syserr("recvfrom");
        }

        char const *client_ip = inet_ntoa(client_address.sin_addr);
        uint16_t client_port = ntohs(client_address.sin_port);

        process_client(bytes_received, buffer, client_ip, client_port);
    }
}

int main(int argc, char* argv[]) {
    unique_ptr<node_parameters_t> node_params = parse_args(argc, argv);
    
    logging::logDebug("Arguments:");
    for (int i = 0; i < argc; ++i) {
        logging::logDebug("argv[", i, "]: ", argv[i]);
    }
    logging::logDebug("Node parameters:");
    logging::logDebug("Server address: ", inet_ntoa(node_params->server_address.sin_addr));
    logging::logDebug("Server port: ", ntohs(node_params->server_address.sin_port));
    logging::logDebug("IsPeerSet: ", node_params->peer_address_set);
    if(node_params->peer_address_set){
        logging::logDebug("Peer address: ", inet_ntoa(node_params->peer_address.sin_addr));
        logging::logDebug("Peer port: ", ntohs(node_params->peer_address.sin_port));
    }

    int server_socket = init_server(&node_params->server_address);
    logging::logDebug("Server started, waiting for clients...");
    run_server(server_socket);

    logging::logDebug("Server shutting down...");
    close(server_socket);

    return 0;
}
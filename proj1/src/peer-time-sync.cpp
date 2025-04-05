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
#include "messaging.h"
#include "packets.h"

using namespace std;

static const int SOCK_TIMEOUT = 4;

using message_type_t = uint8_t;

static inline int init_server(sockaddr_in& server_address) {
    logging::logDebug("Initializing socket...");

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        syserr("cannot create a socket");
    }

    logging::logDebug("Socket created successfully.");
    logging::logDebug("Binding socket...");

    if (bind(socket_fd, (struct sockaddr *) &server_address, (socklen_t) sizeof(server_address)) < 0) {
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

    // TODO: Set socket timeout?

    return socket_fd;
}

static inline void process_client(messaging::MessageMediator<message_type_t> message_mediator, messaging::Node& server, size_t bytes_received, char* buffer, const sockaddr_in* client_address) {
    logging::connection::logDebug(client_address, "Received ", bytes_received, ".");

    if(bytes_received < 1){
        logging::connection::logDebug(client_address, "Error: Received empty message.");
        logging::log_bad_message(bytes_received, buffer);
        return;
    }

    message_type_t message_type = buffer[0];
    logging::connection::logDebug(client_address, "Message type: ", std::to_string(message_type));

    if (!message_mediator.handle_message(server, client_address, message_type, bytes_received-1, buffer+1)) {
        // If we reach here, one of handlers failed to handle the message.
        logging::connection::logDebug(client_address, "Handler failed to process message.");
        logging::log_bad_message(bytes_received, buffer);
    }
}

static inline void run_server(int socket_fd, node_parameters_t& parameters) {
    messaging::MessageMediator<message_type_t> message_mediator;
    
    messaging::Node server;

    if(parameters.peer_address_set){
        //packets::send_hello(socket_fd, &parameters.peer_address);
    }

    const static size_t BUFFER_SIZE = 65535;
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

        process_client(message_mediator, server, bytes_received, buffer, &client_address);
    }
}

int main(int argc, char* argv[]) {
    node_parameters_t node_params = parse_args(argc, argv);
    
    logging::logDebug("Arguments:");
    for (int i = 0; i < argc; ++i) {
        logging::logDebug("argv[", i, "]: ", argv[i]);
    }
    logging::logDebug("Node parameters:");
    logging::logDebug("Server address: ", inet_ntoa(node_params.server_address.sin_addr));
    logging::logDebug("Server port: ", ntohs(node_params.server_address.sin_port));
    logging::logDebug("IsPeerSet: ", node_params.peer_address_set);
    if(node_params.peer_address_set){
        logging::logDebug("Peer address: ", inet_ntoa(node_params.peer_address.sin_addr));
        logging::logDebug("Peer port: ", ntohs(node_params.peer_address.sin_port));
    }

    int server_socket = init_server(node_params.server_address);
    logging::logDebug("Server started, waiting for clients...");
    run_server(server_socket, node_params);

    logging::logDebug("Server shutting down...");
    close(server_socket);

    return 0;
}
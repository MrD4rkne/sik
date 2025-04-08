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
#include "domain.h"

using namespace std;

static const int SOCK_TIMEOUT = 4;

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

static inline domain::peer parse_peer_address(const sockaddr_in& peer_address) {
    logging::connection::logDebug(&peer_address, "Parsing peer address");

    domain::peer_address_length_t peer_address_length = sizeof(peer_address.sin_addr);
    std::vector<uint8_t> peer_address_bytes(peer_address_length);
    std::copy(reinterpret_cast<const uint8_t*>(&peer_address.sin_addr), 
              reinterpret_cast<const uint8_t*>(&peer_address.sin_addr) + peer_address_length, 
              peer_address_bytes.begin());

    logging::connection::logDebug(&peer_address, "Parsed peer address: ", logging::parse(reinterpret_cast<const char*>(peer_address_bytes.data()), peer_address_length));
    return domain::peer(ntohs(peer_address.sin_port), peer_address_bytes);
}

static inline void process_client(messaging::MessageMediator<packets::message_type_t> message_mediator, messaging::Node& server, size_t bytes_received, char* buffer, sockaddr_in* client_address, int socket_fd) {
    logging::connection::logDebug(client_address, "Received ", bytes_received, ".");

    logging::connection::logDebug(client_address, "Buffer: ", logging::parse(buffer, bytes_received));

    if(bytes_received < 1){
        logging::connection::logDebug(client_address, "Error: Received empty message.");
        logging::log_bad_message(bytes_received, buffer);
        return;
    }

    packets::message_type_t message_type = packets::mappers::get_message_type(buffer, bytes_received);
    logging::connection::logDebug(client_address, "Message type: ", std::to_string(message_type));

    messaging::MessageSender message_sender(socket_fd, client_address);
    domain::peer peer = parse_peer_address(*client_address);

    if (!message_mediator.handle_message(server, peer, message_type, bytes_received, buffer, message_sender)) {
        // If we reach here, one of handlers failed to handle the message.
        logging::connection::logDebug(client_address, "Handler failed to process message.");
        logging::log_bad_message(bytes_received, buffer);
    }
}

static inline void run_server(int socket_fd, node_parameters_t& parameters) {
    messaging::MessageMediator<packets::message_type_t> message_mediator;
    message_mediator.register_handler(packets::MSG_TYPE_HELLO, new messaging::hello_message_handler());
    message_mediator.register_handler(packets::MSG_TYPE_HELLO_RSP, new messaging::hello_response_handler());
    
    messaging::Node server;

    if(parameters.peer_address_set){
        string hello_message = packets::mappers::create_hello_packet();

        sockaddr_in* p = &parameters.peer_address;
        logging::connection::logDebug(p, "Sending hello message to peer");

        // try catch memory issues?
        messaging::MessageSender message_sender(socket_fd, p);
        if (!message_sender.send_message(hello_message.c_str(), hello_message.size())) {
            logging::connection::logDebug(&parameters.peer_address, "Failed to send hello message to peer.");
        }

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

        process_client(message_mediator, server, bytes_received, buffer, &client_address, socket_fd);
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
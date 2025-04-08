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

static inline int init_server(logging::Logger &logger, sockaddr_in& server_address) {
    logger.logDebug("Initializing socket...");

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        logger.logError("Failed to create socket.");
        syserr("cannot create a socket");
    }

    logger.logDebug("Socket created successfully.");
    logger.logDebug("Binding socket...");

    if (bind(socket_fd, (struct sockaddr *) &server_address, (socklen_t) sizeof(server_address)) < 0) {
        logger.logError("Failed to bind socket.");
        syserr("bind");
    }

    logger.logDebug("Socket bound successfully.");

    if constexpr (logging::LOG_DEBUG) {
        sockaddr_in bound_address;
        socklen_t bound_address_len = sizeof(bound_address);
        if (getsockname(socket_fd, (struct sockaddr *)&bound_address, &bound_address_len) < 0) {
            logger.logError("Failed to get socket name.");
            syserr("getsockname");
        }

        logger.logDebug("Socket bound on IP: ", inet_ntoa(bound_address.sin_addr), 
                        ", Port: ", ntohs(bound_address.sin_port));
    }

    // TODO: Set socket timeout?

    return socket_fd;
}

static inline domain::peer parse_peer_address(logging::Logger &logger, const sockaddr_in& peer_address) {
    logger.logDebug("Parsing peer address: ", inet_ntoa(peer_address.sin_addr), ", Port: ", ntohs(peer_address.sin_port));

    domain::peer_address_length_t peer_address_length = sizeof(peer_address.sin_addr);
    std::vector<uint8_t> peer_address_bytes(peer_address_length);
    std::copy(reinterpret_cast<const uint8_t*>(&peer_address.sin_addr), 
              reinterpret_cast<const uint8_t*>(&peer_address.sin_addr) + peer_address_length, 
              peer_address_bytes.begin());

    return domain::peer(ntohs(peer_address.sin_port), peer_address_bytes);
}

static inline void process_client(
    messaging::MessageMediator<packets::message_type_t> message_mediator, 
    domain::Node& server, 
    logging::Logger &logger,
    const domain::peer& peer,
    size_t bytes_received, 
    char* buffer, 
    messaging::MessageSender& message_sender) 
    {
        logger.logDebug("Received ", bytes_received, ".");

        logger.logDebug("Buffer: ", logging::parse(buffer, bytes_received));

    if(bytes_received < 1){
        logger.logError("Received empty message.");
        logger.logDebug(bytes_received, buffer);
        return;
    }

    packets::message_type_t message_type = packets::mappers::get_message_type(buffer, bytes_received);
    logger.logDebug("Message type: ", std::to_string(message_type));

    if (!message_mediator.handle_message(server, logger, peer, message_type, bytes_received, buffer, message_sender)) {
        // If we reach here, one of handlers failed to handle the message.
        logger.logError("Handler failed to process message.");
        logger.log_bad_message(bytes_received, buffer);
    }
}

static inline void run_server(int socket_fd, logging::Logger &logger, node_parameters_t& parameters) {
    messaging::MessageMediator<packets::message_type_t> message_mediator;
    message_mediator.register_handler(packets::MSG_TYPE_HELLO, new messaging::hello_message_handler());
    message_mediator.register_handler(packets::MSG_TYPE_HELLO_RSP, new messaging::hello_response_handler());
    
    domain::Node server;

    if(parameters.peer_address_set){
        string hello_message = packets::mappers::create_hello_packet();

        logging::Logger sender_logger(parse_peer_address(logger, parameters.peer_address));

        sockaddr_in* p = &parameters.peer_address;
        logger.logDebug("Sending hello message to peer");

        // try catch memory issues?
        messaging::MessageSender message_sender(socket_fd, p, sender_logger);
        if (!message_sender.send_message(hello_message.c_str(), hello_message.size())) {
            logger.logError("Failed to send hello message to peer.");
        }
    }

    const static size_t BUFFER_SIZE = 65535;
    char buffer[BUFFER_SIZE];
    for (;;) {
        sockaddr_in client_address;
        socklen_t client_address_len = sizeof(client_address);

        logger.logDebug("Waiting for a message...");

        memset(buffer, 0, BUFFER_SIZE);
        ssize_t bytes_received = recvfrom(socket_fd, buffer, BUFFER_SIZE, 0,
                                          (struct sockaddr *) &client_address, &client_address_len);
        if (bytes_received < 0) {
            logger.logError("Failed to receive message.");
            syserr("recvfrom");
        }

        domain::peer peer = parse_peer_address(logger, client_address);

        logger.logDebug("Received message from peer: ", peer);
        logging::Logger peer_logger(peer);
        messaging::MessageSender message_sender(socket_fd, &client_address, peer_logger);

        process_client(message_mediator, server, peer_logger, peer, bytes_received, buffer, message_sender);
        peer_logger.logDebug("Processed message from peer");

        logger.logDebug("Message processed successfully.");
    }
}

int main(int argc, char* argv[]) {
    node_parameters_t node_params = parse_args(argc, argv);

    logging::Logger logger;
    
    logger.logDebug("Arguments:");
    for (int i = 0; i < argc; ++i) {
        logger.logDebug("argv[", i, "]: ", argv[i]);
    }
    logger.logDebug("Node parameters:");
    logger.logDebug("Server address: ", inet_ntoa(node_params.server_address.sin_addr));
    logger.logDebug("Server port: ", ntohs(node_params.server_address.sin_port));
    logger.logDebug("IsPeerSet: ", node_params.peer_address_set);
    if(node_params.peer_address_set){
        logger.logDebug("Peer address: ", inet_ntoa(node_params.peer_address.sin_addr));
        logger.logDebug("Peer port: ", ntohs(node_params.peer_address.sin_port));
    }

    int server_socket = init_server(logger, node_params.server_address);
    logger.logDebug("Server started, waiting for clients...");
    run_server(server_socket, logger, node_params);

    logger.logDebug("Server shutting down...");
    close(server_socket);

    return 0;
}
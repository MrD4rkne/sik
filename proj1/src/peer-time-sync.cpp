#include <arpa/inet.h>
#include <iostream>

#include "domain.h"
#include "handlers.h"
#include "input.h"
#include "logging.h"
#include "server.h"

using namespace std;
using namespace domain;

static const int SOCK_TIMEOUT = 1; // seconds

static const int DELAY_AFTER_BECOMING_LEADER = 2; // seconds
static const int DELAY_BETWEEN_SYNCS = 5;         // seconds
static const int SYNC_TIMEOUT = 5;                // seconds

static inline handlers::MessageMediator<message_type_t> init_mediator() {
    handlers::MessageMediator<message_type_t> message_mediator;
    message_mediator.register_handler(
        packets::MSG_TYPE_HELLO,
        std::make_shared<handlers::hello_message_handler>());
    message_mediator.register_handler(
        packets::MSG_TYPE_HELLO_RSP,
        std::make_shared<handlers::hello_response_handler>());
    message_mediator.register_handler(
        packets::MSG_TYPE_CONNECT,
        std::make_shared<handlers::connect_handler>());
    message_mediator.register_handler(
        packets::MSG_TYPE_ACK_CONNECT,
        std::make_shared<handlers::ack_connect_handler>());
    message_mediator.register_handler(
        packets::MSG_TYPE_LEADER, std::make_shared<handlers::leader_handler>());
    message_mediator.register_handler(
        packets::MSG_TYPE_SYNC_START,
        std::make_shared<handlers::sync_start_handler>());
    message_mediator.register_handler(
        packets::MSG_TYPE_DELAY_REQUEST,
        std::make_shared<handlers::delay_request_handler>());
    message_mediator.register_handler(
        packets::MSG_TYPE_DELAY_RESPONSE,
        std::make_shared<handlers::delay_response_handler>());

    return message_mediator;
}

static inline domain::Node init_node() {
    return domain::Node(
        natural_time::Clock::from_seconds(DELAY_AFTER_BECOMING_LEADER),
        natural_time::Clock::from_seconds(DELAY_BETWEEN_SYNCS),
        natural_time::Clock::from_seconds(SYNC_TIMEOUT));
}

int main(int argc, char* argv[]) {
    node_parameters_t node_params = parse_args(argc, argv);

    logging::Logger logger;

    logger.logDebug("Arguments:");
    for (int i = 0; i < argc; ++i) {
        logger.logDebug("argv[", i, "]: ", argv[i]);
    }
    logger.logDebug("Node parameters:");
    logger.logDebug("Server address: ",
                    inet_ntoa(node_params.server_address.sin_addr));
    logger.logDebug("Server port: ",
                    ntohs(node_params.server_address.sin_port));
    logger.logDebug("IsPeerSet: ", node_params.peer_address_set);
    if (node_params.peer_address_set) {
        logger.logDebug("Peer address: ",
                        inet_ntoa(node_params.peer_address.sin_addr));
        logger.logDebug("Peer port: ",
                        ntohs(node_params.peer_address.sin_port));
    }

    int server_socket =
        server::init_server(logger, node_params.server_address, SOCK_TIMEOUT);
    logger.logDebug("Server started, waiting for clients...");

    Node server = init_node();
    handlers::MessageMediator<domain::message_type_t> message_mediator =
        init_mediator();

    try {
        server::run_server(server_socket, logger, node_params, server,
                           message_mediator);
    } catch (const std::exception& e) {
        logger.logError("Server encountered an error: ", e.what());
        close(server_socket);
        return 1;
    }

    logger.logDebug("Server shutting down...");
    close(server_socket);

    return 0;
}
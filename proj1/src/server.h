#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>

#include "domain.h"
#include "handlers.h"
#include "input.h"
#include "logging.h"

namespace server {

/// @brief Initialize the server socket.
/// @param logger The logger to use.
/// @param server_address The server address.
/// @param sock_timeout The socket timeout in seconds.
/// @return The socket file descriptor.
/// @throws std::runtime_error if the socket cannot be created or bound.
int init_server(logging::Logger& logger, sockaddr_in& server_address,
                int sock_timeout);

/// @brief Run the server.
/// @param socket_fd The socket file descriptor.
/// @param logger The logger to use.
/// @param parameters The node parameters.
/// @param server The server node.
/// @param message_mediator The message mediator.
/// @throws std::runtime_error if the server cannot be run.
void run_server(
    int socket_fd, logging::Logger& logger, node_parameters_t& parameters,
    domain::Node& server,
    handlers::MessageMediator<domain::message_type_t> message_mediator);

} // namespace server

#endif
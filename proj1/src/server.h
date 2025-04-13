#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>

#include "logging.h"
#include "input.h"
#include "domain.h"
#include "handlers.h"

namespace server {

int init_server(logging::Logger& logger,
                              sockaddr_in& server_address,
                                int sock_timeout);

void run_server(int socket_fd, logging::Logger& logger,
                              node_parameters_t& parameters,
                              domain::Node& server,
                              handlers::MessageMediator<domain::message_type_t> message_mediator);

} // namespace server

#endif
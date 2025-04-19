#include <netinet/in.h>

#include "domain.h"
#include "err.h"
#include "logging.h"
#include "server.h"

namespace server {

using namespace domain;
using namespace handlers;
using namespace packets;
using namespace messaging;
using namespace logging;
using namespace std;

static inline domain::peer parse_peer_address(logging::Logger& logger,
                                              const sockaddr_in& peer_address) {
    logger.logDebug("Parsing peer address: ", inet_ntoa(peer_address.sin_addr),
                    ", Port: ", ntohs(peer_address.sin_port));

    domain::peer_address_length_t peer_address_length =
        sizeof(peer_address.sin_addr);
    std::array<uint8_t, 4> peer_address_bytes;
    std::copy(reinterpret_cast<const uint8_t*>(&peer_address.sin_addr),
              reinterpret_cast<const uint8_t*>(&peer_address.sin_addr) +
                  peer_address_length,
              peer_address_bytes.data());

    return domain::peer(ntohs(peer_address.sin_port), peer_address_bytes);
}

static inline void
process_client(handlers::MessageMediator<message_type_t> message_mediator,
               domain::Node& server, logging::Logger& logger,
               const domain::peer& peer, size_t bytes_received, char* buffer,
               messaging::MessageSender& message_sender) {
    logger.logDebug("Received ", bytes_received, ".");

    logger.logDebug("Buffer: ", logging::parse(buffer, bytes_received));

    if (bytes_received < 1) {
        logger.logError("Received empty message.");
        logger.logDebug(bytes_received, buffer);
        return;
    }

    message_type_t message_type =
        packets::get_message_type(buffer, bytes_received);

    try {
        auto result = message_mediator.handle_message(
            server, logger, peer, message_type, bytes_received, buffer,
            message_sender);

        if (result.is_success()) {
            logger.logDebug("Message processed successfully.");
        } else {
            logger.log_bad_message(bytes_received, buffer);
            logger.logDebug(result.get_error_message());
        }
    } catch (const std::invalid_argument& e) {
        logger.log_bad_message(bytes_received, buffer);
        logger.logError(
            "Handler failed to process message, packet was invalid ", e.what());
    } catch (const std::exception& e) {
        logger.log_bad_message(bytes_received, buffer);
        logger.logError("Handler failed to process message: ", e.what());
    }
}

int init_server(logging::Logger& logger, sockaddr_in& server_address,
                int sock_timeout) {
    logger.logDebug("Initializing socket...");

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        logger.logError("Failed to create socket.");
        syserr("cannot create a socket");
    }

    logger.logDebug("Socket created successfully.");
    logger.logDebug("Binding socket...");

    if (bind(socket_fd, (struct sockaddr*)&server_address,
             (socklen_t)sizeof(server_address)) < 0) {
        logger.logError("Failed to bind socket.");
        syserr("bind");
    }

    logger.logDebug("Socket bound successfully.");

    if constexpr (logging::LOG_DEBUG) {
        sockaddr_in bound_address;
        socklen_t bound_address_len = sizeof(bound_address);
        if (getsockname(socket_fd, (struct sockaddr*)&bound_address,
                        &bound_address_len) < 0) {
            logger.logError("Failed to get socket name.");
            syserr("getsockname");
        }

        logger.logDebug(
            "Socket bound on IP: ", inet_ntoa(bound_address.sin_addr),
            ", Port: ", ntohs(bound_address.sin_port));
    }

    logger.logDebug("Setting socket timeout...");
    struct timeval tv;
    tv.tv_sec = sock_timeout;
    tv.tv_usec = 0;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("Error");
    }
    logger.logDebug("Socket timeout set to ", sock_timeout, " seconds.");

    return socket_fd;
}

void run_server(
    int socket_fd, logging::Logger& logger, node_parameters_t& parameters,
    domain::Node& server,
    handlers::MessageMediator<domain::message_type_t> message_mediator) {

    if (parameters.peer_address_set) {
        domain::peer peer = parse_peer_address(logger, parameters.peer_address);

        logging::Logger sender_logger(peer);
        logger.logDebug("Sending hello message to peer");

        messaging::MessageSender message_sender(socket_fd, logger);
        handlers::send_hello(server, sender_logger, peer, message_sender);
    }

    const static size_t BUFFER_SIZE = 65535;
    char buffer[BUFFER_SIZE];
    for (;;) {
        {
            messaging::MessageSender message_sender(socket_fd, logger);
            handlers::start_synchronization(server, logger, message_sender);
        }

        server.get_local_synchronization().validate_sync_timeout(
            server.get_time());

        sockaddr_in client_address;
        socklen_t client_address_len = sizeof(client_address);

        memset(buffer, 0, BUFFER_SIZE);

        ssize_t bytes_received =
            recvfrom(socket_fd, buffer, BUFFER_SIZE, 0,
                     (struct sockaddr*)&client_address, &client_address_len);
        if (bytes_received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }

            logger.logError("Failed to receive message.");
            syserr("recvfrom");
        }

        domain::peer peer = parse_peer_address(logger, client_address);

        logger.logDebug("Received message from peer: ", peer);
        logging::Logger peer_logger(peer);
        messaging::MessageSender message_sender(socket_fd, peer_logger);

        process_client(message_mediator, server, peer_logger, peer,
                       (size_t)bytes_received, buffer, message_sender);
    }
}

} // namespace server
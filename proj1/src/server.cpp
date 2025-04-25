#include <netinet/in.h>

#include "domain.h"
#include "logging.h"
#include "server.h"

namespace server {

using namespace domain;
using namespace handlers;
using namespace packets;
using namespace messaging;
using namespace logging;
using namespace std;

static inline peers::peer parse_peer_address(logging::Logger& logger,
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

    return peers::peer(ntohs(peer_address.sin_port), peer_address_bytes);
}

static inline void
process_client(handlers::MessageMediator<message_type_t> message_mediator,
               domain::Node& server, logging::Logger& logger,
               const peers::peer& peer, size_t bytes_received, char* buffer,
               messaging::MessageSender& message_sender) {
    logger.logDebug("Received ", bytes_received, ".");
    logger.logDebug("Buffer: ", logging::parse(buffer, bytes_received));

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
        logger.logWarning(
            "Handler failed to process message, packet was invalid ", e.what());
    } catch (const std::exception& e) {
        logger.logError("Handler failed to process message: ", e.what());
    }
}

int init_server(logging::Logger& logger, sockaddr_in& server_address,
                suseconds_t sock_timeout) {
    logger.logDebug("Initializing socket...");

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        throw std::runtime_error("socket(): Failed to create socket.");
    }

    logger.logDebug("Socket created successfully.");
    logger.logDebug("Binding socket...");

    if (bind(socket_fd, (struct sockaddr*)&server_address,
             (socklen_t)sizeof(server_address)) < 0) {
        throw std::runtime_error("bind(): Failed to bind socket to address.");
    }

    logger.logDebug("Socket bound successfully.");

    logger.logDebug("Setting socket timeout...");
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = sock_timeout;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        throw std::runtime_error("setsockopt(): Failed to set socket timeout.");
    }

    logger.logDebug("Socket timeout set to ", sock_timeout, " seconds.");
    return socket_fd;
}

peers::peer parse_peer_address(logging::Logger& logger, int socket_fd) {
    sockaddr_in bound_address;
    socklen_t bound_address_len = sizeof(bound_address);
    if (getsockname(socket_fd, (struct sockaddr*)&bound_address,
                    &bound_address_len) < 0) {
        throw std::runtime_error("getsockname(): Failed to get socket name.");
    }

    domain::peer_address_length_t peer_address_length =
        sizeof(bound_address.sin_addr);
    std::array<uint8_t, 4> peer_address_bytes;
    std::copy(reinterpret_cast<const uint8_t*>(&bound_address.sin_addr),
              reinterpret_cast<const uint8_t*>(&bound_address.sin_addr) +
                  peer_address_length,
              peer_address_bytes.data());
    peers::peer peer(ntohs(bound_address.sin_port), peer_address_bytes);

    logger.logDebug("Server: ", peer);
    return peer;
}

void run_server(
    int socket_fd, logging::Logger& logger, node_parameters_t& parameters,
    domain::Node& server,
    handlers::MessageMediator<domain::message_type_t> message_mediator) {

    if (parameters.peer_address_set) {
        peers::peer peer = parse_peer_address(logger, parameters.peer_address);

        logging::Logger sender_logger(peer);
        logger.logDebug("Sending hello message to peer");

        messaging::MessageSender message_sender(socket_fd, logger);
        auto result =
            handlers::send_hello(server, sender_logger, peer, message_sender);
        if (!result.is_success()) {
            logger.logDebug("Failed to send hello message: ",
                            result.get_error_message());
        }
    }

    const static size_t BUFFER_SIZE = 65535;
    char buffer[BUFFER_SIZE];
    for (;;) {
        //logger.logDebug("Current time: ", server.get_absolute_timestamp());
        //logger.logDebug("Synced time: ", server.get_synced_timestamp());

        {
            messaging::MessageSender message_sender(socket_fd, logger);
            auto result =
                handlers::try_send_start_syncs(server, logger, message_sender);
            if (!result.is_success()) {
                logger.logDebug("Failed to send sync_starts: ",
                                result.get_error_message());
            }
        }

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

            throw std::runtime_error(
                "recvfrom(): Failed to receive message from client.");
        }

        //logger.logDebug("Current time: ", server.get_absolute_timestamp());

        {
            auto ongoing_sync_timeout_result =
                server.validate_ongoing_sync_timeout();
            if (!ongoing_sync_timeout_result.is_success()) {
                logger.logDebug(
                    ongoing_sync_timeout_result.get_error_message());
            } else {
                logger.logDebug("Ongoing sync timed out.");
            }
        }

        {
            auto sync_timeout_result = server.validate_sync_timeout();
            if (!sync_timeout_result.is_success()) {
                logger.logDebug(sync_timeout_result.get_error_message());
            } else {
                logger.logDebug("Sync timed out.");
            }
        }

        peers::peer peer = parse_peer_address(logger, client_address);

        logger.logDebug("Received message from peer: ", peer);
        logging::Logger peer_logger(peer);
        messaging::MessageSender message_sender(socket_fd, peer_logger);

        process_client(message_mediator, server, peer_logger, peer,
                       (size_t)bytes_received, buffer, message_sender);
    }
}

} // namespace server
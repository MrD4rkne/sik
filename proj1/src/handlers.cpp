#include "handlers.h"
#include <algorithm>

namespace handlers {

using namespace domain;
using namespace results;
using namespace messaging;
using namespace packets;
using namespace logging;

static const std::string PEER_NOT_KNOWN = "Peer not known.";

template<typename T>
static inline T deserialize_packet(logging::Logger& logger, char* buffer,
                                   size_t read_bytes) {

    logger.logDebug("Deserializing packet of type: ", typeid(T).name());

    auto packet = packets::mappers<T>::deserialize_packet(buffer, read_bytes);

    logger.logDebug("Deserialized packet of type: ", typeid(T).name());
    logger.logDebug("Packet: ", packet);
    return packet;
}

Result hello_message_handler::handle(Node& node, logging::Logger& logger,
                                     const domain::peer& peer,
                                     size_t read_bytes, char* buffer,
                                     MessageSender& message_sender) {
    logger.logDebug("Received hello message.");
    (void)handlers::deserialize_packet<packets::hello_packet_t>(logger, buffer,
                                                                read_bytes);

    hello_response_packet_t hello_message_response{.peers = node.get_peers()};

    auto add_result = node.add_peer(peer);
    if (!add_result.is_success()) {
        return add_result;
    }

    logger.logDebug("Added peer to the list of peers.");

    try {
        message_sender.send_message(peer, hello_message_response);
    } catch (const std::invalid_argument& e) {
        throw std::runtime_error("Failed to send hello response: " +
                                 std::string(e.what()));
    }

    return Result::Success();
}

Result hello_response_handler::handle(Node& node, logging::Logger& logger,
                                      const domain::peer& peer,
                                      size_t read_bytes, char* buffer,
                                      MessageSender& message_sender) {
    logger.logDebug("Received hello response message.");
    hello_response_packet_t hello_response_packet =
        handlers::deserialize_packet<hello_response_packet_t>(logger, buffer,
                                                              read_bytes);

    auto result = node.acknowledge_hello_rsp(peer);
    if (!result.is_success()) {
        return result;
    }

    if (std::find(hello_response_packet.peers.begin(),
                  hello_response_packet.peers.end(),
                  peer) != hello_response_packet.peers.end()) {
        return Result::Failure("Peer is in the list of peers.");
    }

    auto& server = node.get_self();
    logger.logDebug("Server: ", server);
    if (std::find(hello_response_packet.peers.begin(),
                  hello_response_packet.peers.end(),
                  server) != hello_response_packet.peers.end()) {
        return Result::Failure("Server is in the list of peers.");
    }

    logger.logDebug("Sending CONNECT messages to peers.");
    bool success = true;
    for (const auto& new_peer : hello_response_packet.peers) {
        logger.logDebug("Sending CONNECT message to ", new_peer);
        try {
            packets::connect_packet_t connect_packet;
            message_sender.send_message(new_peer, connect_packet);
            node.add_waiting_for_connect_ack(new_peer);
        } catch (const std::exception& e) {
            logger.logError("Failed to send connect to peer: ", e.what());
            success = false;
        }
    }

    if (success) {
        logger.logDebug("Sent CONNECT messages to all peers.");
    } else {
        logger.logWarning("Failed to send CONNECT messages to some peers.");
    }
    return Result::Success();
}

Result connect_handler::handle(Node& node, logging::Logger& logger,
                               const domain::peer& peer, size_t read_bytes,
                               char* buffer, MessageSender& message_sender) {
    logger.logDebug("Received connect message.");
    (void)handlers::deserialize_packet<packets::connect_packet_t>(
        logger, buffer, read_bytes);

    auto result = node.add_peer(peer);
    if (!result.is_success()) {
        return result;
    }

    packets::ack_connect_packet_t ack_connect_packet;
    message_sender.send_message(peer, ack_connect_packet);
    return Result::Success();
}

Result ack_connect_handler::handle(Node& node, logging::Logger& logger,
                                   const domain::peer& peer, size_t read_bytes,
                                   char* buffer,
                                   MessageSender& /*message_sender*/) {
    logger.logDebug("Received ack_connect message.");

    (void)handlers::deserialize_packet<packets::ack_connect_packet_t>(
        logger, buffer, read_bytes);

    auto result = node.acknowledge_connect(peer);
    if (!result.is_success()) {
        return result;
    }

    logger.logDebug("Peer acknowledged.");
    return Result::Success();
}

Result leader_handler::handle(Node& node, logging::Logger& logger,
                              const domain::peer& /*peer*/, size_t read_bytes,
                              char* buffer, MessageSender& /*message_sender*/) {
    logger.logDebug("Received leader message.");

    auto leader_packet = handlers::deserialize_packet<packets::leader_packet_t>(
        logger, buffer, read_bytes);

    switch (leader_packet.synchronized) {
    case domain::local_synchronization::LEADER:
        logger.logDebug("Making this node a leader.");
        return node.set_leader();
    case domain::local_synchronization::NOT_SYNCHRONIZED:
        logger.logDebug("Stripping leader status.");
        return node.unset_leader();
    default:
        return Result::Failure("Invalid value of synchronized.");
    }
}

Result sync_start_handler::handle(Node& node, logging::Logger& logger,
                                  const domain::peer& peer, size_t read_bytes,
                                  char* buffer, MessageSender& message_sender) {
    logger.logDebug("Received sync_start message.");
    timestamp_t time_of_receiving_sync_start = node.get_synced_timestamp();

    auto sync_start_packet =
        handlers::deserialize_packet<packets::sync_start_packet_t>(
            logger, buffer, read_bytes);

    if (!node.has_peer(peer)) {
        return Result::Failure(PEER_NOT_KNOWN);
    }

    auto sync_result = node.start_sync(peer, sync_start_packet.synchronized,
                                       sync_start_packet.timestamp,
                                       time_of_receiving_sync_start);
    if (!sync_result.is_success()) {
        return sync_result;
    }

    delay_request_packet_t delay_request_packet{
        .message = packets::MSG_TYPE_DELAY_REQUEST};
    message_sender.send_message(peer, delay_request_packet);

    node.set_time_of_sending_response();
    logger.logDebug("Started synchronization with peer.");
    return Result::Success();
}

Result delay_request_handler::handle(Node& node, logging::Logger& logger,
                                     const domain::peer& peer,
                                     size_t read_bytes, char* buffer,
                                     MessageSender& message_sender) {
    logger.logDebug("Received delay request message.");
    timestamp_t currentTime = node.get_synced_timestamp();

    (void)handlers::deserialize_packet<packets::delay_request_packet_t>(
        logger, buffer, read_bytes);

    auto result = node.mark_sync_response(peer);
    if (!result.is_success()) {
        return result;
    }

    delay_response_packet_t delay_response_packet{
        .message = packets::MSG_TYPE_DELAY_RESPONSE,
        .synchronized = node.get_local_synchronization().get_synchronized(),
        .timestamp = currentTime};
    message_sender.send_message(peer, delay_response_packet);
    return Result::Success();
}

Result delay_response_handler::handle(Node& node, logging::Logger& logger,
                                      const domain::peer& peer,
                                      size_t read_bytes, char* buffer,
                                      MessageSender& /*message_sender*/) {
    logger.logDebug("Received delay response message.");

    auto delay_response_packet =
        handlers::deserialize_packet<packets::delay_response_packet_t>(
            logger, buffer, read_bytes);

    logger.logDebug("Current synchronized:",
                    (int)node.get_local_synchronization().get_synchronized());

    auto offset_result =
        node.finish_sync(peer, delay_response_packet.synchronized,
                         delay_response_packet.timestamp);
    if (!offset_result.is_success()) {
        return Result::Failure(offset_result.get_error_message());
    }

    logger.logDebug("New time: ", node.get_synced_timestamp());
    logger.logDebug("New synchronized:",
                    (int)node.get_local_synchronization().get_synchronized());
    logger.logDebug("Synchronization completed.");
    return Result::Success();
}

Result get_time_handler::handle(Node& node, logging::Logger& logger,
                                const domain::peer& peer, size_t read_bytes,
                                char* buffer, MessageSender& message_sender) {
    logger.logDebug("Received get_synced_timestamp.");

    (void)handlers::deserialize_packet<packets::get_time_packet_t>(
        logger, buffer, read_bytes);

    synchronized_t synchronized =
        node.get_local_synchronization().get_synchronized();
    timestamp_t time;
    if (synchronized == local_synchronization::NOT_SYNCHRONIZED) {
        time = node.get_absolute_timestamp();
    } else {
        time = node.get_synced_timestamp();
    }

    time_packet_t time_packet{.message = packets::MSG_TYPE_TIME,
                              .synchronized = synchronized,
                              .timestamp = time};

    message_sender.send_message(peer, time_packet);
    return Result::Success();
}

void send_hello(Node& node, logging::Logger& logger, const domain::peer& peer,
                MessageSender& message_sender) {
    logger.logDebug("Sending hello message to peer: ", peer);

    try {
        packets::hello_packet_t hello_packet;
        message_sender.send_message(peer, hello_packet);
    } catch (const std::exception& e) {
        logger.logError("Failed to send hello message: ", e.what());
        return;
    }

    node.add_waiting_for_hello_rsp(peer);
    logger.logDebug("Hello message sent to peer: ", peer);
}

void start_synchronization(Node& node, logging::Logger& logger,
                           MessageSender& message_sender) {
    logger.logDebug("Starting synchronization...");

    auto result = node.begin_sending_sync();
    if (!result.is_success()) {
        logger.logDebug("Cannot start synchronization: ",
                        result.get_error_message());
        return;
    }

    logger.logDebug("Synchronization started.");

    auto peers = node.get_peers();
    for (const auto& peer : peers) {
        logger.logDebug("Sending SYNC message to ", peer);
        try {
            node.mark_send_sync(peer);
            sync_start_packet_t sync_start_packet{
                .message = packets::MSG_TYPE_SYNC_START,
                .synchronized =
                    node.get_local_synchronization().get_synchronized(),
                .timestamp = node.get_synced_timestamp()};
            message_sender.send_message(peer, sync_start_packet);
        } catch (const std::exception& e) {
            logger.logError("Failed to send SYNC_START message: ", e.what());
        }
    }

    node.end_sending_sync();
    logger.logDebug("Ended sending sync.");
}

} // namespace handlers
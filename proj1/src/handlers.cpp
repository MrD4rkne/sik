#include "handlers.h"

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

    if (!message_sender.send_message(peer, hello_message_response)) {
        return Result::Failure("Failed to send hello response message.");
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

    logger.logDebug("Sending CONNECT messages to peers.");
    bool success = true;
    for (const auto& new_peer : hello_response_packet.peers) {
        logging::Logger peer_logger(new_peer);

        packets::connect_packet_t connect_packet;
        if (!message_sender.send_message(new_peer, connect_packet)) {
            peer_logger.logError("Failed to send CONNECT message.");
            success = false;
        }

        try {
            node.add_waiting_for_connect_ack(peer);
        } catch (const std::exception& e) {
            peer_logger.logError("Failed to note peer was sent connect msg: ",
                                 e.what());
            success = false;
        }
    }

    if (!success) {
        return Result::Failure("Failed to send CONNECT messages to all peers.");
    }

    logger.logDebug("Sent CONNECT messages to peers.");
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
    if (!message_sender.send_message(peer, ack_connect_packet)) {
        return Result::Failure("Failed to send ACK_CONNECT message.");
    }

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

    auto& local_sync = node.get_local_synchronization();
    switch (leader_packet.synchronized) {
    case domain::local_synchronization::LEADER:
        logger.logDebug("Making this node a leader.");
        return local_sync.set_leader(node.get_time());
    case domain::local_synchronization::NOT_SYNCHRONIZED:
        logger.logDebug("Stripping leader status.");
        return local_sync.unset_leader();
    default:
        return Result::Failure("Invalid value of synchronized.");
    }
}

Result sync_start_handler::handle(Node& node, logging::Logger& logger,
                                  const domain::peer& peer, size_t read_bytes,
                                  char* buffer, MessageSender& message_sender) {
    logger.logDebug("Received sync_start message.");
    timestamp_t time_of_receiving_sync_start = node.get_time();

    auto sync_start_packet =
        handlers::deserialize_packet<packets::sync_start_packet_t>(
            logger, buffer, read_bytes);

    if (!node.has_peer(peer)) {
        return Result::Failure(PEER_NOT_KNOWN);
    }

    auto& sync = node.get_local_synchronization();
    auto sync_result = sync.start_sync(peer, sync_start_packet.synchronized,
                                       sync_start_packet.timestamp,
                                       time_of_receiving_sync_start);
    if (!sync_result.is_success()) {
        return sync_result;
    }

    delay_request_packet_t delay_request_packet{
        .message = packets::MSG_TYPE_DELAY_REQUEST};
    if (!message_sender.send_message(peer, delay_request_packet)) {
        return Result::Failure("Failed to send DELAY_REQUEST message.");
    }

    timestamp_t time_of_sending_delay_request = node.get_time();
    logger.logDebug("Time of sending delay request: ",
                    time_of_sending_delay_request);
    sync.set_time_of_sending_response(time_of_sending_delay_request);
    logger.logDebug("Started synchronization with peer.");
    return Result::Success();
}

Result delay_request_handler::handle(Node& node, logging::Logger& logger,
                                     const domain::peer& peer,
                                     size_t read_bytes, char* buffer,
                                     MessageSender& message_sender) {
    logger.logDebug("Received delay request message.");
    timestamp_t currentTime = node.get_time();

    (void)handlers::deserialize_packet<packets::delay_request_packet_t>(
        logger, buffer, read_bytes);

    auto result = node.mark_sync_response(peer, currentTime);
    if (!result.is_success()) {
        return result;
    }

    delay_response_packet_t delay_response_packet{
        .message = packets::MSG_TYPE_DELAY_RESPONSE,
        .synchronized = node.get_local_synchronization().get_synchronized(),
        .timestamp = currentTime};
    if (!message_sender.send_message(peer, delay_response_packet)) {
        return Result::Failure("Failed to send DELAY_RESPONSE message.");
    }

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

    auto& sync = node.get_local_synchronization();
    auto currentTime = node.get_time();
    auto offset_result =
        sync.finish_sync(peer, currentTime, delay_response_packet.synchronized,
                         delay_response_packet.timestamp);
    if (!offset_result.is_success()) {
        return Result::Failure(offset_result.get_error_message());
    }

    auto offset = offset_result.get_value();

    logger.logDebug("Correcting time.");
    logger.logDebug("Current time: ", node.get_time());
    node.correct_time(offset);
    logger.logDebug("New time: ", node.get_time());
    logger.logDebug("Synchronization completed.");

    return Result::Success();
}

Result get_time_handler::handle(Node& node, logging::Logger& logger,
                                const domain::peer& peer, size_t read_bytes,
                                char* buffer, MessageSender& message_sender) {
    logger.logDebug("Received get_time.");

    (void)handlers::deserialize_packet<packets::get_time_packet_t>(
        logger, buffer, read_bytes);

    time_packet_t time_packet{
        .message = packets::MSG_TYPE_TIME,
        .synchronized = node.get_local_synchronization().get_synchronized(),
        .timestamp = node.get_time()};

    if (!message_sender.send_message(peer, time_packet)) {
        return Result::Failure("Failed to send time packet.");
    }

    return Result::Success();
}

bool send_hello(Node& node, logging::Logger& logger, const domain::peer& peer,
                MessageSender& message_sender) {
    packets::hello_packet_t hello_packet;
    if (!message_sender.send_message(peer, hello_packet)) {
        logger.logError("Failed to send hello message to peer.");
        return false;
    }

    node.add_waiting_for_hello_rsp(peer);
    return true;
}

void start_synchronization(Node& node, logging::Logger& logger,
                           MessageSender& message_sender) {
    auto result = node.begin_sending_sync();
    if (!result.is_success()) {
        logger.logDebug("Cannot start synchronization: ",
                        result.get_error_message());
        return;
    }

    try{
        logger.logDebug("Synchronization started.");

        auto peers = node.get_peers();
        for (const auto& peer : peers) {
            Logger peer_logger(peer);
            peer_logger.logDebug("Sending SYNC message");

            timestamp_t time = node.mark_send_sync(peer);
            sync_start_packet_t sync_start_packet{
                .message = packets::MSG_TYPE_SYNC_START,
                .synchronized = node.get_local_synchronization().get_synchronized(),
                .timestamp = time};

            if (!message_sender.send_message(peer, sync_start_packet)) {
                logger.logError("Failed to send SYNC_START message to peer.");
            }
        }
    }catch (const std::exception& e) {
        logger.logError("Failed to send SYNC_START message: ", e.what());
        node.end_sending_sync();
        throw;
    }

    node.end_sending_sync();
    logger.logDebug("Sent SYNC_START messages to peers.");
}

} // namespace handlers
#include "handlers.h"

namespace handlers {

bool hello_message_handler::handle(Node& node, logging::Logger& logger,
                                   const domain::peer& peer, size_t read_bytes,
                                   char* buffer,
                                   MessageSender& message_sender) {
    logger.logDebug("Received hello message.");

    auto* hello_packet = packets::deserialize_packet<packets::hello_packet_t>(
        buffer, read_bytes);
    if (hello_packet == nullptr) {
        logger.logError("Failed to parse hello packet.");
        return false;
    }

    logger.logDebug("Parsed hello packet.");

    if (node.has_peer(peer)) {
        logger.logDebug("Peer already exists.");
        return false;
    }

    std::string hello_message_response =
        packets::create_hello_response_packet(node.get_peers());

    logger.logDebug("Sending hello response message.");

    if (!message_sender.send_message(peer, hello_message_response.c_str(),
                                     hello_message_response.size())) {
        logger.logError(peer, "Failed to send hello response message.");
        return false;
    }

    logger.logDebug("Sent hello response message.");

    node.add_peer(peer);

    return true;
}

bool hello_response_handler::handle(Node& node, logging::Logger& logger,
                                    const domain::peer& peer, size_t read_bytes,
                                    char* buffer,
                                    MessageSender& message_sender) {
    logger.logDebug("Received hello response message.");

    std::vector<domain::peer> hello_response_packet =
        packets::parse_hello_response(buffer, read_bytes, logger);

    if constexpr (logging::LOG_DEBUG) {
        logger.logDebug("Parsed hello response packet.");
        logger.logDebug("Received peers: ");
        for (const auto& peer : hello_response_packet) {
            logger.logDebug(peer);
        }
    }

    try {
        node.acknowledge_hello_rsp(peer);
    } catch (const std::exception& e) {
        logger.logError("Failed to acknowledge hello rsp", e.what());
        return false;
    }

    logger.logDebug("Sending CONNECT messages to peers.");

    bool success = true;

    for (const auto& peer : hello_response_packet) {
        logging::Logger peer_logger(peer);
        peer_logger.logDebug("Sending CONNECT message to peer: ", peer);

        packets::connect_packet_t connect_packet;
        std::string connect_message =
            packets::serialize_packet(&connect_packet);
        if (!message_sender.send_message(peer, connect_message.c_str(),
                                         connect_message.size())) {
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

    return success;
}

bool connect_handler::handle(Node& node, logging::Logger& logger,
                             const domain::peer& peer, size_t read_bytes,
                             char* buffer, MessageSender& message_sender) {
    logger.logDebug("Received connect message.");

    auto* connect_packet =
        packets::deserialize_packet<packets::connect_packet_t>(buffer,
                                                               read_bytes);
    if (connect_packet == nullptr) {
        logger.logError("Failed to parse connect packet.");
        return false;
    }

    logger.logDebug("Parsed connect packet.");

    if (node.has_peer(peer)) {
        logger.logDebug("Peer already exists.");
        return false;
    }

    try {
        node.add_peer(peer);
    } catch (const std::exception& e) {
        logger.logError("Failed to add peer", e.what());
        return false;
    }

    packets::ack_connect_packet_t ack_connect_packet;
    std::string connect_message =
        packets::serialize_packet(&ack_connect_packet);
    if (!message_sender.send_message(peer, connect_message.c_str(),
                                     connect_message.size())) {
        logger.logError("Failed to send ACK_CONNECT message.");
        return false;
    }

    return true;
}

bool ack_connect_handler::handle(Node& node, logging::Logger& logger,
                                 const domain::peer& peer, size_t read_bytes,
                                 char* buffer,
                                 MessageSender& /*message_sender*/) {
    logger.logDebug("Received ack_connect message.");

    auto* connect_packet =
        packets::deserialize_packet<packets::ack_connect_packet_t>(buffer,
                                                                   read_bytes);
    if (connect_packet == nullptr) {
        logger.logError("Failed to parse ack_connect packet.");
        return false;
    }

    logger.logDebug("Parsed packet.");

    try {
        node.acknowledge_connect(peer);
    } catch (const std::exception& e) {
        logger.logError("Failed to get ack connect: ", e.what());
        return false;
    }

    return true;
}

bool leader_handler::handle(Node& node, logging::Logger& logger,
    const domain::peer& /*peer*/, size_t read_bytes,
    char* buffer,
    MessageSender& /*message_sender*/) {
    logger.logDebug("Received leader message.");

    auto* leader_packet =
        packets::deserialize_packet<packets::leader_packet_t>(buffer,
                                                               read_bytes);
    if (leader_packet == nullptr) {
        logger.logError("Failed to parse leader packet.");
        return false;
    }

    logger.logDebug("Parsed leader packet.");

    auto& local_sync = node.get_local_synchronization();

    switch(leader_packet->synchronized) {
        case domain::local_synchronization::LEADER:
            if (local_sync.is_leader()) {
                logger.logError("Already a leader.");
                return false;
            }

            logger.logDebug("Making this node a leader.");
            local_sync.set_leader(node.get_time());
            return true;
        case domain::local_synchronization::NOT_SYNCHRONIZED:
            if (!local_sync.is_leader()) {
                logger.logError("This node is not a leader.");
                return false;
            }

            logger.logDebug("Stripping leader status.");
            local_sync.unset_leader();
            return true;

        default:
            logger.logError("Invalid synchronization value: ", leader_packet->synchronized);
            return false;
    }
}

bool sync_start_handler::handle(Node& node, logging::Logger& logger,
    const domain::peer& peer, size_t read_bytes,
    char* buffer,
    MessageSender& message_sender) {
    logger.logDebug("Received sync_start message.");

    timestamp_t t2 = node.get_time();

    auto* sync_start_packet =
        packets::deserialize_packet<packets::sync_start_packet_t>(buffer,
                                                               read_bytes);
    if (sync_start_packet == nullptr) {
        logger.logError("Failed to parse sync_start packet.");
        return false;
    }

    logger.logDebug("Parsed sync_start packet.");

    if (!node.has_peer(peer)) {
        logger.logDebug("Unknown peer.");
        return false;
    }

    timestamp_t t3 = sync_start_packet->timestamp;

    if(!node.get_local_synchronization().start_sync(peer, sync_start_packet->synchronized,
                    sync_start_packet->timestamp, t2, t3)) {
        logger.logError("Failed to start synchronization.");
        return false;
    }

    delay_request_packet_t delay_request_packet{
        .message = packets::MSG_TYPE_DELAY_REQUEST
    };

    std::string delay_request_message =
        packets::serialize_packet(&delay_request_packet);

    if (!message_sender.send_message(peer, delay_request_message.c_str(),
                                     delay_request_message.size())) {
        logger.logError("Failed to send DELAY_REQUEST message.");
        return false;
    }

    logger.logDebug("Started synchronization with peer.");

    
    return true;
}

bool delay_request_handler::handle(Node& node, logging::Logger& logger,
    const domain::peer& peer, size_t read_bytes,
    char* buffer,
    MessageSender& message_sender) {
    logger.logDebug("Received delay request message.");

    timestamp_t currentTime = node.get_time();

    auto* delay_request_packet =
        packets::deserialize_packet<packets::delay_request_packet_t>(buffer,
                                                               read_bytes);
    if (delay_request_packet == nullptr) {
        logger.logError("Failed to parse delay request packet.");
        return false;
    }

    logger.logDebug("Parsed delay request packet.");

    if (!node.has_peer(peer)) {
        logger.logError("Unknown peer.");
        return false;
    }

    if (!node.validate_sync_request(peer, currentTime)) {
        logger.logError("Invalid delay request.");
        return false;
    }

    node.mark_sync_response(peer, currentTime);

    delay_response_packet_t delay_response_packet{
        .message = packets::MSG_TYPE_DELAY_RESPONSE,
        .synchronized = node.get_local_synchronization().get_synchronized(),
        .timestamp = currentTime
    };

    logger.logDebug("Sending delay response message.");

    logger.logDebug("Synchronized: ",
                             delay_response_packet.synchronized);
    logger.logDebug("Timestamp: ", delay_response_packet.timestamp);

    std::string sync_start_message =
        packets::serialize_packet(&delay_response_packet);

    if (!message_sender.send_message(peer, sync_start_message.c_str(),
                                     sync_start_message.size())) {
        logger.logError("Failed to send DELAY_RESPONSE message.");
        return false;
    }

    return true;
}

bool delay_response_handler::handle(Node& node, logging::Logger& logger,
    const domain::peer& peer, size_t read_bytes,
    char* buffer,
    MessageSender& /*message_sender*/) {
    logger.logDebug("Received delay response message.");

    timestamp_t t4 = node.get_time();

    auto* delay_response_packet =
        packets::deserialize_packet<packets::delay_response_packet_t>(buffer,
                                                               read_bytes);
    if (delay_response_packet == nullptr) {
        logger.logError("Failed to parse delay response packet.");
        return false;
    }

    logger.logDebug("Parsed delay response packet.");

    logger.logDebug("Synchronized: ",
                             delay_response_packet->synchronized);
    logger.logDebug("Timestamp: ", delay_response_packet->timestamp);

    if (!node.has_peer(peer)) {
        logger.logError("Unknown peer.");
        return false;
    }

    auto& sync = node.get_local_synchronization();

    if (!sync.is_sync_response_valid(peer, t4, delay_response_packet->synchronized)) {
        logger.logError("Invalid delay response.");
        return false;
    }

    sync.finish_sync(peer, t4, delay_response_packet->synchronized);
    return true;
}

void start_synchronization(Node& node, logging::Logger& logger,
                            MessageSender& message_sender) {
    if (!node.can_start_synchronization()) {
        logger.logDebug("Cannot start synchronization.");
        return;
    }

    node.send_begin_sync();

    logger.logDebug("Synchronization started.");

    auto peers = node.get_peers();
    for (const auto& peer : peers) {
        Logger peer_logger(peer);
        peer_logger.logDebug("Sending SYNC message");

        timestamp_t time = node.mark_send_sync(peer);
        sync_start_packet_t sync_start_packet{
            .message = packets::MSG_TYPE_SYNC_START,
            .synchronized = node.get_local_synchronization().get_synchronized(),
            .timestamp = time
        };

        peer_logger.logDebug("Synchronized: ",
                             sync_start_packet.synchronized);
        peer_logger.logDebug("Timestamp: ", sync_start_packet.timestamp);
        

        std::string sync_start_message = packets::serialize_packet(&sync_start_packet);
        if (!message_sender.send_message(peer, sync_start_message.c_str(),
                                         sync_start_message.size())) {
            logger.logError("Failed to send SYNC_START message to peer.");
        }
    }

    logger.logDebug("Sent SYNC_START messages to peers.");
}

} // namespace handlers
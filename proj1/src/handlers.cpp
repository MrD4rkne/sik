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
            local_sync.set_leader();
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

} // namespace handlers
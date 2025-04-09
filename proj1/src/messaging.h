#ifndef MESSAGES_H
#define MESSAGES_H

#include <cstdint>
#include <map>
#include <memory>
#include <netinet/in.h>
#include <string>

#include "domain.h"
#include "packets.h"

namespace messaging {

using namespace domain;

class MessageSender {
  public:
    MessageSender(int socket_fd, logging::Logger& logger)
        : socket_fd(socket_fd), logger(logger) {
    }

    bool send_message(domain::peer target, const char* buffer, size_t bytes_to_send);

  private:
    int socket_fd;
    logging::Logger& logger;
};

class MessageHandler {
  public:
    virtual bool handle(Node& node, logging::Logger& logger,
                        const domain::peer& peer, size_t read_bytes,
                        char* buffer, MessageSender& message_sender) = 0;

    virtual ~MessageHandler() = default;
};

template<typename T>
class MessageMediator {
  public:
    MessageMediator() = default;
    ~MessageMediator() = default;

    void register_handler(T message_type, MessageHandler* handler) {
        handlers.insert({message_type, handler});
    }

    bool handle_message(Node& node, logging::Logger& logger,
                        const domain::peer& peer, T message_type,
                        size_t read_bytes, char* buffer,
                        MessageSender& message_sender) {
        auto range = handlers.equal_range(message_type);
        bool success = range.first != range.second;

        for (auto it = range.first; it != range.second; ++it) {
            try {
                success &= it->second->handle(node, logger, peer, read_bytes,
                                              buffer, message_sender);
            } catch (const std::exception& e) {
                logger.logError(peer, "Exception: ", e.what());
                success &= false;
            }
        }

        return success;
    }

  private:
    std::multimap<T, MessageHandler*> handlers;
};

class hello_message_handler : public MessageHandler {
  public:
    bool handle(Node& node, logging::Logger& logger, const domain::peer& peer,
                size_t read_bytes, char* buffer,
                MessageSender& message_sender) override {
        logger.logDebug("Received hello message.");

        packets::hello_packet_t* hello_packet =
            packets::mappers::get_hello_packet(buffer, read_bytes);
        if (hello_packet == nullptr) {
            logger.logError("Failed to parse hello packet.");
            return false;
        }

        logger.logDebug("Parsed hello packet.");

        auto set = node.get_peers();
        std::vector<domain::peer> peers_vector(set.begin(), set.end());
        std::string hello_message_response =
            packets::mappers::create_hello_response_packet(peers_vector);

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
};

class hello_response_handler : public MessageHandler {
  public:
    bool handle(Node& node, logging::Logger& logger, const domain::peer& peer,
                size_t read_bytes, char* buffer,
                MessageSender& /*message_sender*/) override {
        logger.logDebug("Received hello response message.");

        try {
            std::vector<domain::peer> hello_response_packet =
                packets::mappers::parse_hello_response(buffer, read_bytes,
                                                       logger);

            if constexpr (logging::LOG_DEBUG) {
                logger.logDebug("Parsed hello response packet.");
                logger.logDebug("Received peers: ");
                for (const auto& peer : hello_response_packet) {
                    logger.logDebug(peer);
                }
            }


            logger.logDebug("Adding peers to node.");
            node.add_range(hello_response_packet);
            node.add_peer(peer);

            logger.logDebug("Sending CONNECT messages to peers.");
            for (const auto& peer : hello_response_packet) {
                logging::Logger peer_logger(peer);
                peer_logger.logDebug("Sending CONNECT message to peer: ", peer);
                // TODO: Implement CONNECT message sending
            }

        } catch (const std::exception& e) {
            logger.logError("Exception: ", e.what());
            return false;
        }

        return true;
    }
};

} // namespace messaging
#endif
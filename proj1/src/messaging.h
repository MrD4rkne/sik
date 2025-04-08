#ifndef MESSAGES_H
#define MESSAGES_H

#include <map>
#include <string>
#include <cstdint>
#include <netinet/in.h>
#include <memory>

#include "packets.h"
#include "domain.h"

namespace messaging {

using namespace domain;

class MessageSender{
    public:
    
    MessageSender(int socket_fd, sockaddr_in* address, logging::Logger &logger) : socket_fd(socket_fd), address(address), logger(logger) {
        if (address == nullptr) {
            throw std::invalid_argument("Address cannot be null");
        }
    }
    
    bool send_message(const char* buffer, size_t bytes_to_send) {
        logger.logDebug("Sending message of ", bytes_to_send, " bytes.");  
        
        logger.logDebug("Buffer: ", logging::parse(buffer, bytes_to_send));
        
        try {
            size_t total_sent = 0;
            ssize_t sent_bytes = 0;
            while (total_sent < bytes_to_send ) {
                sent_bytes = sendto(socket_fd, buffer + total_sent, bytes_to_send - total_sent, 0, (sockaddr*)address, sizeof(*address));
                if (sent_bytes < 0) {
                    logger.logError("Failed to send message: ", strerror(errno));
                    error("sendto");
                    return false;
                }
                total_sent += sent_bytes;
    
                logger.logDebug("Sent ", sent_bytes, " of ", bytes_to_send, " bytes.");
            }
    
            logger.logDebug("Sent total ", total_sent, " bytes.");
    
        } catch (const std::bad_alloc& e) {
            logger.logError("Memory allocation failed: ", e.what());
            return false;
        }
        return true;
    }
    
    private: 
    int socket_fd;
    sockaddr_in* address;
    logging::Logger& logger;
    };

class MessageHandler {
public:
    virtual bool handle(Node& node, logging::Logger &logger, const domain::peer &peer, size_t read_bytes, char* buffer, MessageSender& message_sender) = 0;

    virtual ~MessageHandler() = default;
};
    
template<typename T>
class MessageMediator{
    public:
        MessageMediator() = default;
        ~MessageMediator() = default;

        void register_handler(T message_type, MessageHandler* handler) {
            handlers.insert({message_type, handler});
        }

        bool handle_message(Node& node, logging::Logger &logger, const domain::peer &peer, T message_type, size_t read_bytes, char* buffer, MessageSender& message_sender) {
            auto range = handlers.equal_range(message_type);
            bool success = range.first != range.second;

            for (auto it = range.first; it != range.second; ++it) {
                try{
                    success &= it->second->handle(node, logger, peer, read_bytes, buffer, message_sender);
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
        bool handle(Node& node, logging::Logger &logger, const domain::peer &peer, size_t read_bytes, char* buffer, MessageSender& message_sender) override {
            logger.logDebug("Received hello message.");

            packets::hello_packet_t* hello_packet = packets::mappers::get_hello_packet(buffer, read_bytes);
            if (hello_packet == nullptr) {
                logger.logError("Failed to parse hello packet.");
                return false;
            }

            logger.logDebug("Parsed hello packet.");
            
            auto set = node.get_peers();
            std::vector<domain::peer> peers_vector(set.begin(), set.end());
            std::string hello_message_response = packets::mappers::create_hello_response_packet(peers_vector);

            logger.logDebug("Sending hello response message.");

            if (!message_sender.send_message(hello_message_response.c_str(), hello_message_response.size())) {
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
        bool handle(Node& node, logging::Logger &logger, const domain::peer &peer, size_t read_bytes, char* buffer, MessageSender& /*message_sender*/) override {
            logger.logDebug("Received hello response message.");

            try{
                std::vector<domain::peer> hello_response_packet = packets::mappers::parse_hello_response(buffer, read_bytes, logger);

                if constexpr (logging::LOG_DEBUG) {
                    logger.logDebug("Parsed hello response packet.");
                    logger.logDebug("Received peers: ");
                    for (const auto& peer : hello_response_packet) {
                        logger.logDebug(peer);
                    }
                }

                // TODO: add all or none
                for (const auto& peer : hello_response_packet) {
                    node.add_peer(peer);
                }
                node.add_peer(peer);

            } catch (const std::exception& e) {
                logger.logError("Exception: ", e.what());
                return false;
            }

            return true;
        }
    };


} // namespace messaging
#endif
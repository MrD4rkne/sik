#ifndef MESSAGES_H
#define MESSAGES_H

#include <map>
#include <set>
#include <string>
#include <cstdint>
#include <netinet/in.h>
#include <memory>

#include "packets.h"
#include "domain.h"

namespace messaging {

class Node{
public:
    Node() : peers{} {}

    void add_peer(domain::peer peer) {
        peers.insert(peer);
    }

    const std::set<domain::peer>& get_peers() const {
        return peers;
    }

private:
    std::set<domain::peer> peers;
};

class MessageSender{
    public:
    
    MessageSender(int socket_fd, sockaddr_in* address) : socket_fd(socket_fd), address(address) {
        if (address == nullptr) {
            throw std::invalid_argument("Address cannot be null");
        }
    }
    
    bool send_message(const char* buffer, size_t bytes_to_send) {
        logging::connection::logDebug(address, "Sending message of ", bytes_to_send, " bytes.");  
        
        logging::connection::logDebug(address, "Buffer: ", logging::parse(buffer, bytes_to_send));
        
        try {
            size_t total_sent = 0;
            ssize_t sent_bytes = 0;
            while (total_sent < bytes_to_send ) {
                sent_bytes = sendto(socket_fd, buffer + total_sent, bytes_to_send - total_sent, 0, (sockaddr*)address, sizeof(*address));
                if (sent_bytes < 0) {
                    error("sendto");
                    return false;
                }
                total_sent += sent_bytes;
    
                logging::connection::logDebug(address, "Sent ", sent_bytes, " of ", bytes_to_send, " bytes.");
            }
    
            logging::connection::logDebug(address, "Sent total ", total_sent, " bytes.");
    
        } catch (const std::bad_alloc& e) {
            logging::connection::logDebug(address, "Memory allocation failed: ", e.what());
            return false;
        }
        return true;
    }
    
    private: 
    int socket_fd;
    sockaddr_in* address;
    };

class MessageHandler {
public:
    virtual bool handle(Node& node, const domain::peer &peer, size_t read_bytes, char* buffer, MessageSender& message_sender) = 0;

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

        bool handle_message(Node& node, const domain::peer &peer, T message_type, size_t read_bytes, char* buffer, MessageSender& message_sender) {
            auto range = handlers.equal_range(message_type);
            bool success = range.first != range.second;

            for (auto it = range.first; it != range.second; ++it) {
                try{
                    success &= it->second->handle(node, peer, read_bytes, buffer, message_sender);
                } catch (const std::exception& e) {
                    logging::connection::logDebug(peer, "Exception: ", e.what());
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
        bool handle(Node& node, const domain::peer &peer, size_t read_bytes, char* buffer, MessageSender& message_sender) override {
            logging::connection::logDebug(peer, "Received hello message.");

            packets::hello_packet_t* hello_packet = packets::mappers::get_hello_packet(buffer, read_bytes);
            if (hello_packet == nullptr) {
                logging::connection::logDebug(peer, "Failed to parse hello packet.");
                return false;
            }

            logging::connection::logDebug(peer, "Parsed hello packet.");
            
            auto set = node.get_peers();
            std::vector<domain::peer> peers_vector(set.begin(), set.end());
            std::string hello_message_response = packets::mappers::create_hello_response_packet(peers_vector);

            logging::connection::logDebug(peer, "Sending hello response message.");

            if (!message_sender.send_message(hello_message_response.c_str(), hello_message_response.size())) {
                logging::connection::logDebug(peer, "Failed to send hello response message.");
                return false;
            }

            logging::connection::logDebug(peer, "Sent hello response message.");

            node.add_peer(peer);

            return true;
        }
    };

class hello_response_handler : public MessageHandler {
    public:
        bool handle(Node& node,  const domain::peer &peer, size_t read_bytes, char* buffer, MessageSender& /*message_sender*/) override {
            logging::connection::logDebug(peer, "Received hello response message.");

            try{
                std::vector<domain::peer> hello_response_packet = packets::mappers::parse_hello_response(buffer, read_bytes);

                if constexpr (logging::LOG_DEBUG) {
                    logging::connection::logDebug(peer, "Parsed hello response packet.");
                    logging::connection::logDebug(peer, "Received peers: ");
                    for (const auto& peer : hello_response_packet) {
                        logging::connection::logDebug(peer);
                    }
                }

                // TODO: add all or none
                for (const auto& peer : hello_response_packet) {
                    node.add_peer(peer);
                }
                node.add_peer(peer);

            } catch (const std::exception& e) {
                logging::connection::logDebug(peer, "Exception: ", e.what());
                return false;
            }

            return true;
        }
    };


} // namespace messaging
#endif
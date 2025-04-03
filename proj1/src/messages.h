#ifndef MESSAGES_H
#define MESSAGES_H

#include <map>
#include <set>
#include <string>
#include <cstdint>

class Node{
public:
    Node() : peers{} {}

    void add_peer(const std::string& ip, uint16_t port) {
        peers.insert({ip, port});
    }

    std::set<std::pair<std::string, uint16_t>> get_peers() const {
        return peers;
    }

private:
    std::set<std::pair<std::string, uint16_t>> peers;
};

class MessageHandler {
public:
    virtual bool handle(Node& node, const std::string& ip, uint16_t port, size_t read_bytes, char* buffer) = 0;

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

        bool handle_message(Node& node, const std::string& ip, uint16_t port, T message_type, size_t read_bytes, char* buffer) {
            auto range = handlers.equal_range(message_type);

            bool success = range.first != range.second;
            for (auto it = range.first; it != range.second; ++it) {
                success &= it->second->handle(node, ip, port, read_bytes, buffer);
            }

            return success;
        }

    private:
        std::multimap<T, MessageHandler*> handlers;
};

#endif
#ifndef PACKETS_H
#define PACKETS_H

#include <cstdint>
#include <vector>
#include <string>

namespace packets {
    using message_type_t = uint8_t;
    using count_t = uint16_t;
    using peer_address_length_t = uint8_t;
    using port_t = uint16_t;
    using timestamp_t = uint64_t;
    using synchronized_t = uint8_t;
    
    const inline uint8_t MSG_TYPE_UNKNOWN = 0x00;
    const inline uint8_t MSG_TYPE_HELLO = 0x01;
    const inline uint8_t MSG_TYPE_HELLO_RSP = 0x02;

    typedef struct {
        const message_type_t message = MSG_TYPE_HELLO;
    } __attribute__((__packed__)) hello_packet_t;

    struct peer_t {
        std::string peer_address;
        port_t peer_port;

        bool operator<(const peer_t& other) const {
            if (peer_address == other.peer_address) {
                return peer_port < other.peer_port;
            }
            return peer_address < other.peer_address;
        }

        bool operator==(const peer_t& other) const {
            return peer_address == other.peer_address && peer_port == other.peer_port;
        }
    };

    static inline bool is_valid_adress_length(peer_address_length_t length) {
        const peer_address_length_t IPV4_ADDRESS_LENGTH = 4;
        return length == IPV4_ADDRESS_LENGTH;
    }

    namespace mappers{
        inline message_type_t get_message_type(const char* buffer, size_t buffer_size){
            if (buffer_size < sizeof(message_type_t)) {
                return MSG_TYPE_UNKNOWN;
            }
            return static_cast<message_type_t>(buffer[0]);
        }

        inline std::vector<peer_t> parse_hello_response(const char* buffer, size_t buffer_size){
            size_t current_size = 0;
            if (buffer[current_size] != MSG_TYPE_HELLO_RSP) {
                throw std::runtime_error("Invalid message type.");
            }

            current_size += sizeof(message_type_t);

            count_t count = ntohs(*reinterpret_cast<const count_t*>(buffer + current_size));
            current_size += sizeof(count_t);

            std::vector<peer_t> peers(count);

            logging::logDebug("Parsing hello response with ", count, " peers.");
            
            for (count_t i = 0; i < count; ++i) {
                if (current_size + sizeof(peer_address_length_t) > buffer_size) {
                    throw std::runtime_error("Buffer size is too small to parse peer address length.");
                }

                peer_t peer;

                peer_address_length_t peer_address_length = (peer_address_length_t)buffer[current_size];
                current_size += sizeof(peer_address_length_t);

                if (!is_valid_adress_length(peer_address_length)) {
                    throw std::runtime_error("Invalid peer address length.");
                }

                if (current_size + peer_address_length > buffer_size) {
                    throw std::runtime_error("Buffer size is too small to parse peer address.");
                }

                uint32_t peer_address = ntohl(*(uint32_t*)(buffer + current_size));
                char str[INET_ADDRSTRLEN];
                if (inet_ntop(AF_INET, &peer_address, str, INET_ADDRSTRLEN) == nullptr) {
                    throw std::runtime_error("Failed to convert peer address to string.");
                }
                peer.peer_address = std::string(str);

                current_size += peer_address_length;

                if (current_size + sizeof(port_t) > buffer_size) {
                    throw std::runtime_error("Buffer size is too small to parse peer port.");
                }

                peer.peer_port = *(port_t*)(buffer + current_size);
                logging::logDebug("Port before: ", peer.peer_port);
                peer.peer_port = (port_t)htons(peer.peer_port);
                logging::logDebug("Port after: ", peer.peer_port);

                current_size += sizeof(port_t);

                peers[i] = peer;
            }

            return peers;
        }

        inline std::string create_hello_response_packet(std::vector<peer_t> peers) {
            size_t peers_size = 0;
            for (const auto& peer : peers) {
                peers_size += sizeof(peer_address_length_t); // Size for address length
                peers_size += 4;      // Size for the address string
                peers_size += sizeof(port_t);               // Size for the port
            }

            size_t packet_size = sizeof(message_type_t) + sizeof(count_t) + peers_size;
            std::string packet(packet_size, '\0');

            packet[0] = MSG_TYPE_HELLO_RSP;
            *reinterpret_cast<count_t*>(&packet[sizeof(message_type_t)]) = htons(peers.size());
            size_t current_size = sizeof(message_type_t) + sizeof(count_t);
            for (const auto& peer : peers) {
                in_addr addr;
                if (inet_pton(AF_INET, peer.peer_address.c_str(), &addr) != 1) {
                    throw std::runtime_error("Invalid IP address format.");
                }

                addr.s_addr = htonl(addr.s_addr);

                peer_address_length_t peer_address_length = sizeof(addr.s_addr);
                packet[current_size] = peer_address_length;
                current_size += sizeof(peer_address_length_t);

                memcpy(&packet[current_size], &addr, peer_address_length);
                current_size += peer_address_length;

                logging::logDebug("Port before: ", peer.peer_port);
                port_t peer_port = (port_t)htons(peer.peer_port);
                logging::logDebug("Port after: ", peer_port);

                memcpy(&packet[current_size], &peer_port, sizeof(port_t));
                current_size += sizeof(port_t);
            }
            return packet;
        }

        inline std::string create_hello_packet() {
            std::string packet(sizeof(hello_packet_t), '\0');
            packet[0] = MSG_TYPE_HELLO;
            return packet;
        }

        inline hello_packet_t* get_hello_packet(const char*, size_t buffer_size){
            if (buffer_size < sizeof(hello_packet_t)) {
                return nullptr;
            }
            
            hello_packet_t* packet = new hello_packet_t();
            return packet;
        }
    }
}

#endif
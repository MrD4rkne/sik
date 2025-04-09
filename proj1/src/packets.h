#ifndef PACKETS_H
#define PACKETS_H

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include <cstring>

#include "domain.h"
#include "logging.h"

namespace packets {
using namespace domain;

const inline uint8_t MSG_TYPE_UNKNOWN = 0x00;
const inline uint8_t MSG_TYPE_HELLO = 0x01;
const inline uint8_t MSG_TYPE_HELLO_RSP = 0x02;
const inline uint8_t MSG_TYPE_CONNECT = 0x03;
const inline uint8_t MSG_TYPE_ACK_CONNECT = 0x04;

typedef struct {
    const domain::message_type_t message = MSG_TYPE_HELLO;
} __attribute__((__packed__)) hello_packet_t;

typedef struct {
    const domain::message_type_t message = MSG_TYPE_CONNECT;
} __attribute__((__packed__)) connect_packet_t;

typedef struct {
    const domain::message_type_t message = MSG_TYPE_ACK_CONNECT;
} __attribute__((__packed__)) ack_connect_packet_t;

namespace mappers {
static inline bool is_valid_adress_length(peer_address_length_t length) {
    const peer_address_length_t IPV4_ADDRESS_LENGTH = 4;
    return length == IPV4_ADDRESS_LENGTH;
}

static inline peer parse_peer(const char* buffer, size_t buffer_size) {
    if (buffer_size < sizeof(peer_address_length_t)) {
        throw std::runtime_error("Buffer size is too small to parse peer.");
    }

    peer_address_length_t peer_address_length =
        (peer_address_length_t)buffer[0];
    if (!is_valid_adress_length(peer_address_length)) {
        throw std::runtime_error("Invalid peer address length.");
    }

    buffer += sizeof(peer_address_length_t);
    buffer_size -= sizeof(peer_address_length_t);

    if (buffer_size < peer_address_length + sizeof(port_t)) {
        throw std::runtime_error(
            "Buffer size is too small to parse peer address and port.");
    }

    std::vector<uint8_t> peer_address(peer_address_length);
    std::copy(buffer, buffer + peer_address_length, peer_address.begin());
    std::reverse(peer_address.begin(), peer_address.end());

    buffer += peer_address_length;
    buffer_size -= peer_address_length;

    if (buffer_size < sizeof(port_t)) {
        throw std::runtime_error(
            "Buffer size is too small to parse peer port.");
    }

    port_t peer_port = *(port_t*)buffer;
    peer_port = (port_t)ntohs(peer_port);

    return peer(peer_port, peer_address);
}

static inline void peer_to_network_order(const peer& peer, char* buffer,
                                         size_t buffer_size) {
    if (buffer_size < sizeof(peer_address_length_t) +
                          peer.get_address_length() + sizeof(port_t)) {
        throw std::runtime_error(
            "Buffer size is too small to convert peer to network order.");
    }

    peer_address_length_t peer_address_length = peer.get_address_length();
    buffer[0] = peer_address_length;

    buffer += sizeof(peer_address_length_t);

    auto adress = std::vector<uint8_t>(peer.get_address());
    std::reverse(adress.begin(), adress.end());

    std::copy(adress.begin(), adress.end(), buffer);

    buffer += peer_address_length;

    port_t peer_port = peer.get_port();
    peer_port = (port_t)htons(peer_port);
    memcpy(buffer, &peer_port, sizeof(port_t));
}

static inline size_t get_peer_size(const peer& peer) {
    return sizeof(peer_address_length_t) + peer.get_address_length() +
           sizeof(port_t);
}

inline message_type_t get_message_type(const char* buffer, size_t buffer_size) {
    if (buffer_size < sizeof(message_type_t)) {
        return MSG_TYPE_UNKNOWN;
    }
    return static_cast<message_type_t>(buffer[0]);
}

inline std::vector<peer> parse_hello_response(const char* buffer,
                                              size_t buffer_size,
                                              logging::Logger& logger) {
    size_t current_size = 0;
    if (buffer[current_size] != MSG_TYPE_HELLO_RSP) {
        throw std::runtime_error("Invalid message type.");
    }

    current_size += sizeof(message_type_t);

    count_t count =
        ntohs(*reinterpret_cast<const count_t*>(buffer + current_size));
    current_size += sizeof(count_t);

    std::vector<peer> peers;
    peers.reserve(count);

    logger.logDebug("Parsing hello response with ", count, " peers.");

    for (count_t i = 0; i < count; ++i) {
        try {
            peers.push_back(
                parse_peer(buffer + current_size, buffer_size - current_size));
            current_size += get_peer_size(peers[i]);
        } catch (const std::exception& e) {
            logger.logError("Error parsing peer ", i, e.what());
            throw;
        }
    }

    return peers;
}

inline std::string create_hello_response_packet(std::vector<peer> peers) {
    size_t peers_size = 0;
    for (const auto& peer : peers) {
        peers_size += get_peer_size(peer);
    }

    size_t packet_size = sizeof(message_type_t) + sizeof(count_t) + peers_size;
    std::string packet(packet_size, '\0');

    packet[0] = MSG_TYPE_HELLO_RSP;
    *reinterpret_cast<count_t*>(&packet[sizeof(message_type_t)]) =
        htons(peers.size());
    size_t current_size = sizeof(message_type_t) + sizeof(count_t);
    for (const auto& peer : peers) {
        peer_to_network_order(peer, &packet[current_size],
                              packet_size - current_size);
        current_size += get_peer_size(peer);
    }
    return packet;
}

template <typename PacketType>
inline PacketType* deserialize_packet(const char* buffer, size_t buffer_size) {
    if (buffer_size < sizeof(PacketType)) {
        return nullptr;
    }

    return reinterpret_cast<PacketType*>(const_cast<char*>(buffer));
}

template <typename PacketType>
inline std::string serialize_packet(PacketType* packet) {
    std::string serialized_packet(sizeof(PacketType), '\0');
    std::memcpy(&serialized_packet[0], packet, sizeof(PacketType));
    return serialized_packet;
}
} // namespace mappers
} // namespace packets

#endif
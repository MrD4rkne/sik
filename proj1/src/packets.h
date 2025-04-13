#ifndef PACKETS_H
#define PACKETS_H

#include <cstdint>
#include <cstring>
#include <vector>

#include "domain.h"
#include "logging.h"

namespace packets {

// Functions for network byte order conversion
inline static uint64_t htonll(uint64_t host_val) {
    return ((((uint64_t)htonl(host_val)) << 32) + htonl((host_val) >> 32));
}

inline static uint64_t ntohll(uint64_t net_val) {
    return ((((uint64_t)ntohl(net_val)) << 32) + ntohl((net_val) >> 32));
}

const inline uint8_t MSG_TYPE_UNKNOWN = 0;
const inline uint8_t MSG_TYPE_HELLO = 1;
const inline uint8_t MSG_TYPE_HELLO_RSP = 2;
const inline uint8_t MSG_TYPE_CONNECT = 3;
const inline uint8_t MSG_TYPE_ACK_CONNECT = 4;
const inline uint8_t MSG_TYPE_SYNC_START = 11;
const inline uint8_t MSG_TYPE_DELAY_REQUEST = 12;
const inline uint8_t MSG_TYPE_DELAY_RESPONSE = 13;
const inline uint8_t MSG_TYPE_LEADER = 21;

typedef struct {
    const domain::message_type_t message = MSG_TYPE_HELLO;
} __attribute__((__packed__)) hello_packet_t;

std::ostream& operator<<(std::ostream& os, const hello_packet_t& packet);

typedef struct {
    const domain::message_type_t message = MSG_TYPE_CONNECT;
} __attribute__((__packed__)) connect_packet_t;

std::ostream& operator<<(std::ostream& os, const connect_packet_t& packet);

typedef struct {
    const domain::message_type_t message = MSG_TYPE_ACK_CONNECT;
} __attribute__((__packed__)) ack_connect_packet_t;

std::ostream& operator<<(std::ostream& os, const ack_connect_packet_t& packet);

typedef struct {
    const domain::message_type_t message = MSG_TYPE_LEADER;
    domain::synchronized_t synchronized;
} __attribute__((__packed__)) leader_packet_t;

std::ostream& operator<<(std::ostream& os, const leader_packet_t& packet);

typedef struct {
    const domain::message_type_t message = MSG_TYPE_SYNC_START;
    domain::synchronized_t synchronized;
    natural_time::timestamp_t timestamp;
} __attribute__((__packed__)) sync_start_packet_t;

std::ostream& operator<<(std::ostream& os, const sync_start_packet_t& packet);

typedef struct {
    const domain::message_type_t message = MSG_TYPE_DELAY_REQUEST;
} __attribute__((__packed__)) delay_request_packet_t;

std::ostream& operator<<(std::ostream& os,
                         const delay_request_packet_t& packet);

typedef struct {
    const domain::message_type_t message = MSG_TYPE_DELAY_RESPONSE;
    domain::synchronized_t synchronized;
    natural_time::timestamp_t timestamp;
} __attribute__((__packed__)) delay_response_packet_t;

std::ostream& operator<<(std::ostream& os,
                         const delay_response_packet_t& packet);

domain::message_type_t get_message_type(const char* buffer, size_t buffer_size);

std::vector<domain::peer> parse_hello_response(const char* buffer,
                                               size_t buffer_size,
                                               logging::Logger& logger);

std::string create_hello_response_packet(std::vector<domain::peer> peers);

template<typename PacketType>
inline PacketType* deserialize_packet(const char* buffer, size_t buffer_size) {
    if (buffer_size < sizeof(PacketType)) {
        return nullptr;
    }

    PacketType* packet =
        reinterpret_cast<PacketType*>(const_cast<char*>(buffer));

    if constexpr (std::is_same_v<PacketType, sync_start_packet_t> ||
                  std::is_same_v<PacketType, delay_response_packet_t>) {
        packet->timestamp =
            (natural_time::timestamp_t)ntohll(packet->timestamp);
    }

    return packet;
}

template<typename PacketType>
inline std::string serialize_packet(PacketType* packet) {
    if constexpr (std::is_same_v<PacketType, sync_start_packet_t> ||
                  std::is_same_v<PacketType, delay_response_packet_t>) {
        packet->timestamp =
            (natural_time::timestamp_t)htonll(packet->timestamp);
    }

    std::string serialized_packet(sizeof(PacketType), '\0');
    std::memcpy(&serialized_packet[0], packet, sizeof(PacketType));
    return serialized_packet;
}

} // namespace packets

#endif
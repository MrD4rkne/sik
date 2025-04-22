#ifndef PACKETS_H
#define PACKETS_H

#include <cstdint>
#include <cstring>
#include <netinet/in.h>
#include <vector>

#include "domain.h"
#include "logging.h"

namespace packets::details {
inline uint64_t htonll(uint64_t host_val) {
    return ((((uint64_t)htonl(host_val)) << 32) + htonl((host_val) >> 32));
}

inline uint64_t ntohll(uint64_t net_val) {
    return ((((uint64_t)htonl(net_val)) << 32) + htonl((net_val) >> 32));
}
} // namespace packets::details

namespace packets {

const inline uint8_t MSG_TYPE_UNKNOWN = 0;
const inline uint8_t MSG_TYPE_HELLO = 1;
const inline uint8_t MSG_TYPE_HELLO_RSP = 2;
const inline uint8_t MSG_TYPE_CONNECT = 3;
const inline uint8_t MSG_TYPE_ACK_CONNECT = 4;
const inline uint8_t MSG_TYPE_SYNC_START = 11;
const inline uint8_t MSG_TYPE_DELAY_REQUEST = 12;
const inline uint8_t MSG_TYPE_DELAY_RESPONSE = 13;
const inline uint8_t MSG_TYPE_LEADER = 21;
const inline uint8_t MSG_TYPE_GET_TIME = 31;
const inline uint8_t MSG_TYPE_TIME = 32;

typedef struct hello_packet {
    const domain::message_type_t message = MSG_TYPE_HELLO;
} __attribute__((__packed__)) hello_packet_t;
std::ostream& operator<<(std::ostream& os, const hello_packet_t& packet);

typedef struct hello_response_packet {
    const domain::message_type_t message = MSG_TYPE_HELLO_RSP;
    const std::vector<domain::peer> peers;
} hello_response_packet_t;
std::ostream& operator<<(std::ostream& os,
                         const hello_response_packet_t& packet);

typedef struct connect_packet {
    const domain::message_type_t message = MSG_TYPE_CONNECT;
} __attribute__((__packed__)) connect_packet_t;

std::ostream& operator<<(std::ostream& os, const connect_packet_t& packet);

typedef struct ack_connect_packet {
    const domain::message_type_t message = MSG_TYPE_ACK_CONNECT;
} __attribute__((__packed__)) ack_connect_packet_t;

std::ostream& operator<<(std::ostream& os, const ack_connect_packet_t& packet);

typedef struct leader_packet {
    const domain::message_type_t message = MSG_TYPE_LEADER;
    domain::synchronized_t synchronized;
} __attribute__((__packed__)) leader_packet_t;

std::ostream& operator<<(std::ostream& os, const leader_packet_t& packet);

typedef struct sync_start_packet {
    const domain::message_type_t message = MSG_TYPE_SYNC_START;
    domain::synchronized_t synchronized;
    natural_time::timestamp_t timestamp;
} __attribute__((__packed__)) sync_start_packet_t;

std::ostream& operator<<(std::ostream& os, const sync_start_packet_t& packet);

typedef struct delay_request_packet {
    const domain::message_type_t message = MSG_TYPE_DELAY_REQUEST;
} __attribute__((__packed__)) delay_request_packet_t;

std::ostream& operator<<(std::ostream& os,
                         const delay_request_packet_t& packet);

typedef struct delay_response_packet {
    const domain::message_type_t message = MSG_TYPE_DELAY_RESPONSE;
    domain::synchronized_t synchronized;
    natural_time::timestamp_t timestamp;
} __attribute__((__packed__)) delay_response_packet_t;

std::ostream& operator<<(std::ostream& os,
                         const delay_response_packet_t& packet);

typedef struct get_time_packet {
    const domain::message_type_t message = MSG_TYPE_GET_TIME;
} __attribute__((__packed__)) get_time_packet_t;
std::ostream& operator<<(std::ostream& os, const get_time_packet_t& packet);

typedef struct time_packet {
    const domain::message_type_t message = MSG_TYPE_TIME;
    domain::synchronized_t synchronized;
    natural_time::timestamp_t timestamp;
} __attribute__((__packed__)) time_packet_t;
std::ostream& operator<<(std::ostream& os, const time_packet_t& packet);

domain::message_type_t get_message_type(const char* buffer, size_t buffer_size);

template<typename PacketType>
struct mappers {

    /// @brief Serialize a packet to a string.
    /// @param packet The packet to serialize.
    /// @return The serialized packet as a string.
    static std::string serialize_packet(PacketType& packet) {
        if constexpr (std::is_same_v<PacketType, sync_start_packet_t> ||
                      std::is_same_v<PacketType, delay_response_packet_t> ||
                      std::is_same_v<PacketType, time_packet_t>) {
            packet.timestamp =
                (natural_time::timestamp_t)packets::details::htonll(
                    packet.timestamp);
        }

        std::string serialized_packet(sizeof(PacketType), '\0');
        std::memcpy(&serialized_packet[0], &packet, sizeof(PacketType));
        return serialized_packet;
    }

    /// @brief Deserialize a packet from a buffer.
    /// @param buffer The buffer containing the serialized packet.
    /// @param buffer_size The size of the buffer.
    /// @return The deserialized packet.
    /// @throws std::invalid_argument if the buffer size is not equal to the packet size.
    static PacketType deserialize_packet(const char* buffer,
                                         size_t buffer_size) {
        if (buffer_size != sizeof(PacketType)) {
            throw std::invalid_argument(
                "Buffer size is not equal to packet size.");
        }

        PacketType packet;
        std::memcpy(&packet, buffer, sizeof(PacketType));

        if constexpr (std::is_same_v<PacketType, sync_start_packet_t> ||
                      std::is_same_v<PacketType, delay_response_packet_t>) {
            packet.timestamp =
                (natural_time::timestamp_t)packets::details::ntohll(
                    packet.timestamp);
        }

        return packet;
    }
};

template<>
struct mappers<hello_response_packet_t> {
    /// @brief Serialize a packet to a string.
    /// @param peers The packet to serialize.
    /// @return The serialized packet as a string.
    static std::string serialize_packet(hello_response_packet_t& peers);

    /// @brief Deserialize a packet from a buffer.
    /// @param buffer The buffer containing the serialized packet.
    /// @param buffer_size The size of the buffer.
    /// @return The deserialized packet.
    /// @throws std::invalid_argument if the buffer contains an invalid message.
    static hello_response_packet_t deserialize_packet(const char* buffer,
                                                      size_t buffer_size);
};

} // namespace packets

#endif
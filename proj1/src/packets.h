#ifndef PACKETS_H
#define PACKETS_H

#include <cstdint>
#include <cstring>
#include <vector>

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

message_type_t get_message_type(const char* buffer, size_t buffer_size);

std::vector<peer> parse_hello_response(const char* buffer, size_t buffer_size,
                                       logging::Logger& logger);

std::string create_hello_response_packet(std::vector<peer> peers);

template<typename PacketType>
inline PacketType* deserialize_packet(const char* buffer, size_t buffer_size) {
    if (buffer_size < sizeof(PacketType)) {
        return nullptr;
    }

    return reinterpret_cast<PacketType*>(const_cast<char*>(buffer));
}

template<typename PacketType>
inline std::string serialize_packet(PacketType* packet) {
    std::string serialized_packet(sizeof(PacketType), '\0');
    std::memcpy(&serialized_packet[0], packet, sizeof(PacketType));
    return serialized_packet;
}

} // namespace packets

#endif
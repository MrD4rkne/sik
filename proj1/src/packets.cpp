#include <algorithm>
#include <cstring>
#include <netinet/in.h>

#include "packets.h"

namespace packets {

using namespace domain;

static inline bool is_valid_adress(domain::peer_address_length_t length,
                                   const std::array<uint8_t, 4>& address) {
    const domain::peer_address_length_t IPV4_ADDRESS_LENGTH = 4;
    return length == IPV4_ADDRESS_LENGTH && address.size() == length;
}

static inline bool is_vlaid_port(domain::port_t port) {
    return port != 0;
}

static inline peers::peer parse_peer(const char* buffer, size_t buffer_size) {
    if (buffer_size < sizeof(domain::peer_address_length_t)) {
        throw std::invalid_argument("Buffer size is too small to parse peer.");
    }

    peer_address_length_t peer_address_length =
        static_cast<peer_address_length_t>(buffer[0]);

    buffer += sizeof(peer_address_length_t);
    buffer_size -= sizeof(peer_address_length_t);

    if (buffer_size < peer_address_length + sizeof(port_t)) {
        throw std::invalid_argument(
            "Buffer size is too small to parse peer address and port.");
    }

    std::array<uint8_t, 4> peer_address;
    std::copy_n(buffer, peer_address_length, peer_address.data());
    if (!is_valid_adress(peer_address_length, peer_address)) {
        throw std::invalid_argument("Invalid peer address.");
    }

    buffer += peer_address_length;
    buffer_size -= peer_address_length;

    if (buffer_size < sizeof(port_t)) {
        throw std::invalid_argument(
            "Buffer size is too small to parse peer port.");
    }

    const port_t peer_port = ntohs(*reinterpret_cast<const port_t*>(buffer));
    if (!is_vlaid_port(peer_port)) {
        throw std::invalid_argument("Invalid peer port.");
    }

    return peers::peer(peer_port, peer_address);
}

static inline void peer_to_network_order(const peers::peer& peer, char* buffer,
                                         size_t buffer_size) {
    const size_t required_size =
        sizeof(peer_address_length_t) +
        static_cast<peer_address_length_t>(peer.get_address().size()) +
        sizeof(port_t);
    if (buffer_size < required_size) {
        throw std::runtime_error(
            "Buffer size is too small to convert peer to network order.");
    }

    peer_address_length_t peer_address_length =
        static_cast<peer_address_length_t>(peer.get_address().size());
    memcpy(buffer, &peer_address_length, sizeof(peer_address_length_t));

    buffer += sizeof(peer_address_length_t);

    auto address = peer.get_address();
    std::copy(address.begin(), address.end(), buffer);

    buffer += peer_address_length;

    port_t peer_port = htons(peer.get_port());
    memcpy(buffer, &peer_port, sizeof(port_t));
}

static inline size_t get_peer_size(const peers::peer& peer) {
    return sizeof(peer_address_length_t) + peer.get_address().size() +
           sizeof(port_t);
}

message_type_t get_message_type(const char* buffer, size_t buffer_size) {
    return (buffer_size < sizeof(message_type_t))
               ? MSG_TYPE_UNKNOWN
               : static_cast<message_type_t>(buffer[0]);
}

hello_response_packet_t
mappers<hello_response_packet_t>::deserialize_packet(const char* buffer,
                                                     size_t buffer_size) {
    size_t current_size = 0;
    if (buffer[current_size] != MSG_TYPE_HELLO_RSP) {
        throw std::invalid_argument("Invalid message type.");
    }

    current_size += sizeof(message_type_t);

    count_t count =
        ntohs(*reinterpret_cast<const count_t*>(buffer + current_size));
    current_size += sizeof(count_t);

    std::vector<peers::peer> peers;
    peers.reserve(count);

    for (count_t i = 0; i < count; ++i) {
        peers.push_back(
            parse_peer(buffer + current_size, buffer_size - current_size));
        current_size += get_peer_size(peers[i]);
    }

    if (current_size != buffer_size) {
        throw std::invalid_argument("Buffer size does not match packet size.");
    }

    hello_response_packet_t hello_response_packet{
        .message = MSG_TYPE_HELLO_RSP,
        .peers = std::move(peers),
    };

    return hello_response_packet;
}

std::string mappers<hello_response_packet_t>::serialize_packet(
    hello_response_packet_t& hello_response_packet) {
    size_t peers_size = 0;
    for (const auto& peer : hello_response_packet.peers) {
        peers_size += get_peer_size(peer);
    }

    size_t packet_size = sizeof(message_type_t) + sizeof(count_t) + peers_size;
    std::string packet(packet_size, '\0');

    packet[0] = MSG_TYPE_HELLO_RSP;
    *reinterpret_cast<count_t*>(&packet[sizeof(message_type_t)]) =
        htons(hello_response_packet.peers.size());
    size_t current_size = sizeof(message_type_t) + sizeof(count_t);
    for (const auto& peer : hello_response_packet.peers) {
        peer_to_network_order(peer, &packet[current_size],
                              packet_size - current_size);
        current_size += get_peer_size(peer);
    }
    return packet;
}

std::ostream& operator<<(std::ostream& os, const hello_packet_t& packet) {
    os << "Hello Packet: ";
    os << "Message Type: " << static_cast<int>(packet.message);
    return os;
}

std::ostream& operator<<(std::ostream& os,
                         const hello_response_packet_t& packet) {
    os << "Hello Response Packet: ";
    os << "Count: " << packet.peers.size() << " peers:" << std::endl;
    for (const auto& peer : packet.peers) {
        os << "Peer: " << peer << std::endl;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const connect_packet_t& packet) {
    os << "Connect Packet: ";
    os << "Message Type: " << static_cast<int>(packet.message);
    return os;
}

std::ostream& operator<<(std::ostream& os, const ack_connect_packet_t& packet) {
    os << "Ack Connect Packet: ";
    os << "Message Type: " << static_cast<int>(packet.message);
    return os;
}

std::ostream& operator<<(std::ostream& os, const leader_packet_t& packet) {
    os << "Leader Packet: ";
    os << "Message Type: " << static_cast<int>(packet.message);
    os << ", Synchronized: " << static_cast<int>(packet.synchronized);
    return os;
}

std::ostream& operator<<(std::ostream& os, const sync_start_packet_t& packet) {
    os << "Sync Start Packet: ";
    os << "Message Type: " << static_cast<int>(packet.message);
    os << ", Synchronized: " << static_cast<int>(packet.synchronized);
    os << ", Timestamp: " << packet.timestamp;
    return os;
}

std::ostream& operator<<(std::ostream& os,
                         const delay_request_packet_t& packet) {
    os << "Delay Request Packet: ";
    os << "Message Type: " << static_cast<int>(packet.message);
    return os;
}

std::ostream& operator<<(std::ostream& os,
                         const delay_response_packet_t& packet) {
    os << "Delay Response Packet: ";
    os << "Message Type: " << static_cast<int>(packet.message);
    os << ", Synchronized: " << static_cast<int>(packet.synchronized);
    os << ", Timestamp: " << packet.timestamp;
    return os;
}

std::ostream& operator<<(std::ostream& os, const get_time_packet_t& packet) {
    os << "Get time packet: ";
    os << "Message Type: " << static_cast<int>(packet.message);
    return os;
}

std::ostream& operator<<(std::ostream& os, const time_packet_t& packet) {
    os << "Time Packet: ";
    os << "Message Type: " << static_cast<int>(packet.message);
    os << ", Synchronized: " << static_cast<int>(packet.synchronized);
    os << ", Timestamp: " << packet.timestamp;
    return os;
}

} // namespace packets
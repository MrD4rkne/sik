#include "messaging.h"
#include <cstring>
#include <memory>
#include <netinet/in.h>
#include <array>

#include "domain.h"
#include "err.h"
#include "logging.h"
#include "packets.h"

namespace messaging {

static inline constexpr size_t MAX_CONTENT_SIZE = 65507; // Maximum UDP packet size

static inline void get_peer_address(const domain::peer& target,
                                    sockaddr_in& address,
                                    socklen_t& address_len) {
    constexpr socklen_t IPV4_ADDRESS_LENGTH = sizeof(sockaddr_in::sin_addr) / sizeof(uint8_t);

    if (target.get_address_length() != IPV4_ADDRESS_LENGTH) {
        throw std::runtime_error("Invalid peer address length.");
    }

    address.sin_family = AF_INET;
    address.sin_port = htons(target.get_port());

    std::array<uint8_t, IPV4_ADDRESS_LENGTH> address_bytes = {0};
    auto& target_address = target.get_address();
    std::copy(target_address.begin(), target_address.end(), address_bytes.begin());
    std::memcpy(&(address.sin_addr), address_bytes.data(), sizeof(address.sin_addr));

    address_len = sizeof(sockaddr_in);
}

bool MessageSender::send_message(domain::peer target, const char* buffer,
                                 size_t bytes_to_send) {
    logger.logDebug("Sending message of ", bytes_to_send, " bytes.");

    logger.logDebug("Buffer: ", logging::parse(buffer, bytes_to_send));

    if (bytes_to_send > MAX_CONTENT_SIZE) {
        logger.logError("Message size exceeds maximum limit.");
        return false;
    }

    sockaddr_in address;
    socklen_t address_len = 0;
    try {
        get_peer_address(target, address, address_len);
    } catch (const std::exception& e) {
        this->logger.logError("Address preparation failed: ", e.what());
        return false;
    }

    size_t total_sent = 0;
    while (total_sent < bytes_to_send) {
        ssize_t sent_bytes = sendto(this->socket_fd, buffer + total_sent,
                                    bytes_to_send - total_sent, 0,
                                    (sockaddr*)&address, address_len);
        if (sent_bytes < 0) {
            this->logger.logError("Failed to send message: ", strerror(errno));
            error("sendto");
            return false;
        }
        total_sent += (size_t)sent_bytes;

        this->logger.logDebug("Sent ", sent_bytes, " of ", bytes_to_send,
                              " bytes.");
    }

    this->logger.logDebug("Sent total ", total_sent, " bytes.");
    return true;
}
} // namespace messaging
#ifndef LOGGING_H
#define LOGGING_H

#include <iostream>
#include <iomanip>
#include <string>
#include <arpa/inet.h>
#include <sstream>

namespace logging {

#ifdef DEBUG
constexpr inline bool LOG_DEBUG = true;
#else
constexpr inline bool LOG_DEBUG = false;
#endif

    template <typename... Args>
    inline void logDebug(const Args&... args) {
        if constexpr (LOG_DEBUG) {
            std::cerr << "[DEBUG] ";
            (std::cerr << ... << args) << std::endl;
        }
    }

    inline std::string parse(const char* buffer, size_t buffer_size) {
        std::stringstream ss;
        for(size_t i = 0; i < buffer_size; ++i) {
            if (i > 0) {
                ss << " ";
            }
            ss << (int)buffer[i];
        }

        return ss.str();
    }

    namespace connection {

        template <typename... Args>
        inline void logDebugWithIp(const char* client_ip, uint16_t client_port, const Args&... args) {
            if constexpr (LOG_DEBUG) {
                logging::logDebug("[", client_ip, ":", client_port, "] ", args...);
            }
        }

        template <typename... Args>
        inline void logDebug(const sockaddr_in* client_address, const Args&... args) {
            if (!client_address) {
                logging::logDebug("Client address is null.");
                return;
            }

            char const *client_ip = inet_ntoa(client_address->sin_addr);
            uint16_t client_port = ntohs(client_address->sin_port);

            connection::logDebugWithIp(client_ip, client_port, args...);
        }
    } // namespace connection

    /// @brief Log a bad message received from the client. Prints "ERROR MSG" followed by the hex representation of the first few bytes of the message.
    /// @param bytes_received the number of bytes received
    /// @param buffer the buffer containing the prefix of the message
    /// @param max_bytes the maximum number of bytes from message to log (default is 10)
    inline void log_bad_message(size_t bytes_received, const char* buffer, size_t max_bytes = 10) {
        std::cerr << "ERROR MSG ";
        for (size_t i = 0; i < bytes_received && i < max_bytes; ++i) {
            std::cerr << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(static_cast<unsigned char>(buffer[i]));
        }
        std::cerr << std::dec << std::endl;
    }
} // namespace logging

#endif
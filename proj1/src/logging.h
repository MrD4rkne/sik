#ifndef LOGGING_H
#define LOGGING_H

#include <iostream>
#include <iomanip>
#include <string>

namespace logging {

#ifdef DEBUG
constexpr inline bool LOG_DEBUG = true;
#else
constexpr inline bool LOG_DEBUG = false;
#endif

    template <typename... Args>
    static inline void logDebug(Args&&... args) {
        if constexpr (LOG_DEBUG) {
            std::cerr << "[DEBUG] ";
            (std::cerr << ... << std::forward<Args>(args)) << std::endl;
        }
    }

    namespace connection {
        template <typename... Args>
        static inline void logDebug(const char* client_ip, uint16_t client_port, Args&&... args) {
            if constexpr (LOG_DEBUG) {
                std::cerr << "[DEBUG] [" << client_ip << ":" << client_port << "] ";
                (std::cerr << ... << std::forward<Args>(args)) << std::endl;
            }
        }
    } // namespace connection

    /// @brief Log a bad message received from the client. Prints "ERROR MSG" followed by the hex representation of the first few bytes of the message.
    /// @param bytes_received the number of bytes received
    /// @param buffer the buffer containing the prefix of the message
    /// @param max_bytes the maximum number of bytes from message to log (default is 10)
    static inline void log_bad_message(size_t bytes_received, const char* buffer, size_t max_bytes = 10) {
        std::cerr << "ERROR MSG ";
        for (size_t i = 0; i < bytes_received && i < max_bytes; ++i) {
            std::cerr << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(static_cast<unsigned char>(buffer[i]));
        }
        std::cerr << std::dec << std::endl;
    }
} // namespace logging

#endif
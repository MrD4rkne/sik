#ifndef LOGGING_H
#define LOGGING_H

#include <iostream>
#include <string>

#ifdef DEBUG
static constexpr inline bool LOG_DEBUG = true;
#else
static constexpr inline bool LOG_DEBUG = false;
#endif

namespace logging {
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
} // namespace logging

#endif
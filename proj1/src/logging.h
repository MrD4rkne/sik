#ifndef LOGGING_H
#define LOGGING_H

#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

#include "domain.h"

namespace logging {

#ifdef DEBUG
constexpr inline bool LOG_DEBUG = true;
#else
constexpr inline bool LOG_DEBUG = false;
#endif

class Logger {
  public:
    Logger(const domain::peer& peer)
        : peer(std::make_shared<domain::peer>(peer)), is_peer_set(true) {
    }
    Logger() : peer(nullptr), is_peer_set(false) {
    }

    template<typename... Args>
    void logDebug(const Args&... args) {
        if (!is_debug_enabled) {
            return;
        }

        const static std::string debug_level = "DEBUG";
        log(debug_level, args...);
    }

    template<typename... Args>
    void logError(const Args&... args) {
        if (!is_debug_enabled) {
            return;
        }

        const static std::string error_level = "ERROR";
        log(error_level, args...);
    }

    template<typename... Args>
    void logWarning(const Args&... args) {
        if (!is_debug_enabled) {
            return;
        }

        const static std::string error_level = "WARNING";
        log(error_level, args...);
    }

    /// @brief Log a bad message received from the client. Prints "ERROR MSG"
    /// followed by the hex representation of the first few bytes of the
    /// message.
    /// @param bytes_received the number of bytes received
    /// @param buffer the buffer containing the prefix of the message
    /// @param max_bytes the maximum number of bytes from message to log
    /// (default is 10)
    inline void log_bad_message(size_t bytes_received, const char* buffer,
                                size_t max_bytes = 10) {
        std::cerr << "ERROR MSG ";
        for (size_t i = 0; i < bytes_received && i < max_bytes; ++i) {
            std::cerr << std::hex << std::setfill('0') << std::setw(2)
                      << static_cast<int>(
                             static_cast<unsigned char>(buffer[i]));
        }
        std::cerr << std::dec << std::endl;
    }

  private:
    std::shared_ptr<domain::peer> peer;
    bool is_peer_set;
    std::ostream& out = std::cout;
    bool is_debug_enabled = LOG_DEBUG;

    template<typename... Args>
    void log(const std::string& level, const Args&... args) {
        out << "[" << level << "] ";

        if (is_peer_set) {
            out << "[" << *peer << "]: ";
        }

        (out << ... << args) << std::endl;
    }
};

inline std::string parse(const char* buffer, size_t buffer_size) {
    std::stringstream ss;
    for (size_t i = 0; i < buffer_size; ++i) {
        if (i > 0) {
            ss << " ";
        }
        ss << (int)buffer[i];
    }

    return ss.str();
}

} // namespace logging

#endif
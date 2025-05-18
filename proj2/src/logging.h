#ifndef LOGGING_H
#define LOGGING_H

#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

#include "ip.h"

namespace logging {

#ifdef DEBUG
constexpr inline bool LOG_DEBUG = true;
#else
constexpr inline bool LOG_DEBUG = false;
#endif

class Logger {
  public:
    Logger(const std::string& prefix) : prefix(prefix), out(std::cerr) {
    }

    Logger() : out(std::cerr) {
    }

    template<typename... Args>
    void log_debug(const Args&... args) {
        if (!is_debug_enabled) {
            return;
        }

        const static std::string debug_level = "DEBUG";
        log(debug_level, args...);
    }

    /// @brief Log an error message. Prints "ERROR" followed by the message.
    /// @tparam ...Args
    /// @param ...args
    template<typename... Args>
    void log_error(const Args&... args) {
        const static std::string debug_level = "ERROR";
        log(debug_level, args...);
    }

    template<typename... Args>
    void log_warning(const Args&... args) {
        if (!is_debug_enabled) {
            return;
        }

        const static std::string error_level = "WARNING";
        log(error_level, args...);
    }

    inline void log_bad_message(const ip::IPAddress& host,
                                const std::string player_id,
                                const std::string& message) {
        out << "ERROR: "
            << "bad message from " << host << ", ";
        out << player_id << ": " << message << "\n";
    }

  private:
    std::string prefix;
    std::ostream& out = std::cerr;
    bool is_debug_enabled = LOG_DEBUG;

    template<typename... Args>
    void log(const std::string& level, const Args&... args) {
        out << level;

        if (!prefix.empty()) {
            out << " " << prefix;
        }
        out << ": ";

        (out << ... << args) << std::endl;
    }
};

class LoggerFactory {
  public:
    static Logger create_logger(const std::string& prefix) {
        return Logger(prefix);
    }

    static Logger create_logger(const ip::IPAddress& ip) {
        return LoggerFactory::create_logger(ip.to_string());
    }
};

} // namespace logging

#endif
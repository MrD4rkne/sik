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
    Logger(const std::string& prefix)
        : prefix(prefix), out(std::cout), err(std::cerr) {
    }

    Logger() : out(std::cout), err(std::cerr) {
    }

    template<typename... Args>
    void log_info(const Args&... args) const {
        log(out, "", args...);
    }

    template<typename... Args>
    void log_debug(const Args&... args) const {
        if (!is_debug_enabled) {
            return;
        }

        const static std::string debug_level = "DEBUG";
        log(out, debug_level, args...);
    }

    /// @brief Log an error message. Prints "ERROR" followed by the message.
    /// @tparam ...Args
    /// @param ...args
    template<typename... Args>
    void log_error(const Args&... args) const {
        const static std::string debug_level = "ERROR";
        log(err, debug_level, args...);
    }

    template<typename... Args>
    void log_warning(const Args&... args) const {
        if (!is_debug_enabled) {
            return;
        }

        const static std::string error_level = "WARNING";
        log(out, error_level, args...);
    }

    inline void log_bad_message(const ip::IPAddress& host,
                                const std::string player_id,
                                const std::string& message) const {
        err << "ERROR: "
            << "bad message from " << host << ", ";
        err << player_id << ": " << message << "\n";
    }

  private:
    std::string prefix;
    std::ostream& out = std::cout;
    std::ostream& err = std::cerr;
    bool is_debug_enabled = LOG_DEBUG;

    template<typename... Args>
    void log(std::ostream& out_stream, const std::string& level,
             const Args&... args) const {
        out_stream << level;

        if (!prefix.empty()) {
            out_stream << " " << prefix;
        }

        if (!level.empty() || !prefix.empty()) {
            out_stream << ": ";
        }

        (out_stream << ... << args) << std::endl;
    }
};

} // namespace logging

#endif
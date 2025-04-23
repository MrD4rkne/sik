#ifndef CLOCK_H
#define CLOCK_H

#include <chrono>

namespace natural_time {

using timestamp_t = uint64_t;
using offset_t = __int128_t;

/// @brief A class representing a clock that can be used to get the current
class Clock {
  public:
    Clock()
        : start_time(std::chrono::system_clock::now()),
          synced_start_time(start_time) {
    }

    /// @brief Get the current timestamp in milliseconds since the clock was
    /// started.
    /// @return The current timestamp in milliseconds.
    timestamp_t get_timestamp() const {
        auto now = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - start_time);
        return static_cast<timestamp_t>(duration.count());
    }

    /// @brief Get the current timestamp in milliseconds synced by offsets.
    /// @return The current synced timestamp in milliseconds.
    timestamp_t get_synced_timestamp() const {
        auto now = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - synced_start_time);
        return static_cast<timestamp_t>(duration.count());
    }

    /// @brief Correct the synced start time by a given offset.
    /// @param offset The offset in milliseconds to correct the synced start
    void correct_time(offset_t offset) {
        bool is_negative = offset < 0;
        if (is_negative) {
            offset = -offset;
        }

        auto offset_in_ms =
            std::chrono::milliseconds(static_cast<long long>(offset));

        if (is_negative) {
            synced_start_time -= offset_in_ms;
        } else {
            synced_start_time += offset_in_ms;
        }
    }

    /// @brief Convert seconds to timestamp_t.
    /// @param seconds The number of seconds to convert.
    static timestamp_t from_seconds(double seconds) {
        return static_cast<timestamp_t>(seconds * 1000.0);
    }

  private:
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point synced_start_time;
};

} // namespace natural_time

#endif // CLOCK_H
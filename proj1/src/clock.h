#ifndef CLOCK_H
#define CLOCK_H

#include <chrono>

namespace natural_time {

using timestamp_t = uint64_t;
using offset_t = __int128_t;

class Clock {
  public:
    Clock() : start_time(std::chrono::steady_clock::now()) {
    }

    timestamp_t get_timestamp() const {
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - start_time);
        return static_cast<timestamp_t>(duration.count());
    }

    double get_timestamp_seconds() const {
        return static_cast<double>(get_timestamp()) / 1000.0;
    }

    void correct_time(offset_t offset) {
        bool is_negative = offset < 0;
        if (is_negative) {
            offset = -offset;
        }

        auto offset_in_ms =
            std::chrono::milliseconds(static_cast<long long>(offset));

        if (is_negative) {
            start_time -= offset_in_ms;
        } else {
            start_time += offset_in_ms;
        }
    }

    static timestamp_t from_seconds(double seconds) {
        return static_cast<timestamp_t>(seconds * 1000.0);
    }

  private:
    std::chrono::steady_clock::time_point start_time;
};

} // namespace natural_time

#endif // CLOCK_H
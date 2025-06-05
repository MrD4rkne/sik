#ifndef TYPES_H
#define TYPES_H

#include "results.h"
#include <cctype>
#include <cstddef>
#include <cstdint>

namespace types {
using rational_t = double;
using k_t = int64_t;

constexpr rational_t MIN_COEFF = -100.0;
constexpr rational_t MAX_COEFF = 100.0;

constexpr k_t MIN_K = 1;
constexpr k_t MAX_K = 10000;

constexpr size_t MIN_N = 1;
constexpr size_t MAX_N = 8;

constexpr rational_t MIN_OFFSET = -5.0;
constexpr rational_t MAX_OFFSET = 5.0;

constexpr size_t PRECISION = 7;

/// @brief Validates a user ID.
/// @param user_id The user ID to validate.
/// @return True if the user ID is valid, false otherwise.
/// @attention https://moodle.mimuw.edu.pl/mod/forum/discuss.php?d=11111#p23236
inline results::Result is_valid_user_id(const std::string& user_id) {
    if (user_id.empty()) {
        return results::Result::Failure("User ID cannot be empty.");
    }

    // Check alphanumeric characters
    for (char c : user_id) {
        if (!isalnum(c)) {
            return results::Result::Failure(
                "User ID must contain only alphanumeric characters.");
        }
    }

    return results::Result::Success();
}

} // namespace types

#endif
#ifndef TYPES_H
#define TYPES_H

#include <cstddef>
#include <cstdint>

namespace types {
using rational_t = double;
using k_t = uint16_t;

constexpr rational_t MIN_COEFF = -100.0;
constexpr rational_t MAX_COEFF = 100.0;

constexpr k_t MIN_K = 1;
constexpr k_t MAX_K = 10000;

constexpr size_t MIN_N = 1;
constexpr size_t MAX_N = 8;

constexpr rational_t MIN_OFFSET = -5.0;
constexpr rational_t MAX_OFFSET = 5.0;

constexpr size_t PRECISION = 7;
} // namespace types

#endif
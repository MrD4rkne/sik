#ifndef POLYNOMIAL_H
#define POLYNOMIAL_H

#include "logging.h"
#include "messages.h"
#include <math.h>
#include <vector>

namespace polynomial {

/// @brief Class representing a polynomial with rational coefficients.
class Polynomial {
  public:
    static constexpr types::rational_t DEFAULT = 0.0;

    Polynomial(const std::vector<types::rational_t>& coeffs, uint32_t k)
        : coeffs(coeffs), points(k, DEFAULT) {
    }

    /// @brief Get number of puts (updates) made to the simulated polynomial.
    /// @return The number of puts.
    uint64_t get_puts() const {
        return puts;
    }

    /// @brief Put a value at a specific point in the polynomial.
    /// @param point The point at which to put the value.
    void put(size_t point, types::rational_t value) {
        if (point >= points.size()) {
            throw std::out_of_range("Point is out of range.");
        }

        points[point] += value;
        ++puts;
    }

    /// @brief Get simulated values of the polynomial at all points.
    const std::vector<types::rational_t>& get_points() const {
        return points;
    }

    /// @brief  Calculate the local score for a point in the polynomial.
    /// @param point The point at which to calculate the score.
    /// @param value The new value to put at the point.
    /// @return The change in score resulting from the put operation.
    double local_score(size_t point, types::rational_t value) const {
        if (point >= points.size()) {
            throw std::out_of_range("Point is out of range.");
        }

        double polynomial_value = evaluate(point);
        double new_value = points[point] + value;

        double old_score = squared_error(points[point], polynomial_value);
        double new_score = squared_error(new_value, polynomial_value);
        return old_score - new_score;
    }

    /// @brief Calculate the total score of the polynomial based on current
    /// points.
    double score() const {
        double score = 0.0;
        for (size_t i = 0; i < points.size(); ++i) {
            score += squared_error(points[i], evaluate((double)i));
            logger.log_debug("Score: ", score);
        }

        return score;
    }

    /// @brief Evaluate polynomial at point x
    double evaluate(double x) const {
        double result = 0.0;
        for (size_t i = coeffs.size() - 1; i > 0; --i) {
            result += coeffs[i];
            result *= x;
        }
        result += coeffs[0];

        logger.log_debug("Evaluating polynomial at x=", x, " result=", result);
        return result;
    }

    /// @brief Calculate the squared error between a value and the polynomial's
    /// value at a point.
    /// @param y The actual value at the point.
    /// @param fx The value of the polynomial at the point.
    /// @return The squared error between the actual value and the polynomial's
    /// value.
    double squared_error(double y, double fx) const {
        double diff = y - fx;
        return std::pow(diff, 2);
    }

  private:
    std::vector<types::rational_t> coeffs;
    std::vector<types::rational_t> points;
    uint64_t puts = 0;
    logging::Logger logger;
};

} // namespace polynomial

#endif
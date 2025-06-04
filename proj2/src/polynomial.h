#ifndef POLYNOMIAL_H
#define POLYNOMIAL_H

#include "messages.h"
#include <math.h>
#include <vector>

namespace polynomial {

class Polynomial {
  public:
    static constexpr types::rational_t DEFAULT = 0.0;

    Polynomial(const std::vector<types::rational_t>& coeffs, uint32_t k)
        : coeffs(coeffs), points(k, DEFAULT) {
    }

    uint64_t get_puts() const {
        return puts;
    }

    void put(size_t point, types::rational_t value) {
        if (point >= points.size()) {
            throw std::out_of_range("Point is out of range.");
        }

        points[point] += value;
        ++puts;
    }

    const std::vector<types::rational_t>& get_points() const {
        return points;
    }

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

    double score() const {
        double score = 0.0;
        for (size_t i = 0; i < points.size(); ++i) {
            score += squared_error(points[i], evaluate((double)i));
        }

        return score;
    }

    // Evaluate polynomial at point x
    double evaluate(double x) const {
        double result = 0.0;
        double x_power = 1.0;

        for (size_t i = 0; i < coeffs.size(); ++i) {
            result += coeffs[i] * x_power;
            x_power *= x;
        }

        return result;
    }

    double squared_error(double y, double fx) const {
        double diff = y - fx;
        return std::pow(diff, 2);
    }

  private:
    std::vector<types::rational_t> coeffs;
    std::vector<types::rational_t> points;
    uint64_t puts = 0;
};

} // namespace polynomial

#endif
#ifndef STRATEGIES_H
#define STRATEGIES_H

#include "cin.h"
#include "client.h"
#include "messages.h"
#include "polynomial.h"
#include "types.h"

namespace strategies {
class AutoStrategy : public client::strategy {
  public:
    AutoStrategy() : is_first_put(true), polynomial(nullptr), coeffs(nullptr) {
    }

    void add_coeffs(const std::vector<types::rational_t>& new_coeffs) override {
        if (coeffs) {
            throw std::runtime_error("Coefficients already set");
        }

        coeffs = std::make_shared<std::vector<types::rational_t>>(new_coeffs);
    }

    bool has_put_pending() override {
        return !is_waiting_for_response &&
               (polynomial != nullptr || (coeffs != nullptr && is_first_put));
    }

    std::pair<types::k_t, types::rational_t> get_put_pending() override {
        if (!has_put_pending()) {
            throw std::runtime_error("No put pending");
        }

        if (is_first_put) {
            is_first_put = false;

            constexpr types::k_t point = 0;
            types::rational_t offset = highest_legal_towards(coeffs->at(point));
            coeffs->at(point) += offset;
            return {point, offset};
        }

        // Polynomial is already initialized, so we can use it to get the next
        // point.
        auto [point, value] = calculate_best_put();
        return {point, value};
    }

    void mark_put_sent(const types::k_t point,
                       const types::rational_t value) override {
        if (is_waiting_for_response) {
            throw std::runtime_error("Already waiting for response");
        }

        if (!coeffs) {
            throw std::runtime_error("Coefficients are not set");
        }

        if (!polynomial) {
            coeffs->at(point) += value;
        } else {
            polynomial->put(point, value);
        }

        is_waiting_for_response = true;
    }

    static double highest_legal_towards(double value) {
        types::rational_t offset = std::min(value, messages::MAX_OFFSET);
        offset = std::max(offset, messages::MIN_OFFSET);
        return offset;
    }

    std::pair<types::k_t, types::rational_t> calculate_best_put() {
        if (!polynomial) {
            throw std::runtime_error("Polynomial is not initialized");
        }

        types::k_t best_point = 0;
        types::rational_t best_value = 0;
        types::rational_t best_improvement = 0;
        for (types::k_t point = 0; point < polynomial->get_points().size();
             ++point) {
            types::rational_t best_guess = highest_legal_towards(
                polynomial->evaluate(point) - polynomial->get_points()[point]);
            double improvement = polynomial->local_score(point, best_guess);
            if (improvement > best_improvement) {
                best_value = best_guess;
                best_point = point;
                best_improvement = improvement;
            }
        }

        return {best_point, best_value};
    }

    results::Result
    add_bad_put_response(const types::k_t point,
                         const types::rational_t value) override {
        if (!is_waiting_for_response) {
            return results::Result::Failure(
                "Received bad put response, but not waiting for response.");
        }

        return results::Result::Failure(
            "Algorithm always sends valid puts, so this should not happen. "
            "Server is incorrect.");
    }

    results::Result
    add_penalty_response(const types::k_t point,
                         const types::rational_t value) override {
        if (!is_waiting_for_response) {
            return results::Result::Failure(
                "Received penalty response, but not waiting for response.");
        }

        return results::Result::Success();
    }

    results::Result
    add_state_response(const std::vector<types::rational_t>& state) override {
        if (!is_waiting_for_response) {
            return results::Result::Failure(
                "Received state response, but not waiting for response.");
        }

        if (!polynomial) {
            polynomial =
                std::make_shared<polynomial::Polynomial>(*coeffs, state.size());
        }

        if (state.size() != polynomial->get_points().size()) {
            return results::Result::Failure(
                "Received state with different size than coefficients.");
        }

        is_waiting_for_response = false;
        return results::Result::Success();
    }

  private:
    bool is_first_put = true;
    std::shared_ptr<polynomial::Polynomial> polynomial;
    std::shared_ptr<std::vector<types::rational_t>> coeffs = nullptr;
    bool is_waiting_for_response = false;
};

class UserStrategy : public client::strategy {
  public:
    UserStrategy(std::shared_ptr<cin_fd_handler> handler)
        : cin_handler(handler), logger(), put_pending(nullptr) {
    }

    void add_coeffs(const std::vector<types::rational_t>&) override {
        // User strategy does not use coefficients directly.
        // This method can be used to notify the user about new coefficients.
        logger.log_debug(
            "Received new coefficients, but user strategy does not use them.");
    }

    bool has_put_pending() override {
        cin_handler->start_listenning();

        if (put_pending) {
            return true;
        }

        if (!cin_handler->has_input()) {
            return false;
        }

        std::string input = cin_handler->get_input();
        cin_handler->pop_input();

        try {
            put_pending =
                std::make_shared<std::pair<types::k_t, types::rational_t>>(
                    messages::deserialize_put(input));
        } catch (const std::invalid_argument& e) {
            logger.log_error("invalid input line ", input);
            return false;
        }

        return true;
    }

    void mark_put_sent(const types::k_t, const types::rational_t) override {
        // TODO: LOG
    }

    std::pair<types::k_t, types::rational_t> get_put_pending() override {
        if (!put_pending) {
            throw std::runtime_error("No put pending");
        }

        auto result = *put_pending;
        put_pending.reset();
        return result;
    }

    results::Result
    add_bad_put_response(const types::k_t point,
                         const types::rational_t value) override {
        return results::Result::Success();
    }

    results::Result
    add_penalty_response(const types::k_t point,
                         const types::rational_t value) override {
        return results::Result::Success();
    }

    results::Result
    add_state_response(const std::vector<types::rational_t>& coeffs) override {
        return results::Result::Success();
    }

  private:
    std::shared_ptr<cin_fd_handler> cin_handler;
    logging::Logger logger;
    std::shared_ptr<std::pair<types::k_t, types::rational_t>> put_pending;
};

} // namespace strategies

#endif
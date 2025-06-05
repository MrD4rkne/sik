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
        return !waiting_for_state &&
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

    void mark_put_sent(const size_t point,
                       const types::rational_t value) override {
        if (!coeffs) {
            throw std::runtime_error("Coefficients are not set");
        }

        if (!polynomial) {
            // TODO: fix this
            coeffs->at(point) += value;
        } else {
            polynomial->put(point, value);
        }

        has_sent_any_put = true;
        waiting_for_state = true;
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
        for (size_t point = 0; point < polynomial->get_points().size();
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

    results::Result add_bad_put_response(const size_t k,
                                         const types::rational_t v) override {
        if (!has_sent_any_put) {
            return results::Result::Failure(
                "Received state response, but hasn't sent any put yet.");
        }

        if (polynomial) {
            if (k >= polynomial->get_points().size()) {
                return results::Result::Failure(
                    "Received bad put response for point out of range.");
            }
        }

        logger.log_info("Received bad put response for point ", k,
                        " with value ", v);

        return results::Result::Failure(
            "Algorithm always sends valid puts, so this should not happen. "
            "Server is incorrect.");
    }

    results::Result add_penalty_response(const size_t k,
                                         const types::rational_t v) override {
        if (!has_sent_any_put) {
            return results::Result::Failure(
                "Received state response, but hasn't sent any put yet.");
        }

        if (polynomial) {
            if (k >= polynomial->get_points().size()) {
                return results::Result::Failure(
                    "Received bad put response for point out of range.");
            }
        }

        logger.log_info("Received penalty response for point ", k,
                        " with value ", v);

        return results::Result::Success();
    }

    results::Result
    add_state_response(const std::vector<types::rational_t>& state) override {
        if (!has_sent_any_put) {
            return results::Result::Failure(
                "Received state response, but hasn't sent any put yet.");
        }

        if (!polynomial) {
            polynomial =
                std::make_shared<polynomial::Polynomial>(*coeffs, state.size());
        }

        if (polynomial && state.size() != polynomial->get_points().size()) {
            return results::Result::Failure(
                "Received state response with different size than "
                "coefficients.");
        }

        std::stringstream ss;
        ss << "Received state response with coefficients: ";
        for (const auto& coeff : *coeffs) {
            ss << coeff << " ";
        }
        logger.log_info(ss.str());

        waiting_for_state = false;
        return results::Result::Success();
    }

  private:
    bool is_first_put = true;
    std::shared_ptr<polynomial::Polynomial> polynomial;
    std::shared_ptr<std::vector<types::rational_t>> coeffs = nullptr;
    bool has_sent_any_put = false;
    bool waiting_for_state = false;
    logging::Logger logger;
};

class UserStrategy : public client::strategy {
  public:
    UserStrategy(std::shared_ptr<cin_fd_handler> handler)
        : cin_handler(handler), logger(), put_pending(nullptr) {
    }

    void add_coeffs(const std::vector<types::rational_t>& coeffs) override {
        std::stringstream ss;
        ss << "Received coefficients: ";
        for (const auto& coeff : coeffs) {
            ss << coeff << " ";
        }
        logger.log_info(ss.str());
    }

    bool has_put_pending() override {
        cin_handler->start_listening();

        if (put_pending) {
            logger.log_debug("Already have a pending PUT");
            return true;
        }

        if (!cin_handler->has_input()) {
            logger.log_debug("No input available, waiting for user input.");
            return false;
        }

        logger.log_info("User input available, processing it.");

        do {
            std::string input = cin_handler->get_input();
            cin_handler->pop_input();

            try {
                put_pending =
                    std::make_shared<std::pair<types::k_t, types::rational_t>>(
                        messages::deserialize_put(input));
            } catch (const std::invalid_argument& e) {
                logger.log_error("invalid input line ", input);
            }
        } while (!put_pending && cin_handler->has_input());

        return put_pending != nullptr;
    }

    void mark_put_sent(const size_t k, const types::rational_t v) override {
        logger.log_info("PUT ", v, " in ", k, " sent to server.");
        put_sent = true;
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
    add_bad_put_response(const size_t point,
                         const types::rational_t value) override {
        if (!put_sent) {
            return results::Result::Failure(
                "Received bad put response, but no put was sent.");
        }

        if (state_size != NOT_SET && point > (size_t)state_size) {
            return results::Result::Failure(
                "Received state response with different size than expected.");
        }

        logger.log_info("Received bad put response for point ", point,
                        " with value ", value);

        return results::Result::Success();
    }

    results::Result
    add_penalty_response(const size_t point,
                         const types::rational_t value) override {
        if (!put_sent) {
            return results::Result::Failure(
                "Received penalty response, but no put was sent.");
        }

        if (state_size != NOT_SET && point > (size_t)state_size) {
            return results::Result::Failure(
                "Received state response with different size than expected.");
        }

        logger.log_info("Received penalty response for point ", point,
                        " with value ", value);

        return results::Result::Success();
    }

    results::Result
    add_state_response(const std::vector<types::rational_t>& coeffs) override {
        if (!put_sent) {
            return results::Result::Failure(
                "Received state response, but no put was sent.");
        }

        if (state_size != NOT_SET && (size_t)state_size != coeffs.size()) {
            return results::Result::Failure(
                "Received state response with different size than expected.");
        }

        state_size = (ssize_t)coeffs.size();

        std::stringstream ss;
        ss << "Received state response with coefficients: ";
        for (const auto& coeff : coeffs) {
            ss << coeff << " ";
        }
        logger.log_info(ss.str());

        return results::Result::Success();
    }

  private:
    bool put_sent = false;
    std::shared_ptr<cin_fd_handler> cin_handler;
    logging::Logger logger;
    std::shared_ptr<std::pair<types::k_t, types::rational_t>> put_pending;
    constexpr static ssize_t NOT_SET = -1;
    ssize_t state_size = NOT_SET;
};

} // namespace strategies

#endif
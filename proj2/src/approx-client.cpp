#include <iostream>

#include "cin.h"
#include "client.h"
#include "input.h"
#include "ip.h"
#include "logging.h"
#include "network.h"
#include "polynomial.h"
#include <arpa/inet.h>
#include <iomanip>
#include <netinet/in.h>
#include <sstream>
#include <unistd.h>

static inline std::string PLAYER_ID_ARG = "-u";
static inline std::string PORT_NUMBER_ARG = "-p";
static inline std::string SERVER_ARG = "-s";
static inline std::string IPV4_FLAG = "-4";
static inline std::string IPV6_FLAG = "-6";
static inline std::string STRATEGY_FLAG = "-a";

using port_t = ip::port_t;

class AutoStrategy : public client::strategy {
  public:
    AutoStrategy() : is_first_put(true), polynomial(nullptr), coeffs(nullptr) {
    }

    void
    add_coeffs(const std::vector<messages::rational_t>& new_coeffs) override {
        if (coeffs) {
            throw std::runtime_error("Coefficients already set");
        }

        coeffs =
            std::make_shared<std::vector<messages::rational_t>>(new_coeffs);
    }

    bool has_put_pending() override {
        return !is_waiting_for_response &&
               (polynomial != nullptr || (coeffs != nullptr && is_first_put));
    }

    std::pair<messages::k_t, messages::offset_t> get_put_pending() override {
        if (!has_put_pending()) {
            throw std::runtime_error("No put pending");
        }

        if (is_first_put) {
            is_first_put = false;

            constexpr messages::k_t point = 0;
            messages::offset_t offset =
                highest_legal_towards(coeffs->at(point));
            coeffs->at(point) += offset;
            return {point, offset};
        }

        // Polynomial is already initialized, so we can use it to get the next
        // point.
        auto [point, value] = calculate_best_put();
        return {point, value};
    }

    void mark_put_sent(const messages::k_t point,
                       const messages::offset_t value) override {
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
        messages::offset_t offset = std::min(value, messages::MAX_OFFSET);
        offset = std::max(offset, messages::MIN_OFFSET);
        return offset;
    }

    std::pair<messages::k_t, messages::offset_t> calculate_best_put() {
        if (!polynomial) {
            throw std::runtime_error("Polynomial is not initialized");
        }

        messages::k_t best_point = 0;
        messages::offset_t best_value = 0;
        messages::offset_t best_improvement = 0;
        for (messages::k_t point = 0; point < polynomial->get_points().size();
             ++point) {
            messages::offset_t best_guess = highest_legal_towards(
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
    add_bad_put_response(const messages::k_t point,
                         const messages::offset_t value) override {
        if (!is_waiting_for_response) {
            return results::Result::Failure(
                "Received bad put response, but not waiting for response.");
        }

        return results::Result::Failure(
            "Algorithm always sends valid puts, so this should not happen. "
            "Server is incorrect.");
    }

    results::Result
    add_penalty_response(const messages::k_t point,
                         const messages::offset_t value) override {
        if (!is_waiting_for_response) {
            return results::Result::Failure(
                "Received penalty response, but not waiting for response.");
        }

        return results::Result::Success();
    }

    results::Result add_state_response(
        const std::vector<messages::rational_t>& state) override {
        if (!is_waiting_for_response) {
            return results::Result::Failure(
                "Received state response, but not waiting for response.");
        }

        if (!polynomial) {
            polynomial =
                std::make_shared<polynomial::Polynomial>(*coeffs, state.size());
        }

        is_waiting_for_response = false;
        return results::Result::Success();
    }

  private:
    bool is_first_put = true;
    std::shared_ptr<polynomial::Polynomial> polynomial;
    std::shared_ptr<std::vector<messages::rational_t>> coeffs = nullptr;
    bool is_waiting_for_response = false;
};

class UserStrategy : public client::strategy {
  public:
    UserStrategy(std::shared_ptr<cin_fd_handler> handler)
        : cin_handler(handler), logger(), put_pending(nullptr) {
    }

    void add_coeffs(const std::vector<messages::rational_t>&) override {
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
                std::make_shared<std::pair<messages::k_t, messages::offset_t>>(
                    messages::deserialize_put(input));
        } catch (const std::invalid_argument& e) {
            logger.log_error("invalid input line ", input);
            return false;
        }

        return true;
    }

    void mark_put_sent(const messages::k_t, const messages::offset_t) override {
        // TODO: LOG
    }

    std::pair<messages::k_t, messages::offset_t> get_put_pending() override {
        if (!put_pending) {
            throw std::runtime_error("No put pending");
        }

        auto result = *put_pending;
        put_pending.reset();
        return result;
    }

    results::Result
    add_bad_put_response(const messages::k_t point,
                         const messages::offset_t value) override {
        return results::Result::Success();
    }

    results::Result
    add_penalty_response(const messages::k_t point,
                         const messages::offset_t value) override {
        return results::Result::Success();
    }

    results::Result add_state_response(
        const std::vector<messages::rational_t>& coeffs) override {
        return results::Result::Success();
    }

  private:
    std::shared_ptr<cin_fd_handler> cin_handler;
    logging::Logger logger;
    std::shared_ptr<std::pair<messages::k_t, messages::offset_t>> put_pending;
};

int main(int argc, char* argv[]) {
    std::unordered_map<std::string, input::arg_t> allowed_args = {
        input::arg_t::get_arg(PLAYER_ID_ARG, true),
        input::arg_t::get_arg(PORT_NUMBER_ARG, true),
        input::arg_t::get_arg(SERVER_ARG, true),
        input::arg_t::get_flag(IPV4_FLAG),
        input::arg_t::get_flag(IPV6_FLAG),
        input::arg_t::get_flag(STRATEGY_FLAG)};

    logging::Logger logger;

    try {
        input::args_parses_t args_map(argc, argv, allowed_args);

        std::string player_id = args_map.get_value(PLAYER_ID_ARG);
        logger.log_debug("Player ID: ", player_id);
        port_t port_number = input::parse_input<port_t>(
            PORT_NUMBER_ARG, args_map.get_value(PORT_NUMBER_ARG), 1, 65535);
        logger.log_debug("Server Port number: ", port_number);
        std::string server_address = args_map.get_value(SERVER_ARG);
        logger.log_debug("Server address: ", server_address);

        bool ipv4_flag = args_map.has_flag(IPV4_FLAG);
        logger.log_debug("IPv4 flag: ", ipv4_flag);
        bool ipv6_flag = args_map.has_flag(IPV6_FLAG);
        logger.log_debug("IPv6 flag: ", ipv6_flag);

        ip::IPAddress::Type ip_type = ip::IPAddress::Type::None;
        if (ipv4_flag != ipv6_flag) {
            ip_type = ipv4_flag ? ip::IPAddress::Type::IPv4
                                : ip::IPAddress::Type::IPv6;
        }

        ip::IPAddress ip_adress =
            network::IpParser::parse(server_address, port_number, ip_type);
        logger.log_debug("Parsed IP address: ", ip_adress.to_string());

        auto poller = std::make_shared<fd::FDPoller>();

        std::shared_ptr<client::strategy> strategy = nullptr;

        if (args_map.has_flag(STRATEGY_FLAG)) {
            logger.log_debug("Using auto strategy");
            strategy = std::make_shared<AutoStrategy>();
        } else {
            logger.log_debug("Using user strategy");
            auto cin_handler = std::make_shared<cin_fd_handler>();
            strategy = std::make_shared<UserStrategy>(cin_handler);
            poller->add_socket(STDIN_FILENO, cin_handler);
        }

        client::client client(player_id, ip_adress, logger, strategy, poller);
        client.init();
        client.run();
    } catch (const std::exception& e) {
        logger.log_error(e.what());
        return 1;
    }

    logger.log_info("Client finished successfully.");
    return 0;
}
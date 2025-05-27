#include <iostream>

#include "client.h"
#include "input.h"
#include "ip.h"
#include "logging.h"
#include "network.h"
#include <arpa/inet.h>
#include <iomanip>
#include <netinet/in.h>
#include <sstream>
#include <unistd.h>
#include "cin.h"

static inline std::string PLAYER_ID_ARG = "-u";
static inline std::string PORT_NUMBER_ARG = "-p";
static inline std::string SERVER_ARG = "-s";
static inline std::string IPV4_FLAG = "-4";
static inline std::string IPV6_FLAG = "-6";
static inline std::string STRATEGY_FLAG = "-a";

using port_t = ip::port_t;

class AutoStrategy : public client::strategy {
  public:
    AutoStrategy() = default;

    bool has_put_pending() override {
        return false;
    }

    std::pair<messages::k_t, messages::offset_t> get_put_pending() override {
        throw std::runtime_error("No put pending");
    }

    results::Result add_bad_put_response(const messages::k_t point,
                              const messages::offset_t value) override {
                                return results::Result::Success();
                              }

    results::Result add_penalty_response(const messages::k_t point,
                              const messages::offset_t value) override {
                                return results::Result::Success();
                              }

    results::Result add_state_response(
        const std::vector<messages::rational_t>& coeffs) override {
            return results::Result::Success();
        }
};

class UserStrategy : public client::strategy {
  public:
    UserStrategy(std::shared_ptr<cin_fd_handler> handler) : cin_handler(handler), logger(), put_pending(nullptr) {
    }

    bool has_put_pending() override {
        cin_handler->start_listenning();

        if(put_pending) {
            return true;
        }

        if(!cin_handler->has_input()) {
            return false;
        }

        std::string input = cin_handler->get_input();
        cin_handler->pop_input();

        try{
            put_pending = std::make_shared<std::pair<messages::k_t, messages::offset_t>>(
                messages::deserialize_put(input));
        } catch (const std::invalid_argument& e) {
            logger.log_error("invalid input line ", input);
            return false;
        }

        return true;
    }

    std::pair<messages::k_t, messages::offset_t> get_put_pending() override {
        if(!put_pending) {
            throw std::runtime_error("No put pending");
        }

        auto result = *put_pending;
        put_pending.reset();
        return result;
    }

    results::Result add_bad_put_response(const messages::k_t point,
                              const messages::offset_t value) override {
                                return results::Result::Success();
                              }

    results::Result add_penalty_response(const messages::k_t point,
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

        if(args_map.has_flag(STRATEGY_FLAG)) {
            logger.log_debug("Using auto strategy");
            strategy = std::make_shared<AutoStrategy>();
        } else {
            logger.log_debug("Using user strategy");
            auto cin_handler = std::make_shared<cin_fd_handler>();
            strategy = std::make_shared<UserStrategy>(cin_handler);
            poller->add_socket(STDIN_FILENO,
                               cin_handler);
        }

        client::client client(player_id, ip_adress, logger,
                              strategy, poller);
        client.init();
        client.run();
    } catch (const std::exception& e) {
        logger.log_error(e.what());
        return 1;
    }

    return 0;
}
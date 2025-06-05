#ifndef SERVER_H
#define SERVER_H

#include "file.h"
#include "handlers.h"
#include "ip.h"
#include "polynomial.h"

namespace server {

constexpr uint64_t DELAY_BEFORE_COEFF = 0;                     // milliseconds
constexpr uint64_t MAX_DELAY_BETWEEN_CONNECT_AND_HELLO = 3000; // milliseconds
const std::string UNKNOWN_ID = "UNKNOWN";
constexpr types::rational_t POINT_DEFAULT = 0.0f;
constexpr uint64_t PENALTY_ON_PUT_BEFORE_RESPONSE = 20;
constexpr uint64_t PENALTY_ON_BAD_PUT = 10;
constexpr types::rational_t MIN_VALUE = -5.0f;
constexpr types::rational_t MAX_VALUE = 5.0f;
constexpr uint64_t DELAY_AFTER_BAD_PUT = 1000;
constexpr uint64_t PENALTY_DELAY = 0;

class Server;

class PlayersManager : public messages::MessageSender {
  public:
    virtual void disconnect(const ip::IPAddress player_ip) = 0;

    virtual ~PlayersManager() = default;
};

class COEFFProvider {
  public:
    virtual size_t get_available_coeffs_count() const = 0;

    virtual bool has_coeffs() const = 0;

    virtual void request_coeffs() = 0;

    virtual const std::string& get_coeffs() = 0;

    virtual void pop_coeffs() = 0;

    virtual ~COEFFProvider() = default;
};

class Server {
  public:
    Server(uint16_t k, uint32_t m,
           std::shared_ptr<COEFFProvider> coeff_provider)
        : logger(), coeff_provider(coeff_provider), k{k}, m{m} {
    }

    results::Result mark_message_from(const ip::IPAddress client);

    int get_max_timeout();

    std::vector<std::pair<std::string, types::rational_t>> get_scorings();

    void mark_coeff_sent(const ip::IPAddress sender);

    std::vector<ip::IPAddress> get_timedout_newbies();

    results::TypedResult<
        std::tuple<ip::IPAddress, uint64_t, std::vector<types::rational_t>>>
    dispatch_coeffs();

    void add_client(const ip::IPAddress ip_address);

    results::Result forget(const ip::IPAddress ip_address);

    bool known_player(const ip::IPAddress ip_address) const;

    results::Result mark_hello(const ip::IPAddress sender,
                               const std::string player_id);

    results::Result can_send_put(const ip::IPAddress sender);

    std::string get_player_id(const ip::IPAddress sender);

    results::Result validate_put(const ip::IPAddress sender,
                                 const types::k_t point,
                                 const types::rational_t value);

    std::vector<types::rational_t> process_put(const ip::IPAddress sender,
                                               const types::k_t point,
                                               const types::rational_t value);

    std::vector<ip::IPAddress> get_players();

    void mark_put_response_sent(const ip::IPAddress sender);

    bool is_game_ongoing() const;

  private:
    struct player {
        std::string id;
        std::chrono::time_point<std::chrono::system_clock> connect_time;
        uint64_t put_responses_to_be_sent;
        uint64_t penalty;
        std::unique_ptr<polynomial::Polynomial> polynomial;
        bool sent_hello;
        bool received_coeff;

        bool has_sent_hello() const {
            return sent_hello;
        }

        bool was_sent_coeff() const {
            return received_coeff;
        }
    };

    logging::Logger logger;

    std::shared_ptr<COEFFProvider> coeff_provider;

    const uint16_t k;
    const uint32_t m;
    uint64_t total_puts = 0;

    uint64_t players_before_hello = 0;
    bool game_ongoing = true;

    std::unordered_map<ip::IPAddress, player> players;
    std::deque<ip::IPAddress> waiting_for_coeffs;
};

} // namespace server

#endif
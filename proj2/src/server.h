#ifndef SERVER_H
#define SERVER_H

#include "coeff.h"
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

/// @brief Interface for managing game.
class Server {
  public:
    Server(uint16_t k, uint32_t m,
           std::shared_ptr<coeff::COEFFProvider> coeff_provider)
        : logger(), coeff_provider(coeff_provider), k{k}, m{m} {
    }

    /// @brief Marks a message from a client.
    /// @param client The IP address of the client.
    /// @return Result indicating success or failure.
    /// @throws std::runtime_error if the player is not found in the list of
    /// players.
    results::Result mark_message_from(const ip::IPAddress client);

    /// @brief Gets the minimum time to the next event.
    int get_max_timeout();

    /// @brief Gets the current scroings of players.
    std::vector<std::pair<std::string, types::rational_t>> get_scorings();

    /// @brief Mark that COEFF was sent to the player.
    void mark_coeff_sent(const ip::IPAddress sender);

    /// @brief Gets the players that have not sent hello yet and are timed out.
    std::vector<ip::IPAddress> get_timedout_newbies();

    /// @brief Get the coefficients to dispatch to a player.
    /// @return A TypedResult containing the player's IP address, delay before
    /// sending coefficients, and the coefficients themselves.
    results::TypedResult<
        std::tuple<ip::IPAddress, uint64_t, std::vector<types::rational_t>>>
    dispatch_coeffs();

    /// @brief Adds a new client to the server.
    void add_client(const ip::IPAddress ip_address);

    /// @brief Forgets a player by their IP address.
    results::Result forget(const ip::IPAddress ip_address);

    /// @brief Checks if a player is known by their IP address.
    bool known_player(const ip::IPAddress ip_address) const;

    /// @brief Marks that player has sent hello.
    results::Result mark_hello(const ip::IPAddress sender,
                               const std::string player_id);

    /// @brief Checks if a player can send a PUT request.
    /// @note It increments the penalty for the player if PUT was not expected.
    /// Also increments the number of PUT responses to be sent.
    results::Result can_send_put(const ip::IPAddress sender);

    /// @brief Gets the player ID by their IP address.
    std::string get_player_id(const ip::IPAddress sender);

    /// @brief Validates a PUT request.
    /// @note It increments the penalty for the player if the PUT is invalid.
    /// Also increments the number of PUT responses to be sent.
    results::Result validate_put(const ip::IPAddress sender,
                                 const types::k_t point,
                                 const types::rational_t value);

    /// @brief Processes a PUT request and updates the polynomial.
    std::vector<types::rational_t> process_put(const ip::IPAddress sender,
                                               const types::k_t point,
                                               const types::rational_t value);

    /// @brief Gets the list of players.
    std::vector<ip::IPAddress> get_players();

    /// @brief Marks that a PUT response was sent to the player.
    void mark_put_response_sent(const ip::IPAddress sender);

    /// @brief Checks if the game is ongoing.
    /// @return True if the game is ongoing, false otherwise.
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

    std::shared_ptr<coeff::COEFFProvider> coeff_provider;

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
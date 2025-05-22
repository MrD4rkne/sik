#ifndef SERVER_H
#define SERVER_H

#include "file.h"
#include "handlers.h"
#include "ip.h"

#include <algorithm>
#include <chrono>
#include <queue>
#include <unordered_map>

namespace server {

constexpr static uint64_t DELAY_BEFORE_COEFF = 1000; // milliseconds
constexpr static uint64_t MAX_DELAY_BETWEEN_CONNECT_AND_HELLO =
    3000; // milliseconds
const static std::string UNKNOWN_ID = "UNKNOWN";
constexpr static messages::rational_t POINT_DEFAULT = 0.0f;
constexpr static uint64_t PENALTY_ON_PUT_BEFORE_RESPONSE = 20;
constexpr static uint64_t PENALTY_ON_BAD_PUT = 10;
constexpr static messages::rational_t MIN_VALUE = -5.0f;
constexpr static messages::rational_t MAX_VALUE = 5.0f;
constexpr static uint64_t DELAY_AFTER_BAD_PUT = 1000;
constexpr static uint64_t PENALTY_DELAY = 0;

class Server;
class Polynomial {
  public:
    static constexpr messages::rational_t DEFAULT = 0.0;

    Polynomial(const std::vector<messages::rational_t>& coeffs, uint32_t k)
        : coeffs(coeffs), points(k, DEFAULT) {
    }

    uint64_t get_puts() const {
        return puts;
    }

    void put(uint32_t point, messages::rational_t value) {
        if (point >= points.size()) {
            throw std::out_of_range("Point is out of range.");
        }

        points[point] += value;
        ++puts;
    }

    const std::vector<messages::rational_t>& get_points() const {
        return points;
    }

    double score() const {
        double score = 0.0;
        for (size_t i = 0; i < points.size(); ++i) {
            score += squared_error(points[i], i);
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

    // Calculate squared error (y - f(x))^2
    double squared_error(double y, double x) const {
        double fx = evaluate(x);
        double diff = y - fx;
        return diff * diff;
    }

  private:
    std::vector<messages::rational_t> coeffs;
    std::vector<messages::rational_t> points;
    uint64_t puts = 0;
};

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

static inline int
get_diff_time(const std::chrono::time_point<std::chrono::system_clock>& start,
              const std::chrono::time_point<std::chrono::system_clock>& end) {
    auto diff = end - start;
    if (diff < std::chrono::milliseconds(0)) {
        return 0;
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();
}

class Server {
  public:
    Server(uint16_t k, uint8_t n, uint32_t m,
           std::shared_ptr<COEFFProvider> coeff_provider)
        : logger(), coeff_provider(coeff_provider), k{k}, n{n}, m{m} {
    }

    results::Result mark_message_from(const ip::IPAddress client) {
        auto it = players.find(client);
        if (it == players.end()) {
            throw std::runtime_error(
                "Player not found in the list of players.");
        }

        if (it->second.has_sent_hello()) {
            return results::Result::Success();
        }

        return results::Result::Failure("Player has not sent hello yet.");
    }

    int get_max_timeout() {
        int timeout = INT_MAX;
        if (players_before_hello > 0) {
            for (const auto& pair : players) {
                if (!pair.second.has_sent_hello()) {
                    auto now = std::chrono::system_clock::now();
                    auto remaining = get_diff_time(
                        now, pair.second.connect_time +
                                 std::chrono::milliseconds(
                                     MAX_DELAY_BETWEEN_CONNECT_AND_HELLO));
                    timeout = std::min(timeout, remaining);
                }
            }
        }

        if (timeout == INT_MAX) {
            return -1;
        }

        return timeout;
    }

    std::vector<std::pair<std::string, messages::rational_t>> get_scorings() {
        std::vector<std::pair<std::string, messages::rational_t>> scores;
        for (const auto& pair : players) {
            messages::rational_t score = 0;
            if (pair.second.has_sent_hello()) {
                score = pair.second.polynomial->get_puts();
            }

            score += pair.second.penalty;
            scores.push_back({pair.second.id, score});
        }

        // Sort scores lexicographically by id.
        std::sort(
            scores.begin(), scores.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
        return scores;
    }

    void mark_coeff_sent(const ip::IPAddress sender) {
        auto it = players.find(sender);
        if (it == players.end()) {
            throw std::runtime_error(
                "Player not found in the list of players.");
        }

        if (it->second.received_coeff) {
            return;
        }

        it->second.received_coeff = true;
    }

    std::vector<ip::IPAddress> get_timedout_newbies() {
        auto now = std::chrono::system_clock::now();
        std::vector<ip::IPAddress> to_disconnect;

        for (const auto& pair : players) {
            if (pair.second.has_sent_hello()) {
                continue;
            }

            auto time_to_send_hello =
                pair.second.connect_time +
                std::chrono::milliseconds(MAX_DELAY_BETWEEN_CONNECT_AND_HELLO);
                auto time_t_value = std::chrono::system_clock::to_time_t(time_to_send_hello);
                auto now_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                std::cout << "Time to send hello: "
                          << std::put_time(std::localtime(&time_t_value), "%Y-%m-%d %H:%M:%S")
                          << std::endl;
                std::cout << "Current time: "
                          << std::put_time(std::localtime(&now_t), "%Y-%m-%d %H:%M:%S")
                          << std::endl;
            if (time_to_send_hello < now) {
                to_disconnect.push_back(pair.first);
            }
        }

        return to_disconnect;
    }

    results::TypedResult<
        std::pair<ip::IPAddress, std::vector<messages::rational_t>>>
    dispatch_coeffs() {
        if (waiting_for_coeffs.empty()) {
            return results::TypedResult<
                std::pair<ip::IPAddress, std::vector<messages::rational_t>>>::
                Failure("No coefficients to dispatch.");
        }

        if (coeff_provider->get_available_coeffs_count() == 0) {
            coeff_provider->request_coeffs();
            return results::TypedResult<
                std::pair<ip::IPAddress, std::vector<messages::rational_t>>>::
                Failure("No coefficients available.");
        }

        auto [player, timepoint] = waiting_for_coeffs.front();
        auto msg = coeff_provider->get_coeffs();
        coeff_provider->pop_coeffs();

        auto it = players.find(player);
        if (it == players.end()) {
            throw std::runtime_error(
                "Player not found in the list of players.");
        }

        logger.log_info(it->second.id, " gets coefficients: ", msg, ".");

        uint64_t delay = 0;
        auto now = std::chrono::system_clock::now();
        if (timepoint > now) {
            auto remaining = timepoint - std::chrono::system_clock::now();
            delay =
                std::chrono::duration_cast<std::chrono::milliseconds>(remaining)
                    .count();
        }

        messages::coeff_message_t coeff_message =
            messages::deserialize_message<messages::coeff_message_t>(msg);
        it->second.polynomial =
            std::make_unique<Polynomial>(coeff_message.coeffs, k);

        waiting_for_coeffs.pop_front();

        return results::TypedResult<
            std::pair<ip::IPAddress, std::vector<messages::rational_t>>>::
            Success({player, it->second.polynomial->get_points()});
    }

    void add_client(const ip::IPAddress ip_address) {
        logger.log_info("New client ", ip_address, ".");

        players[ip_address] = {.id = UNKNOWN_ID,
                               .connect_time = std::chrono::system_clock::now(),
                               .put_responses_to_be_sent = 0,
                               .penalty = 0,
                               .polynomial = nullptr,
                               .sent_hello = false,
                               .received_coeff = false};

        ++players_before_hello;
    }

    void forget(const ip::IPAddress ip_address) {
        {
            const auto it = players.find(ip_address);
            if (it == players.end()) {
                throw std::runtime_error(
                    "Player not found in the list of players.");
            }

            if (it->second.polynomial) {
                total_puts -= it->second.polynomial->get_puts();
            }

            logger.log_info(it->second.id, " disconnected.");
            players.erase(it);
        }

        auto it = std::remove_if(
            waiting_for_coeffs.begin(), waiting_for_coeffs.end(),
            [&](const auto& pair) { return pair.first == ip_address; });
        if (it == waiting_for_coeffs.end()) {
            return;
        }

        --players_before_hello;
        waiting_for_coeffs.erase(it, waiting_for_coeffs.end());
    }

    bool known_player(const ip::IPAddress ip_address) const {
        return players.find(ip_address) != players.end();
    }

    results::Result mark_hello(const ip::IPAddress sender,
                               const std::string player_id) {
        auto& player = players[sender];
        if (player.has_sent_hello()) {
            return results::Result::Failure("Already sent hello.");
        }

        --players_before_hello;
        player.id = player_id;
        player.sent_hello = true;
        logger.log_info(sender, " is now known as ", player_id, ".");

        waiting_for_coeffs.push_back(std::make_pair(
            sender, std::chrono::system_clock::now() +
                        std::chrono::milliseconds(DELAY_BEFORE_COEFF)));

        return results::Result::Success();
    }

    results::Result can_send_put(const ip::IPAddress sender) {
        auto it = players.find(sender);
        if (it == players.end()) {
            throw std::runtime_error(
                "Player not found in the list of players.");
        }

        if (!it->second.has_sent_hello()) {
            it->second.penalty += PENALTY_ON_PUT_BEFORE_RESPONSE;
            ++it->second.put_responses_to_be_sent;
            return results::Result::Failure("Player has not sent hello yet.");
        }

        if (!it->second.was_sent_coeff()) {
            it->second.penalty += PENALTY_ON_PUT_BEFORE_RESPONSE;
            ++it->second.put_responses_to_be_sent;
            return results::Result::Failure(
                "Player hasn't been sent coefficients yet.");
        }

        if (it->second.put_responses_to_be_sent > 0) {
            it->second.penalty += PENALTY_ON_PUT_BEFORE_RESPONSE;
            ++it->second.put_responses_to_be_sent;
            return results::Result::Failure(
                "Player hasn't been sent all PUT responses yet.");
        }

        return results::Result::Success();
    }

    std::string get_player_id(const ip::IPAddress sender) {
        auto it = players.find(sender);
        if (it == players.end()) {
            throw std::runtime_error(
                "Player not found in the list of players.");
        }

        return it->second.id;
    }

    results::TypedResult<std::vector<messages::rational_t>>
    process_put(const ip::IPAddress sender, const uint16_t point,
                const messages::rational_t value) {
        if (!can_send_put(sender).is_success()) {
            throw std::runtime_error(
                "Player hasn't sent hello or received coefficients yet.");
        }

        auto& player = players[sender];
        ++player.put_responses_to_be_sent;

        if (point > k || value < MIN_VALUE || value > MAX_VALUE) {
            player.penalty += PENALTY_ON_BAD_PUT;
            return results::TypedResult<std::vector<messages::rational_t>>::
                Failure("Invalid PUT parameters.");
        }

        ++total_puts;
        player.polynomial->put(point, value);

        return results::TypedResult<std::vector<messages::rational_t>>::Success(
            player.polynomial->get_points());
    }

    void mark_put_response_sent(const ip::IPAddress sender) {
        auto it = players.find(sender);
        if (it == players.end()) {
            logger.log_warning("Player not found in the list of players.");
            return;
        }

        if (it->second.put_responses_to_be_sent == 0) {
            throw std::runtime_error("Player has no PUT responses to be sent.");
        }

        --it->second.put_responses_to_be_sent;
    }

    bool is_game_ongoing() const {
        return game_ongoing;
    }

  private:
    struct player {
        std::string id;
        std::chrono::time_point<std::chrono::system_clock> connect_time;
        uint64_t put_responses_to_be_sent;
        uint64_t penalty;
        std::unique_ptr<Polynomial> polynomial;
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
    const uint8_t n;
    const uint32_t m;
    uint64_t total_puts = 0;

    uint64_t players_before_hello = 0;
    bool game_ongoing = true;

    std::unordered_map<ip::IPAddress, player> players;
    std::deque<std::pair<ip::IPAddress,
                         std::chrono::time_point<std::chrono::system_clock>>>
        waiting_for_coeffs;
};

} // namespace server

#endif
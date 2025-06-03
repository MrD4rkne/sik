#include "server.h"
#include "types.h"
#include <limits.h>

#include <algorithm>
#include <chrono>
#include <queue>
#include <string>
#include <unordered_map>

namespace server {

static inline uint64_t
get_diff_time(const std::chrono::time_point<std::chrono::system_clock>& start,
              const std::chrono::time_point<std::chrono::system_clock>& end) {
    auto diff = end - start;
    if (diff < std::chrono::milliseconds(0)) {
        return 0;
    }
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(diff)
        .count();
}

results::Result Server::mark_message_from(const ip::IPAddress client) {
    auto it = players.find(client);
    if (it == players.end()) {
        throw std::runtime_error("Player not found in the list of players.");
    }

    if (it->second.has_sent_hello()) {
        return results::Result::Success();
    }

    return results::Result::Failure("Player has not sent hello yet.");
}

int Server::get_max_timeout() {
    int timeout = INT_MAX;
    if (players_before_hello > 0) {
        for (const auto& pair : players) {
            if (!pair.second.has_sent_hello()) {
                auto now = std::chrono::system_clock::now();
                auto remaining = get_diff_time(
                    now, pair.second.connect_time +
                             std::chrono::milliseconds(
                                 MAX_DELAY_BETWEEN_CONNECT_AND_HELLO));
                timeout = std::min(timeout, (int)remaining);
            }
        }
    }

    if (timeout == INT_MAX) {
        return -1;
    }

    return timeout;
}

std::vector<std::pair<std::string, types::rational_t>> Server::get_scorings() {
    std::vector<std::pair<std::string, types::rational_t>> scores;
    for (const auto& pair : players) {
        types::rational_t score = 0;
        if (pair.second.has_sent_hello()) {
            score = (double)pair.second.polynomial->get_puts();
        }

        score += (double)pair.second.penalty;
        scores.push_back({pair.second.id, score});
    }

    // Sort scores lexicographically by id.
    std::sort(scores.begin(), scores.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    return scores;
}

void Server::mark_coeff_sent(const ip::IPAddress sender) {
    auto it = players.find(sender);
    if (it == players.end()) {
        logger.log_warning("Player not found in the list of players.");
        return;
    }

    if (it->second.received_coeff) {
        return;
    }

    it->second.received_coeff = true;
}

std::vector<ip::IPAddress> Server::get_timedout_newbies() {
    auto now = std::chrono::system_clock::now();
    std::vector<ip::IPAddress> to_disconnect;

    for (const auto& pair : players) {
        if (pair.second.has_sent_hello()) {
            continue;
        }

        auto time_to_send_hello =
            pair.second.connect_time +
            std::chrono::milliseconds(MAX_DELAY_BETWEEN_CONNECT_AND_HELLO);
        if (time_to_send_hello < now) {
            to_disconnect.push_back(pair.first);
        }
    }

    return to_disconnect;
}

results::TypedResult<
    std::tuple<ip::IPAddress, uint64_t, std::vector<types::rational_t>>>
Server::dispatch_coeffs() {
    if (waiting_for_coeffs.empty()) {
        return results::TypedResult<std::tuple<
            ip::IPAddress, uint64_t, std::vector<types::rational_t>>>::
            Failure("No coefficients to dispatch.");
    }

    if (coeff_provider->get_available_coeffs_count() == 0) {
        coeff_provider->request_coeffs();
        return results::TypedResult<std::tuple<
            ip::IPAddress, uint64_t, std::vector<types::rational_t>>>::
            Failure("No coefficients available.");
    }

    auto [player, timepoint] = waiting_for_coeffs.front();
    auto msg = coeff_provider->get_coeffs();
    coeff_provider->pop_coeffs();

    auto it = players.find(player);
    if (it == players.end()) {
        throw std::runtime_error("Player not found in the list of players.");
    }

    logger.log_info(it->second.id, " gets coefficients: ", msg, ".");

    uint64_t delay = 0;
    auto now = std::chrono::system_clock::now();
    if (timepoint > now) {
        auto remaining = timepoint - std::chrono::system_clock::now();
        delay = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
                    remaining)
                    .count();
    }

    messages::coeff_message_t coeff_message =
        messages::deserialize_message<messages::coeff_message_t>(msg);
    it->second.polynomial =
        std::make_unique<polynomial::Polynomial>(coeff_message.coeffs, k);

    waiting_for_coeffs.pop_front();

    return results::TypedResult<
        std::tuple<ip::IPAddress, uint64_t, std::vector<types::rational_t>>>::
        Success({player, delay, coeff_message.coeffs});
}

void Server::add_client(const ip::IPAddress ip_address) {
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

results::Result Server::forget(const ip::IPAddress ip_address) {
    {
        const auto it = players.find(ip_address);
        if (it == players.end()) {
            return results::Result::Failure(
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
    if (it != waiting_for_coeffs.end()) {
        --players_before_hello;
        waiting_for_coeffs.erase(it, waiting_for_coeffs.end());
    }

    return results::Result::Success();
}

bool Server::known_player(const ip::IPAddress ip_address) const {
    return players.find(ip_address) != players.end();
}

results::Result Server::mark_hello(const ip::IPAddress sender,
                                   const std::string player_id) {
    auto& player = players[sender];
    if (player.has_sent_hello()) {
        return results::Result::Failure("Already sent hello.");
    }

    auto id_validate_result = types::is_valid_user_id(player_id);
    if (!id_validate_result.is_success()) {
        return id_validate_result;
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

results::Result Server::can_send_put(const ip::IPAddress sender) {
    auto it = players.find(sender);
    if (it == players.end()) {
        throw std::runtime_error("Player not found in the list of players.");
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

std::string Server::get_player_id(const ip::IPAddress sender) {
    auto it = players.find(sender);
    if (it == players.end()) {
        throw std::runtime_error("Player not found in the list of players.");
    }

    return it->second.id;
}

results::TypedResult<std::vector<types::rational_t>>
Server::process_put(const ip::IPAddress sender, const types::k_t point,
                    const types::rational_t value) {
    if (!can_send_put(sender).is_success()) {
        throw std::runtime_error(
            "Player hasn't sent hello or received coefficients yet.");
    }

    auto& player = players[sender];
    ++player.put_responses_to_be_sent;

    if (point < 0 || point > k || value < MIN_VALUE || value > MAX_VALUE) {
        player.penalty += PENALTY_ON_BAD_PUT;
        return results::TypedResult<std::vector<types::rational_t>>::Failure(
            "Invalid PUT parameters.");
    }

    ++total_puts;
    player.polynomial->put((uint32_t)point, value);
    logger.log_info(player.id, " put ", value, " in ", point, ".");

    if (total_puts == m) {
        game_ongoing = false;
    }

    return results::TypedResult<std::vector<types::rational_t>>::Success(
        player.polynomial->get_points());
}

std::vector<ip::IPAddress> Server::get_players() {
    std::vector<ip::IPAddress> result;
    for (const auto& pair : players) {
        result.push_back(pair.first);
    }
    return result;
}

void Server::mark_put_response_sent(const ip::IPAddress sender) {
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

bool Server::is_game_ongoing() const {
    return game_ongoing;
}

} // namespace server
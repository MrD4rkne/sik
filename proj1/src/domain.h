#ifndef DOMAIN_H
#define DOMAIN_H

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "clock.h"
#include "peers.h"
#include "results.h"

namespace domain {
using message_type_t = uint8_t;
using count_t = uint16_t;
using peer_address_length_t = uint8_t;
using port_t = uint16_t;
using synchronized_t = uint8_t;
using timestamp_t = natural_time::timestamp_t;

inline std::ostream& operator<<(std::ostream& os, const peers::peer& p) {
    return os << p.to_string();
}

/// @brief Class representing synchronization process.
class synchronization {
  public:
    synchronization(const peers::peer& peer, synchronized_t synchronized,
                    natural_time::timestamp_t t1, natural_time::timestamp_t t2,
                    natural_time::timestamp_t timeout)
        : synchronizingWith(peer), synchronized(synchronized),
          time_of_delay_request_sent(0), timestamps{t1, t2, 0},
          SYNC_PROCESS_TIMEOUT(timeout) {
    }

    /// @brief Check if synchronization process timed out.
    /// @param time The current time.
    /// @return True if the synchronization process timed out, false otherwise.
    bool should_be_abandoned(natural_time::timestamp_t time) const;

    /// @brief Check if the synchronization process is valid.
    /// @param peer The peer to that sent sync_response.
    /// @param time The current time.
    /// @param sync The synchronized value from the sync_response.
    /// @return Success if the sync_response is valid, otherwise an error
    /// message.
    results::Result is_sync_response_valid(const peers::peer& peer,
                                           natural_time::timestamp_t time,
                                           synchronized_t sync) const;

    /// @brief Check if the synchronization process is with the given peer.
    /// @param peer The peer to check.
    bool is_with_peer(const peers::peer& peer) const;

    /// @brief Finish the synchronization process.
    /// @param peer The peer that sent the sync_response.
    /// @param time The current time.
    /// @param sync The synchronized value from the sync_response.
    /// @param t4 The timestamp from the sync_response.
    /// @return The offset between the local clock and the peer's clock.
    results::TypedResult<natural_time::offset_t>
    finish_sync(const peers::peer& peer, natural_time::timestamp_t time,
                synchronized_t sync, natural_time::timestamp_t t4);

    /// @brief Set the time of sending the response.
    /// @param time The current time.
    /// @param t3 The synced timestamp after sending delay_request.
    /// @throws std::runtime_error if t3 is already set.
    void set_time_of_sending_response(natural_time::timestamp_t time,
                                      natural_time::timestamp_t t3);

  private:
    const peers::peer synchronizingWith;
    const synchronized_t synchronized;
    timestamp_t time_of_delay_request_sent;
    std::array<natural_time::timestamp_t, 3> timestamps;
    bool t3_set = false;

    const natural_time::timestamp_t SYNC_PROCESS_TIMEOUT;
};

/// @brief Synchronization class for local synchronization.
/// This class manages the synchronization processes and current sync of the
/// node.
class local_synchronization {
  public:
    constexpr static synchronized_t NOT_SYNCHRONIZED = 255;
    constexpr static synchronized_t MINIMAL_SYNCHRONIZED_FOR_SYNC = 253;
    constexpr static synchronized_t LEADER = 0;

    local_synchronization(natural_time::timestamp_t sync_process_timeout,
                          natural_time::timestamp_t sync_timeout)
        : synchronizedWith(nullptr), synchronized(NOT_SYNCHRONIZED),
          last_sync_time(0), time_of_becoming_leader(0), sync_obj(nullptr),
          SYNC_PROCESS_TIMEOUT(sync_process_timeout),
          SYNCHRONIZATION_TIMEOUT(sync_timeout) {
    }

    /// @brief Check current synchronization level.
    /// @return The current synchronization level.
    synchronized_t get_synchronized() const;

    bool is_synchronized() const;

    bool is_leader() const;

    /// @brief Check if timeout has passed since the node became a leader.
    /// @param time The current time.
    /// @param timeout The timeout value.
    /// @return True if the timeout has passed, false otherwise.
    /// @throws std::runtime_error if the node is not a leader.
    bool has_time_passed_since_becoming_leader(timestamp_t time,
                                               timestamp_t timeout) const;

    /// @brief Set the leader status.
    /// @param time The current time.
    /// @return Result indicating success or failure.
    results::Result set_leader(natural_time::timestamp_t time);

    /// @brief Unset the leader status.
    /// @return Result indicating success or failure.
    /// @throws std::runtime_error if the node is not a leader.
    results::Result unset_leader();

    /// @brief Check if the synchronization process timed out. If it did, reset.
    results::Result
    timeout_synchronization_process(natural_time::timestamp_t time);

    /// @brief Check if synchronization process can be started.
    /// @param peer The peer to synchronize with.
    /// @param sync The peer's synchronization level.
    /// @return Result indicating success or failure.
    results::Result can_synchronize_with(const peers::peer& peer,
                                         synchronized_t sync) const;

    /// @brief Set the synchronization status.
    /// @param sync The synchronization level.
    /// @param peer_ptr The peer to synchronize with.
    /// @param time The current time.
    /// @return Result indicating success or failure.
    results::Result set_synchronized(synchronized_t sync,
                                     const peers::peer& peer_ptr,
                                     natural_time::timestamp_t time);

    /// @brief Start the synchronization process.
    /// @param peer The peer to synchronize with.
    /// @param time The current time.
    /// @param sync The synchronization level.
    /// @param t1 The synced timestamp when the sync_start was received.
    /// @param t2 The timestamp from the sync_start message.
    /// @return Result indicating success or failure.
    results::Result start_sync(const peers::peer& peer, timestamp_t time,
                               synchronized_t sync,
                               natural_time::timestamp_t t1,
                               natural_time::timestamp_t t2);

    /// @brief Set the time of sending the response of current synchronization
    /// process.
    /// @param time The current time.
    /// @param t3 The synced timestamp after sending delay_request.
    /// @throws std::runtime_error if t3 is already set.
    /// @throws std::runtime_error if no sync is in progress.
    void set_time_of_sending_response(natural_time::timestamp_t time,
                                      natural_time::timestamp_t t3);

    /// @brief Check if the current sync has timed out.
    /// @param time The current time.
    results::Result validate_sync_timeout(natural_time::timestamp_t time);

    /// @brief Invalidate current synchronization process if it is ongoing.
    void invalidate_ongoing_sync();

    /// @brief Finish the synchronization process.
    /// @param peer The peer that sent the sync_response.
    /// @param time The current time.
    /// @param sync The synchronized value from the sync_response.
    /// @param t4 The timestamp from the sync_response.
    /// @return The offset between the local clock and the peer's clock.
    /// @throws std::runtime_error if no sync is in progress.
    results::TypedResult<natural_time::offset_t>
    finish_sync(const peers::peer& peer, natural_time::timestamp_t time,
                synchronized_t sync, natural_time::timestamp_t t4);

  private:
    void desynchronize();

    std::unique_ptr<const peers::peer> synchronizedWith;
    synchronized_t synchronized;
    natural_time::timestamp_t last_sync_time;
    natural_time::timestamp_t time_of_becoming_leader;
    std::unique_ptr<synchronization> sync_obj;

    const natural_time::timestamp_t SYNC_PROCESS_TIMEOUT;
    const natural_time::timestamp_t SYNCHRONIZATION_TIMEOUT;
};

/// @brief Synchronization point holding info about syncs to this peer.
class synchronization_point {
  public:
    synchronization_point(natural_time::timestamp_t timeout,
                          natural_time::timestamp_t delay_between_syncs)
        : sent_to{}, last_sync_time(0), SYNC_TIMEOUT(timeout),
          is_sending_sync(false), DELAY_BWTWEEN_SYNCS(delay_between_syncs) {
    }

    /// @brief Register sending sync start to a peer.
    /// @param peer The peer to register.
    /// @param time The current time.
    void register_sync_start_with_peer(const peers::peer& peer,
                                       natural_time::timestamp_t time);

    /// @brief Check if the delay request is valid.
    /// @param peer The peer that sent the delay request.
    /// @param time The current time.
    results::Result is_delay_request_valid(const peers::peer& peer,
                                           natural_time::timestamp_t time);

    /// @brief Register a delay request from a peer.
    /// @param peer The peer that sent the delay request.
    /// @param time The current time.
    /// @return Result indicating success or failure.
    results::Result register_delay_request(const peers::peer& peer,
                                           natural_time::timestamp_t time);

    /// @brief Start sending sync_starts to peers.
    /// @param time The current time.
    /// @return Result indicating success or failure.
    /// @throws std::runtime_error if already sending syncs.
    results::Result start_sending_sync(natural_time::timestamp_t time);

    /// @brief End sending sync_starts to peers.
    /// @throws std::runtime_error if not sending syncs.
    void end_sending_sync();

    /// @brief Check if the delay has passed since the last sync.
    /// @param time The current time.
    /// @return True if the delay has passed, false otherwise.
    bool have_delay_passed_since_last_sync(
        natural_time::timestamp_t time) const noexcept;

  private:
    std::map<peers::peer, natural_time::timestamp_t> sent_to;
    natural_time::timestamp_t last_sync_time;
    const natural_time::timestamp_t SYNC_TIMEOUT;
    bool is_sending_sync = false;
    const natural_time::timestamp_t DELAY_BWTWEEN_SYNCS;
};

struct peer_status_t {};

class Node {
  public:
    Node(std::unique_ptr<peers::host_peer_provider> host_peer_provider,
         natural_time::timestamp_t delay_after_becoming_leader,
         natural_time::timestamp_t delay_bwtween_syncs,
         natural_time::timestamp_t sync_process_timeout,
         natural_time::timestamp_t synchronization_timeout)
        : peers{}, waiting_for_connect_ack{}, waiting_for_hello_rsp{}, clock{},
          local_sync(sync_process_timeout, synchronization_timeout),
          sync_point(sync_process_timeout, delay_bwtween_syncs),
          DELAY_AFTER_BECOMING_LEADER(delay_after_becoming_leader),
          host_peer_provider(std::move(host_peer_provider)) {
    }

    /// @brief Start sending sync_starts to peers.
    /// @return Result indicating success or failure.
    /// @throws std::runtime_error if already sending syncs.
    results::Result begin_sending_sync();

    /// @brief End sending sync_starts to peers.
    /// @throws std::runtime_error if not sending syncs.
    /// @throws std::runtime_error if the delay has not passed since the last
    /// sync.
    void end_sending_sync();

    /// @brief Register sending sync start to a peer.
    /// @param peer The peer to register.
    void mark_send_sync(const peers::peer& peer);

    /// @brief Register sending sync start to a peer.
    /// @param peer The peer to register.
    /// @param time The current time.
    /// @return Result indicating success or failure.
    results::Result mark_sync_response(const peers::peer& peer);

    /// @brief Register sending sync start to a peer.
    /// @param peer The peer to register.
    /// @param time The current time.
    /// @return Result indicating success or failure.
    results::Result set_leader();

    /// @brief Unset the leader status.
    /// @return Result indicating success or failure.
    results::Result unset_leader();

    /// @brief Start the synchronization process.
    /// @param peer The peer to synchronize with.
    /// @param sync The synchronization level.
    /// @param t1 The timestamp from the sync_start message.
    /// @param t2 The synced timestamp when the sync_start was received.
    /// @return Result indicating success or failure.
    results::Result start_sync(const peers::peer& peer, synchronized_t sync,
                               natural_time::timestamp_t t1,
                               natural_time::timestamp_t t2);

    /// @brief Finish the synchronization process.
    /// @param peer The peer that sent the sync_response.
    /// @param sync The synchronized value from the sync_response.
    /// @param t4 The timestamp from the sync_response.
    /// @return The offset between the local clock and the peer's clock.
    results::Result finish_sync(const peers::peer& peer, synchronized_t sync,
                                natural_time::timestamp_t t4);

    /// @brief Set the time of sending the response of current synchronization
    /// process.
    /// @throws std::runtime_error if t3 is already set or if no sync is in
    /// progress.
    void set_time_of_sending_response();

    /// @brief Correct synced timestamp.
    /// @param offset The offset to correct the time.
    void correct_time(natural_time::offset_t offset);

    /// @brief Add a peer to the list of peers.
    /// @param peer The peer to add.
    /// @return Result indicating success or failure. returns true if peer was
    /// added or fals eif it was already known.
    results::TypedResult<bool> add_peer(const peers::peer& peer);

    /// @brief If the peer is in the list of peers.
    /// @param peer The peer to check.
    /// @return True if the peer is in the list, false otherwise.
    bool has_peer(const peers::peer& peer) const;

    /// @brief Add a peer to the list of peers that are waiting for connect ack.
    /// @param peer The peer to add.
    void add_waiting_for_connect_ack(const peers::peer& peer);

    /// @brief Add a peer to the list of peers that are waiting for hello
    /// response.
    /// @param peer The peer to add.
    void add_waiting_for_hello_rsp(const peers::peer& peer);

    /// @brief Acknowledge a peer that is waiting for connect ack.
    /// @param peer The peer to acknowledge.
    /// @return Result indicating success or failure.
    results::Result acknowledge_connect(const peers::peer& peer);

    /// @brief Acknowledge a peer that is waiting for hello response.
    /// @param peer The peer to acknowledge.
    /// @return Result indicating success or failure.
    results::Result acknowledge_hello_rsp(const peers::peer& peer);

    /// @brief Validate if ongoing synchronization process has timed out.
    results::Result validate_ongoing_sync_timeout();

    /// @brief Validate if current sync has timed out.
    results::Result validate_sync_timeout();

    /// @brief Get all known peers.
    /// @return A vector of peers.
    std::vector<peers::peer> get_peers() const;

    /// @brief Get the current timestamp.
    /// @return The current timestamp.
    natural_time::timestamp_t get_synced_timestamp() const;

    /// @brief Get the current timestamp.
    /// @return The current timestamp.
    natural_time::timestamp_t get_absolute_timestamp() const;

    /// @brief Get the current synced time.
    /// @return The current synced time.
    const local_synchronization& get_local_synchronization() const;

    /// @brief Check if the node is on the list of peers.
    /// @tparam Iterator The iterator type.
    /// @return True if the node is on the list, false otherwise.
    template<typename Iterator>
    bool is_node_on_list(Iterator begin, Iterator end) const {
        return host_peer_provider->check_if_on_list(begin, end);
    }

  private:
    std::set<peers::peer> peers;
    std::set<peers::peer> waiting_for_connect_ack;
    std::set<peers::peer> waiting_for_hello_rsp;
    natural_time::Clock clock;
    local_synchronization local_sync;
    synchronization_point sync_point;

    const natural_time::timestamp_t DELAY_AFTER_BECOMING_LEADER;
    const count_t MAX_PEERS = 65535;
    const std::unique_ptr<peers::host_peer_provider> host_peer_provider;
};

} // namespace domain

#endif
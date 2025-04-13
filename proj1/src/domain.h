#ifndef DOMAIN_H
#define DOMAIN_H

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "clock.h"
#include "results.h"

namespace domain {
using message_type_t = uint8_t;
using count_t = uint16_t;
using peer_address_length_t = uint8_t;
using port_t = uint16_t;
using synchronized_t = uint8_t;
using timestamp_t = natural_time::timestamp_t;

class peer {
  public:
    peer(port_t port, const std::vector<uint8_t>& address)
        : peer_port(port), peer_address(address) {
    }

    port_t get_port() const noexcept;

    const std::vector<uint8_t> get_address() const noexcept;

    peer_address_length_t get_address_length() const noexcept;

    bool operator<(const peer& other) const noexcept;

    bool operator==(const peer& other) const;

    bool operator!=(const peer& other) const;

    std::string to_string() const;

  private:
    port_t peer_port;
    std::vector<uint8_t> peer_address;
};

inline std::ostream& operator<<(std::ostream& os, const domain::peer& p) {
    return os << p.to_string();
}

class synchronization {
  public:
    synchronization(const peer& peer, synchronized_t synchronized,
                    natural_time::timestamp_t t1, natural_time::timestamp_t t2,
                    natural_time::timestamp_t timeout)
        : synchronizedWith(peer),
          synchronized(synchronized), timestamps{t1, t2, 0},
          SYNC_TIMEOUT(timeout) {
    }

    bool should_be_abandoned(natural_time::timestamp_t time) const;

    results::Result is_sync_response_valid(const peer& peer,
                                           natural_time::timestamp_t time,
                                           synchronized_t sync) const;

    results::TypedResult<natural_time::offset_t>
    finish_sync(const peer& peer, natural_time::timestamp_t time,
                synchronized_t sync, natural_time::timestamp_t t4);

    void set_time_of_sending_response(timestamp_t t3);

  private:
    const peer& synchronizedWith;
    const synchronized_t synchronized;
    std::array<natural_time::timestamp_t, 3> timestamps;
    bool t3_set = false;

    const natural_time::timestamp_t SYNC_TIMEOUT;
};

class local_synchronization {
  public:
    const static synchronized_t NOT_SYNCHRONIZED = 255;
    const static synchronized_t LEADER = 0;

    local_synchronization(natural_time::timestamp_t timeout)
        : synchronizedWith(nullptr), synchronized(NOT_SYNCHRONIZED),
          SYNC_TIMEOUT(timeout) {
    }

    synchronized_t get_synchronized() const;

    bool is_synchronized() const;

    bool is_leader() const;

    results::Result set_leader(natural_time::timestamp_t time);

    results::Result unset_leader();

    bool can_start_sync(natural_time::timestamp_t time,
                        natural_time::timestamp_t delay) const;

    void timeout_synchronization(natural_time::timestamp_t time);

    results::Result can_synchronize_with(const peer& peer,
                                         synchronized_t sync) const;

    results::Result set_synchronized(synchronized_t sync, const peer& peer_ptr,
                                     natural_time::timestamp_t time);

    results::Result start_sync(const domain::peer& peer, synchronized_t sync,
                               natural_time::timestamp_t t1,
                               natural_time::timestamp_t t2);

    void set_time_of_sending_response(natural_time::timestamp_t t3);

    void validate_sync_timeout(natural_time::timestamp_t time);

    results::TypedResult<natural_time::offset_t>
    finish_sync(const peer& peer, natural_time::timestamp_t time,
                synchronized_t sync, natural_time::timestamp_t t4);

  private:
    std::unique_ptr<const peer> synchronizedWith;
    synchronized_t synchronized;
    natural_time::timestamp_t last_sync_time;
    std::unique_ptr<synchronization> sync_obj;

    const natural_time::timestamp_t SYNC_TIMEOUT;
};

class synchronization_point {
  public:
    synchronization_point(natural_time::timestamp_t timeout)
        : sent_to{}, SYNC_TIMEOUT(timeout) {
    }

    void register_sync_start_with_peer(const domain::peer& peer,
                                       natural_time::timestamp_t time);

    results::Result is_delay_request_valid(const domain::peer& peer,
                                           natural_time::timestamp_t time);

    results::Result register_delay_request(const domain::peer& peer,
                                           natural_time::timestamp_t time);

    results::Result start_sync(natural_time::timestamp_t time);

    bool have_delay_passed_since_last_sync(
        natural_time::timestamp_t time,
        natural_time::timestamp_t delay) const noexcept;

  private:
    std::map<peer, natural_time::timestamp_t> sent_to;
    natural_time::timestamp_t last_sync_time;
    const natural_time::timestamp_t SYNC_TIMEOUT;
};

struct peer_status_t {};

class Node {
  public:
    Node(natural_time::timestamp_t delay_after_becoming_leader,
         natural_time::timestamp_t delay_bwtween_syncs,
         natural_time::timestamp_t sync_timeout)
        : peers{}, waiting_for_connect_ack{}, waiting_for_hello_rsp{}, clock{},
          local_sync(delay_after_becoming_leader), sync_point(sync_timeout),
          DELAY_AFTER_BECOMING_LEADER(delay_after_becoming_leader),
          DELAY_BWTWEEN_SYNCS(delay_bwtween_syncs) {
    }

    local_synchronization& get_local_synchronization();

    bool can_start_synchronization() const;

    void send_begin_sync();

    natural_time::timestamp_t mark_send_sync(const domain::peer& peer);

    results::Result mark_sync_response(const domain::peer& peer,
                                       natural_time::timestamp_t time);

    void correct_time(natural_time::offset_t offset);

    results::Result add_peer(const domain::peer& peer);

    void add_range(const std::vector<domain::peer>& new_peers);

    bool has_peer(const domain::peer& peer) const;

    void add_waiting_for_connect_ack(const domain::peer& peer);

    void add_waiting_for_hello_rsp(const domain::peer& peer);

    results::Result acknowledge_connect(const domain::peer& peer);

    results::Result acknowledge_hello_rsp(const domain::peer& peer);

    peer_status_t& get_peer_status(const domain::peer& peer);

    std::vector<peer> get_peers() const;

    natural_time::timestamp_t get_time() const;

  private:
    std::map<domain::peer, peer_status_t> peers;
    std::set<domain::peer> waiting_for_connect_ack;
    std::set<domain::peer> waiting_for_hello_rsp;
    natural_time::Clock clock;
    local_synchronization local_sync;
    synchronization_point sync_point;

    const natural_time::timestamp_t DELAY_AFTER_BECOMING_LEADER;
    const natural_time::timestamp_t DELAY_BWTWEEN_SYNCS;
};

} // namespace domain

#endif
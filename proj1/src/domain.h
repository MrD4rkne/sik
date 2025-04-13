#ifndef DOMAIN_H
#define DOMAIN_H

#include <cstdint>
#include <map>
#include <ostream>
#include <set>
#include <sstream>
#include <string>
#include <chrono>
#include <vector>
#include <memory>
#include <array>

namespace domain {

using message_type_t = uint8_t;
using count_t = uint16_t;
using peer_address_length_t = uint8_t;
using port_t = uint16_t;
using timestamp_t = uint64_t;
using offset_t = __int128_t;
using synchronized_t = uint8_t;

class peer {
  public:
    peer(uint16_t port, const std::vector<uint8_t>& address)
        : peer_port(port), peer_address(address) {
    }

    port_t get_port() const noexcept {
        return peer_port;
    }

    const std::vector<uint8_t> get_address() const noexcept {
        return peer_address;
    }

    peer_address_length_t get_address_length() const noexcept {
        return peer_address.size();
    }

    bool operator<(const peer& other) const noexcept {
        if (peer_address != other.peer_address) {
            return peer_address < other.peer_address;
        }
        return peer_port < other.peer_port;
    }

    bool operator==(const peer& other) const {
        return peer_address == other.peer_address &&
               peer_port == other.peer_port;
    }

    bool operator!=(const peer& other) const {
        return !(*this == other);
    }

    ~peer() = default;

    std::string to_string() const {
        std::stringstream ss;
        for (size_t i = 0; i < peer_address.size(); ++i) {
            ss << static_cast<int>(peer_address[i]);
            if (i < peer_address.size() - 1) {
                ss << ".";
            }
        }

        ss << ":" << peer_port;
        return ss.str();
    }

  private:
    port_t peer_port;
    std::vector<uint8_t> peer_address;
};

inline std::ostream& operator<<(std::ostream& os, const domain::peer& p) {
    return os << p.to_string();
}

class Result {
    public:
        static Result Success() {
                return Result();
        }

        static Result Failure(const std::string& error_message) {
                return Result(error_message);
        }

        bool is_success() const noexcept {
                return error_message.empty();
        }

        const std::string& get_error_message() const {
            return error_message;
        }

    private:
        explicit Result(const std::string& error_message = "")
                : error_message(error_message) {
        }

        std::string error_message;
};

template <typename T>
class TypedResult{
    static_assert(std::is_default_constructible<T>::value, "T must be default constructible");

    public:
        static TypedResult<T> Success(T value) {
                return TypedResult<T>(value);
        }

        static TypedResult<T> Failure(const std::string& error_message) {
                return TypedResult<T>(error_message);
        }

        bool is_success() const noexcept {
                return error_message.empty();
        }

        T get_value() const {
            if (!is_success()) {
                throw std::runtime_error("Cannot get value from a failed result");
            }
            return value;
        }

        const std::string& get_error_message() const {
            return error_message;
        }

    private:
        explicit TypedResult(const std::string& error_message = "")
                : error_message(error_message) {
        }

        explicit TypedResult(T value)
                : value(value) {
        }

        std::string error_message;
        T value{};
};

class Clock {
    public:
        Clock() : start_time(std::chrono::steady_clock::now()) {
        }

        timestamp_t get_timestamp() const {
                auto now = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time);
                return static_cast<timestamp_t>(duration.count());
        }

        double get_timestamp_seconds() const {
                return static_cast<double>(get_timestamp()) / 1000.0;
        }

        void correct_time(offset_t offset) {
                bool is_negative = offset < 0;
                if (is_negative) {
                    offset = -offset;
                }

                auto offset_in_ms = std::chrono::milliseconds(static_cast<long long>(offset));

                if (is_negative) {
                    start_time -= offset_in_ms;
                } else {
                    start_time += offset_in_ms;
                }
        }

        static timestamp_t from_seconds(double seconds) {
                return static_cast<timestamp_t>(seconds * 1000.0);
        }

    private:
        std::chrono::steady_clock::time_point start_time;
};

class synchronization{
    public:
        synchronization(const peer& peer, synchronized_t synchronized, timestamp_t t1, timestamp_t t2, timestamp_t timeout): synchronizedWith(peer), synchronized(synchronized), timestamps{t1,t2,0}, SYNC_TIMEOUT(timeout) {
        }

        bool should_be_abandoned(timestamp_t time) const {
            return time - timestamps[2] > SYNC_TIMEOUT;
        }

        Result is_sync_response_valid(const peer& peer, timestamp_t time, synchronized_t sync) const {
            if (sync != synchronized) {
                return Result::Failure("Synchronization mismatch.");
            }

            if (synchronizedWith != peer) {
                return Result::Failure("Peer is not the one the sync in with.");
            }

            if(should_be_abandoned(time)) {
                return Result::Failure("Synchronization should be abandoned. Timeout.");
            }

            return Result::Success();
        }

        TypedResult<offset_t> finish_sync(const peer& peer, timestamp_t time, synchronized_t sync, timestamp_t t4) {
            if(!t3_set) {
                return TypedResult<offset_t>::Failure("T3 is not set.");
            }

            Result result = is_sync_response_valid(peer, time, sync);
            if (!result.is_success()) {
                return TypedResult<offset_t>::Failure(result.get_error_message());
            }

            auto [t1, t2, t3] = timestamps;
            auto offset = (static_cast<offset_t>(t2) - static_cast<offset_t>(t1) + static_cast<offset_t>(t3) - static_cast<offset_t>(t4)) / 2;
            return TypedResult<offset_t>::Success(offset);
        }

        void set_time_of_sending_response(timestamp_t t3) {
            if (t3_set) {
                throw std::runtime_error("T3 is already set.");
            }

            timestamps[2] = t3;
            t3_set = true;
        }

    private:
        const peer& synchronizedWith;
        const synchronized_t synchronized;
        std::array<timestamp_t, 3> timestamps;
        bool t3_set = false;

        const timestamp_t SYNC_TIMEOUT;

};

class local_synchronization {
    public:
    const static synchronized_t NOT_SYNCHRONIZED = 255;
    const static synchronized_t LEADER = 0;

        local_synchronization(timestamp_t timeout) : synchronizedWith(nullptr), synchronized(NOT_SYNCHRONIZED), SYNC_TIMEOUT(timeout) {
        }

        synchronized_t get_synchronized() const {
                return synchronized;
        }

        bool is_synchronized() const {
                return synchronized != NOT_SYNCHRONIZED;
        }

        bool is_leader() const {
                return synchronized == LEADER;
        }

        Result set_leader(timestamp_t time) {
            if (is_leader()) {
                return Result::Failure("Already a leader");
            }

            synchronized = LEADER;
            last_sync_time = time;
            synchronizedWith.reset();
            return Result::Success();
        }

        Result unset_leader() {
            if (!is_leader()) {
                return Result::Failure("Not a leader");
            }

            synchronized = NOT_SYNCHRONIZED;
            last_sync_time = 0;
            synchronizedWith.reset();

            return Result::Success();
        }

        bool can_start_sync(timestamp_t time, timestamp_t delay) const {
            if(!is_synchronized()){
                return false;
            }

            return time - last_sync_time > delay;
        }

        void timeout_synchronization(timestamp_t time) {
            if (!is_synchronized()){
                return;
            }
            
            if (time - last_sync_time > SYNC_TIMEOUT) {
                synchronized = NOT_SYNCHRONIZED;
                synchronizedWith.reset();
                last_sync_time = 0;
            }
        }

        Result can_synchronize_with(const peer& peer, synchronized_t sync) const {
            const synchronized_t MAX_SYNC = 254;

            if(sync >= MAX_SYNC) {
                return Result::Failure("Synchronization value exceeds maximum.");
            }

            if(sync >= synchronized){
                return Result::Failure("Synchronization value is not less than current.");
            }

            if(!is_synchronized() || *synchronizedWith != peer) {
                if(synchronized - sync < 2){
                    return Result::Failure("Synchronization value is not less than current by 2.");
                }
            }

            return Result::Success();
        }

        Result set_synchronized(synchronized_t sync, std::unique_ptr<peer> peer_ptr,
                                                    timestamp_t time) {
                auto can_synchronize = can_synchronize_with(*peer_ptr, sync);
                if(!can_synchronize.is_success()) {
                    return can_synchronize;
                }

                synchronized = sync + 1;
                last_sync_time = time;
                synchronizedWith = std::move(peer_ptr);
                return Result::Success();
        }

        Result start_sync(const domain::peer& peer, synchronized_t sync, timestamp_t t1, timestamp_t t2) {
            if(sync_obj){
                // Sync is already in progress
                return Result::Failure("Synchronization already in progress");
            }

            auto can_synchronize = can_synchronize_with(peer, sync);
            if(!can_synchronize.is_success()) {
                return can_synchronize;
            }
    
            sync_obj = std::make_unique<synchronization>(peer, sync, t1, t2, SYNC_TIMEOUT);
            return Result::Success();
        }

        void set_time_of_sending_response(timestamp_t t3) {
            if(!sync_obj) {
                throw std::runtime_error("No sync in progress");
            }

            sync_obj->set_time_of_sending_response(t3);
        }
    
        void validate_sync_timeout(timestamp_t time) {
            if(!sync_obj) {
                return;
            }
    
            if(!sync_obj->should_be_abandoned(time)) {
                return;
            }
    
            sync_obj.reset();
        }

        TypedResult<offset_t> finish_sync(const peer& peer, timestamp_t time, synchronized_t sync, timestamp_t t4) {
            if(!sync_obj) {
                throw std::runtime_error("No sync in progress");
            }

            auto sync_time_result = sync_obj->finish_sync(peer, time, sync, t4);
            if (!sync_time_result.is_success()) {
                return sync_time_result;
            }

            set_synchronized(sync, std::make_unique<domain::peer>(peer), time);

            // Abandon finished sync
            sync_obj.reset();

            auto offset = sync_time_result.get_value();
            return TypedResult<offset_t>::Success(offset);
        }

    private:
        std::unique_ptr<const peer> synchronizedWith;
        synchronized_t synchronized;
        timestamp_t last_sync_time;
        std::unique_ptr<synchronization> sync_obj;

        const timestamp_t SYNC_TIMEOUT;
};

class synchronization_point{
    public:

        synchronization_point(timestamp_t timeout): sent_to{}, SYNC_TIMEOUT(timeout) {
        }

        void register_sync_start_with_peer(const domain::peer& peer, timestamp_t time) {
            sent_to[peer] = time;

            last_sync_time = time;
        }

        Result is_delay_request_valid(const domain::peer& peer, timestamp_t time) {
            auto it = sent_to.find(peer);
            if (it == sent_to.end()) {
                return Result::Failure("Peer not found");
            }

            if (time - it->second > SYNC_TIMEOUT) {
                sent_to.erase(it);
                return Result::Failure("Delay request timeout");
            }

            return Result::Success();
        }

        Result register_delay_request(const domain::peer& peer, timestamp_t time) {
            auto validation_result = is_delay_request_valid(peer, time);
            if (!validation_result.is_success()) {
                return validation_result;
            }

            sent_to.erase(peer);
            return Result::Success();
        }

        Result start_sync(timestamp_t time) {
            if(! have_delay_passed_since_last_sync(time, SYNC_TIMEOUT)){
                return Result::Failure("Delay has not passed since last sync");
            }
            last_sync_time = time;
            return Result::Success();
        }

        bool have_delay_passed_since_last_sync(timestamp_t time, timestamp_t delay) const noexcept {
            return time - last_sync_time > delay;
        }

    private:
        std::map<peer,timestamp_t> sent_to;
        timestamp_t last_sync_time;
        const timestamp_t SYNC_TIMEOUT;
};

struct peer_status_t {

};

class Node {
  public:
    Node(timestamp_t delay_after_becoming_leader, timestamp_t delay_bwtween_syncs, timestamp_t sync_timeout)
        : peers{}, waiting_for_connect_ack{}, waiting_for_hello_rsp{}, clock{}, local_sync(delay_after_becoming_leader), sync_point(sync_timeout), 
        DELAY_AFTER_BECOMING_LEADER(delay_after_becoming_leader), DELAY_BWTWEEN_SYNCS(delay_bwtween_syncs) {
    }

    local_synchronization& get_local_synchronization() {
        return local_sync;
    }

    bool can_start_synchronization() const {
        if(!local_sync.can_start_sync(clock.get_timestamp(), DELAY_AFTER_BECOMING_LEADER)) {
            return false;
        }

        return sync_point.have_delay_passed_since_last_sync(clock.get_timestamp(), DELAY_BWTWEEN_SYNCS);
    }

    void send_begin_sync() {
        if(!can_start_synchronization()) {
            throw std::runtime_error("Cannot start synchronization");
        }

        sync_point.start_sync(clock.get_timestamp());
    }

    timestamp_t mark_send_sync(const domain::peer& peer) {
        timestamp_t time = clock.get_timestamp();
        sync_point.register_sync_start_with_peer(peer, time);
        return time;
    }

    Result mark_sync_response(const domain::peer& peer, timestamp_t time) {
        return sync_point.register_delay_request(peer, time);
    }

    void correct_time(offset_t offset) {
        clock.correct_time(offset);
    }

    Result add_peer(const domain::peer& peer) {
        if (peers.find(peer) != peers.end()) {
            return Result::Failure("Peer already exists.");
        }

        peers.insert({peer, peer_status_t{}});
        return Result::Success();
    }

    void add_range(const std::vector<domain::peer>& new_peers) {
        for (const auto& peer : new_peers) {
            peers.insert({peer, peer_status_t{}});
        }
    }

    bool has_peer(const domain::peer& peer) const {
        return peers.find(peer) != peers.end();
    }

    void add_waiting_for_connect_ack(const domain::peer& peer) {
        waiting_for_connect_ack.insert(peer);
    }

    void add_waiting_for_hello_rsp(const domain::peer& peer) {
        waiting_for_hello_rsp.insert(peer);
    }

    Result acknowledge_connect(const domain::peer& peer) {
        auto it = waiting_for_connect_ack.find(peer);
        if (it == waiting_for_connect_ack.end()) {
            return Result::Failure("Connect ack was not expected.");
        }

        waiting_for_connect_ack.erase(it);

        return add_peer(peer);
    }

    Result acknowledge_hello_rsp(const domain::peer& peer) {
        auto it = waiting_for_hello_rsp.find(peer);
        if (it == waiting_for_hello_rsp.end()) {
            return Result::Failure("Hello response was not expected.");
        }

        waiting_for_hello_rsp.erase(it);
        return add_peer(peer);
    }

    peer_status_t& get_peer_status(const domain::peer& peer) {
        auto it = peers.find(peer);
        if (it != peers.end()) {
            return it->second;
        }

        throw std::runtime_error("Peer not found");
    }

    std::vector<peer> get_peers() const {
        std::vector<peer> peer_vector;
        peer_vector.reserve(peers.size());
        for (const auto& pair : peers) {
            peer_vector.push_back(pair.first);
        }
        return peer_vector;
    }

    timestamp_t get_time() const {
        return clock.get_timestamp();
    }

  private:
    std::map<domain::peer, peer_status_t> peers;
    std::set<domain::peer> waiting_for_connect_ack;
    std::set<domain::peer> waiting_for_hello_rsp;
    Clock clock;
    local_synchronization local_sync;
    synchronization_point sync_point;

    const timestamp_t DELAY_AFTER_BECOMING_LEADER;
    const timestamp_t DELAY_BWTWEEN_SYNCS;
};

} // namespace domain

#endif
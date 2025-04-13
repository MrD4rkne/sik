#include "domain.h"

#include <sstream>

namespace domain {

using namespace natural_time;
using namespace results;

port_t peer::get_port() const noexcept {
    return this->peer_port;
}

const std::vector<uint8_t> peer::get_address() const noexcept {
    return this->peer_address;
}

peer_address_length_t peer::get_address_length() const noexcept {
    return static_cast<peer_address_length_t>(this->peer_address.size());
}

bool peer::operator<(const peer& other) const noexcept {
    if (peer_address != other.peer_address) {
        return peer_address < other.peer_address;
    }
    return peer_port < other.peer_port;
}

bool peer::operator==(const peer& other) const {
    return peer_address == other.peer_address && peer_port == other.peer_port;
}

bool peer::operator!=(const peer& other) const {
    return !(*this == other);
}

std::string peer::to_string() const {
    std::stringstream ss;
    for (size_t i = 0; i < this->peer_address.size(); ++i) {
        ss << static_cast<int>(this->peer_address[i]);
        if (i < this->peer_address.size() - 1) {
            ss << ".";
        }
    }

    ss << ":" << peer_port;
    return ss.str();
}

bool synchronization::should_be_abandoned(timestamp_t time) const {
    return time - timestamps[2] > SYNC_TIMEOUT;
}

results::Result
synchronization::is_sync_response_valid(const peer& peer, timestamp_t time,
                                        synchronized_t sync) const {
    if (sync != synchronized) {
        return Result::Failure("Synchronization mismatch.");
    }

    if (synchronizedWith != peer) {
        return Result::Failure("Peer is not the one the sync in with.");
    }

    if (should_be_abandoned(time)) {
        return Result::Failure("Synchronization should be abandoned. Timeout.");
    }

    return Result::Success();
}

TypedResult<offset_t> synchronization::finish_sync(const peer& peer,
                                                   timestamp_t time,
                                                   synchronized_t sync,
                                                   timestamp_t t4) {
    if (!t3_set) {
        return TypedResult<offset_t>::Failure("T3 is not set.");
    }

    Result result = this->is_sync_response_valid(peer, time, sync);
    if (!result.is_success()) {
        return TypedResult<offset_t>::Failure(result.get_error_message());
    }

    auto [t1, t2, t3] = timestamps;
    auto offset = (static_cast<offset_t>(t2) - static_cast<offset_t>(t1) +
                   static_cast<offset_t>(t3) - static_cast<offset_t>(t4)) /
                  2;
    return TypedResult<offset_t>::Success(offset);
}

void synchronization::set_time_of_sending_response(timestamp_t t3) {
    if (t3_set) {
        throw std::runtime_error("T3 is already set.");
    }

    timestamps[2] = t3;
    t3_set = true;
}

synchronized_t local_synchronization::get_synchronized() const {
    return synchronized;
}

bool local_synchronization::is_synchronized() const {
    return synchronized != NOT_SYNCHRONIZED;
}

bool local_synchronization::is_leader() const {
    return synchronized == LEADER;
}

Result local_synchronization::set_leader(timestamp_t time) {
    if (is_leader()) {
        return Result::Failure("Already a leader");
    }

    synchronized = LEADER;
    last_sync_time = time;
    synchronizedWith.reset();
    return Result::Success();
}

Result local_synchronization::unset_leader() {
    if (!is_leader()) {
        return Result::Failure("Not a leader");
    }

    synchronized = NOT_SYNCHRONIZED;
    last_sync_time = 0;
    synchronizedWith.reset();

    return Result::Success();
}

bool local_synchronization::can_start_sync(timestamp_t time,
                                           timestamp_t delay) const {
    if (!is_synchronized()) {
        return false;
    }

    return time - last_sync_time > delay;
}

void local_synchronization::timeout_synchronization(timestamp_t time) {
    if (!is_synchronized()) {
        return;
    }

    if (time - last_sync_time > SYNC_TIMEOUT) {
        synchronized = NOT_SYNCHRONIZED;
        synchronizedWith.reset();
        last_sync_time = 0;
    }
}

Result local_synchronization::can_synchronize_with(const peer& peer,
                                                   synchronized_t sync) const {
    const synchronized_t MAX_SYNC = 254;

    if (sync >= MAX_SYNC) {
        return Result::Failure("Synchronization value exceeds maximum.");
    }

    if (sync >= synchronized) {
        return Result::Failure(
            "Synchronization value is not less than current.");
    }

    if (!is_synchronized() || *synchronizedWith != peer) {
        if (synchronized - sync < 2) {
            return Result::Failure(
                "Synchronization value is not less than current by 2.");
        }
    }

    return Result::Success();
}

Result local_synchronization::set_synchronized(synchronized_t sync,
                                               const peer& peer,
                                               timestamp_t time) {
    auto can_synchronize = can_synchronize_with(peer, sync);
    if (!can_synchronize.is_success()) {
        return can_synchronize;
    }

    synchronized = sync + 1;
    last_sync_time = time;
    synchronizedWith = std::make_unique<const domain::peer>(peer);
    return Result::Success();
}

Result local_synchronization::start_sync(const domain::peer& peer,
                                         synchronized_t sync, timestamp_t t1,
                                         timestamp_t t2) {
    if (sync_obj) {
        // Sync is already in progress
        return Result::Failure("Synchronization already in progress");
    }

    auto can_synchronize = can_synchronize_with(peer, sync);
    if (!can_synchronize.is_success()) {
        return can_synchronize;
    }

    sync_obj =
        std::make_unique<synchronization>(peer, sync, t1, t2, SYNC_TIMEOUT);
    return Result::Success();
}

void local_synchronization::set_time_of_sending_response(timestamp_t t3) {
    if (!sync_obj) {
        throw std::runtime_error("No sync in progress");
    }

    sync_obj->set_time_of_sending_response(t3);
}

void local_synchronization::validate_sync_timeout(timestamp_t time) {
    if (!sync_obj) {
        return;
    }

    if (!sync_obj->should_be_abandoned(time)) {
        return;
    }

    sync_obj.reset();
}

TypedResult<offset_t> local_synchronization::finish_sync(const peer& peer,
                                                         timestamp_t time,
                                                         synchronized_t sync,
                                                         timestamp_t t4) {
    if (!sync_obj) {
        throw std::runtime_error("No sync in progress");
    }

    auto sync_time_result = sync_obj->finish_sync(peer, time, sync, t4);
    if (!sync_time_result.is_success()) {
        return sync_time_result;
    }

    set_synchronized(sync, peer, time);

    // Abandon finished sync
    sync_obj.reset();

    auto offset = sync_time_result.get_value();
    return TypedResult<offset_t>::Success(offset);
}

void synchronization_point::register_sync_start_with_peer(
    const domain::peer& peer, timestamp_t time) {
    sent_to[peer] = time;

    last_sync_time = time;
}

Result synchronization_point::is_delay_request_valid(const domain::peer& peer,
                                                     timestamp_t time) {
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

Result synchronization_point::register_delay_request(const domain::peer& peer,
                                                     timestamp_t time) {
    auto validation_result = is_delay_request_valid(peer, time);
    if (!validation_result.is_success()) {
        return validation_result;
    }

    sent_to.erase(peer);
    return Result::Success();
}

Result synchronization_point::start_sync(timestamp_t time) {
    if (!have_delay_passed_since_last_sync(time, SYNC_TIMEOUT)) {
        return Result::Failure("Delay has not passed since last sync");
    }
    last_sync_time = time;
    return Result::Success();
}

bool synchronization_point::have_delay_passed_since_last_sync(
    timestamp_t time, timestamp_t delay) const noexcept {
    return time - last_sync_time > delay;
}

local_synchronization& Node::get_local_synchronization() {
    return local_sync;
}

bool Node::can_start_synchronization() const {
    if (!local_sync.can_start_sync(clock.get_timestamp(),
                                   DELAY_AFTER_BECOMING_LEADER)) {
        return false;
    }

    return sync_point.have_delay_passed_since_last_sync(clock.get_timestamp(),
                                                        DELAY_BWTWEEN_SYNCS);
}

void Node::send_begin_sync() {
    if (!can_start_synchronization()) {
        throw std::runtime_error("Cannot start synchronization");
    }

    sync_point.start_sync(clock.get_timestamp());
}

timestamp_t Node::mark_send_sync(const domain::peer& peer) {
    timestamp_t time = clock.get_timestamp();
    sync_point.register_sync_start_with_peer(peer, time);
    return time;
}

Result Node::mark_sync_response(const domain::peer& peer, timestamp_t time) {
    return sync_point.register_delay_request(peer, time);
}

void Node::correct_time(natural_time::offset_t offset) {
    clock.correct_time(offset);
}

Result Node::add_peer(const domain::peer& peer) {
    if (peers.find(peer) != peers.end()) {
        return Result::Failure("Peer already exists.");
    }

    peers.insert({peer, peer_status_t{}});
    return Result::Success();
}

void Node::add_range(const std::vector<domain::peer>& new_peers) {
    for (const auto& peer : new_peers) {
        peers.insert({peer, peer_status_t{}});
    }
}

bool Node::has_peer(const domain::peer& peer) const {
    return peers.find(peer) != peers.end();
}

void Node::add_waiting_for_connect_ack(const domain::peer& peer) {
    waiting_for_connect_ack.insert(peer);
}

void Node::add_waiting_for_hello_rsp(const domain::peer& peer) {
    waiting_for_hello_rsp.insert(peer);
}

Result Node::acknowledge_connect(const domain::peer& peer) {
    auto it = waiting_for_connect_ack.find(peer);
    if (it == waiting_for_connect_ack.end()) {
        return Result::Failure("Connect ack was not expected.");
    }

    waiting_for_connect_ack.erase(it);

    return add_peer(peer);
}

Result Node::acknowledge_hello_rsp(const domain::peer& peer) {
    auto it = waiting_for_hello_rsp.find(peer);
    if (it == waiting_for_hello_rsp.end()) {
        return Result::Failure("Hello response was not expected.");
    }

    waiting_for_hello_rsp.erase(it);
    return add_peer(peer);
}

peer_status_t& Node::get_peer_status(const domain::peer& peer) {
    auto it = peers.find(peer);
    if (it != peers.end()) {
        return it->second;
    }

    throw std::runtime_error("Peer not found");
}

std::vector<peer> Node::get_peers() const {
    std::vector<peer> peer_vector;
    peer_vector.reserve(peers.size());
    for (const auto& pair : peers) {
        peer_vector.push_back(pair.first);
    }
    return peer_vector;
}

timestamp_t Node::get_time() const {
    return clock.get_timestamp();
}

} // namespace domain
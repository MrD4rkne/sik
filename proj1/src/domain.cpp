#include "domain.h"

#include <sstream>

namespace domain {

using namespace natural_time;
using namespace results;

port_t peer::get_port() const noexcept {
    return this->peer_port;
}

const std::array<uint8_t, 4> peer::get_address() const noexcept {
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
    return time - time_of_delay_request_sent > SYNC_TIMEOUT;
}

results::Result
synchronization::is_sync_response_valid(const peer& peer, timestamp_t time,
                                        synchronized_t sync) const {
    if (sync != synchronized) {
        return Result::Failure("Synchronization mismatch.");
    }
    
    if (!is_with_peer(peer)) {
        return Result::Failure("Peer is not the one the sync in with.");
    }

    if (should_be_abandoned(time)) {
        return Result::Failure("Synchronization should be abandoned. Timeout.");
    }

    return Result::Success();
}

bool synchronization::is_with_peer(const peer& peer) const {
    return synchronizingWith == peer;
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

void synchronization::set_time_of_sending_response(timestamp_t time, timestamp_t t3) {
    if (t3_set) {
        throw std::runtime_error("T3 is already set.");
    }

    timestamps[2] = t3;
    time_of_delay_request_sent = time;
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

void local_synchronization::timeout_synchronization(timestamp_t time) {
    if (!is_synchronized() || is_leader()) {
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
    if (sync > MINIMAL_SYNCHRONIZED_FOR_SYNC) {
        return Result::Failure("Synchronization value exceeds minimum.");
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

void local_synchronization::set_time_of_sending_response(timestamp_t time, timestamp_t t3) {
    if (!sync_obj) {
        throw std::runtime_error("No sync in progress");
    }

    sync_obj->set_time_of_sending_response(time, t3);
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

void local_synchronization::invalidate_ongoing_sync() {
    if (sync_obj) {
        sync_obj.reset();
    }
}

TypedResult<offset_t> local_synchronization::finish_sync(const peer& peer,
                                                         timestamp_t time,
                                                         synchronized_t sync,
                                                         timestamp_t t4) {
    if (!sync_obj) {
        throw std::runtime_error("No sync in progress");
    }

    if(!sync_obj->is_with_peer(peer)){
        return TypedResult<offset_t>::Failure("Peer mismatch");
    }

    auto sync_time_result = sync_obj->finish_sync(peer, time, sync, t4);
    if (!sync_time_result.is_success()) {
        sync_obj.reset();
        return sync_time_result;
    }

    set_synchronized(sync, peer, time);

    // Abandon finished sync
    sync_obj.reset();

    auto offset = sync_time_result.get_value();
    return TypedResult<offset_t>::Success(offset);
}

bool local_synchronization::has_time_passed_since_becoming_leader(
    timestamp_t time, timestamp_t timeout) const {
    if (!is_leader()) {
        return false;
    }

    return time - last_sync_time > timeout;
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

results::Result synchronization_point::start_sending_sync(natural_time::timestamp_t time){
    if (is_sending_sync) {
        return Result::Failure("Already sending sync");
    }

    if(! have_delay_passed_since_last_sync(time)){
        return Result::Failure("Delay has not passed since last sync");
    }

    is_sending_sync = true;
    last_sync_time = time;
    return Result::Success();
}

void synchronization_point::end_sending_sync() {
    if (!is_sending_sync) {
        throw std::runtime_error("Not sending sync");
    }
    is_sending_sync = false;
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

bool synchronization_point::have_delay_passed_since_last_sync(
    timestamp_t time) const noexcept {
        if(last_sync_time == 0){
            return true;
        }
    return time - last_sync_time > DELAY_BWTWEEN_SYNCS;
}

Result Node::begin_sending_sync() {
    synchronized_t sync = local_sync.get_synchronized();
    if(sync > local_synchronization::MINIMAL_SYNCHRONIZED_FOR_SYNC){
        return Result::Failure("Synchronization value is too high.");
    }

    if(local_sync.is_leader() && !local_sync.has_time_passed_since_becoming_leader(clock.get_timestamp(), DELAY_AFTER_BECOMING_LEADER)){
        return Result::Failure("Delay has not passed since becoming leader.");
    }

    return sync_point.start_sending_sync(clock.get_timestamp());
}

void Node::end_sending_sync(){
    sync_point.end_sending_sync();
}

timestamp_t Node::mark_send_sync(const domain::peer& peer) {
    timestamp_t time = clock.get_timestamp();
    sync_point.register_sync_start_with_peer(peer, time);
    return time;
}

Result Node::mark_sync_response(const domain::peer& peer) {
    return sync_point.register_delay_request(peer, clock.get_timestamp());
}

Result Node::set_leader(){
    auto result = local_sync.set_leader(clock.get_timestamp());
    if(!result.is_success()){
        return result;
    }

    local_sync.invalidate_ongoing_sync();
    return Result::Success();
}

Result Node::unset_leader(){
    return local_sync.unset_leader();
}

const local_synchronization& Node::get_local_synchronization() const {
    return local_sync;
}

Result Node::start_sync(const domain::peer& peer, synchronized_t sync,
                         timestamp_t t1, timestamp_t t2) {
    return local_sync.start_sync(peer, sync, t1, t2);
}

Result Node::finish_sync(const domain::peer& peer,
    synchronized_t sync,
    natural_time::timestamp_t t4){
        auto result = local_sync.finish_sync(peer, clock.get_timestamp(), sync, t4);
        if(!result.is_success()){
            return Result::Failure(result.get_error_message());
        }

        auto offset = result.get_value();
        clock.correct_time(offset);
        return Result::Success();
    }

void Node::set_time_of_sending_response() {
    local_sync.set_time_of_sending_response(clock.get_timestamp(), clock.get_synced_timestamp());
}

void Node::correct_time(natural_time::offset_t offset) {
    clock.correct_time(offset);
}

Result Node::add_peer(const domain::peer& peer) {
    if (peers.find(peer) != peers.end()) {
        return Result::Failure("Peer already exists.");
    }

    // if(peers.size() >= (size_t)MAX_PEERS) {
    //     return Result::Failure("Maximum number of peers reached.");
    // }

    peers.insert(peer);
    return Result::Success();
}

void Node::add_range(const std::vector<domain::peer>& new_peers) {
    for (const auto& peer : new_peers) {
        peers.insert(peer);
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

void Node::validate_curr_sync_timeout() {
    local_sync.timeout_synchronization(clock.get_timestamp());
}

void Node::validate_sync_timeout() {
    local_sync.validate_sync_timeout(clock.get_timestamp());
}

std::vector<peer> Node::get_peers() const {
    std::vector<peer> peer_vector;
    peer_vector.reserve(peers.size());
    for (const auto& peer : peers) {
        peer_vector.push_back(peer);
    }
    return peer_vector;
}

timestamp_t Node::get_time() const {
    return clock.get_timestamp();
}

} // namespace domain
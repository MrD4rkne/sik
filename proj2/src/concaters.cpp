#include "concaters.h"
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace concaters {

static inline const std::string END_OF_MESSAGE = "\r\n";

std::vector<std::string> MessageConcater::put_data(const std::string& data) {
    buffer += data;
    std::vector<std::string> messages;

    size_t pos = 0;
    while ((pos = buffer.find(concaters::END_OF_MESSAGE)) !=
           std::string::npos) {
        messages.push_back(buffer.substr(0, pos));
        buffer.erase(0, pos + concaters::END_OF_MESSAGE.size());
    }

    return messages;
}

void MessageBuffer::add_message(
    const std::string& message,
    const std::function<void(const std::string&)>& callback,
    uint64_t send_in_millis) {
    Message msg;
    msg.data = std::string(message);
    msg.callback = callback;
    msg.send_time = std::chrono::system_clock::now() -
                    std::chrono::milliseconds(send_in_millis);
    buffer.push(msg);
}

uint64_t MessageBuffer::next_message_time() const {
    if (current_message != nullptr || buffer.empty()) {
        return UINT64_MAX;
    }

    auto& message = current_message ? *current_message : buffer.top();
    auto now = std::chrono::system_clock::now();
    auto time_left = message.send_time - now;
    if (time_left < std::chrono::milliseconds(0)) {
        return 0;
    }

    uint64_t millis =
        std::chrono::duration_cast<std::chrono::milliseconds>(time_left)
            .count();
    return millis;
}

bool MessageBuffer::has_message() const {
    return next_message_time() == 0;
}

static const std::string EMPTY = "";

const std::string& MessageBuffer::get_buffer() {
    if (!has_message()) {
        throw std::runtime_error("Buffer is empty");
    }

    if (!current_message) {
        auto& message = buffer.top();
        current_message = std::make_unique<Message>(message);
        buffer.pop();
    }

    return current_message->data;
}

void MessageBuffer::mark_sent(size_t bytes) {
    if (current_message == nullptr) {
        throw std::runtime_error("Buffer is empty");
    }

    current_message->data.erase(0, bytes);
    if (current_message->data.empty()) {
        current_message->callback(current_message->data);
        current_message.reset();
    }
}

} // namespace concaters
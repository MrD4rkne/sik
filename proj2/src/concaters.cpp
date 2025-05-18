#include "concaters.h"
#include <stdexcept>

namespace concaters{

static inline const std::string END_OF_MESSAGE = "\r\n";

std::vector<std::string> MessageConcater::put_data(const std::string& data) {
    buffer += data;
    std::vector<std::string> messages;

    size_t pos = 0;
    while ((pos = buffer.find(concaters::END_OF_MESSAGE)) != std::string::npos) {
        messages.push_back(buffer.substr(0, pos));
        buffer.erase(0, pos + concaters::END_OF_MESSAGE.size());
    }

    return messages;
}

void MessageBuffer::add_message(
    const std::string& message,
    const std::function<void(const std::string&)>& callback) {
    Message msg;
    msg.data = message;
    msg.callback = callback;
    buffer.push(msg);
}

bool MessageBuffer::has_message() const {
    return !buffer.empty();
}

const std::string& MessageBuffer::get_buffer() const {
    if (!has_message()) {
        throw std::runtime_error("Buffer is empty");
    }

    auto& message = buffer.front();
    return message.data;
}

void MessageBuffer::mark_sent(size_t bytes) {
    if (!has_message()) {
        throw std::runtime_error("Buffer is empty");
    }

    auto& message = buffer.front();
    message.data.erase(0, bytes);
    if (message.data.empty()) {
        buffer.pop();
    }
}

} // namespace concaters
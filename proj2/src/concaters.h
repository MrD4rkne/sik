#ifndef CONCATERS_H
#define CONCATERS_H

#include <chrono>
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <vector>

namespace concaters {

class MessageConcater {
  public:
    std::vector<std::string> put_data(const std::string& data);

  private:
    std::string buffer;
};

class MessageBuffer {

  public:
    MessageBuffer() = default;

    void add_message(const std::string& message,
                     const std::function<void(const std::string&)>& callback,
                     uint64_t send_in_millis = 0);

    bool has_message() const;

    uint64_t next_message_time() const;

    const std::string& get_buffer();

    void mark_sent(size_t bytes);

  private:
    struct Message {
        std::string data;
        std::function<void(const std::string&)> callback;
        std::chrono::time_point<std::chrono::system_clock> send_time;

        bool operator<(const Message& other) const {
            return send_time > other.send_time;
        }
    };

    std::unique_ptr<Message> current_message;
    std::priority_queue<Message> buffer;
};

} // namespace concaters

#endif
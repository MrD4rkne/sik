#ifndef CONCATERS_H
#define CONCATERS_H

#include <queue>
#include <vector>
#include <functional>
#include <string>

namespace concaters {

class MessageConcater {
  public:
    std::vector<std::string> put_data(const std::string& data);

  private:
    std::string buffer;
};

class MessageBuffer {

  public:
    void add_message(const std::string& message,
                     const std::function<void(const std::string&)>& callback);

    bool has_message() const;

    const std::string& get_buffer() const;

    void mark_sent(size_t bytes);

  private:
    struct Message {
        std::string data;
        std::function<void(const std::string&)> callback;
    };

    std::queue<Message> buffer;
};

} // namespace concaters

#endif
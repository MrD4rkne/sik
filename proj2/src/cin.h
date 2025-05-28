#ifndef CIN_H
#define CIN_H

#include "concaters.h"
#include "fd.h"
#include <deque>
#include <unistd.h>

class cin_fd_handler : public fd::FDHandler {
  public:
    cin_fd_handler() : message_concater("\n") {
    }

    void handle(int socket_fd, short events) override {
        if (socket_fd != STDIN_FD) {
            throw std::invalid_argument("Invalid file descriptor");
        }

        if (events & POLLIN) {
            std::string input;
            char buffer[1024]; // Buffer for reading
            ssize_t bytes_read = read(socket_fd, buffer, sizeof(buffer) - 1);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0'; // Null-terminate the buffer
                input = std::string(buffer, bytes_read);

                auto messages = message_concater.put_data(input);
                for (const auto& message : messages) {
                    input_buffer.push_back(message);
                }

                if (!messages.empty()) {
                    waiting_for_input = false;
                }
            } else if (bytes_read == 0) {
                // TODO
            } else {
                // TODO
            }
        }
    }

    void start_listenning() {
        waiting_for_input = true;
    }

    void stop_listenning() {
        waiting_for_input = false;
    }

    bool has_input() const {
        return !input_buffer.empty();
    }

    std::string get_input() {
        if (!has_input()) {
            throw std::runtime_error("No input available");
        }

        return input_buffer.front();
    }

    void pop_input() {
        if (!has_input()) {
            throw std::runtime_error("No input available");
        }

        input_buffer.pop_front();
    }

    short get_events(int socket_fd) const override {
        if (socket_fd != STDIN_FD) {
            throw std::invalid_argument("Invalid file descriptor");
        }

        if (waiting_for_input) {
            return POLLIN;
        }

        return 0;
    }

    int get_event_change_time(int fd) const override {
        return NO_TIMEOUT;
    }

  private:
    constexpr static int NO_TIMEOUT = -1;
    constexpr static int STDIN_FD = 0;
    bool waiting_for_input;
    concaters::MessageConcater message_concater;
    std::deque<std::string> input_buffer;
};

#endif
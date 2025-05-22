#ifndef FILE_H
#define FILE_H

#include "concaters.h"
#include "fd.h"
#include <fcntl.h>
#include <functional>
#include <stdexcept>

namespace file {
class FileHandler : public fd::FDHandler {
  public:
    FileHandler(const std::string& filePath,
                std::function<void(const std::string msg)> on_receive)
        : fd(DEFAULT_FD), filePath(filePath), concater{},
          on_receive(on_receive) {
    }

    void open_file() {
        if (fd != DEFAULT_FD) {
            throw new std::runtime_error("File already opened.");
        }

        fd = open(filePath.c_str(), O_RDONLY);
        if (fd < -1) {
            throw std::runtime_error("Could not open the file");
        }
    }

    void close_file() {
        if (fd == DEFAULT_FD) {
            throw new std::runtime_error("File not opened.");
        }

        close(fd);
        fd = DEFAULT_FD;
    }

    bool is_waiting_for_line() const {
        return waiting_for_line;
    }

    void request_line() {
        if (is_waiting_for_line()) {
            throw std::runtime_error("Already waiting for line.");
        }

        waiting_for_line = true;
    }

    void handle(int socket_fd, short events) override {
        if (socket_fd != fd) {
            throw std::runtime_error("FD mismatch");
        }

        if (events != POLLIN) {
            return;
        }

        char buffer[1024];
        ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
        if (bytes < 0) {
            // TODO: do not throw when managable errno.
            throw std::runtime_error("Error when reading from file.");
        }

        buffer[bytes] = 0;
        auto messages = concater.put_data(std::string(buffer));
        for (auto& msg : messages) {
            on_receive(msg);
        }

        if (messages.size() > 0) {
            waiting_for_line = false;
        }
    }

    short get_events(int socket_fd) const override {
        if (socket_fd != fd) {
            throw std::runtime_error("FD mismatch");
        }

        if (is_waiting_for_line()) {
            return POLLIN;
        }

        return 0;
    }

    int get_event_change_time(int fd) const override {
        if (fd != this->fd) {
            throw std::runtime_error("FD mismatch");
        }

        return -1;
    }

    int get_fd() const {
        return fd;
    }

  private:
    constexpr static int DEFAULT_FD = -1;
    int fd;
    const std::string filePath;
    concaters::MessageConcater concater;

    bool waiting_for_line = false;
    std::function<void(const std::string msg)> on_receive;
};
} // namespace file

#endif
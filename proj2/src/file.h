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

        logger.log_debug("Opening file: ", filePath);

        fd = open(filePath.c_str(), O_RDONLY);
        if (fd < 0) {
            throw std::runtime_error("Could not open the file: " + std::string(strerror(errno)));
        }

        logger.log_debug("File opened successfully: ", filePath);
        waiting_for_line = false;
        logger.log_debug("FD: ", (int)fd);
    }

    void close_file() {
        if (fd == DEFAULT_FD) {
            throw new std::runtime_error("File not opened.");
        }

        logger.log_debug("Closing file: ", filePath);

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

        logger.log_debug("Requesting line from file: ", filePath);
        waiting_for_line = true;
    }

    void handle(int socket_fd, short events) override {
        logger.log_debug("Handling events for file: ", filePath);

        if (socket_fd != fd) {
            throw std::runtime_error("FD mismatch");
        }

        if (events != POLLIN) {
            return;
        }

        char buffer[1024];
        ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
        if (bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                logger.log_debug("When reading from file, received EAGAIN or EWOULDBLOCK", filePath);
                return;
            }

            logger.log_error("Error reading from file: ", filePath, " - ", strerror(errno));
            throw std::system_error(errno, std::iostream_category(),
                                    "Error reading from file: " + filePath);
        }

        logger.log_debug("Read ", bytes, " bytes from file: ", filePath);

        buffer[bytes] = 0;
        auto messages = concater.put_data(std::string(buffer));
        for (auto& msg : messages) {
            logger.log_debug("Read message: ", msg);
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
            logger.log_debug("File is waiting for line: ", filePath);
            return POLLIN;
        }

        return 0;
    }

    int get_event_change_time(int socket_fd) const override {
        if (socket_fd != this->fd) {
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
    logging::Logger logger{};

    bool waiting_for_line = false;
    std::function<void(const std::string msg)> on_receive;
};
} // namespace file

#endif
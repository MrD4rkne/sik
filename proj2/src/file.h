#ifndef FILE_H
#define FILE_H

#include "coeff.h"
#include "concaters.h"
#include "fd.h"
#include <fcntl.h>
#include <functional>
#include <stdexcept>

/// @brief Stuff related to file handling, including reading from files
namespace file {

/// @brief Handler for reading lines from a file and processing them into
/// messages.
class FileHandler : public fd::FDHandler {
  public:
    FileHandler(const std::string& filePath,
                std::function<void(const std::string msg)> on_receive)
        : fd(DEFAULT_FD), filePath(filePath), concater{},
          on_receive(on_receive) {
    }

    /// @brief Open the file for reading. Throws an exception if the file is
    /// already
    void open_file() {
        if (fd != DEFAULT_FD) {
            throw new std::runtime_error("File already opened.");
        }

        logger.log_debug("Opening file: ", filePath);

        fd = open(filePath.c_str(), O_RDONLY);
        if (fd < 0) {
            throw std::runtime_error("Could not open the file: " +
                                     std::string(strerror(errno)));
        }

        logger.log_debug("File opened successfully: ", filePath);
        waiting_for_line = false;
        logger.log_debug("FD: ", (int)fd);
    }

    /// @brief Close the file.
    /// @throws std::runtime_error if the file is not opened.
    void close_file() {
        if (fd == DEFAULT_FD) {
            throw new std::runtime_error("File not opened.");
        }

        logger.log_debug("Closing file: ", filePath);

        close(fd);
        fd = DEFAULT_FD;
    }

    /// @brief Check if the file is opened.
    /// @return true if the file is opened, false otherwise.
    bool is_waiting_for_line() const {
        return waiting_for_line;
    }

    /// @brief Request a line from the file. If the file is already waiting for
    /// a
    void request_line() {
        if (is_waiting_for_line()) {
            throw std::runtime_error("Already waiting for line.");
        }

        logger.log_debug("Requesting line from file: ", filePath);
        waiting_for_line = true;
    }

    /// @brief Handle the file descriptor events.
    /// @note We only handle POLLIN events, which indicate that the file is
    /// ready for reading.
    /// @param socket_fd The file descriptor of the file.
    /// @param events The events that occurred on the file descriptor.
    /// @throws std::runtime_error if the file descriptor does not match the one
    /// in this handler.
    /// @throws std::system_error if there is an error reading from the file.
    /// @note Do nothing if the events are not POLLIN.
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
                logger.log_debug(
                    "When reading from file, received EAGAIN or EWOULDBLOCK",
                    filePath);
                return;
            }

            logger.log_error("Error reading from file: ", filePath, " - ",
                             strerror(errno));
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

    /// @brief Get the events forthe poll.
    /// @param socket_fd The file descriptor of the file.
    /// @throws std::runtime_error if the file descriptor does not match the one
    /// in this handler.
    /// @return POLLIN if the file is waiting for a line, 0 otherwise.
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

    /// @brief Get the file descriptor of the file.
    /// @return The file descriptor of the file.
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

class COEFFFromFileProvider : public coeff::COEFFProvider {
  public:
    COEFFFromFileProvider(const std::string& file_name)
        : file_handler(
              std::make_shared<file::FileHandler>(file_name, on_new_line)) {
        file_handler->open_file();
    }

    size_t get_available_coeffs_count() const {
        return coeffs.size();
    }

    bool has_coeffs() const {
        return !coeffs.empty();
    }

    void request_coeffs() {
        if (file_handler->is_waiting_for_line()) {
            return;
        }
        file_handler->request_line();
    }

    const std::string& get_coeffs() {
        if (coeffs.empty()) {
            throw std::runtime_error("No coefficients available.");
        }

        return coeffs.front();
    }

    void pop_coeffs() {
        if (coeffs.empty()) {
            throw std::runtime_error("No coefficients available.");
        }
        coeffs.pop_front();
    }

    std::shared_ptr<file::FileHandler> get_fd_handler() {
        return file_handler;
    }

  private:
    std::function<void(const std::string msg)> on_new_line =
        [&](const std::string& line) { coeffs.push_back(line); };

    std::deque<std::string> coeffs;
    std::shared_ptr<file::FileHandler> file_handler;
};
} // namespace file

#endif
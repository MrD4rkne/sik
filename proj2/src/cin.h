#ifndef CIN_H
#define CIN_H

#include "concaters.h"
#include "fd.h"
#include <deque>
#include <unistd.h>

/// @brief Handler for standard input (stdin) that reads data and processes it
/// into messages.
class cin_fd_handler : public fd::FDHandler {
  public:
    cin_fd_handler() : message_concater("\n"), logger{} {
    }

    void handle(int fd, short events) override {
        if (fd != STDIN_FD) {
            throw std::invalid_argument("Invalid file descriptor");
        }

        logger.log_debug("Handling standard input");

        if ((events & POLLIN) == 0) {
            return;
        }

        std::string input;
        char buffer[1024]; // Buffer for reading
        ssize_t bytes_read = read(STDIN_FD, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0'; // Null-terminate the buffer
            input = std::string(buffer, (size_t)bytes_read);

            logger.log_debug("Read from stdin: ", input);

            auto messages = message_concater.put_data(input);
            for (const auto& message : messages) {
                input_buffer.push_back(message);
            }

            if (!messages.empty()) {
                waiting_for_input = false;
            }

        } else if (bytes_read == 0) {
            logger.log_info("End of input stream detected, closing stdin handler");
            throw std::domain_error("End of input stream detected, closing stdin handler");
        } else {
            logger.log_error("Error reading from standard input: ", strerror(errno));
            
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                return;
            }

            throw std::system_error(errno, std::generic_category(),
                                    "Error reading from standard input");
        }
    }

    void start_listenning() {
        if (waiting_for_input) {
            return;
        }

        logger.log_info("Starting to listen for standard input");
        waiting_for_input = true;
    }

    void stop_listenning() {
        if (!waiting_for_input) {
            return;
        }
        
        logger.log_info("Stopping listening for standard input");
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
        if (socket_fd == CLOSED_FD) {
            throw std::length_error("File descriptor is closed");
        }

        if (socket_fd != STDIN_FD) {
            throw std::invalid_argument("Invalid file descriptor");
        }

        if (waiting_for_input) {
            logger.log_debug("STDIN hander is waiting for input");
            return POLLIN;
        }

        return 0;
    }

    int get_event_change_time(int fd) const override {
        if (fd == CLOSED_FD) {
            throw std::length_error("File descriptor is closed");
        }
        
        if (fd != STDIN_FD) {
            throw std::invalid_argument("Invalid file descriptor");
        }

        return NO_TIMEOUT;
    }

  private:
    constexpr static int CLOSED_FD = -1;
    constexpr static int NO_TIMEOUT = -1;
    constexpr static int STDIN_FD = 0;
    bool waiting_for_input;
    concaters::MessageConcater message_concater;
    std::deque<std::string> input_buffer;
    logging::Logger logger;
};

#endif
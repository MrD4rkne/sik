#ifndef FD_H
#define FD_H

#include "logging.h"
#include <deque>
#include <memory>
#include <poll.h>
#include <unordered_map>
#include <vector>

/// @brief Stuff related to file descriptors (FDs) and their handling.
namespace fd {

/// @brief Interface for handling file descriptors events from poller.
class FDHandler {
  public:
    virtual void handle(int socket_fd, short events) = 0;

    virtual short get_events(int socket_fd) const = 0;

    virtual int get_event_change_time(int fd) const = 0;

    virtual ~FDHandler() = default;
};

/// @brief Poller for file descriptors (FDs) that uses the poll system call.
class FDPoller {
  public:
    void add_socket(int fd, std::shared_ptr<FDHandler> handler,
                    short events = POLLIN);

    /// @brief Remove a socket from the poller.
    /// @param fd The file descriptor of the socket to remove.
    /// @throws std::invalid_argument if the file descriptor is not found.
    void remove_socket(int fd);

    /// @brief Change the events for a socket.
    /// @param timeout_ms The timeout in milliseconds for the poll call. -1 for
    /// no timeout.
    /// @note This method checks for timeout from each handler and takes the
    /// minimum timeout.
    int poll_sockets(int timeout_ms = -1);

    /// @brief Check if there are any ready sockets to be processed.
    bool has_ready_socket();

    /// @brief Process the next ready socket.
    void process_next();

    /// @brief Clear all ready sockets from the last poll call.
    /// @note This method should be called after processing all ready sockets.
    void clear_round();

  private:
    struct descriptor {
        int fd;
        size_t index;
        std::shared_ptr<FDHandler> handler;
    };

    std::deque<int> ready_fds;
    std::vector<pollfd> poll_fds;
    std::unordered_map<int, descriptor> fd_to_index;
    logging::Logger logger;
};

}; // namespace fd

#endif
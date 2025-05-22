#ifndef FD_H
#define FD_H

#include "logging.h"
#include <memory>
#include <poll.h>
#include <unordered_map>
#include <vector>
#include <deque>

namespace fd {

class FDHandler {
  public:
    virtual void handle(int socket_fd, short events) = 0;

    virtual short get_events(int socket_fd) const = 0;

    virtual int get_event_change_time(int fd) const = 0;

    virtual ~FDHandler() = default;
};

class FDPoller {
  public:
    void add_socket(int fd, std::shared_ptr<FDHandler> handler,
                    short events = POLLIN);

    void remove_socket(int fd);

    int poll_sockets(int timeout_ms = -1);

    bool has_ready_socket();

    void process_next();

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
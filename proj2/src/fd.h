#ifndef FD_H
#define FD_H

#include "logging.h"
#include <memory>
#include <poll.h>
#include <unordered_map>
#include <vector>

namespace fd {

class FDHandler {
  public:
    virtual void handle(int socket_fd, short events) = 0;

    virtual short get_events(int socket_fd) const = 0;

    virtual uint64_t get_event_change_time(int fd) const = 0;

    virtual ~FDHandler() = default;
};

class FDPoller {
  public:
    void add_socket(int fd, std::shared_ptr<FDHandler> handler,
                    short events = POLLIN);

    void remove_socket(int fd);

    int poll_sockets(uint64_t timeout_ms = UINT64_MAX);

    bool has_events(int fd, short event_mask = POLLIN);

    std::vector<int> get_ready_sockets(short event_mask = POLLIN);

    void handle();

  private:
    struct descriptor {
        int fd;
        size_t index;
        std::shared_ptr<FDHandler> handler;
    };

    std::vector<pollfd> poll_fds;
    std::unordered_map<int, descriptor> fd_to_index;
    logging::Logger logger;
};

}; // namespace fd

#endif
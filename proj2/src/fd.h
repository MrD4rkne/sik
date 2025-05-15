#ifndef FD_H
#define FD_H

#include <poll.h>
#include <vector>
#include <unordered_map>
#include <memory>

namespace fd{

class FDHandler{
  public:
    virtual void handle(int socket_fd, short events) = 0;

    virtual short get_events(int socket_fd) const = 0;

    virtual ~FDHandler() = default;
};

class FDPoller {
  public:
    void add_socket(int fd, std::shared_ptr<FDHandler> handler, short events = POLLIN);

    void remove_socket(int fd);

    int poll_sockets(int timeout_ms = -1);

    bool has_events(int fd, short event_mask = POLLIN);

    std::vector<int> get_ready_sockets(short event_mask = POLLIN);

    void handle();

  private:
    struct descriptor{
        int fd;
        size_t index;
        std::shared_ptr<FDHandler> handler;
    };

    std::vector<pollfd> poll_fds;
    std::unordered_map<int, descriptor> fd_to_index;
};

};

#endif
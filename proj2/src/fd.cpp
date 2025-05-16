#include "fd.h"
#include <stdexcept>

namespace fd {
void FDPoller::add_socket(int fd, std::shared_ptr<FDHandler> handler,
                          short events) {
    if (fd_to_index.find(fd) != fd_to_index.end()) {
        throw std::invalid_argument("Socket already exists in poller");
    }

    pollfd pfd;
    pfd.fd = fd;
    pfd.events = events;
    pfd.revents = 0;
    poll_fds.push_back(pfd);
    fd_to_index[fd] = {fd, poll_fds.size() - 1, handler};
}

void FDPoller::remove_socket(int fd) {
    if (fd_to_index.find(fd) != fd_to_index.end()) {
        size_t idx = fd_to_index[fd].index;

        // Swap with the last element and pop
        if (idx != poll_fds.size() - 1) {
            std::swap(poll_fds[idx], poll_fds.back());
            fd_to_index[poll_fds[idx].fd].index = idx;
        }

        poll_fds.pop_back();
        fd_to_index.erase(fd);
    }
}

int FDPoller::poll_sockets(int timeout_ms) {
    // Reset revents before polling
    for (auto& pfd : poll_fds) {
        pfd.revents = 0;
    }

    for (const auto& descriptor : fd_to_index) {
        short events = descriptor.second.handler->get_events(descriptor.first);
        poll_fds[descriptor.second.index].events = events;
    }

    return poll(poll_fds.data(), poll_fds.size(), timeout_ms);
}

bool FDPoller::has_events(int fd, short event_mask) {
    auto it = fd_to_index.find(fd);
    if (it == fd_to_index.end()) {
        throw std::invalid_argument("Socket not found in poller");
    }

    size_t idx = it->second.index;
    return poll_fds[idx].revents & event_mask;
}

std::vector<int> FDPoller::get_ready_sockets(short event_mask) {
    std::vector<int> ready;
    for (const auto& pfd : poll_fds) {
        if (pfd.revents & event_mask) {
            ready.push_back(pfd.fd);
        }
    }
    return ready;
}

void FDPoller::handle() {
    for (const auto& pfd : poll_fds) {
        if (pfd.revents == 0) {
            continue; // No events
        }

        auto it = fd_to_index.find(pfd.fd);
        if (it != fd_to_index.end()) {
            it->second.handler->handle(pfd.fd, pfd.revents);
        }
    }
}

} // namespace fd
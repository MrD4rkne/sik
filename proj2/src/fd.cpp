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
    if (has_ready_socket()) {
        return 0;
    }

    // Reset revents before polling
    for (auto& pfd : poll_fds) {
        pfd.revents = 0;
    }

    for (const auto& descriptor : fd_to_index) {
        short events = descriptor.second.handler->get_events(descriptor.first);
        auto max_timeout =
            descriptor.second.handler->get_event_change_time(descriptor.first);
        if (max_timeout > 0) {
            if (timeout_ms > 0) {
                timeout_ms =
                    std::min(timeout_ms, static_cast<int>(max_timeout));
            } else {
                timeout_ms = max_timeout;
            }
        }

        poll_fds[descriptor.second.index].events = events;
    }

    logger.log_debug(
        "Polling sockets with timeout: " + std::to_string(timeout_ms) + " ms");

    int result = poll(poll_fds.data(), poll_fds.size(), timeout_ms);
    if (result >= 0) {
        for (auto& pfd : poll_fds) {
            if (pfd.revents == 0) {
                continue;
            }

            ready_fds.push_back(pfd.fd);
        }
    }

    return result;
}

bool FDPoller::has_ready_socket() {
    while (!ready_fds.empty()) {
        int fd = ready_fds.front();
        if (fd_to_index.find(fd) != fd_to_index.end()) {
            return true;
        }

        ready_fds.pop_front();
    }

    // Empty, no ready sockets.
    return false;
}

void FDPoller::process_next() {
    if (!has_ready_socket()) {
        return;
    }

    int fd = ready_fds.front();

    auto it = fd_to_index.find(fd);
    if (it == fd_to_index.end()) {
        throw std::runtime_error("Socket not found in poller");
    }

    auto& descriptor = it->second;
    short revents = poll_fds[descriptor.index].revents;

    try{
    descriptor.handler->handle(fd, revents);
    }
    catch(const std::domain_error& e) {
        remove_socket(fd);
        return;
    }

    ready_fds.pop_front();
}

void FDPoller::clear_round() {
    ready_fds.clear();
}

} // namespace fd
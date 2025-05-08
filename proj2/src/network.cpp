#include <netinet/in.h>
#include <string>

#include "network.h"

namespace network {

void MessageSender::send_message(const std::string& message) {
    ssize_t bytes_sent =
        sendto(socket_fd, message.c_str(), message.size(), 0, nullptr, 0);
    if (bytes_sent < 0) {
        throw std::runtime_error("sendto() failed");
    }

    // TODO: Handle the case where not all bytes were sent
}

} // namespace network
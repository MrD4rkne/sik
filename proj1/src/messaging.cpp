#include <netinet/in.h>

#include "messaging.h"
#include "logging.h"
#include "packets.h"
#include "err.h"

namespace messaging{

static inline bool send_message(int socket_fd, const char* buffer, size_t bytes_to_send, sockaddr_in* adress) {
    ssize_t sent_bytes = 0;
    size_t total_sent = 0;

    logging::connection::logDebug(adress, "Sending ", bytes_to_send, " bytes.");
    logging::connection::logDebug(adress, "Buffer: ", buffer);

    while (total_sent < bytes_to_send) {
        sent_bytes = sendto(socket_fd, buffer + total_sent, bytes_to_send - total_sent, 0, (sockaddr*)adress, sizeof(*adress));
        if (sent_bytes < 0) {
            error("sendto");
            return false;
        }
        total_sent += sent_bytes;

        logging::connection::logDebug(adress, "Sent ", sent_bytes, " of ", bytes_to_send, " bytes.");
    }

    logging::connection::logDebug(adress, "Sent ", total_sent, " bytes.");
    return true;
}

inline bool send_hello(int socket_fd, sockaddr_in* address){
    char buffer[1];
    buffer[0] = packets::MSG_TYPE_HELLO;

    logging::connection::logDebug(address, "Sending hello message.");
    
    return send_message(socket_fd, buffer, sizeof(buffer), address);
}

class hello_message_handler : public MessageHandler {
public:
    bool handle(Node& /*node*/, const sockaddr_in* client_address, size_t /*read_bytes*/, char* /*buffer*/) override {
        logging::connection::logDebug(client_address, "Received hello message.");
        return true;
    }
};

} // namespace messaging
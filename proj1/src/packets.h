#ifndef PACKETS_H
#define PACKETS_H

#include <cstdint>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "logging.h"
#include "messages.h"
#include "err.h"
#include "common.h"

namespace packets {
    static inline void send_message(int socket_fd, const char* buffer, size_t bytes_to_send, sockaddr_in* adress) {
        ssize_t sent_bytes = 0;
        size_t total_sent = 0;

        while (total_sent < bytes_to_send) {
            sent_bytes = sendto(socket_fd, buffer + total_sent, bytes_to_send - total_sent, 0, (sockaddr*)adress, sizeof(*adress));
            if (sent_bytes < 0) {
                error("sendto");
            }
            total_sent += sent_bytes;

            logging::connection::logDebug(adress, "Sent", sent_bytes, " of ", bytes_to_send, " bytes.");
        }
    }

    inline bool send_hello(int socket_fd, size_t bytes_to_send, sockaddr_in* adress){
        
    }

    class hello_message_handler : public MessageHandler {
    public:
        bool handle(Node& node, const sockaddr_in* client_address, size_t read_bytes, char* buffer) override {
            if(read_bytes > 0){
                logging::connection::logDebug(client_address, "Error: Received message too long.");
                return false;
            }

            // Handle hello message

            char const *client_ip = inet_ntoa(client_address->sin_addr);
            uint16_t client_port = ntohs(client_address->sin_port);
            return true;
        }
    };
}



#endif
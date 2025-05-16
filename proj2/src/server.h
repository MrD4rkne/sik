#ifndef SERVER_H
#define SERVER_H

#include "fd.h"
#include "ip.h"
#include "network.h"

#include <unordered_map>

namespace server {

class Server {
  public:
    Server() : logger() {
    }

    void run(int socket_fd) {
        auto on_connect = [&](const ip::IPAddress ip) { add_client(ip); };
        auto on_disconnect = [&](const ip::IPAddress ip) { forget(ip); };
        auto on_message = [](const ip::IPAddress ip,
                             const std::string message) {};

        fd::FDPoller poller;
        auto ptr = std::make_shared<network::SocketHandler>(
            socket_fd, poller, on_message, on_connect, on_disconnect);
        poller.add_socket(socket_fd, ptr);

        while (true) {
            int result = poller.poll_sockets();
            if (result < 0) {
                // TODO: handle error
                logger.log_error("Poll error: " + std::string(strerror(errno)));
                break;
            }

            poller.handle();
        }
    }

    void add_client(const ip::IPAddress ip_address) {
        // Add client to the server
        logger.log_debug("New player: ", ip_address);

        players[ip_address] = {};
    }

    void forget(const ip::IPAddress ip_address) {
        // Remove client from the server
        logger.log_debug("Forgetting player: ", ip_address);
        players.erase(ip_address);
    }

  private:
    struct player {};

    logging::Logger logger;
    std::shared_ptr<network::MessageSender> message_sender;
    std::unordered_map<ip::IPAddress, player> players;
};

} // namespace server

#endif
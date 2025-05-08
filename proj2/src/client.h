#ifndef CLIENT_H
#define CLIENT_H

#include <memory>
#include "ip.h"
#include "network.h"
#include "logging.h"

namespace client{

class strategy{
    // Define the strategy class here
};

class client{
    public:
    client(const std::string& player_id, const ip::IPAddress& server_address,
           const strategy& strat, network::MessageSender& message_sender,
            logging::Logger& logger)
        : player_id(player_id), strat(std::make_unique<strategy>(strat)), ip_address(server_address),
        message_sender(message_sender), logger(logger) {}

    void run();

    private:
    std::string player_id;
    std::unique_ptr<strategy> strat;
    ip::IPAddress ip_address;
    network::MessageSender& message_sender;
    logging::Logger& logger;
};

} // namespace client

#endif
#ifndef SERVER_RUNNER_H
#define SERVER_RUNNER_H

#include "file.h"
#include "handlers.h"
#include "ip.h"
#include "logging.h"
#include "network.h"
#include "server.h"
#include "types.h"
#include <cstdint>
#include <memory>
#include <string>

namespace server_runner {

typedef struct server_args {
    ip::port_t port_number;
    uint16_t k;
    uint8_t n;
    uint32_t m;
    std::string file_name;
} server_args_t;

/// @brief A manager for handling player connections and messages in the server.
class PlayersManager : public messages::MessageSender {
  public:
    virtual void disconnect(const ip::IPAddress player_ip) = 0;

    virtual ~PlayersManager() = default;
};

/// @brief A manager for handling player connections and messages in the server.
class Manager : public PlayersManager {
  public:
    Manager(std::shared_ptr<network::SocketHandler> message_sender)
        : message_sender(message_sender) {
    }

    void send_message(const ip::IPAddress ip, const std::string& message,
                      const std::function<void(const std::string&)>& callback,
                      uint64_t delay) override {
        message_sender->send_message(ip, message, callback, delay);
    }

    void disconnect(const ip::IPAddress ip) override {
        message_sender->disconnect(ip);
    }

    void force_flush(const ip::IPAddress ip) {
        message_sender->force_flush(ip);
    }

  private:
    std::shared_ptr<network::SocketHandler> message_sender;
};

typedef struct game {
    std::shared_ptr<server::Server> server;
    std::shared_ptr<fd::FDPoller> poller;
    std::shared_ptr<Manager> player;
} game_t;

/// @brief A class that runs the server and manages the game state.
class runner {
  public:
    runner(int socket_fd, const server_args_t& args,
           handlers::message_handler<server::Server>& msg_handler,
           std::shared_ptr<file::COEFFFromFileProvider> coeff_provider);

    void run();

  private:
    std::shared_ptr<game_t> game;
    logging::Logger logger;
};

/// @brief Handle a message from a player.
results::Result handler_hello(const ip::IPAddress sender,
                              messages::MessageSender&, server::Server& state,
                              logging::Logger& logger,
                              const std::string& message);

/// @brief Handle a PUT message from a player.
results::Result handler_put(const ip::IPAddress sender,
                            messages::MessageSender& msg_sender,
                            server::Server& state, logging::Logger& logger,
                            const std::string& message);
} // namespace server_runner

#endif
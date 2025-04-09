#ifndef HANDLERS_H
#define HANDLERS_H

#include "domain.h"
#include "logging.h"
#include "messaging.h"
#include "packets.h"

namespace handlers {

using namespace domain;
using namespace packets;
using namespace logging;
using namespace messaging;

class hello_message_handler : public MessageHandler {
  public:
    bool handle(Node& node, logging::Logger& logger, const domain::peer& peer,
                size_t read_bytes, char* buffer,
                MessageSender& message_sender) override;
};

class hello_response_handler : public MessageHandler {
  public:
    bool handle(Node& node, logging::Logger& logger, const domain::peer& peer,
                size_t read_bytes, char* buffer,
                MessageSender& message_sender) override;
};

class connect_handler : public MessageHandler {
  public:
    bool handle(Node& node, logging::Logger& logger, const domain::peer& peer,
                size_t read_bytes, char* buffer,
                MessageSender& message_sender) override;
};

class ack_connect_handler : public MessageHandler {
  public:
    bool handle(Node& node, logging::Logger& logger, const domain::peer& peer,
                size_t read_bytes, char* buffer,
                MessageSender& /*message_sender*/) override;
};

} // namespace handlers

#endif
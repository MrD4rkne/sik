#include "client.h"
#include "messages.h"

namespace client{

void client::run() {
    logger.log_debug("Client started with player ID: " + player_id);

    messages::hello_message_t hello_message{
        .player_id = player_id
    };
    message_sender.send_message(hello_message);


}

} // namespace client
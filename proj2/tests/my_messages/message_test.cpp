#include <iostream>
#include <sstream>
#include <cassert>

#include "messages.h"

int main(){

    // Test the hello_message_t struct
    messages::hello_message_t hello_msg;
    hello_msg.player_id = "player1";
    std::string serialized_hello_msg = messages::serialize_message(hello_msg);
    assert(serialized_hello_msg == "HELLO player1 \r\n");

    messages::hello_message_t deserialized_hello_msg = messages::deserialize_message<messages::hello_message_t>(serialized_hello_msg);
    assert(deserialized_hello_msg.player_id == hello_msg.player_id);

    return 0;
}
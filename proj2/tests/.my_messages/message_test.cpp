#include <iostream>
#include <sstream>
#include <cassert>

#include "messages.h"

void test_hello_message() {
    messages::hello_message_t hello_msg;
    hello_msg.player_id = "player1";

    // Serialize the message
    std::string serialized_msg = messages::serialize_message(hello_msg);
    assert(serialized_msg == "HELLO player1\r\n");

    // Deserialize the message
    messages::hello_message_t deserialized_msg = messages::deserialize_message<messages::hello_message_t>(serialized_msg);
    assert(deserialized_msg.player_id == hello_msg.player_id);
}

int main(){
    test_hello_message();
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
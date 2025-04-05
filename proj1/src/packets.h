#ifndef PACKETS_H
#define PACKETS_H

#include <cstdint>

namespace packets {
    using message_type_t = uint8_t;
    
    const inline uint8_t MSG_TYPE_HELLO = 0x01;
    const inline uint8_t MSG_TYPE_HELLO_RSP = 0x02;
}

#endif
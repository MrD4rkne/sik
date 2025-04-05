#ifndef PACKETS_H
#define PACKETS_H

#include <cstdint>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace packets {
    const inline uint8_t MSG_TYPE_HELLO = 0x01;
    const inline uint8_t MSG_TYPE_HELLO_RSP = 0x02;
}

#endif
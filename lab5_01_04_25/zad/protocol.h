#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

typedef struct __attribute__((__packed__)) {
    uint32_t file_length;
    uint16_t name_length;
} data_header;

#endif

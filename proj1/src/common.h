#ifndef MIM_COMMON_H
#define MIM_COMMON_H

#include <cstdint>
#include <netinet/in.h>

uint16_t read_port(const std::string& string);

size_t read_size(const std::string& string);

sockaddr_in get_server_address(const std::string& host, uint16_t port);

#endif

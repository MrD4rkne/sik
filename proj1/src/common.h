#ifndef MIM_COMMON_H
#define MIM_COMMON_H

#include <cstdint>
#include <netinet/in.h>
#include <string>

/// @brief Read a port number from a string.
/// @param string The string to read the port number from.
/// @return Parsed port
/// @throws std::invalid_argument if the string is not a valid port number.
uint16_t read_port(const std::string& string);

/// @brief Read a size from a string.
/// @param string The string to read the size from.
/// @return Parsed size
/// @throws std::invalid_argument if the string is not a valid size.
size_t read_size(const std::string& string);

/// @brief Get the ipv4 server address from a dotted host and port.
/// @param host The host name or IP address.
/// @param port The port number.
/// @return The server address.
/// @throws std::runtime_error if the address cannot be resolved.
sockaddr_in get_server_address(const std::string& host, uint16_t port);

/// @brief Get the ipv4 server address from a host and port.
/// @param host The host name or IP address.
/// @param port The port number.
/// @return The server address.
/// @throws std::runtime_error if the address cannot be resolved.
sockaddr_in get_peer_address(const std::string& host, uint16_t port);

#endif

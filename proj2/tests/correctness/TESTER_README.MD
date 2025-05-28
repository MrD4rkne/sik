# Network Testing Tool Documentation

This document describes the usage of `tester.py`, a Python script designed to test network communication protocols with a focus on TCP socket communication.

## Overview

The `tester.py` script provides a command-line interface for testing TCP client-server communications. It allows you to create sockets, establish connections, send/receive messages, and validate message formats and responses.

## Running the Tester

```bash
python3 tester.py < test_script.txt
```

Or interactively:

```bash
python3 tester.py
# Now enter commands manually
```

## Commands Reference

### Host and Socket Setup

#### Define a Host
```
host <hostname> <address> <port>
```
Registers a hostname with an IP address and port.

Example:
```
host server 127.0.0.1 10000
```

#### Create a Socket
```
socket <sockname> <port>
```
Creates a socket bound to the specified local port.

Example:
```
socket client_socket 20000
```

### Connection Management

#### Set Socket to Listen Mode
```
listen <sockname>
```
Sets the socket to listen mode without accepting connections.

Example:
```
listen server_socket
```

#### Accept a Connection
```
accept <sockname> <hostname> <timeout> <id>
```
Accepts a connection on a listening socket with the specified timeout.

Parameters:
- `sockname`: The socket to accept connections on
- `hostname`: The hostname to assign to the accepted connection
- `timeout`: Timeout in seconds (or 'None' for no timeout)
- `id`: Server's ID (use "UNKNOWN" when simulating a server)

Example:
```
accept server_socket client1 5.0 UNKNOWN
```

#### Connect to a Host
```
connect <sockname> <hostname> <id>
```
Connects a socket to a host.

Parameters:
- `sockname`: The socket to use for the connection
- `hostname`: The host to connect to
- `id`: Server's ID (use "UNKNOWN" when simulating a server)

Example:
```
connect client_socket server UNKNOWN
```

#### Close a Connection
```
close <sockname> <hostname>
```
Closes a connection.

Example:
```
close client_socket server
```

### Data Exchange

#### Send a Message
```
send <sockname> <hostname> <message>
```
Sends a message to the specified host.

Example:
```
send client_socket server GET DATA\r\n
```

#### Send Invalid Message
```
send_invalid <sockname> <hostname> <message>
```
Sends an invalid message and prints an error message in the required format.
This will trigger an "ERROR: bad message from [host]:port, id: message" output.

Example:
```
send_invalid client_socket server INVALID\r\n
```

#### Receive Data
```
receive <sockname> <hostname> <timeout> <format>
```
Waits to receive data from the specified host with timeout and format validation.

Parameters:
- `sockname`: Socket to receive on
- `hostname`: Host to receive from
- `timeout`: Time to wait in seconds (or 'None' for no timeout)
- `format`: Format string for validation

Example:
```
receive server_socket client 5.0 "OK \f"
```

Note that all messages should end with \r\n, it is checked before comparing format.

### Connection Verification

#### Expect Connection Close
```
expect_close <sockname> <hostname> <timeout>
```
Expects the connection to be closed by the peer within the timeout period.

Example:
```
expect_close client_socket server 5.0
```

#### Expect Unsuccessful Connect
```
expect_unsuccessful_connect <sockname> <hostname>
```
Expects that a connection attempt will fail.

Example:
```
expect_unsuccessful_connect client_socket server
```

### Other Commands

#### Sleep
```
sleep <seconds>
```
Pauses execution for the specified number of seconds.

Example:
```
sleep 1.5
```

## Format Specification

When receiving data, you can validate it against an expected format:

- Raw strings: Match exactly as specified
- `\f`: Float with 7 decimal precision and '.' as separator
- `\f[bottom;top]`: Float within the specified range, also with 7 decimal precision and '.' as separator

Messages must end with `\r\n` (CRLF) to be considered valid. The format validation is performed after stripping the CRLF.

### Format Examples

- `"OK \f"` - Expects string "OK " followed by a floating-point number
- `"STATUS \f[0;100]"` - Expects "STATUS " followed by a float between 0 and 100
- `"COMMAND EXECUTE"` - Expects the exact string "COMMAND EXECUTE"

## Complete Example

```
# Define hosts
host server 127.0.0.1 8000
host client 127.0.0.1 8001

# Create sockets
socket server_socket 8000
socket client_socket 8001

# Start listening on server
listen server_socket

# Connect client to server
connect client_socket server UNKNOWN

# Accept the connection on server
accept server_socket client 5.0 UNKNOWN

# Client sends a command to server
send client_socket server GET DATA\r\n

# Server responds with data
receive client_socket server 5.0 "DATA \f[0;100]"

# Send an invalid message and check error handling
send_invalid client_socket server BAD REQUEST\r\n

# Wait for server to close the connection
expect_close client_socket server 5.0

# Sleep between operations
sleep 1

# Close connections
close server_socket client
```

## Note on CRLF Line Endings

All received messages are expected to end with `\r\n` (CRLF). Messages without this ending are rejected as invalid.

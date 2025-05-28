from time import sleep
import socket
import re
import sys

def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)

class interpreter:
    sockets: dict
    hosts: dict
    connections: dict
    BUFFER_SIZE: int

    def __init__(self, BUFFER_SIZE: int = 70_000):
        self.sockets = dict()
        self.hosts = dict()
        self.connections = dict()
        self.BUFFER_SIZE = BUFFER_SIZE
        # Flag to control the listener threads
        self.running = True

    @staticmethod
    # https://stackoverflow.com/a/21605790
    def __str_to_raw__(s):
        raw_map = {8:r'\b', 7:r'\a', 12:r'\f', 10:r'\n', 13:r'\r', 9:r'\t', 11:r'\v'}
        return r''.join(i if ord(i) > 32 else raw_map.get(ord(i), i) for i in s)
        
    @staticmethod
    def __process_escapes__(s):
        """Process escape sequences in strings, particularly \r\n."""
        # Replace common escape sequences
        s = s.replace('\\r', '\r')
        s = s.replace('\\n', '\n')
        s = s.replace('\\t', '\t')
        s = s.replace('\\b', '\b')
        s = s.replace('\\f', '\f')
        return s

    def handle_host(self, hostname: str, address: str, port: str):
        if hostname in self.hosts:
            raise ValueError(f"Host {hostname} already exists")
        
        if port != "*" and not port.isdigit():
            raise ValueError(f"Port {port} is not a valid number or '*'")
        
        self.hosts[hostname] = (address, port)

    def handle_socket(self, sockname: str, port: int):
        if sockname in self.sockets:
            raise ValueError(f"Socket {sockname} already exists")
        
        # Listen on both IPv4 and IPv6 by creating an IPv6 socket with dual-stack support
        family = socket.AF_INET6
        addr = ('::', port)
        self.sockets[sockname] = socket.socket(family, socket.SOCK_STREAM)
        self.sockets[sockname].setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sockets[sockname].bind(addr)

    def handle_connect(self, sockname: str, hostname: str, id: str):
        host_addr, host_port = self.hosts[hostname]
        if host_port == "*":
            raise ValueError(f"Host {hostname} has no port specified")
        
        host_port = int(host_port)
        
        # Determine if IPv4 or IPv6
        try:
            # Check if it's an IPv4 address
            socket.inet_pton(socket.AF_INET, host_addr)
            addr_family = socket.AF_INET
        except socket.error:
            try:
                # Check if it's an IPv6 address
                socket.inet_pton(socket.AF_INET6, host_addr)
                addr_family = socket.AF_INET6
            except socket.error:
                # It's a hostname, resolve it
                addr_info = socket.getaddrinfo(host_addr, host_port, socket.AF_UNSPEC, socket.SOCK_STREAM)
                addr_family = addr_info[0][0]
                host_addr = addr_info[0][4][0]

        # Create a new socket for connection
        client_socket = socket.socket(addr_family, socket.SOCK_STREAM)
        client_socket.connect((host_addr, host_port))
        
        # Store the connection
        if sockname not in self.connections:
            self.connections[sockname] = {}
        self.connections[sockname][hostname] = {client_socket, id}
        
        print(f"Connected {sockname} to {hostname}")

    def handle_listen(self, sockname: str):
        # Just set the socket to listen mode without spawning a thread
        if sockname not in self.sockets:
            raise ValueError(f"Socket {sockname} does not exist")
        
        sock = self.sockets[sockname]
        sock.listen(5)
        print(f"Started listening on {sockname}")
    
    def handle_accept(self, sockname: str, hostname: str, timeout: float, my_name: str):
        if sockname not in self.sockets:
            raise ValueError(f"Socket {sockname} does not exist")
        if hostname not in self.hosts:
            print(f"ERROR: Host '{hostname}' not defined in hosts. Current hosts: {list(self.hosts.keys())}")
            raise ValueError(f"Host '{hostname}' not defined in hosts.")
        sock = self.sockets[sockname]
        sock.settimeout(timeout)
        client, addr = sock.accept()
        print(f"Accepted connection from {addr} on {sockname}, assigned name: {my_name}")
        host, port = self.hosts[hostname]
        client_address, client_port = addr[0], addr[1]
        if port == "*":
            port = client_port
            self.hosts[hostname] = (host, port)
        if client_address != host or int(client_port) != int(port):
            raise ValueError(f"Accepted connection from {addr} does not match expected [{host}]:{port}")
        if sockname not in self.connections:
            self.connections[sockname] = {}
        self.connections[sockname][hostname] = (client, my_name)

    def log_invalid_packet(self, host, port, msg, id):
        # Remove trailing \r\n
        ENDING = '\\r\\n'
        if msg.endswith(ENDING):
            msg = msg[:-len(ENDING)]
        print(f"ERROR: bad message from [{host}]:{port}, {id}: {msg}")
        eprint(f"ERROR: bad message from [{host}]:{port}, {id}: {msg}")

    def handle_send(self, sockname: str, hostname: str, message: str, is_invalid: bool = False):
        if sockname not in self.connections or hostname not in self.connections[sockname]:
            raise ValueError(f"No connection between {sockname} and {hostname}")     
        
        client_socket, id = self.connections[sockname][hostname]
        # Process escape sequences like \r\n first
        processed_message = self.__process_escapes__(message)
        # Then convert to raw for display purposes
        raw_message = self.__str_to_raw__(processed_message)
        
        client_socket.sendall(processed_message.encode())
        print(f"Sent message from {sockname} to {hostname}: {raw_message}")

        if is_invalid:
            # Log the invalid packet
            TESTER_IP="00:00:00:00:00:00:00:00"
            port = self.sockets[sockname].getsockname()[1]
            self.log_invalid_packet(TESTER_IP, port, raw_message, id)
    
    FLOAT_REGEX = re.compile(r'^[-+]?[0-9]*\.?[0-9]{0,7}$')

    def parse_received_data(self, data, format_str):
        """Parse received data according to the format string.
        Format can be raw strings separated by spaces or \f for float with 7 decimal precision.
        \f[bottom;top] checks if float is within specified range."""
            
        data_str = data.decode()
        parts = data_str.split(' ')
        result = []
        
        # Split format into parts
        format_parts = format_str.split(' ')
        
        for i, fmt in enumerate(format_parts):
            if i >= len(parts):
                break
            
            if fmt.startswith(r'\f'):
                if not self.FLOAT_REGEX.match(parts[i]):
                    raise ValueError(f"Invalid float format: {parts[i]}")
                
                # Parse as float with 7 decimal places
                try:
                    value = float(parts[i])
                    result.append(value)
                except ValueError:
                    raise ValueError(f"Invalid float value: {parts[i]}")
            else:
                # Raw string comparison
                result.append(parts[i])
                
        return result

    def verify_format_match(self, parsed_data, expected_format):
        """Verify if parsed data matches the expected format."""
        if not isinstance(parsed_data, list):
            raise ValueError("Parsed data is not a list")
        
        if not isinstance(expected_format, str):
            raise ValueError("Expected format is not a string")
            
        format_parts = expected_format.split(' ')
        
        if len(parsed_data) != len(format_parts):
            raise ValueError(f"Data length {len(parsed_data)} does not match format length {len(format_parts)}")
        
        for i, (data_part, fmt_part) in enumerate(zip(parsed_data, format_parts)):
            if fmt_part.startswith(r'\f'):
                if not isinstance(data_part, float):
                    raise ValueError(f"Expected float at position {i}, got {type(data_part)}")
                
                # Check if it's a float range specification
                if '[' in fmt_part and ']' in fmt_part:
                    range_match = re.match(r'\\f\[([\d\.+-]+);([\d\.+-]+)\]', fmt_part)
                    if range_match:
                        bottom = float(range_match.group(1))
                        top = float(range_match.group(2))
                        
                        if not (bottom <= data_part <= top):
                            raise ValueError(f"Float value {data_part} out of range [{bottom}, {top}]")
            elif data_part != fmt_part:
                raise ValueError(f"Data mismatch at position {i}: got {data_part}, expected {fmt_part}")

    def validate_crlf_ending(self, message):
        """Validates that the message ends with CRLF (\r\n).
        Returns a tuple (is_valid, stripped_message)"""
        if message.endswith('\r\n'):
            return True, message[:-2]  # Strip the \r\n
        else:
            return False, message

    def handle_receive(self, sockname: str, hostname: str, timeout, format: str):
        if sockname not in self.connections or hostname not in self.connections[sockname]:
            print(f"No connection between {sockname} and {hostname}")
            return
        
        client_socket, id = self.connections[sockname][hostname]
        client_socket.settimeout(timeout)
        
        try:
            data = client_socket.recv(self.BUFFER_SIZE)
            if not data:
                raise ValueError(f"Connection closed by {hostname}")
            
            decoded_data = data.decode()
            print(f"Received data from {hostname}: '{decoded_data}'")

            is_valid, stripped_data = self.validate_crlf_ending(decoded_data)
            if not is_valid:
                raise ValueError("Received data does not end with CRLF")
            
            # Use the stripped data (without CRLF) for format matching
            parsed_data = self.parse_received_data(stripped_data.encode(), format)
            self.verify_format_match(parsed_data, format)
            
        except socket.timeout:
            raise ValueError(f"Timeout while receiving data from {hostname}")
        
    def expect_close(self, sockname: str, hostname: str, timeout: float):
        if sockname not in self.connections or hostname not in self.connections[sockname]:
            raise ValueError(f"No connection between {sockname} and {hostname}")

        client_socket, id = self.connections[sockname][hostname]
        client_socket.settimeout(timeout)
        
        try:
            data = client_socket.recv(self.BUFFER_SIZE)
            if data:
                raise ValueError(f"Expected disconnection, but received data: {data.decode()}")
        except socket.timeout:
            raise ValueError(f"Timeout while expecting disconnection from {hostname}")
        except Exception as e:
            raise ValueError(f"Error while expecting disconnection: {e}")
        
    def close_connection(self, sockname: str, hostname: str):
        if sockname not in self.connections or hostname not in self.connections[sockname]:
            raise ValueError(f"No connection between {sockname} and {hostname}")
        
        client_socket, id = self.connections[sockname][hostname]
        
        try:
            client_socket.close()
            del self.connections[sockname][hostname]
            print(f"Closed connection from {sockname} to {hostname}")
        except Exception as e:
            raise ValueError(f"Error while closing connection: {e}")
        
    def expect_unsuccessful_connect(self, sockname: str, hostname: str):
        if sockname not in self.sockets or hostname not in self.hosts:
            raise ValueError(f"No socket {sockname} or host {hostname}")
        
        sock = self.sockets[sockname]
        host_addr, host_port, host_id = self.hosts[hostname]
        
        try:
            sock.connect((host_addr, host_port))
            raise ValueError(f"Expected unsuccessful connection to {hostname}, but connected successfully")
        except socket.error:
            print(f"Unsuccessful connection to {hostname} as expected")
        except Exception as e:
            raise ValueError(f"Unexpected error while connecting: {e}")

    def handle_sleep(self, time_seconds: float):
        sleep(time_seconds)

    def parse_line(self, line: str):
        if line.startswith('#') or not line.strip():
            return
        
        print("$ " + line)
        
        elements = line.split()
        cmd = elements[0].lower()
        
        if cmd == 'host':
            self.handle_host(elements[1], elements[2], elements[3])
        elif cmd == 'socket':
            self.handle_socket(elements[1], int(elements[2]))
        elif cmd == 'connect':
            self.handle_connect(elements[1], elements[2], elements[3])
        elif cmd == 'listen':
            self.handle_listen(elements[1])
        elif cmd == 'accept':
            timeout = None if elements[3] == 'None' else float(elements[3])
            self.handle_accept(elements[1], elements[2], timeout, elements[4])
        elif cmd == "expect_close":
            timeout = None if elements[3] == 'None' else float(elements[3])
            self.expect_close(elements[1], elements[2], timeout)
        elif cmd == 'close':
            self.close_connection(elements[1], elements[2])
        elif cmd == 'expect_unsuccessful_connect':
            self.expect_unsuccessful_connect(elements[1], elements[2])
        elif cmd == 'send':
            # Take the message as the raw substring after the third space (preserve spaces)
            message_start = line.find(elements[2]) + len(elements[2]) + 1
            message = line[message_start:]
            self.handle_send(elements[1], elements[2], message)
        elif cmd == 'send_invalid':
            message_start = line.find(elements[2]) + len(elements[2]) + 1
            message = line[message_start:]
            self.handle_send(elements[1], elements[2], message, is_invalid=True)
        elif cmd == 'receive':
            timeout = None if elements[3] == 'None' else float(elements[3])
            format_str = elements[4]
            self.handle_receive(elements[1], elements[2], timeout, format_str)
        elif cmd == 'sleep':
            self.handle_sleep(float(elements[1]))
        else:
            raise ValueError(f"Unknown command: {cmd}")

    def close(self):
        self.running = False
        
        # Close all connections
        for sock_conns in self.connections.values():
            for conn in sock_conns.values():
                try:
                    conn.close()
                except:
                    pass
        
        # Close all sockets
        for sock in self.sockets.values():
            try:
                sock.close()
            except:
                pass


interpret = interpreter()

while True:
    try:
        interpret.parse_line(input())
    except EOFError:
        interpret.close()
        break
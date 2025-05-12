from struct import pack, unpack, calcsize
from time import sleep
import socket
import select
import threading
import re

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

    def handle_host(self, hostname: str, address: str, port: int):
        self.hosts[hostname] = (address, port)

    def handle_socket(self, sockname: str, port: int):
        # Determine if we need IPv4 or IPv6
        family = socket.AF_INET
        addr = ('127.0.0.1', port)
        self.sockets[sockname] = socket.socket(family, socket.SOCK_STREAM)
        self.sockets[sockname].setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sockets[sockname].bind(addr)

    def handle_connect(self, sockname: str, hostname: str):
        host_addr, host_port = self.hosts[hostname]
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
        self.connections[sockname][hostname] = client_socket
        
        print(f"Connected {sockname} to {hostname}")

    def listen_handler(self, sockname):
        sock = self.sockets[sockname]
        sock.listen(5)
        
        while self.running:
            try:
                client, addr = sock.accept()
                print(f"New client connected to {sockname}: {addr}")
                
                # Find a hostname for this client or create one
                hostname = None
                for h_name, h_addr in self.hosts.items():
                    if h_addr[0] == addr[0] and h_addr[1] == addr[1]:
                        hostname = h_name
                        break
                
                if hostname is None:
                    hostname = f"client_{addr[0]}_{addr[1]}"
                    self.hosts[hostname] = (addr[0], addr[1])
                
                if sockname not in self.connections:
                    self.connections[sockname] = {}
                self.connections[sockname][hostname] = client
                
            except Exception as e:
                if not self.running:
                    break
                print(f"Error in listener thread: {e}")

    def handle_listen(self, sockname: str):
        # Start a thread that will accept connections
        listen_thread = threading.Thread(target=self.listen_handler, args=(sockname,))
        listen_thread.daemon = True
        listen_thread.start()
        print(f"Started listening on {sockname}")

    def log_invalid_packet(self, data: bytes, limit: int = 10):
        prefix = data[:min(len(data), limit)]
        prefix_str = ''.join([hex(x).removeprefix('0x').rjust(2, '0') for x in prefix])
        print(f"ERROR MSG {prefix_str}")

    def handle_send(self, sockname: str, hostname: str, message: str):
        if sockname not in self.connections or hostname not in self.connections[sockname]:
            # If not connected, establish connection first
            self.handle_connect(sockname, hostname)
            
        client_socket = self.connections[sockname][hostname]
        message = self.__str_to_raw__(message)

        # Then handle single escapes
        message = message.replace('\\r', '\r').replace('\\n', '\n')
        client_socket.sendall(message.encode())
        print(f"Sent message from {sockname} to {hostname}: {message}")

    def match_datapoint_to_key(self, datapoint, key: str):
        if key.startswith('['): # list
            subkeys = key.removeprefix('[').removesuffix(']').split(';')

            for subkey in subkeys:
                if self.match_datapoint_to_key(datapoint, subkey):
                    return True
        
            return False
        elif key.startswith('('):
            key = key.removeprefix('(').removesuffix(')').split(';')
            lower_bound, upper_bound = [int(x) for x in key]
            return lower_bound <= datapoint <= upper_bound
        elif key == '*':
            return True
        else:
            return datapoint == int(key)

    def verify_data_correctness(self, data, data_keys):
        for datapoint, key in zip(data, data_keys):
            if not self.match_datapoint_to_key(datapoint, key):
                return False
            
        return True

    def handle_receive(self, sockname: str, hostname: str, timeout, format: str = None, data_keys: str = None, is_invalid: bool = False):
        if hostname == 'None' or hostname == '*':
            # Receive from any connected client
            if sockname not in self.connections or not self.connections[sockname]:
                print(f"No connections available for {sockname}")
                return
            
            # Use select to check for available data with timeout
            ready_sockets = []
            for host, conn in self.connections[sockname].items():
                ready_sockets.extend(select.select([conn], [], [], timeout or None)[0])
            
            if not ready_sockets:
                print("Timeout reached, no data available")
                return
            
            client_socket = ready_sockets[0]
            # Find hostname for this socket
            found_hostname = None
            for h_name, conn in self.connections[sockname].items():
                if conn == client_socket:
                    found_hostname = h_name
                    break
            
            data = client_socket.recv(self.BUFFER_SIZE)
            if not data:
                print(f"Connection closed by {found_hostname}")
                self.connections[sockname].pop(found_hostname, None)
                return
            
            print(f"Received data from {found_hostname}: {data.decode()}")
            
        else:
            # Receive from specific host
            if sockname not in self.connections or hostname not in self.connections[sockname]:
                print(f"No connection between {sockname} and {hostname}")
                return
            
            client_socket = self.connections[sockname][hostname]
            client_socket.settimeout(timeout)
            
            try:
                data = client_socket.recv(self.BUFFER_SIZE)
                if not data:
                    print(f"Connection closed by {hostname}")
                    self.connections[sockname].pop(hostname, None)
                    return
                
                print(f"Received data from {hostname}: {data.decode()}")
                
            except socket.timeout:
                print(f"Timeout reached while waiting for data from {hostname}")

    def handle_sleep(self, time_seconds: float):
        sleep(time_seconds)

    def parse_line(self, line: str):
        if line.startswith('#') or not line.strip():
            return
        
        print("$ " + line)
        
        elements = line.split()
        cmd = elements[0].lower()
        
        try:
            if cmd == 'host':
                self.handle_host(elements[1], elements[2], int(elements[3]))
            elif cmd == 'socket':
                self.handle_socket(elements[1], int(elements[2]))
            elif cmd == 'connect':
                self.handle_connect(elements[1], elements[2])
            elif cmd == 'listen':
                self.handle_listen(elements[1])
            elif cmd == 'send':
                # Assume message can have spaces
                message = ' '.join(elements[3:])
                self.handle_send(elements[1], elements[2], message)
            elif cmd == 'receive':
                timeout = None if elements[3] == 'None' else float(elements[3])
                if len(elements) > 4:
                    format_str = '>' + elements[4]
                    data_keys = elements[5].split(',')
                    is_invalid = (elements[-1] == '!')
                    self.handle_receive(elements[1], elements[2], timeout, format_str, data_keys, is_invalid)
                else:
                    self.handle_receive(elements[1], elements[2], timeout)
            elif cmd == 'sleep':
                self.handle_sleep(float(elements[1]))
            else:
                print(f"Unknown command: {cmd}")
        except Exception as e:
            print(f"Error executing command: {e}")
    
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
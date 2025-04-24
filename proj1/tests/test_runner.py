from struct import pack, unpack, calcsize
from time import sleep

import socket

class interpreter:
    sockets: dict
    hosts: dict
    BUFFER_SIZE: int

    def __init__(self, BUFFER_SIZE: int = 70_000):
        self.sockets = dict()
        self.hosts = dict()
        self.BUFFER_SIZE = BUFFER_SIZE

    def handle_host(self, hostname: str, address: str, port: int):
        self.hosts[hostname] = (address, port)

    def handle_socket(self, sockname: str, port: int):
        self.sockets[sockname] = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sockets[sockname].bind(('127.0.0.1', port))

    def log_invalid_packet(self, data: bytes, limit: int = 10):
        prefix = data[:min(len(data), limit)]
        prefix_str = ''.join([hex(x).removeprefix('0x').rjust(2, '0') for x in prefix])
        print(f"ERROR MSG {prefix_str}")

    def handle_send(self, sockname: str, hostname: str, format: str, data: str, is_invalid: bool):
        packet = pack(format, *data)
        self.sockets[sockname].sendto(packet, self.hosts[hostname])

        if is_invalid:
            self.log_invalid_packet(packet)

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

    def handle_receive(self, sockname: str, hostname: str, timeout, format: str, data_keys: str, is_invalid: bool):
        self.sockets[sockname].settimeout(timeout)
        packet, addr = self.sockets[sockname].recvfrom(self.BUFFER_SIZE)

        if (hostname != 'None' and addr != self.hosts[hostname]):
            raise ValueError(f"Invalid host: {addr} != {self.hosts[hostname]}")

        size = calcsize(format)
        print(format, size, len(packet))
        print("$ format: " + format, "size: " + str(size), "len: " + str(len(packet)))
        data = unpack(format, packet)

        print(data)

        if not self.verify_data_correctness(data, data_keys):
            raise ValueError("Invalid data received")
        if is_invalid:
            self.log_invalid_packet(packet)

    def handle_sleep(self, time_seconds: float):
        sleep(time_seconds)

    def parse_line(self, line: str):
        if line.startswith('#') or len(line) == 0:
            return
        
        print("$ " + line)
        
        elements = line.split()
        match elements[0]:
            case 'host':
                self.handle_host(elements[1], elements[2], int(elements[3]))

            case 'socket':
                self.handle_socket(elements[1], int(elements[2]))

            case 'send':
                self.handle_send(sockname=elements[1], 
                                 hostname=elements[2], 
                                 format=('>' + elements[3]), 
                                 data=[int(x) for x in elements[4].split(',')], 
                                 is_invalid=(elements[-1] == '!'))

            case 'receive':
                self.handle_receive(sockname=elements[1],
                                    hostname=elements[2],
                                    timeout=None if elements[3] == 'None' else float(elements[3]),
                                    format=('>' + elements[4]),
                                    data_keys=elements[5].split(','),
                                    is_invalid=(elements[-1] == '!'))

            case 'sleep':
                self.handle_sleep(float(elements[1]))
    
    def close(self):
        for socket in self.sockets.values():
            socket.close()


interpret = interpreter()

while True:
    try:
        interpret.parse_line(input())
    except EOFError:
        interpret.close()
        break

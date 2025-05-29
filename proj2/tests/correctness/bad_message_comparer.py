import sys
import re
import ipaddress

# This script processes a log of "ERROR: bad message from [IP]:PORT" lines,
# unifying IPv6 addresses to their canonical form and leaving IPv4 addresses unchanged.

def unify_ipv6(ip):
    try:
        return ipaddress.IPv6Address(ip).exploded
    except Exception:
        return ip  # Not a valid IPv6, return as is

def process_file(input_filename, output_file):
    with open(input_filename, 'r') as f:
        for line in f:
            match = re.match(r'(ERROR: bad message from \[)([^\]]+)\]:(\d+)(.*)', line)
            if match:
                prefix, ip, port, rest = match.groups()
                try:
                    ip_obj = ipaddress.ip_address(ip)
                    if isinstance(ip_obj, ipaddress.IPv6Address):
                        ip = unify_ipv6(ip)
                    # IPv4: leave as is
                except ValueError:
                    pass  # Not an IP, leave as is
                output_file.write(f"{prefix}{ip}]:{port}{rest}")
            else:
                output_file.write(line)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} input_file output_file")
        sys.exit(1)
    input_fname = sys.argv[1]
    output_fname = sys.argv[2]
    with open(output_fname, 'w') as out_f:
        process_file(input_fname, out_f)
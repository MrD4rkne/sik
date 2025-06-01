#!/bin/bash

# Define colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

if [ "$#" -ne 1 ]; then
    echo -e "${RED}Usage: $0 <code_directory>${NC}"
    exit 1
fi

CLIENT_EXECUTABLE_NAME="approx-client"
SERVER_EXECUTABLE_NAME="approx-server"

code_dir=$1

if ! [ -d "$code_dir" ]; then
    echo -e "${RED}Error: Directory $code_dir does not exist.${NC}"
    exit 1
fi

if ! make -C "$code_dir" ; then
    echo -e "${RED}Error: Build failed.${NC}"
    exit 1
fi

executables=("$CLIENT_EXECUTABLE_NAME" "$SERVER_EXECUTABLE_NAME")
for executable in "${executables[@]}"; do
    if ! [ -f "$code_dir/$executable" ]; then
        echo -e "${RED}Error: Executable $executable not found in $code_dir.${NC}"
        exit 1
    fi
done

run_executable() {
    local code_dir=$1
    local executable_name=$2
    local args=$3
    local output_file=$4
    local error_file=$5

    echo -e "${YELLOW}Running $code_dir/$executable_name with args: $args${NC}"
    
    "$code_dir/$executable_name" $args > "$output_file" 2> "$error_file" &
    EXECUTABLE_PID=$!
}

TEMP_DIR="temp"
if ! mkdir -p "$TEMP_DIR"; then
    echo -e "${RED}Error: Failed to create temporary directory $TEMP_DIR.${NC}"
    exit 1
fi

# Inform user
echo -e "This tests checks what happens when machine does not have IPv6 support.\n"
echo -e "We need sudo to temporarily disable IPv6 support.\n"

# Disable IPv6 support
sudo sysctl -w net.ipv6.conf.all.disable_ipv6=1
sudo sysctl -w net.ipv6.conf.default.disable_ipv6=1

# Test that ipv6 is not working
if ping6 -c 1 :: &>/dev/null; then
    echo -e "${RED}IPv6 is still enabled. Please disable it manually and try again.${NC}"
    exit 1
else
    echo -e "${GREEN}IPv6 is disabled successfully.${NC}"
fi

# Run server
SERVER_OUTPUT_FILE="$TEMP_DIR/server_output.txt"
SERVER_ERROR_FILE="$TEMP_DIR/server_error.txt"

FILE_COEFFS="file.coeffs"
COEFFS_PATH=$(realpath "$FILE_COEFFS")
echo -e "${YELLOW}Using coefficients file: $COEFFS_PATH${NC}"

PORT=8001

run_executable "$code_dir" "$SERVER_EXECUTABLE_NAME" "-p $PORT -f "$COEFFS_PATH"" "$SERVER_OUTPUT_FILE" "$SERVER_ERROR_FILE"
server_pid=$!

# Wait for server to start
sleep 2

if ! kill -0 $server_pid 2>/dev/null; then
    echo -e "${RED}Server failed to start. Check $SERVER_ERROR_FILE for details.${NC}"
    exit 1
else
    echo -e "${GREEN}Server started successfully. Output saved to $SERVER_OUTPUT_FILE.${NC}"
fi

# Run client
CLIENT_OUTPUT_FILE="$TEMP_DIR/client_output.txt"
CLIENT_ERROR_FILE="$TEMP_DIR/client_error.txt"
run_executable "$code_dir" "$CLIENT_EXECUTABLE_NAME" "-s localhost -p $PORT -a -u PLAYER123" "$CLIENT_OUTPUT_FILE" "$CLIENT_ERROR_FILE"
client_pid=$!

# Wait for client to finish
wait $client_pid
if [ $? -ne 0 ]; then
    echo -e "${RED}Client execution failed. Check $CLIENT_ERROR_FILE for details.${NC}"
else
    echo -e "${GREEN}Client executed successfully. Output saved to $CLIENT_OUTPUT_FILE.${NC}"
fi

kill $server_pid
# Wait for server to finish
wait $server_pid

# Enable
echo -e "${YELLOW}Re-enabling IPv6 support...${NC}"
sudo sysctl -w net.ipv6.conf.all.disable_ipv6=0
sudo sysctl -w net.ipv6.conf.default.disable_ipv6=0
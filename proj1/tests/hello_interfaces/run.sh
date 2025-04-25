#!/bin/bash

# Define color codes for output formatting
RED='\033[0;31m'    # Red for errors
GREEN='\033[0;32m'  # Green for success
YELLOW='\033[1;33m' # Yellow for warnings
NC='\033[0m'        # No Color

# Function to gracefully close a process and exit with specified code
function close_process() {
    local code=$1
    local pid=$2
    # Check if process is still running before trying to kill it
    if ps -p $pid > /dev/null; then
        kill $pid
        wait $pid 2>/dev/null
    fi

    exit $code
}

# Check if correct number of arguments is provided
if [ "$#" -ne 1 ]; then
    echo -e "${RED}Usage: $0 <code_directory>${NC}"
    exit 1
fi

# Name of the executable to build and test
EXECUTABLE_NAME="peer-time-sync"

# Get absolute path for code directory and executable
code_dir=$1
executable_path="$(realpath "$code_dir")/$EXECUTABLE_NAME"

# Validate that the code directory exists
if ! [ -d "$code_dir" ]; then
    echo -e "${RED}Error: Directory $code_dir does not exist.${NC}"
    exit 1
fi

# Get adresses to send from
adresses=$(ip -4 -o addr show | awk '{print $2, $4}' | awk '{print $2}' | sed 's/\/.*//')

# Print the addresses
echo -e "${YELLOW}Addresses to send from: ${adresses}${NC}"

# Check if adresses were found
if [ -z "$adresses" ]; then
    echo -e "${RED}Error: No addresses found.${NC}"
    exit 1
fi

# Check if at least two addresses were found
address_count=$(echo "$adresses" | wc -w)
if [ "$address_count" -lt 2 ]; then
    echo -e "${RED}Error: At least two IP addresses are required, but only $address_count found.${NC}"
    exit 1
fi

# Build the project
echo -e "${BLUE}Building project in $code_dir...${NC}"
if ! make -C "$code_dir" debug; then
    echo -e "${RED}Error: Build failed.${NC}"
    exit 1
fi
echo -e "${GREEN}Build successful!${NC}"

# Check if the executable was created
if ! [ -f "$executable_path" ]; then
    echo -e "${RED}Error: Executable $EXECUTABLE_NAME not found in $code_dir.${NC}"
    exit 1
fi

# Build the connect utility
if ! make hello ; then
    echo -e "${RED}Error: Build failed.${NC}"
    exit 1
fi

port="1440"

client_port="1449"
client_ip="127.0.0.1"

start_time=$(date +%s)

failing_ip="0.0.0.0"
good_ip="127.0.0.1"
server_ips=("$failing_ip" "$good_ip")

passed=0
failed=0

for ip in "${server_ips[@]}"; do
    echo "========================="
    echo -e "${YELLOW}Testing with server IP: $ip${NC}"

    for address in $adresses; do
        echo -e "${YELLOW}Using address: $address${NC}"

        expected=1
        if [ "$ip" == "$failing_ip" ]; then
            # as server is listenning on 0.0.0.0 and we send them one of it's interfaces
            expected=0
        fi

        if [ "$address" == "$ip" ]; then
            # As host ip == returned ip
            expected=0
        fi

        connect_output_file="${ip}_${address}_test.out"
        connect_error_file="${ip}_${address}_test.err"

        ./hello "$client_ip" "$client_port" "$address" "$port" > "$connect_output_file" 2> "$connect_error_file" &
        test_pid=$!

        if ! ps -p $test_pid > /dev/null; then
            echo -e "${RED}Error: Failed to start the test process.${NC}"
            exit 1
        fi

        # Define output file paths
        output_file="${ip}_${address}_server.out"
        error_file="${ip}_${address}_server.err"

        # Start the server
        # Set connection parameters
        echo -e "${YELLOW}Starting server on $ip:$port...${NC}"
        "$executable_path" -b $ip -p $port -a $client_ip -r $client_port > "$output_file" 2> "$error_file" &
        PID=$!

        # Wait for server to initialize
        sleep 1
        if ! ps -p $PID > /dev/null; then
            echo -e "${RED}Error: Failed to start the server.${NC}"
            close_process 1 $PID
        fi

        echo -e "${GREEN}Server started successfully!${NC}"

        sleep 5

        # Kill the server process
        kill $PID
        wait $PID 2>/dev/null

        # Check if test process is still running
        if ps -p $test_pid > /dev/null; then
            echo -e "${RED}Error: Test process $test_pid is not running.${NC}"
            close_process 1 $test_pid
        fi

        success=1

        expected_error="ERROR MSG 02000104"

        if grep -P "$expected_error" "$error_file" > /dev/null; then
            success=0
        fi

        echo -e "${YELLOW}Expected success: ${expected}, Real: ${success}${NC}"

        if [ $success -eq $expected ]; then
            echo -e "${GREEN}Test process $test_pid completed successfully!${NC}"
            passed=$((passed + 1))

            rm "$connect_output_file" "$connect_error_file"
            rm "$output_file" "$error_file"
        else
            echo -e "${RED}Test process $test_pid failed!${NC}"
            failed=$((failed + 1))

        fi
    
        echo
    done

    echo
done

# Calculate elapsed time
end_time=$(date +%s)
elapsed_time=$((end_time - start_time))
echo -e "${YELLOW}Elapsed time: $elapsed_time seconds${NC}"

echo -e "${GREEN}All tests completed successfully!${NC}"
exit 0
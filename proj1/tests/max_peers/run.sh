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
adresses=$(ip -4 -o addr show | awk '{print $2, $4}' | head -n 2 | awk '{print $2}' | sed 's/\/.*//')

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

first_address=$(echo "$adresses" | head -1)
second_address=$(echo "$adresses" | tail -1)
echo -e "${YELLOW}First address: $first_address${NC}"
echo -e "${YELLOW}Second address: $second_address${NC}"

# Build the project
echo -e "${BLUE}Building project in $code_dir...${NC}"
if ! make -C "$code_dir"; then
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
if ! make connect ; then
    echo -e "${RED}Error: Build failed.${NC}"
    exit 1
fi

# Define output file paths
output_file="output.txt"
error_file="error.txt"

# Start the server
# Set connection parameters
ip="127.0.0.1"
port="1440"
echo -e "${YELLOW}Starting server on $ip:$port...${NC}"
"$executable_path" -b $ip -p $port > "$output_file" 2> "$error_file" &
PID=$!

# Wait for server to initialize
sleep 1
if ! ps -p $PID > /dev/null; then
    echo -e "${RED}Error: Failed to start the server.${NC}"
    exit 1
fi

echo -e "${GREEN}Server started successfully!${NC}"

# Start sending clients with incrementing ports
start_port=1449
client_port="$start_port"
client_ip="$first_address"

start_time=$(date +%s)
failed_at=-1
# Loop from 1 to 65536
for i in $(seq 1 65536); do
    # Periodically print progress
    if [ $((i % 100)) -eq 0 ]; then
        echo -ne "\r${YELLOW}Running test $i - Client port:     $client_port    ${NC}"
    fi

    connect_output_file="${i}.out"
    connect_error_file="${i}.err"

    if [ $i -eq 30000 ]; then
        client_ip="$second_address"
        client_port="$start_port"
    fi

    timeout 5s ./connect "$client_ip" "$client_port" "$ip" "$port" > "$connect_output_file" 2> "$connect_error_file"
    exit_code=$?
    
    if [ $exit_code -eq 124 ]; then
        echo -e "${YELLOW}Timeout occurred${NC}"
        failed_at=$i
        break
    fi

    if [ $exit_code -ne 0 ]; then
        echo -e "${RED}Error: Test $i failed with exit code $exit_code${NC}"
        echo -e "${RED}Error: Failed to run the test.${NC}"
        echo -e "${RED}Output: '$(cat "$connect_output_file")'${NC}"
        echo -e "${RED}Error: '$(cat "$connect_error_file")'${NC}"
        close_process 1 $PID
    else
        rm "$connect_output_file" "$connect_error_file"
    fi

    # Increment client port for next iteration
    ((client_port++))
done

# Calculate elapsed time
end_time=$(date +%s)
elapsed_time=$((end_time - start_time))
echo -e "${YELLOW}Elapsed time: $elapsed_time seconds${NC}"

# Report failure point
echo -e "${RED}Failed during test $failed_at${NC}"
echo

echo -e "${YELLOW}Closing server${NC}"

# Gracefully shut down the server
kill $PID
wait $PID 2>/dev/null

# Check for expected error message
expexted_err_file="err.exp"
# Check if error output matches expected
if diff "$error_file" "$expexted_err_file" >/dev/null; then
    echo -e "${GREEN}Error output matches expected content${NC}"
else
    echo -e "${RED}Error: Error output does not match expected content${NC}"
    echo -e "${YELLOW}Actual error output:${NC}"
    echo -e "${YELLOW}Actual error output file: $(realpath "$error_file")${NC}"
    echo -e "${YELLOW}Expected error output file: $(realpath "$expexted_err_file")${NC}"
    exit 1
fi

# Max peers
SHOULD_FAIL_AT=65536

if [ $failed_at -eq -1 ]; then
    echo -e "${RED}Error: Test did not fail as expected${NC}"
    exit 1
fi

# Verify if the test failed at the expected limit
if [ $i -eq $SHOULD_FAIL_AT ]; then
    echo -e "${GREEN}Test passed: Failed at the expected $SHOULD_FAIL_AT${NC}"
else
    echo -e "${RED}Error: Test failed at $i peers, but expected at $SHOULD_FAIL_AT peers${NC}"
    exit 1
fi
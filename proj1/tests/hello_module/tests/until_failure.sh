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

# Validate command line arguments
if [ "$#" -ne 2 ]; then
    echo -e "${RED}Usage: $0 <executable> <output_dir>${NC}"
    exit 1
fi

# Build the empty_hello_response utility
if ! make empty_hello_response ; then
    echo -e "${RED}Error: Build failed.${NC}"
    exit 1
fi

# Store command line arguments
EXECUTABLE=$1
OUTPUT_DIR=$2

# Define output file paths
output_file="$OUTPUT_DIR/output.txt"
error_file="$OUTPUT_DIR/error.txt"

# Start the server
echo -e "${YELLOW}Starting server on port 1448...${NC}"
"$EXECUTABLE" -p 1448 > "$output_file" 2> "$error_file" &
PID=$!

# Wait for server to initialize
sleep 1
if ! ps -p $PID > /dev/null; then
    echo -e "${RED}Error: Failed to start the server.${NC}"
    exit 1
fi

echo -e "${GREEN}Server started successfully!${NC}"

# Set connection parameters
ip="127.0.0.1"
port="1448"

# Start sending clients with incrementing ports
client_port=1449
i=0

start_time=$(date +%s)
while true; do
    # Periodically print progress
    if [ $((i % 100)) -eq 0 ]; then
        echo -e "${YELLOW}Running test $i${NC}"
        echo -e "${YELLOW}Client port: $client_port${NC}"
    fi

    timeout 5s ./empty_hello_response "127.0.0.1" "$client_port" "$ip" "$port" $i >/tmp/out.txt 2>/tmp/err.txt
    exit_code=$?
    
    if [ $exit_code -eq 124 ]; then
        echo -e "${YELLOW}Timeout occurred${NC}"
        break
    fi

    if [ $exit_code -ne 0 ]; then
        echo -e "${RED}Error: Test $i failed with exit code $exit_code${NC}"
        echo -e "${RED}Error: Failed to run the test.${NC}"
        echo -e "${RED}Error: '$(cat /tmp/err.txt)'${NC}"
        echo -e "${RED}Output: '$(cat /tmp/out.txt)'${NC}"
        close_process 1 $PID
    fi

    # Increment counters for next iteration
    ((client_port++))
    ((i++))
done

# Calculate elapsed time
end_time=$(date +%s)
elapsed_time=$((end_time - start_time))
echo -e "${YELLOW}Elapsed time: $elapsed_time seconds${NC}"

# Report failure point
echo -e "${RED}Failed during test $i${NC}"
echo

echo -e "${YELLOW}Closing server${NC}"

# Gracefully shut down the server
kill $PID
wait $PID 2>/dev/null

# Check for expected error message
error_msg="ERROR MSG 01"
if grep -q "$error_msg" $error_file; then
    echo -e "${GREEN}Found expected error message in output${NC}"
else
    echo -e "${RED}Error: Expected error message not found in output${NC}"
fi

# Calculate the theoretical maximum number of peers based on UDP packet size limitations
MSG_TYPE_SIZE=1          # Size of message type field in bytes
COUNT_SIZE=2             # Size of count field in bytes
PORT_SIZE=2              # Size of port field in bytes
ADRESS_LENGTH_SIZE=1     # Size of address length field in bytes
ADRESS_SIZE=4            # Size of IPv4 address in bytes
EACH_PEER_SIZE=$(($PORT_SIZE + $ADRESS_LENGTH_SIZE + $ADRESS_SIZE))  # Total bytes per peer
MAX_UDP_DATA_SIZE=65507  # Maximum UDP payload size in bytes
MAX_PEERS=$((($MAX_UDP_DATA_SIZE - $MSG_TYPE_SIZE - $COUNT_SIZE) / $EACH_PEER_SIZE))  # Max number of peers
SHOULD_FAIL_AT=$(($MAX_PEERS + 1))

# Verify if the test failed at the expected limit
if [ $i -eq $SHOULD_FAIL_AT ]; then
    echo -e "${GREEN}Test passed: Failed at the expected $SHOULD_FAIL_AT${NC}"
else
    echo -e "${RED}Error: Test failed at $i peers, but expected at $SHOULD_FAIL_AT peers${NC}"
fi
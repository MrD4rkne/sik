#!/bin/bash

# Define colors for better readability
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# Function to safely terminate a process and exit with a status code
function close_process() {
    local code=$1
    local pid=$2
    # Check if process is still running
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

# Build the test tool
if ! make log ; then
    echo -e "${RED}Error: Build failed.${NC}"
    exit 1
fi

# Store command line arguments
EXECUTABLE=$1
OUTPUT_DIR=$2

# Define output files
output_file="$OUTPUT_DIR/output.txt"
error_file="$OUTPUT_DIR/error.txt"

# ------------------------
# Start the first peer
# ------------------------
echo -e "${YELLOW}Starting server...${NC}"
"$EXECUTABLE" -p 1448 > "$output_file" 2> "$error_file" &
PID=$!

# Wait for server to initialize
sleep 1
if ! ps -p $PID > /dev/null; then
    echo -e "${RED}Error: Failed to start the server.${NC}"
    exit 1
fi

# Define connection parameters
ip="127.0.0.1"
port="1448"

# ------------------------
# Run first test
# ------------------------
echo -e "${YELLOW}Running first test...${NC}"
if ! output=$(./empty_hello_response "127.0.0.1" "1449" "$ip" "$port" 0); then
    echo -e "${RED}Error: Failed to run the first test.${NC}"
    close_process 1 $PID
fi

# ------------------------
# Run second test
# ------------------------
echo -e "${YELLOW}Running second test...${NC}"
if ! output=$(./empty_hello_response "127.0.0.1" "1450" "$ip" "$port" 1); then
    echo -e "${RED}Error: Failed to run the second test.${NC}"
    close_process 1 $PID
fi

echo -e "${GREEN}'$output'${NC}"

# Verify expected output contains the first peer's address
if ! echo "$output" | grep -q "127.0.0.1:1449"; then
    echo -e "${RED}Error: Expected 127.0.0.1:1449 in output but not found.${NC}"
    close_process 1 $PID
fi

# ------------------------
# Run third test
# ------------------------
echo -e "${YELLOW}Running third test...${NC}"
if ! output=$(./empty_hello_response "127.0.0.1" "1451" "$ip" "$port" 2); then
    echo -e "${RED}Error: Failed to run the third test.${NC}"
    close_process 1 $PID
fi

echo -e "${GREEN}'$output'${NC}"

# ------------------------
# Test duplicate address handling
# ------------------------
echo -e "${YELLOW}Running on the same address and port as first test (should fail)...${NC}"

# Try to use the same address again - this should timeout
timeout 2s ./empty_hello_response "127.0.0.1" "1449" "$ip" "$port" 3 > /tmp/error_output 2> /tmp/error_output
code=$?

# ------------------------
# Cleanup
# ------------------------
echo -e "${YELLOW}Closing server...${NC}"
kill $PID
wait $PID 2>/dev/null

# Check if the duplicate address test timed out as expected
if [ $code -eq 124 ]; then
    echo -e "${YELLOW}Timed out (as expected)${NC}"
    # Check if error output contains expected error message
    error_msg="ERROR MSG 01"
    if grep -q "$error_msg" $error_file; then
        echo -e "${GREEN}Found expected error message in output${NC}"
    else
        echo -e "${RED}Error: Expected error message not found in output${NC}"
    fi
else
    echo -e "${RED}Error: Expected timeout but got $code${NC}"
    exit 1
fi
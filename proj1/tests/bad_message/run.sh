#!/bin/bash

# Define colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color


# Default values
ip="127.0.0.1"
port="1444"

if [ "$#" -lt 1 ]; then
    echo -e "${RED}Usage: $0 <code_directory>${NC}"
    exit 1
fi

code_dir=$1
if ! [ -d "$code_dir" ]; then
    echo -e "${RED}Error: Directory $code_dir does not exist.${NC}"
    exit 1
fi

shift

# Parse command line options
while getopts ":b:p:" opt; do
    case $opt in
        b) ip="$OPTARG" ;;
        p) port="$OPTARG" ;;
        \?) echo -e "${RED}Invalid option: -$OPTARG${NC}" >&2; exit 1 ;;
        :) echo -e "${RED}Option -$OPTARG requires an argument.${NC}" >&2; exit 1 ;;
    esac
done

# Shift processed options
shift $((OPTIND-1))

echo -e "${BLUE}Using IP: $ip, Port: $port${NC}"

# Initialize counter for failures
FAILURES=0
TOTAL_TESTS=0

EXECUTABLE_NAME="peer-time-sync"

EXECUTABLE="$code_dir/$EXECUTABLE_NAME"

echo -e "${CYAN}=== Building the project ===${NC}"
if ! make -C "$code_dir"; then
    echo -e "${RED}Error: Build failed.${NC}"
    exit 1
fi

# Check if the executable was created
if ! [ -f "$EXECUTABLE" ]; then
    echo -e "${RED}Error: Executable $EXECUTABLE_NAME not found in $code_dir.${NC}"
    exit 1
fi

# Start the server in background
echo -e "${CYAN}=== Starting the server ===${NC}"
output_file="output.txt"
error_file="error.txt"

$EXECUTABLE -b "$ip" -p "$port" > "$output_file" 2> "$error_file" &
SERVER_PID=$!

sleep 2
# Check if the server started successfully
if ! ps -p $SERVER_PID > /dev/null; then
    echo -e "${RED}Error: Server failed to start.${NC}"
    echo -e "${RED}Check the output and error files for more information.${NC}"
    echo -e "${MAGENTA}Output file: $output_file${NC}"
    echo -e "${MAGENTA}Error file: $error_file${NC}"
    exit 1
fi

# Define an array of bad messages to test
MSGS=(
    '\0'
    '\32'
    '\32\31\30\29\28\27\26\25\24\23'
    '\35\11\12\13\14\15\16\17'
    '\32\31\30\29\28\27\26\25\24\23\21\20'
)

# Test each message
echo -e "${CYAN}=== Running tests ===${NC}"
for msg in "${MSGS[@]}"; do
    echo -e "${BLUE}Sending message: $msg${NC}"
    printf "$msg" | nc -u "$ip" "$port" &
done

# Wait for the server to process the messages
sleep 2

# Check if the server is still running
if ! ps -p $SERVER_PID > /dev/null; then
    echo -e "${RED}Server has stopped.${NC}"
    exit 1
fi

kill -9 $SERVER_PID 2>/dev/null

echo -e "${YELLOW}Server stopped.${NC}"

# Check if the output and error files exist
if [ ! -f "$output_file" ]; then
    echo -e "${RED}Error: Output file $output_file not found.${NC}"
    exit 1
fi
if [ ! -f "$error_file" ]; then
    echo -e "${RED}Error: Error file $error_file not found.${NC}"
    exit 1
fi

# Print the contents of the output and error files
echo -e "${CYAN}=== Server output ===${NC}"
echo -e "${MAGENTA}Output file contents:${NC}"
cat -A "$output_file"
echo -e "${MAGENTA}Error file contents:${NC}"
cat -A "$error_file"

# Check the output and error files
echo -e "${CYAN}=== Test results ===${NC}"
for msg in "${MSGS[@]}"; do
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    error_msg="ERROR MSG "
    for i in {0..9}; do
        byte=$(printf "$msg" | dd bs=1 skip=$i count=1 2>/dev/null | hexdump -v -e '"%02x"')
        if [ -n "$byte" ]; then
            error_msg+="$byte"
        else
            break
        fi
    done

    echo -e "${BLUE}Checking for error message: '$error_msg'${NC}"

    if grep -F -q "$error_msg" "$error_file"; then
        echo -e "${GREEN}✓ Message $msg was processed successfully.${NC}"
    else
        echo -e "${RED}✗ Message $msg was not processed.${NC}"
        FAILURES=$((FAILURES + 1))
    fi
done

# Display summary
echo -e "${CYAN}=== Test Summary ===${NC}"
if [ $FAILURES -eq 0 ]; then
    echo -e "${GREEN}All $TOTAL_TESTS tests passed successfully!${NC}"
else
    echo -e "${RED}$FAILURES out of $TOTAL_TESTS tests failed.${NC}"
    exit 1
fi
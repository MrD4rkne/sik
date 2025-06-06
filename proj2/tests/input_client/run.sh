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

EXECUTABLE_NAME="approx-client"

code_dir=$1

if ! [ -d "$code_dir" ]; then
    echo -e "${RED}Error: Directory $code_dir does not exist.${NC}"
    exit 1
fi

if ! make -C "$code_dir"; then
    echo -e "${RED}Error: Build failed.${NC}"
    exit 1
fi

# Check if the executable was created
if ! [ -f "$code_dir/$EXECUTABLE_NAME" ]; then
    echo -e "${RED}Error: Executable $EXECUTABLE_NAME not found in $code_dir.${NC}"
    exit 1
fi

# Test directory
TEST_DIR="$(dirname "$(readlink -f "$0")")"
TEMP_DIR="$TEST_DIR/temp"
EXECUTABLE="$code_dir/$EXECUTABLE_NAME"

# Create temp directory if it doesn't exist
mkdir -p "$TEMP_DIR"

# Counter for tests
TOTAL=0
PASSED=0

# Function to run a test
run_test() {
    local test_name=$1
    local params=$2
    local expected_exit_code=$3
    local description=$4
    local server_pid=""

    TOTAL=$((TOTAL+1))
    
    echo -e "${YELLOW}Running test: $test_name${NC}"
    echo -e "Parameters: $params"
    echo -e "Expected exit code: $expected_exit_code"
    echo -e "Description: $description"
    
    # Timeout after 2 seconds in case the program hangs
    timeout 2s $EXECUTABLE $params > "$TEMP_DIR/output_$test_name.txt" 2>&1
    local actual_exit_code=$?
    
    if [ $actual_exit_code -eq $expected_exit_code ]; then
        echo -e "${GREEN}Test $test_name PASSED${NC}"
        PASSED=$((PASSED+1))
    else
        echo -e "${RED}Test $test_name FAILED: Expected exit code $expected_exit_code, got $actual_exit_code${NC}"
        echo -e "Output:"
        cat "$TEMP_DIR/output_$test_name.txt"
    fi
    
    echo ""
}

# Function to run a test with a server
run_test_with_server() {
    local test_name=$1
    local params=$2
    local expected_exit_code=$3
    local description=$4
    local port=$5
    local interface=$6
    local server_pid=""

    TOTAL=$((TOTAL+1))
    
    echo -e "${YELLOW}Running test: $test_name${NC}"
    echo -e "Parameters: $params"
    echo -e "Expected exit code: $expected_exit_code"
    echo -e "Description: $description"
    echo -e "Server port: $port"
    echo -e "Server interface: $interface"
    
    # Start netcat server in the background
    if [ "$interface" = "ipv6" ]; then
        echo -e "${YELLOW}Starting IPv6 server on port $port...${NC}"
        nc -6 -l "$port" > "$TEMP_DIR/server_output_$test_name.txt" 2>&1 &
    else
        echo -e "${YELLOW}Starting IPv4 server on port $port...${NC}"
        nc -4 -l "$port" > "$TEMP_DIR/server_output_$test_name.txt" 2>&1 &
    fi
    server_pid=$!
    
    # Give server a moment to start
    sleep 0.5
    
    # Run the client
    timeout 2s $EXECUTABLE $params > "$TEMP_DIR/output_$test_name.txt" 2>&1
    local actual_exit_code=$?
    
    # Kill the server
    if [ -n "$server_pid" ]; then
        kill $server_pid 2>/dev/null || true
        wait $server_pid 2>/dev/null || true
    fi
    
    if [ $actual_exit_code -eq $expected_exit_code ]; then
        echo -e "${GREEN}Test $test_name PASSED${NC}"
        PASSED=$((PASSED+1))
    else
        echo -e "${RED}Test $test_name FAILED: Expected exit code $expected_exit_code, got $actual_exit_code${NC}"
        echo -e "Output:"
        cat "$TEMP_DIR/output_$test_name.txt"
    fi
    echo ""
}

# Test cases

EXPECTED_SUCCESS_CODE=124  # Timeout exit code (program would run longer than timeout)
EXPECTED_FAILURE_CODE=1    # Error exit code

# Default parameters (no parameters)
run_test "default" "" $EXPECTED_FAILURE_CODE "Run with no parameters"

# Valid parameters with server
# Each test uses a different port to avoid conflicts
run_test_with_server "valid" "-u player1 -s localhost -p 8000" $EXPECTED_SUCCESS_CODE "Run with valid parameters" "8000" "ipv4"
run_test_with_server "valid_with_ipv4" "-u player1 -s localhost -p 8001 -4" $EXPECTED_SUCCESS_CODE "Run with valid parameters and IPv4 flag" "8001" "ipv4"
run_test_with_server "valid_with_ipv6" "-u player1 -s ::1 -p 8002 -6" $EXPECTED_SUCCESS_CODE "Run with valid parameters and IPv6 flag" "8002" "ipv6"
run_test_with_server "valid_with_strategy" "-u player1 -s localhost -p 8003 -a" $EXPECTED_SUCCESS_CODE "Run with valid parameters and strategy flag" "8003" "ipv4"
run_test_with_server "valid_with_all_opts" "-u player1 -s localhost -p 8004 -4 -a" $EXPECTED_SUCCESS_CODE "Run with all valid parameters" "8004" "ipv4"

# Player ID tests
run_test_with_server "valid_player_id_1" "-u player123 -s localhost -p 8005" $EXPECTED_SUCCESS_CODE "Valid player ID (alphanumeric)" "8005" "ipv4"
run_test_with_server "valid_player_id_2" "-u Player123 -s localhost -p 8006" $EXPECTED_SUCCESS_CODE "Valid player ID (mixed case)" "8006" "ipv4"
run_test_with_server "valid_player_id_3" "-u 123Player -s localhost -p 8007" $EXPECTED_SUCCESS_CODE "Valid player ID (starting with numbers)" "8007" "ipv4"
run_test "invalid_player_id_1" "-u player-123 -s localhost -p 8008" $EXPECTED_FAILURE_CODE "Invalid player ID (with hyphen)"
run_test "invalid_player_id_2" "-u player_123 -s localhost -p 8009" $EXPECTED_FAILURE_CODE "Invalid player ID (with underscore)"
run_test "invalid_player_id_3" "-u \"player 123\" -s localhost -p 8010" $EXPECTED_FAILURE_CODE "Invalid player ID (with space)"
run_test "empty_player_id" "-u \"\" -s localhost -p 8011" $EXPECTED_FAILURE_CODE "Empty player ID"

# Server address tests
run_test_with_server "valid_server_1" "-u player1 -s localhost -p 8012" $EXPECTED_SUCCESS_CODE "Valid server (localhost)" "8012" "ipv4"
run_test_with_server "valid_server_2" "-u player1 -s 127.0.0.1 -p 8013" $EXPECTED_SUCCESS_CODE "Valid server (IPv4 address)" "8013" "ipv4"
run_test_with_server "valid_server_3" "-u player1 -s ::1 -p 8014 -6" $EXPECTED_SUCCESS_CODE "Valid server (IPv6 address)" "8014" "ipv6"
run_test "empty_server" "-u player1 -s \"\" -p 8015" $EXPECTED_FAILURE_CODE "Empty server address"

# Port tests
run_test_with_server "valid_port_1" "-u player1 -s localhost -p 8016" $EXPECTED_SUCCESS_CODE "Valid port (8016)" "8016" "ipv4"
run_test_with_server "valid_port_2" "-u player1 -s localhost -p 1025" $EXPECTED_SUCCESS_CODE "Valid port (1025)" "1025" "ipv4"
run_test_with_server "valid_port_3" "-u player1 -s localhost -p 65535" $EXPECTED_SUCCESS_CODE "Valid port (maximum value)" "65535" "ipv4"
run_test "invalid_port_1" "-u player1 -s localhost -p 0" $EXPECTED_FAILURE_CODE "Invalid port (0)"
run_test "invalid_port_2" "-u player1 -s localhost -p 65536" $EXPECTED_FAILURE_CODE "Invalid port (above max)"
run_test "invalid_port_3" "-u player1 -s localhost -p -1" $EXPECTED_FAILURE_CODE "Invalid port (negative)"
run_test "invalid_port_4" "-u player1 -s localhost -p abc" $EXPECTED_FAILURE_CODE "Invalid port (not a number)"
run_test "invalid_port_5" "-u player1 -s localhost -p 8000a" $EXPECTED_FAILURE_CODE "Invalid port (not a number)"
run_test "invalid_port_6" "-u player1 -s localhost -p a8000" $EXPECTED_FAILURE_CODE "Invalid port (not a number)"

# Not provided argument values
run_test "no_player_id" "-u -s localhost -p 8017" $EXPECTED_FAILURE_CODE "No player ID provided"
run_test "no_server" "-u player1 -s -p 8018" $EXPECTED_FAILURE_CODE "No server provided"
run_test "no_port" "-u player1 -s localhost -p" $EXPECTED_FAILURE_CODE "No port provided"
run_test "no_player_id_and_server" "-u -s -p 8019" $EXPECTED_FAILURE_CODE "No player ID and server provided"
run_test "no_player_id_and_port" "-u -s localhost -p" $EXPECTED_FAILURE_CODE "No player ID and port provided"
run_test "no_server_and_port" "-u player1 -s -p" $EXPECTED_FAILURE_CODE "No server and port provided"

# Missing required parameters
run_test "missing_player_id" "-s localhost -p 8020" $EXPECTED_FAILURE_CODE "Missing -u parameter"
run_test "missing_server" "-u player1 -p 8021" $EXPECTED_FAILURE_CODE "Missing -s parameter"
run_test "missing_port" "-u player1 -s localhost" $EXPECTED_FAILURE_CODE "Missing -p parameter"
run_test "missing_player_id_and_server" "-p 8022" $EXPECTED_FAILURE_CODE "Missing -u and -s parameters"
run_test "missing_player_id_and_port" "-s localhost" $EXPECTED_FAILURE_CODE "Missing -u and -p parameters"
run_test "missing_server_and_port" "-u player1" $EXPECTED_FAILURE_CODE "Missing -s and -p parameters"

# IPv4 and IPv6 flags together
run_test_with_server "ipv4_and_ipv6_flags to ipv6 server" "-u player1 -s localhost -p 8023 -4 -6" $EXPECTED_SUCCESS_CODE "Both IPv4 and IPv6 flags provided" "8023" "ipv6"
run_test_with_server "ipv4_and_ipv6_flags to ipv4 server" "-u player1 -s localhost -p 8023 -4 -6" $EXPECTED_SUCCESS_CODE "Both IPv4 and IPv6 flags provided" "8023" "ipv4"

# Unreachable server test
run_test "unreachable_server" "-u player1 -s nonexistent.example.com -p 8024" $EXPECTED_FAILURE_CODE "Unreachable server"

# Connection refused test (no server running on this port)
run_test "connection_refused" "-u player1 -s localhost -p 9999" $EXPECTED_FAILURE_CODE "Connection refused (no server)"

# Duplicated arguments test
run_test "duplicated_args" "-u player1 -s localhost -p 8025 -u player2" $EXPECTED_FAILURE_CODE "Duplicated arguments (player ID)"
run_test "duplicated_args_server" "-u player1 -s localhost -p 8026 -s localhost" $EXPECTED_FAILURE_CODE "Duplicated arguments (server)"
run_test "duplicated_args_port" "-u player1 -s localhost -p 8027 -p 8028" $EXPECTED_FAILURE_CODE "Duplicated arguments (port)"
run_test "duplicated_strategy" "-u player1 -s localhost -p 8027 -a -a" $EXPECTED_FAILURE_CODE "Duplicated arguments (strategy)"
run_test "duplicated_ipv4" "-u player1 -s localhost -p 8027 -4 -4" $EXPECTED_FAILURE_CODE "Duplicated arguments (IPv4)"
run_test "duplicated_ipv6" "-u player1 -s localhost -p 8027 -6 -6" $EXPECTED_FAILURE_CODE "Duplicated arguments (IPv6)"

# Print summary
echo -e "${YELLOW}=== Test Summary ===${NC}"
echo -e "Total tests: $TOTAL"
echo -e "Passed tests: $PASSED"
echo -e "Failed tests: $((TOTAL-PASSED))"

if [ $PASSED -eq $TOTAL ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed!${NC}"
    exit 1
fi

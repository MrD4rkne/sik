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

EXECUTABLE_NAME="peer-time-sync"

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

failure_bind_test() {
    local test_name=$1
    local adress=$2
    local port=$3
    local expected_exit_code=$4
    local description=$5

    TOTAL=$((TOTAL+1))
    
    echo -e "${YELLOW}Running test: $test_name${NC}"
    echo -e "Address: $adress"
    echo -e "Port: $port"
    echo -e "Expected exit code: $expected_exit_code"
    echo -e "Description: $description"

    params="-b $adress -p $port"

    # Run first instance to bind to the specified address and port
    $EXECUTABLE -b $adress -p $port > "$TEMP_DIR/output_${test_name}_first.txt" 2>&1 &
    first_pid=$!
    sleep 0.5  # Give time for the first instance to start
    
    # Now try to run the second instance with the same address and port
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

    # Kill the first instance
    kill $first_pid
}

# Test cases

EXPECTED_SUCCESS_CODE=124
EXPECTED_FAILURE_CODE=1

# Default parameters (no parameters)
run_test "default" "" $EXPECTED_SUCCESS_CODE "Run with default parameters"

# Valid bind address tests
run_test "valid_bind_1" "-b 127.0.0.1" $EXPECTED_SUCCESS_CODE "Valid bind address (localhost)"
run_test "valid_bind_2" "-b 0.0.0.0" $EXPECTED_SUCCESS_CODE "Valid bind address (all interfaces)"

# Valid port tests
run_test "valid_port_1" "-p 8080" $EXPECTED_SUCCESS_CODE "Valid port (8080)"
run_test "valid_port_2" "-p 0" $EXPECTED_SUCCESS_CODE "Valid port (0 - any port)"
run_test "valid_port_3" "-p 65535" $EXPECTED_SUCCESS_CODE "Valid port (max value)"

# Invalid port tests
run_test "invalid_port_1" "-p 65536" $EXPECTED_FAILURE_CODE "Invalid port (above max)"
run_test "invalid_port_2" "-p -1" $EXPECTED_FAILURE_CODE "Invalid port (negative)"
run_test "invalid_port_3" "-p abc" $EXPECTED_FAILURE_CODE "Invalid port (not a number)"

# Peer tests
run_test "peer_address_only" "-a 192.168.1.1" $EXPECTED_FAILURE_CODE "Only peer address without port (should fail)"
run_test "peer_port_only" "-r 8080" $EXPECTED_FAILURE_CODE "Only peer port without address (should fail)"
run_test "valid_peer" "-a 192.168.1.1 -r 8080" $EXPECTED_SUCCESS_CODE "Valid peer address and port"
run_test "valid_peer_hostname" "-a localhost -r 8080" $EXPECTED_SUCCESS_CODE "Valid peer hostname and port"
run_test "valid_peer_hostname" "-a localhost -r 0" $EXPECTED_FAILURE_CODE "Valid peer hostname and invalid peer port"
run_test "valid_peer_hostname" "-a localhost -r -1" $EXPECTED_FAILURE_CODE "Valid peer hostname and invalid peer port"
run_test "valid_peer_hostname" "-a localhost -r abc" $EXPECTED_FAILURE_CODE "Valid peer hostname and invalid peer port"
run_test "valid_peer_hostname" "-a localhost -r 65536" $EXPECTED_FAILURE_CODE "Valid peer hostname and invalid peer port"

# Parameter order tests
run_test "parameter_order_1" "-p 8080 -b 127.0.0.1 -a localhost -r 9090" $EXPECTED_SUCCESS_CODE "Valid parameters in different order"
run_test "parameter_order_2" "-a localhost -r 9090 -p 8080 -b 127.0.0.1" $EXPECTED_SUCCESS_CODE "Valid parameters in different order"

# Combination tests
run_test "combination_1" "-b 127.0.0.1 -p 8080 -a localhost -r 9090" $EXPECTED_SUCCESS_CODE "All valid parameters"
run_test "combination_2" "-b 127.0.0.1 -p 0 -a localhost -r 9090" $EXPECTED_SUCCESS_CODE "Valid parameters with port 0"

# Invalid combination tests
run_test "invalid_combination_1" "-b invalid_ip -p 8080" $EXPECTED_FAILURE_CODE "Invalid bind address"
run_test "invalid_combination_2" "-b 127.0.0.1 -p 8080 -a localhost -r 99999" $EXPECTED_FAILURE_CODE "Invalid peer port"

# Multiple instances of the same parameter
run_test "duplicate_params" "-p 8080 -p 9090" $EXPECTED_FAILURE_CODE "Duplicate parameters"

# Failure to bind tests
failure_bind_test "bind_test_1" "127.0.0.1" "8080" $EXPECTED_FAILURE_CODE "Attempt to bind to the same address and port"

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
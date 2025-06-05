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

EXECUTABLE_NAME="approx-server"

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
run_test "default" "" $EXPECTED_FAILURE_CODE "Run with no parameters"

# Valid parameters.
run_tets "valid" "-p 65535 -k 500 -n 8 -m 100 -f empty.coeff" $EXPECTED_SUCCESS_CODE "Run with valid parameters"

# Valid port tests
run_test "valid_port_1" "-p 8080 -f empty.coeff" $EXPECTED_SUCCESS_CODE "Valid port (8080)"
run_test "valid_port_2" "-p 0 -f empty.coeff" $EXPECTED_SUCCESS_CODE "Valid port (0 - any port)"
run_test "valid_port_3" "-p 65535 -f empty.coeff" $EXPECTED_SUCCESS_CODE "Valid port (max value)"

# Invalid port tests
run_test "invalid_port_1" "-p 65536 -f empty.coeff" $EXPECTED_FAILURE_CODE "Invalid port (above max)"
run_test "invalid_port_2" "-p -1 -f empty.coeff" $EXPECTED_FAILURE_CODE "Invalid port (negative)"
run_test "invalid_port_3" "-p abc -f empty.coeff" $EXPECTED_FAILURE_CODE "Invalid port (not a number)"

# Not provided argument values
run_test "no_bind_port" "-f empty.coeff -p" $EXPECTED_FAILURE_CODE "No bind port provided"
run_test "no_k" "-f empty.coeff -k" $EXPECTED_FAILURE_CODE "No K provided"
run_test "no_n" "-f empty.coeff -n" $EXPECTED_FAILURE_CODE "No N provided"
run_test "no_m" "-f empty.coeff -m" $EXPECTED_FAILURE_CODE "No M provided"
run_test "no_k_and_m" "-f empty.coeff -k -n 1" $EXPECTED_FAILURE_CODE "No K provided"
run_test "no_k_and_n" "-f empty.coeff -k -m 1" $EXPECTED_FAILURE_CODE "No K provided"
run_test "no_n_and_k" "-f empty.coeff -n -k 1" $EXPECTED_FAILURE_CODE "No N provided"
run_test "no_n_and_m" "-f empty.coeff -n -m 1" $EXPECTED_FAILURE_CODE "No N provided"
run_test "no_m_and_k" "-f empty.coeff -m -k 1" $EXPECTED_FAILURE_CODE "No M provided"
run_test "no_m_and_n" "-f empty.coeff -m -n 1" $EXPECTED_FAILURE_CODE "No M provided"

# Invalid K tests.
run_test "alphanumeric_K" "-f empty.coeff -k 123a" $EXPECTED_FAILURE_CODE "Letters in K"
run_test "rational_K" "-f empty.coeff -k 123.1" $EXPECTED_FAILURE_CODE "K is not an integer"
run_test "negative_K" "-f empty.coeff -k -1" $EXPECTED_FAILURE_CODE "K is negative"
run_test "zero_K" "-f empty.coeff -k -0" $EXPECTED_FAILURE_CODE "K is zero"
run_test "too_large_K" "-f empty.coeff -k 10001" $EXPECTED_FAILURE_CODE "K is too large"
run_test "very_large_K" "-f empty.coeff -k 99999999999999999999999999999999999999999999999999999999999999999999999999999999999999999" $EXPECTED_FAILURE_CODE "K is too large"
run_test "space_K" "-f empty.coeff -k  " $EXPECTED_FAILURE_CODE "K is a space"

# Invalid N tests.
run_test "alphanumeric_N" "-f empty.coeff -n 1a" $EXPECTED_FAILURE_CODE "Letters in N"
run_test "rational_N" "-f empty.coeff -n 1.1" $EXPECTED_FAILURE_CODE "N is not an integer"
run_test "negative_N" "-f empty.coeff -N -1" $EXPECTED_FAILURE_CODE "N is negative"
run_test "zero_N" "-f empty.coeff -n -0" $EXPECTED_FAILURE_CODE "N is zero"
run_test "too_large_N" "-f empty.coeff -n 9" $EXPECTED_FAILURE_CODE "N is too large"
run_test "very_large_N" "-f empty.coeff -n 99999999999999999999999999999999999999999999999999999999999999999999999999999999999999999" $EXPECTED_FAILURE_CODE "N is too large"
run_test "space_N" "-f empty.coeff -n  " $EXPECTED_FAILURE_CODE "N is a space"

# Invalid M tests.
run_test "alphanumeric_M" "-f empty.coeff -M 1a" $EXPECTED_FAILURE_CODE "Letters in M"
run_test "rational_M" "-f empty.coeff -m 1.1" $EXPECTED_FAILURE_CODE "M is not an integer"
run_test "negative_M" "-f empty.coeff -m -1" $EXPECTED_FAILURE_CODE "M is negative"
run_test "zero_M" "-f empty.coeff -m -0" $EXPECTED_FAILURE_CODE "M is zero"
run_test "too_large_M" "-f empty.coeff -m 12341235" $EXPECTED_FAILURE_CODE "M is too large"
run_test "very_large_M" "-f empty.coeff -m 99999999999999999999999999999999999999999999999999999999999999999999999999999999999999999" $EXPECTED_FAILURE_CODE "N is too large"
run_test "space_M" "-f empty.coeff -m  " $EXPECTED_FAILURE_CODE "M is a space"

# File missing tests
run_test "file_missing" "-k 1 -n 1 -m 1 -p 8006" $EXPECTED_FAILURE_CODE "Missing -f parameter"

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
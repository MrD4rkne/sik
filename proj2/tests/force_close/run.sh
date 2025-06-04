#!/bin/bash

# Define colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# Function to run the executable and set its PID in a global variable
run_executable() {
    local code_dir=$1
    local executable_name=$2
    local args=$3
    local output_file=$4
    local error_file=$5
    
    "$code_dir/$executable_name" $args > "$output_file" 2> "$error_file" &
    EXECUTABLE_PID=$!
}

# Function to compare two files, keeping only ERROR MSG lines
compare_error_msgs() {
    local file1=$1
    local file2=$2
    local temp_dir=$3
    
    # Create temporary files for filtered content
    local filtered1="${temp_dir}/filtered1.$$"
    local filtered2="${temp_dir}/filtered2.$$"

    # Extract lines starting with "ERROR: bad message", sort them
    grep "^ERROR: bad message" "$file1" 2>/dev/null | sort > "$filtered1"
    grep "^ERROR: bad message" "$file2" 2>/dev/null | sort > "$filtered2"
    
    # Compare the filtered files
    diff -q "$filtered1" "$filtered2" >/dev/null
    local result=$?
    
    # Clean up temporary files
    rm -f "$filtered1" "$filtered2"
    
    return $result
}

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

# Compile tester
TESTER_DIR="src/"
TESTER_EXECUTABLE="test_force_close"
if ! make -C "$TESTER_DIR"; then
    echo -e "${RED}Error: Failed to compile tester executable.${NC}"
    exit 1
fi

if ! [ -f "$TESTER_DIR/$TESTER_EXECUTABLE" ]; then
    echo -e "${RED}Error: Tester executable $TESTER_EXECUTABLE not found in $TESTER_DIR.${NC}"
    exit 1
fi

TEMP_DIR="temp"
if ! mkdir -p "$TEMP_DIR"; then
    echo -e "${RED}Error: Failed to create temporary directory $TEMP_DIR.${NC}"
    exit 1
fi

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

check_port() {
    local port="$1"

    # Check if the port is in LISTEN or TIME_WAIT state using ss
    if ss -tan 2>/dev/null | awk '{print $1, $4}' | grep -E "^(LISTEN|TIME-WAIT|TIME_WAIT) " | grep -qE "(:|\.)${port}\$"; then
        return 1  # Port is busy or in TIME_WAIT
    else
        return 0  # Port is free
    fi
}

successes=0
failures=0
runs=0

COEFF_MSG="COEFF 1.0000000 0.0000000 -100.0000000 100.0000000 -99.9999999 99.9999999 0"

TEST_CASES=(
    "localhost 8000 IPv4"
    "localhost 8001 IPv6"
    "localhost 8002 Any"
    "::1 8003 IPv6"
    "127.0.0.1 8804 IPv4"
)
for test_case in "${TEST_CASES[@]}"; do
    ip=$(echo "$test_case" | awk '{print $1}')
    port=$(echo "$test_case" | awk '{print $2}')
    ip_type=$(echo "$test_case" | awk '{print $3}')

    runs=$((runs + 1))

    echo -e "${YELLOW}Running test case: IP=$ip, PORT=$port, TYPE=$ip_type${NC}"

    # Kill any previous server/client processes that may be holding the port
    pkill -f "$SERVER_EXECUTABLE_NAME" 2>/dev/null
    pkill -f "$CLIENT_EXECUTABLE_NAME" 2>/dev/null

    # Give time for system to free ports
    echo -e "${YELLOW}Waiting for the port to be ready${NC}"
    
    MAX_TRY_COUNTS=100
    try_count=0
    while ! check_port 8000; do
        if [ $try_count -ge $MAX_TRY_COUNTS ]; then
            echo -e "${RED}Port 8000 is still unavailable after $MAX_TRY_COUNTS attempts. Exiting.${NC}"
            exit 1
        fi

        echo -e "${YELLOW}Port 8000 is unavailable, retrying in 1 second...${NC}"
        sleep 1
        try_count=$((try_count + 1))
    done

    sleep 1  # Give some time for the port to be ready

    coeff_file="$TEMP_DIR/${ip_type}_coeff.txt"
    printf "%s\r\n" "$COEFF_MSG" > "$coeff_file"
    
    PRG_OUTPUT="$TEMP_DIR/${ip_type}_sut.out"
    PRG_ERROR="$TEMP_DIR/${ip_type}_sut.err"

    TESTER_OUTPUT="$TEMP_DIR/${ip_type}_tester.out"
    TESTER_ERROR="$TEMP_DIR/${ip_type}_tester.err"

    # Run the server
    run_executable "$code_dir" "$SERVER_EXECUTABLE_NAME" "-p $port -f $coeff_file -m 1 -k 10000" "$PRG_OUTPUT" "$PRG_ERROR"
    server_pid=$EXECUTABLE_PID

    success=1

    # Run tester
    timeout 5s "$TESTER_DIR/$TESTER_EXECUTABLE" "$ip" "$port" "$ip_type" "$COEFF_MSG" >"$TESTER_OUTPUT" 2>"$TESTER_ERROR"
    if [ $? -ne 0 ]; then
        echo -e "${RED}Tester failed to run or timed out.${NC}"
        success=0
    fi


    # Check if the server is still running
    if kill -0 $server_pid 2>/dev/null; then
        echo -e "${YELLOW}Server is still running, attempting to close it...${NC}"
        kill $server_pid
        wait $server_pid 2>/dev/null
    else
        success=0
        echo -e "${RED}Server did not run successfully.${NC}"
    fi

    if [ $success -eq 1 ]; then
        successes=$((successes + 1))
        echo -e "${GREEN}Tests completed successfully.${NC}"
    else
        failures=$((failures + 1))
        echo -e "${RED}Tests failed.${NC}"
    fi

    echo
done

if [ $failures -eq 0 ]; then
    echo -e "${GREEN}All tests passed successfully!${NC}"
else
    echo -e "${RED}Some tests failed. Please check the logs.${NC}"
fi

echo -e "${YELLOW}Summary: $successes successes, $failures failures out of $runs runs.${NC}"
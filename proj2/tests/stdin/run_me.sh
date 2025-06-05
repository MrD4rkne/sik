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

    directory=$(dirname "$file1")
    file2_name=$(basename "$file2")
    
    # Create temporary files for filtered content
    local filtered1="$file1.filtered"
    local filtered2="$directory/$file2_name.filtered"

    # Extract lines starting with "ERROR: invalid input line", sort them
    grep "^ERROR: invalid input line" "$file1" 2>/dev/null | sort > "$filtered1"
    grep "^ERROR: invalid input line" "$file2" 2>/dev/null | sort > "$filtered2"
    
    # Compare the filtered files
    diff -q "$filtered1" "$filtered2" >/dev/null
    local result=$?
    
    return $result
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

extract_arg() {
    local arg_name="$1"
    local file_name="$2"

    # if name containes "$arg_name=" then extract it
    if [[ "$file_name" == *"${arg_name}="* ]]; then
        local value=$(echo "$file_name" | grep -oP "(?<=${arg_name}=)\d+")
        if [[ -n "$value" ]]; then
            echo "-${arg_name} $value"
        fi
    fi
}

if [ "$#" -ne 2 ]; then
    echo -e "${RED}Usage: $0 <code_directory> <script_runner>${NC}"
    exit 1
fi

CLIENT_EXECUTABLE_NAME="approx-client"
SERVER_EXECUTABLE_NAME="approx-server"
PYTHON_EXECUTABLE="python3"

ERROR_CODE=1
ERROR_INPUT_ENDING="_error"

code_dir=$1
test_runner=$2

if ! [ -d "$code_dir" ]; then
    echo -e "${RED}Error: Directory $code_dir does not exist.${NC}"
    exit 1
fi

if ! [ -f "$test_runner" ]; then
    echo -e "${RED}Error: $test_runner does not exist.${NC}"
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

# Get all .in files in the current directory
TEST_DIR="$(dirname "$(readlink -f "$0")")"

# Make dir for temp files
mkdir -p "$TEST_DIR"/temp

passed=0
failed=0
unavailable=0

# Find all .in files recursively in the test directory
tests=$(find "$TEST_DIR" -type f -name "*.in" | sort)

# Check if any tests were found
if [ -z "$tests" ]; then
    echo -e "${YELLOW}No test files found in $TEST_DIR${NC}"
    exit 1
fi

echo -e "${YELLOW}Found $(echo "$tests" | wc -l) test files${NC}"
# Run tests
for file in $tests; do
    # Kill any previous server/client processes that may be holding the port
    pkill -f "$SERVER_EXECUTABLE_NAME" 2>/dev/null
    pkill -f "$CLIENT_EXECUTABLE_NAME" 2>/dev/null

    # Give time for system to free ports
    echo -e "${YELLOW}Waiting for the port to be ready${NC}"
    
    MAX_TRY_COUNTS=100
    try_count=0
    
    echo

    while ! check_port 8000; do
        if [ $try_count -ge $MAX_TRY_COUNTS ]; then
            echo -e "${RED}Port 8000 is still unavailable after $MAX_TRY_COUNTS attempts. Exiting.${NC}"
            exit 1
        fi

        # Print status on the same line, show attempt number
        echo -ne "\r${YELLOW}Port 8000 is unavailable, retrying in 1 second... (attempt $((try_count+1))/${MAX_TRY_COUNTS})${NC}"
        sleep 1
        try_count=$((try_count + 1))
    done
    # Clear the line after port becomes available
    echo -ne "\r\033[K"

    sleep 1  # Give some time for the port to be ready

    echo "Processing $file"  
    success=1

    # SUT = System Under Test ;)
    file_name=$(basename "$file")
    program_output_file="$TEST_DIR/temp/${file_name%.in}_sut.out"
    program_error_file="$TEST_DIR/temp/${file_name%.in}_sut.err"
    program_input_file="$file.lines"
    program_expected_bad_lines_file="$file.expected_bad_lines"
    test_output_file="$TEST_DIR/temp/${file_name%.in}_tester.out"
    test_error_file="$TEST_DIR/temp/${file_name%.in}_tester.err"

    # Check if input files exist
    if [ ! -f "$program_input_file" ]; then
        echo -e "${RED}Input file $program_input_file does not exist. Skipping test.${NC}"
        unavailable=$((unavailable+1))
        continue
    fi

    echo -e "${YELLOW}Starting tester${NC}"

    # Run tester
    "$PYTHON_EXECUTABLE" "${test_runner}" < "$file" > "$test_output_file" 2>"$test_error_file" &
    tester_pid=$!

    sleep 1

    # Run the executable directly in the background
    "$code_dir/$CLIENT_EXECUTABLE_NAME" -s :: -p 8000 -u pl4y3r < "$program_input_file" > "$program_output_file" 2> "$program_error_file" &
    pid=$!
   
    if [ $? -ne 0 ]; then
        continue
    fi

    echo -e "${YELLOW}Waiting for the tester to finish...${NC}"
    wait $tester_pid
    tester_exit_code=$?

    if [ $tester_exit_code -ne 0 ]; then
        echo -e "${RED}Tester process exited with error code $tester_exit_code${NC}"
        success=0
    else
        echo -e "${YELLOW}Tester finished successfully with exit code $tester_exit_code.${NC}"
    fi

    echo -e "${YELLOW}Waiting for the program to finish...${NC}"
    
    # Start a background job to kill the process after 2 seconds if it's still running
    (
        sleep 2
        if kill -0 $pid 2>/dev/null; then
            echo -e "${YELLOW}Killing program process $pid after timeout...${NC}"
            kill $pid
        fi
    ) &

    wait $pid
    program_exit_code=$?
    if [ $program_exit_code -eq 143 ]; then
        echo -e "${YELLOW}Program process was killed due to timeout.${NC}"
        program_exit_code=0
    fi

    should_exit_with_code=0
    if [[ "${file_name%.in}" == *"$ERROR_INPUT_ENDING" ]]; then
        should_exit_with_code=$ERROR_CODE
    fi

    echo "Expected exit code: $should_exit_with_code"
    if [ $program_exit_code -ne $should_exit_with_code ]; then
        echo -e "${RED}Program process exited with error code $program_exit_code${NC}"
        success=0
    else
        echo -e "${YELLOW}Program finished successfully with exit code $should_exit_with_code.${NC}"
    fi

    # Verify bad lines are the same
    echo -e "${YELLOW}Comparing bad lines...${NC}"
    if ! compare_error_msgs "$program_error_file" "$program_expected_bad_lines_file"; then
        echo -e "${RED}Bad lines do not match:${NC}"
        success=0
    fi

    if [ $success -eq 0 ]; then
        echo -e "${RED}Test failed for input file: $file${NC}"
        echo -e "${RED}Program output file: ${NC}$program_output_file"
        echo -e "${RED}Program error file: ${NC}$program_error_file"
        echo -e "${RED}Test output file: ${NC}$test_output_file"
        echo -e "${RED}Test error file: ${NC}$test_error_file"

    else
        echo -e "${GREEN}Test passed for input file: ${file}${NC}"
    fi

    echo "===================================="
    echo

    if [ $success -eq 0 ]; then
        failed=$((failed+1))
    else
        passed=$((passed+1))
    fi
done

echo -e "${YELLOW}Total tests passed: $passed${NC}"
echo -e "${RED}Total tests failed: $failed${NC}"

if [ $failed -gt 0 ]; then
    echo -e "${RED}Some tests failed. Please check the output above.${NC}"
    exit 1
else
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
fi
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
    echo "Processing $file"  
    success=1

    # SUT = System Under Test ;)
    file_name=$(basename "$file")
    program_output_file="$TEST_DIR/temp/${file_name%.in}_sut.out"
    program_error_file="$TEST_DIR/temp/${file_name%.in}_sut.err"
    test_output_file="$TEST_DIR/temp/${file_name%.in}_tester.out"
    test_error_file="$TEST_DIR/temp/${file_name%.in}_tester.err"

    if [[ "$(basename "$file")" == _* ]]; then
        echo -e "${YELLOW}Starting tester before executable as $file_name starts with underscore${NC}"

        # Run tester
        "$PYTHON_EXECUTABLE" "${test_runner}" < "$file" > "$test_output_file" 2>"$test_error_file" &
        tester_pid=$!

        sleep 1

        # Run the executable
        run_executable "$code_dir" "$CLIENT_EXECUTABLE_NAME" "-s :: -p 8000 -u pl4y3r -a" "$program_output_file" "$program_error_file"
        pid=$EXECUTABLE_PID

    else

        input_file="$("$file_name%.in").coeffs"

        # Run the executable
        run_executable "$code_dir" "$SERVER_EXECUTABLE_NAME" "-p 8000 -f $input_file" "$program_output_file" "$program_error_file"
        pid=$EXECUTABLE_PID
        # Run tester
        cat "$file" | "$PYTHON_EXECUTABLE" "${test_runner}" > "$test_output_file" 2>"$test_error_file"
        tester_pid=$!
    fi
        
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

    # Assume that all tests ends with proper scoring, so:
    echo -e "${YELLOW}Waiting for the program to finish...${NC}"
    wait $pid
    program_exit_code=$?

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

    # Verify tester error is empty
    if [ -s "$test_error_file" ]; then
        echo -e "${RED}Tester error output is not empty:${NC}"
        cat "$test_error_file"
        success=0
    fi

    # Verify error msgs are the same
    echo -e "${YELLOW}Comparing error messages...${NC}"
    if ! compare_error_msgs "$program_error_file" "$test_output_file" "$TEST_DIR/temp"; then
        echo -e "${RED}Error messages do not match:${NC}"
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
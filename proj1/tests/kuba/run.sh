#!/bin/bash

# Define colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

if [ "$#" -ne 2 ]; then
    echo -e "${RED}Usage: $0 <code_directory> <script_runner>${NC}"
    exit 1
fi

EXECUTABLE_NAME="peer-time-sync"
PYTHON_EXECUTABLE="python3"

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

if ! make -C "$code_dir" debug; then
    echo -e "${RED}Error: Build failed.${NC}"
    exit 1
fi

# Check if the executable was created
if ! [ -f "$code_dir/$EXECUTABLE_NAME" ]; then
    echo -e "${RED}Error: Executable $EXECUTABLE_NAME not found in $code_dir.${NC}"
    exit 1
fi

# Get all .in files in the current directory
TEST_DIR="$(dirname "$(readlink -f "$0")")"

# Make dir for temp files
mkdir -p "$TEST_DIR"/temp

# Run tests
for file in "$TEST_DIR"/*.in; do
    echo "Processing $file"  
    success=1

    file_name=$(basename "$file")
    program_output_file="$TEST_DIR/temp/program_${file_name%.in}.out"
    program_error_file="$TEST_DIR/temp/program_${file_name%.in}.err"

    ${code_dir}/$EXECUTABLE_NAME -b "127.0.0.1" -p "8000" > "$program_output_file" 2> "$program_error_file" &
    pid=$!

    sleep 1

    if ! ps -p $pid > /dev/null; then
        echo -e "${ERED}Error: Process $pid is not running.${NC}"
        continue
    fi

    test_output_file="$TEST_DIR/temp/test_${file_name%.in}.out"
    test_error_file="$TEST_DIR/temp/test_${file_name%.in}.err"

    echo -e "${YELLOW}Running test with input file: $file${NC}"
    if ! cat "$file" | "$PYTHON_EXECUTABLE" "${test_runner}" > "$test_output_file" 2>"$test_error_file"; then
        echo -e "${RED}Error: Test failed for input file: $file${NC}"
        success=0
    fi

    # Wait for the program to finish
    echo -e "${YELLOW}Killing program${NC}"
    kill -9 $pid > /dev/null 2>&1
    echo -e "${YELLOW}Waiting for the program to finish...${NC}"
    wait $pid
    echo -e "${YELLOW}Program finished.${NC}"

    if [ $success -eq 0 ]; then
        echo -e "${RED}Test failed for input file: $file${NC}"
        echo -e "${RED}Program output:${NC}"
        cat "$program_output_file"
        echo -e "${RED}Program error:${NC}"
        cat "$program_error_file"
        echo -e "${RED}Test output:${NC}"
        cat "$test_output_file"
        echo -e "${RED}Test error:${NC}"
        cat "$test_error_file"
        echo -e "${RED}====================================${NC}"

    else
        echo -e "${GREEN}Test passed for input file: ${file}${NC}"
    fi

    echo
done


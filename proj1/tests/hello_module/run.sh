#!/bin/bash

# Define colors for better readability
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Check if correct number of arguments is provided
if [ "$#" -ne 1 ]; then
    echo -e "${RED}Usage: $0 <code_directory>${NC}"
    exit 1
fi

# Name of the executable to build and test
EXECUTABLE_NAME="peer-time-sync"

# Get absolute path for code directory and executable
code_dir=$1
executable_path="$(realpath "$code_dir")/$EXECUTABLE_NAME"

# Validate that the code directory exists
if ! [ -d "$code_dir" ]; then
    echo -e "${RED}Error: Directory $code_dir does not exist.${NC}"
    exit 1
fi

# Build the project
echo -e "${BLUE}Building project in $code_dir...${NC}"
if ! make -C "$code_dir"; then
    echo -e "${RED}Error: Build failed.${NC}"
    exit 1
fi
echo -e "${GREEN}Build successful!${NC}"

# Check if the executable was created
if ! [ -f "$executable_path" ]; then
    echo -e "${RED}Error: Executable $EXECUTABLE_NAME not found in $code_dir.${NC}"
    exit 1
fi

# Find all shell scripts in the tests directory
echo -e "${BLUE}Finding all shell scripts in tests directory...${NC}"
tests=$(find "./tests" -type f -name "*.sh")

# Create output directory for test results
output_dir="output"
echo -e "${BLUE}Creating output directory: $output_dir${NC}"
mkdir -p "$output_dir"
if [ $? -ne 0 ]; then
    echo -e "${RED}Error: Failed to create output directory.${NC}"
    exit 1
fi

# Iterate through each test script
for test in $tests; do
    echo -e "${YELLOW}========================================${NC}"
    echo -e "${YELLOW}Running test: $test${NC}"

    # Create output directory for this specific test
    script_name=$(basename "$test")
    test_local_output_dir="$output_dir/$script_name"
    mkdir -p "$test_local_output_dir"
    output_realpath=$(realpath "$test_local_output_dir")

    # Check if the script is executable
    if [ ! -x "$test" ]; then
        echo -e "${RED}Error: Script $test is not executable.${NC}"
        continue
    fi

    # Prepare to run the test script
    script_dir=$(dirname "$test")
    script_name=$(basename "$test")
    
    # Change to the script's directory before execution
    current_dir=$(pwd)
    if ! cd "$script_dir"; then
        echo -e "${RED}Error: Failed to change to directory $script_dir.${NC}"
        exit 1
    fi

    if ! make clean; then
        echo -e "${RED}Error: Failed to clean previous builds.${NC}"
        exit 1
    fi
    
    # Run the script from its directory
    echo -e "${BLUE}Executing $script_name...${NC}"
    "./$script_name" "$executable_path" "$output_realpath"
    
    # Check the exit status of the test script
    if [ $? -ne 0 ]; then
        echo -e "${RED}Test $test failed.${NC}"
        exit 1
    else
        echo -e "${GREEN}Test $test passed successfully!${NC}"
    fi

    # Go back to the original directory
    if ! cd "$current_dir"; then
        echo -e "${RED}Error: Failed to change back to $current_dir.${NC}"
        exit 1
    fi
done

echo -e "${GREEN}All tests completed successfully!${NC}"
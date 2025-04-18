#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo -e "${RED}Usage: $0 <code_directory>${NC}"
    exit 1
fi

EXECUTABLE_NAME="peer-time-sync"

code_dir=$1
executable_path="$(realpath "$code_dir")/$EXECUTABLE_NAME"

if ! [ -d "$code_dir" ]; then
    echo -e "${RED}Error: Directory $code_dir does not exist.${NC}"
    exit 1
fi

if ! make -C "$code_dir"; then
    echo -e "${RED}Error: Build failed.${NC}"
    exit 1
fi

# Check if the executable was created
if ! [ -f "$executable_path" ]; then
    echo -e "${RED}Error: Executable $EXECUTABLE_NAME not found in $code_dir.${NC}"
    exit 1
fi

# Find all shell scripts in the tests directory
echo "Finding all shell scripts in tests directory..."
tests=$(find "./tests" -type f -name "*.sh")

output_dir="output"
mkdir -p "$output_dir"
if [ $? -ne 0 ]; then
    echo -e "${RED}Error: Failed to create output directory.${NC}"
    exit 1
fi

for test in $tests; do
    echo "Running test: $test"

    script_name=$(basename "$test")
    test_local_output_dir="$output_dir/$script_name"
    mkdir -p "$test_local_output_dir"
    output_realpath=$(realpath "$test_local_output_dir")

    # Check if the script is executable
    if [ ! -x "$test" ]; then
        echo -e "${RED}Error: Script $test is not executable.${NC}"
        continue
    fi

    # Run the test script
    echo "Running test: $test"
    script_dir=$(dirname "$test")
    script_name=$(basename "$test")
    
    # Change to the script's directory before execution
    current_dir=$(pwd)
    if ! cd "$script_dir"; then
        echo -e "${RED}Error: Failed to change to directory $script_dir.${NC}"
        exit 1
    fi
    
    # Run the script from its directory
    "./$script_name" "$executable_path" "$output_realpath"
    
    # Check the exit status of the test script
    if [ $? -ne 0 ]; then
        echo -e "${RED}Test $test failed.${NC}"
        exit 1
    fi

    # Go back to the original directory
    if ! cd "$current_dir"; then
        echo -e "${RED}Error: Failed to change back to $current_dir.${NC}"
        exit 1
    fi
done
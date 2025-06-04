#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

index="ms459531"

dir=$(pwd)
echo -e "${CYAN}Current dir: $dir${NC}"

rm -rf ./temp

# Get all subdirectories (excluding the ones starting with dot)
tests=$(find . -maxdepth 1 -type d -not -path '.')

echo -e "${BLUE}Found tests: ${NC}"
echo "$tests"
total=$(echo "$tests" | wc -l)
echo


for test in $tests; do
    # Check if the test directory exists
    if [ ! -d "$dir/$test" ]; then
        echo -e "${RED}Error: Test directory '${test}' does not exist.${NC}"
        exit 1
    fi

    # Change to the test directory
    cd "$dir/$test" || { echo -e "${RED}Error: Could not change directory to '${test}'.${NC}"; exit 1; }

    # Inform the user about the clean script
    echo -e "${CYAN}Running clean script in '${test}'...${NC}"

    echo -e "Removing the 'build' directory in '${test}'..."
    rm -rf "build/"
    rm -rf "${index}/"
    rm -rf "${index}.zip"

    # Check if the clean.sh script exists
    if [ ! -f "clean.sh" ]; then
        echo -e "${YELLOW}Warning: 'clean.sh' script not found in '${test}'.${NC}"
    else
        # Check if the script is executable
        if [ ! -x "clean.sh" ]; then
            echo -e "${RED}Error: 'clean.sh' is not executable in '${test}'.${NC}"
            exit 1
        fi

        # Run the clean script
        if ! ./clean.sh; then
            echo -e "${RED}Clean script failed in '${test}'.${NC}"
            exit 1
        fi
    fi

    echo -e "${GREEN}Clean script in '${test}' finished.${NC}"

    # Change back to the original directory
    cd "$dir" || { echo -e "${RED}Error: Could not return to the original directory.${NC}"; exit 1; }
    echo
done

echo -e "${GREEN}All clean scripts finished.${NC}"

rm -rf *__pycache__
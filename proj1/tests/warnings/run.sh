#!/bin/bash

# Define colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

if [ "$#" -ne 1 ]; then
    echo -e "${RED}Usage: $0 <code_directory>${NC}"
    exit 1
fi

code_dir=$1

echo -e "${BLUE}Running make in $code_dir...${NC}"

# Change to the code directory
cd "$code_dir" || { echo -e "${RED}Error: Could not change to directory $code_dir${NC}"; exit 1; }

# Run make, capturing both stdout and stderr
make > build_output.log 2> build_warnings.log

# Check if there were any warnings
if grep -q "warning:" build_warnings.log; then
    echo -e "${YELLOW}Compilation produced warnings:${NC}"
    grep --color=always "warning:" build_warnings.log
    exit 1
fi

# Check if make failed
if [ $? -ne 0 ]; then
    echo -e "${RED}Compilation failed.${NC}"
    cat build_output.log
    exit 1
fi

echo -e "${GREEN}Compilation successful with no warnings.${NC}"

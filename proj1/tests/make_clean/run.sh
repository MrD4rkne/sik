#!/bin/bash

# Define colors
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

if [ "$#" -ne 1 ]; then
    echo -e "${RED}Usage: $0 <code_directory>${NC}"
    exit 1
fi

code_dir=$1

# Check if the directory exists
if [ ! -d "$code_dir" ]; then
    echo -e "${RED}Error: Directory '$code_dir' does not exist${NC}"
    exit 1
fi

# List all files in the directory
echo -e "${BLUE}Files in $code_dir:${NC}"
ls -la "$code_dir"

# Remember initial files
echo -e "${YELLOW}Saving initial file list...${NC}"
initial_files=$(find "$code_dir" -type f | sort)

if ! cd "$code_dir"; then
    echo -e "${RED}Error: Could not change to directory $code_dir${NC}"
    exit 1
fi

echo -e "${YELLOW}Running make...${NC}"
if ! make; then
    echo -e "${RED}Error: make failed${NC}"
    exit 1
fi

echo -e "${YELLOW}Running make clean...${NC}"
if ! make clean; then
    echo -e "${RED}Error: make clean failed${NC}"
    exit 1
fi

cd ..

# Check files after make clean
echo -e "${YELLOW}Checking files after make clean...${NC}"
current_files=$(find "$code_dir" -type f | sort)

# Compare files
echo -e "${YELLOW}Comparing files before and after...${NC}"
if diff <(echo "$initial_files") <(echo "$current_files") > /dev/null; then
    echo -e "${GREEN}✓ No files were added or removed during make/clean process${NC}"
else
    echo -e "${RED}✗ File differences detected after make clean!${NC}"
    echo -e "${RED}Files added or removed:${NC}"
    diff <(echo "$initial_files") <(echo "$current_files")
    exit 1
fi
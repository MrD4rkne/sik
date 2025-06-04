#!/bin/bash

# Define colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color
BLUE='\033[0;34m'
CYAN='\033[0;36m'

# Src directory
SRC_DIR="./src"

# Check if the script is run from the correct directory

# Count lines in source directory
count_lines() {
    echo -e "${CYAN}Counting lines in source directory...${NC}"
    
    # Count total lines in .c and .h files
    total_lines=$(find "$SRC_DIR" -type f \( -name "*.c" -o -name "*.h" -o -name "*.cpp" -o -name "*.hpp" \) -exec wc -l {} \; | awk '{total += $1} END {print total}')
    
    # Count lines by file type
    c_lines=$(find "$SRC_DIR" -type f -name "*.c" -exec wc -l {} \; | awk '{total += $1} END {print total}')
    h_lines=$(find "$SRC_DIR" -type f -name "*.h" -exec wc -l {} \; | awk '{total += $1} END {print total}')
    cpp_lines=$(find "$SRC_DIR" -type f -name "*.cpp" -exec wc -l {} \; | awk '{total += $1} END {print total}')
    hpp_lines=$(find "$SRC_DIR" -type f -name "*.hpp" -exec wc -l {} \; | awk '{total += $1} END {print total}')
    
    # Print results
    echo -e "${GREEN}Total lines of code: ${total_lines}${NC}"
    echo -e "${BLUE}C files: ${c_lines:-0} lines${NC}"
    echo -e "${BLUE}H files: ${h_lines:-0} lines${NC}"
    echo -e "${BLUE}CPP files: ${cpp_lines:-0} lines${NC}"
    echo -e "${BLUE}HPP files: ${hpp_lines:-0} lines${NC}"
}

total=0
total_lines=$(find . -type f -exec wc -l {} + | awk '{total += $1} END {print total}')
echo -e "${GREEN}Total lines of code in the project: ${total_lines}${NC}"

# Call the function
count_lines
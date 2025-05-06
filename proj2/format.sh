#!/bin/bash

# Define color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

CODE_DIR="./src"

# Check if the source directory exists
if [ ! -d "$CODE_DIR" ]; then
    echo -e "${RED}Error: Source directory '$CODE_DIR' does not exist.${NC}"
    exit 1
fi
# Check if the source directory is empty
if [ -z "$(ls -A $CODE_DIR)" ]; then
    echo -e "${RED}Error: Source directory '$CODE_DIR' is empty.${NC}"
    exit 1
fi

# Format all C/C++ files using clang-format
echo -e "${YELLOW}Formatting C/C++ files with clang-format...${NC}"

# Find all C/C++ files and format them
find "$CODE_DIR" -type f \( -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) -print0 | 
while IFS= read -r -d '' file; do
    echo -e "${CYAN}Formatting: ${file}${NC}"
    clang-format -i "$file"
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}Successfully formatted ${file}${NC}"
    else
        echo -e "${RED}Failed to format ${file}${NC}"
    fi
done

echo -e "${GREEN}All C/C++ files have been formatted.${NC}"
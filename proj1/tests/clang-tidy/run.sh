#!/bin/bash

# Define color codes
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

if [ "$#" -ne 1 ]; then
    echo -e "${RED}Usage: $0 <code_directory>${NC}"
    exit 1
fi

code_dir=$1

extensions=(
    ".h"
    ".cpp"
)

total_files=0
passed_files=0

make_flags="-std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wcast-qual -Wformat=2 -O2"

echo -e "\n${BLUE}Running clang-tidy with the following flags:${NC}\n${YELLOW}$make_flags${NC}\n"

for ext in "${extensions[@]}"; do
    echo -e "${BLUE}Running clang-tidy on all $ext files in $code_dir${NC}"
    while IFS= read -r file; do
        total_files=$((total_files + 1))
        echo -e "\n${BLUE}Running clang-tidy on ${YELLOW}$file${NC}"
        clang-tidy "$file" -- $make_flags -x c++ -I"$code_dir"
        if [ $? -eq 0 ]; then
            passed_files=$((passed_files + 1))
        fi
    done < <(find "$code_dir" -type f -name "*$ext")
done

echo -e "\n${BLUE}Total files checked: ${NC}$total_files"
echo -e "${BLUE}Files passed: ${GREEN}$passed_files${NC}"
echo -e "${BLUE}Files failed: ${RED}$((total_files - passed_files))${NC}"
echo -e "${BLUE}Pass rate: ${GREEN}$((passed_files * 100 / total_files))%${NC}"
echo -e "${BLUE}Fail rate: ${RED}$(((total_files - passed_files) * 100 / total_files))%${NC}"
echo -e "${BLUE}clang-tidy check completed.${NC}"

if [ $((total_files - passed_files)) -gt 0 ]; then
    echo -e "${RED}Some files failed the clang-tidy check. Please review the output above.${NC}"
    exit 1
else
    echo -e "${GREEN}All files passed the clang-tidy check.${NC}"
    exit 0
fi
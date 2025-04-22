#!/bin/bash

# Define colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

if [ "$#" -ne 1 ]; then
    echo -e "${RED}Usage: $0 <code_directory>${NC}"
    exit 1
fi

code_dir=$1

patterns=(
    "*.h"
    "*.cpp"
    "makefile"
    "Makefile"
)

# Check if all files in the directory match at least one pattern
mismatched_files=()

while IFS= read -r -d '' file; do
    # Get relative path from code_dir
    rel_file=${file#"$code_dir/"}
    
    # Skip if it's a directory
    if [ -d "$file" ]; then
        echo -e "${RED}Directories are not allowed: ${YELLOW}$rel_file${NC}"
        exit 1
    fi
    
    match_found=false
    for pattern in "${patterns[@]}"; do
        if [[ $rel_file == $pattern ]]; then
            match_found=true
            break
        fi
    done
    
    if [ "$match_found" = false ]; then
        mismatched_files+=("$rel_file")
    fi
done < <(find "$code_dir" -type f -print0)

if [ ${#mismatched_files[@]} -gt 0 ]; then
    echo -e "${RED}Error: The following files do not match any allowed pattern:${NC}"
    for file in "${mismatched_files[@]}"; do
        echo -e "  - ${YELLOW}$file${NC}"
    done
    exit 1
fi

echo -e "${GREEN}All files match the allowed patterns.${NC}"
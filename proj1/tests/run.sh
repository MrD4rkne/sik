#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

SRC_DIR="../src"

index="ms459531"

# Run cleanup script
echo -e "${CYAN}Running cleanup script...${NC}"
if ! ./clean.sh; then
    echo -e "${RED}Cleanup script failed.${NC}"
    exit 1
fi
echo -e "${GREEN}Cleanup script finished.${NC}"

# Run prepare script
CURRENT_DIR=$(pwd)
if ! cd ../; then
    echo -e "${RED}Error: Could not change directory.${NC}"
    exit 1
fi

echo -e "${CYAN}Running prepare script...${NC}"
if ! ./prepare.sh; then
    echo -e "${RED}Prepare script failed.${NC}"
    exit 1
fi
echo -e "${GREEN}Prepare script finished.${NC}"

zipName="${index}.zip"
zipPath=$(realpath "$zipName")
echo -e "${YELLOW}Full path to zip file: $zipPath${NC}"

if ! cd "$CURRENT_DIR"; then
    echo -e "${RED}Error: Could not change directory.${NC}"
    exit 1
fi

# Get all subdirectories (excluding the ones starting with dot)
tests=$(find . -maxdepth 1 -type d -not -path '.' -not -path '*/\.*')

echo -e "${BLUE}Found tests: ${NC}"
echo "$tests"
total=$(echo "$tests" | wc -l)
echo

success=0
unavailable=0

for test in $tests; do
    echo
    echo "### Running test $test ###"
    echo

    if ! cd "$test"; then
        echo -e "${RED}Error: changing directory to $test failed${NC}"
        unavailable=$((unavailable+1))
    fi
    
    # Delete folder if it exists
    if [ -d "$index" ]; then
        echo -e "${YELLOW}Deleting existing folder $index...${NC}"
        rm -rf "$index"
    fi

    if ! unzip -q "$zipPath" ; then
        echo -e "${RED}Error: unzipping $zipPath failed${NC}"
        unavailable=$((unavailable+1))
        continue
    fi

    if ! ./run.sh "$index"; then
        echo -e "${RED}Error: test $test failed${NC}"
    else
        success=$((success+1))
        echo -e "${GREEN}Test $test successful.${NC}"
    fi

    if ! cd "$CURRENT_DIR"; then
        echo -e "${RED}Error: changing directory to original failed${NC}"
        exit 1
    fi

    echo
done

echo -e "${BLUE}Tests summary:${NC}"

available=$((total-unavailable))
failed=$((total-success-unavailable))

if [ $available -eq 0 ]; then
    echo -e "${YELLOW}Warning: No tests available.${NC}"
    exit 1
fi

echo -e "${GREEN}Successful tests: $success${NC}"
echo -e "${RED}Failed tests: $failed${NC}"
echo -e "${BLUE}Total available tests: $total${NC}"

if [ $unavailable -gt 0 ]; then
    echo -e "${YELLOW}Unavailable tests: $unavailable${NC}"
    echo -e "${YELLOW}Please check the error messages above.${NC}"
    exit 1
fi

if [ $failed -gt 0 ]; then
    exit 1
fi
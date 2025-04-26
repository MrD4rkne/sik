#!/bin/bash

# Define color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Set variables
USER_ID="ms459531" # Replace with your initials and student number
OUTPUT_ZIP="${USER_ID}.zip"
SRC_DIR="./src"
OUTPUT_DIR="${USER_ID}"

# Check if the source directory exists
if [ ! -d "$SRC_DIR" ]; then
    echo -e "${RED}Error: Source directory '$SRC_DIR' does not exist.${NC}"
    exit 1
fi

# Run 'make clean' in the source directory
echo -e "${CYAN}Running 'make clean' in the source directory...${NC}"
if [ -f "$SRC_DIR/makefile" ]; then
    make -C "$SRC_DIR" clean
else
    echo -e "${RED}Error: No Makefile found in the source directory.${NC}"
    exit 1
fi

# Prepare output directory
echo -e "${CYAN}Preparing submission directory...${NC}"
rm -rf "$OUTPUT_DIR"  # Remove existing folder if it exists
mkdir -p "$OUTPUT_DIR"

# Copy source files into appropriate folders
echo -e "${YELLOW}Copying source files...${NC}"
cp -r "$SRC_DIR/"* "$OUTPUT_DIR/" 

# Create the zip archive
echo -e "${CYAN}Creating zip archive '$OUTPUT_ZIP'...${NC}"
rm -f "$OUTPUT_ZIP"  # Remove existing zip file if it exists
if ! (cd "$OUTPUT_DIR" && zip -r "../$OUTPUT_ZIP" . > /dev/null); then
    echo -e "${RED}Error when zipping!${NC}"
    exit 1
fi

# Clean up temporary folder
echo -e "${YELLOW}Cleaning up temporary files...${NC}"
rm -rf "$OUTPUT_DIR"

# Confirmation message
echo -e "${GREEN}Submission archive '$OUTPUT_ZIP' created successfully!${NC}"

# Check if the zip file contains all files directly

rm -rf temp

mkdir temp

echo -e "${CYAN}Checking zip file contents...${NC}"
cp "$OUTPUT_ZIP" temp/

cd temp

zip_name=$(basename "$OUTPUT_ZIP")
unzip "$zip_name" > /dev/null

if find . -type d ! -path . | grep -q .; then
    echo -e "${RED}Error: The zip archive contains directories.${NC}"
    exit 1
else
    echo -e "${GREEN}Zip archive contains only files (no directories).${NC}"
fi

if ! make ; then
    echo -e "${RED}Error: Make failed!${NC}"
    exit 1
fi

cd ..

rm -rf temp

echo -e "${GREEN}All checks passed successfully!${NC}"
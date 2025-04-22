#!/bin/bash

# Define color codes for output formatting
RED='\033[0;31m'    # Red for errors
GREEN='\033[0;32m'  # Green for success
YELLOW='\033[1;33m' # Yellow for warnings
NC='\033[0m'        # No Color

make clean
rm -rf ./*.txt ./*.err ./*.out
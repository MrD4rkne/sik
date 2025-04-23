#!/bin/bash

# Define colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# Check if port argument is provided
if [ "$#" -ne 1 ]; then
    echo -e "${RED}Usage: $0 <port_number>${NC}"
    exit 1
fi

PORT=$1

# Find PIDs of processes using the specified port using ss
PIDS=$(ss -tulnp | grep ":$PORT" | awk '{print $7}' | sed 's/users:((.*,pid=\([0-9]*\).*/\1/g' | sort -u)

if [ -z "$PIDS" ]; then
    echo -e "${YELLOW}No process found using port $PORT${NC}"
    exit 0
fi

# Kill the processes
for PID in $PIDS; do
    echo -e "${RED}Killing process $PID using port $PORT${NC}"
    kill -9 $PID
done

echo -e "${GREEN}All processes using port $PORT have been terminated${NC}"
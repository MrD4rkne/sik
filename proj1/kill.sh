#!/bin/bash

# Check if port argument is provided
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <port_number>"
    exit 1
fi

PORT=$1

# Find PIDs of processes using the specified port using ss
PIDS=$(sudo ss -tulnp | grep ":$PORT" | awk '{print $7}' | sed 's/users:((.*,pid=\([0-9]*\).*/\1/g' | sort -u)

if [ -z "$PIDS" ]; then
    echo "No process found using port $PORT"
    exit 0
fi

# Kill the processes
for PID in $PIDS; do
    echo "Killing process $PID using port $PORT"
    sudo kill -9 $PID
done

echo "All processes using port $PORT have been terminated"
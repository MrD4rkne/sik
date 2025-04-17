#!/bin/bash

# Check if the correct number of arguments is provided
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <IP> <PORT>"
    exit 1
fi

IP=$1
PORT=$2

# Define the GET_TIME message (31)
GET_TIME_MSG=$'\x1F'

# Send the GET_TIME message to the specified IP and port
RESPONSE=$(echo -n "$GET_TIME_MSG" | nc -u -w 1 "$IP" "$PORT")

# Check if a response was received
if [ -z "$RESPONSE" ]; then
    echo "No response received from $IP:$PORT"
    exit 1
fi

# Parse the response (assuming it starts with message type 32)
if [[ ${RESPONSE:0:1} == $'\x20' ]]; then
    # Extract synchronization level and timestamp
    SYNC_LEVEL=$(printf "%d" "'${RESPONSE:1:1}")
    # Extract timestamp from bytes 2-10 and reverse the bytes
    TIMESTAMP=0
    for i in {8..2}; do
        byte=$(printf "%d" "'${RESPONSE:$i-1:1}")
        TIMESTAMP=$((TIMESTAMP * 256 + byte))
    done
    
    echo "Synchronization Level: $SYNC_LEVEL"
    echo "Timestamp: $TIMESTAMP"
else
    echo "Invalid response received"
    exit 1
fi
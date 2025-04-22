#!/bin/bash

LEADER_MSG_TYPE_HEX="\x15"

LEADER_SET="\x00"
LEADER_UNSET="\xff"

if [ $# -ne 3 ]; then
    echo "Usage: $0 <ip> <port> <set_leader>"
    exit 1
fi

IP=$1
PORT=$2
SET_LEADER=$3

CONTENT=""

if [ "$SET_LEADER" == "set" ]; then
    CONTENT=$LEADER_SET
elif [ "$SET_LEADER" == "unset" ]; then
    CONTENT=$LEADER_UNSET
else
    echo "Invalid set_leader value. Use 'set' or 'unset'."
    exit 1
fi

# Send the message to the specified IP and port

MSG="${LEADER_MSG_TYPE_HEX}${CONTENT}"

echo "Sending message: $MSG to $IP:$PORT"

printf "${LEADER_MSG_TYPE_HEX}${CONTENT}" | nc -u -w 1 $IP $PORT
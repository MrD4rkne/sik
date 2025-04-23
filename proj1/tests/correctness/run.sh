#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <code_directory>"
    exit 1
fi

./run_me.sh $1 ../test_runner.py
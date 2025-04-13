#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <code_directory>"
    exit 1
fi

code_dir=$1

#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <code_directory>"
    exit 1
fi

code_dir=$1

extensions=(
    ".cpp"
    ".h"
)

echo -e "Running clang-tidy on all .cpp and .h files in $code_dir"

for ext in "${extensions[@]}"; do
    echo -e "Running clang-tidy on all $ext files in $code_dir"
    find "$code_dir" -type f -name "*$ext" -exec clang-tidy {} -- -std=c++20 \;
done
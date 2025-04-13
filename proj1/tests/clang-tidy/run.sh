#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <code_directory>"
    exit 1
fi

code_dir=$1

extensions=(
    ".h"
    ".cpp"
)

total_files=0
passed_files=0

# Extract compilation flags from the Makefile
# Extract compilation flags from the Makefile
makefile_path="$code_dir/makefile"

if [ ! -f "$makefile_path" ]; then
    echo "Error: Makefile not found at $makefile_path"
    exit 1
fi

make_flags="-std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wcast-qual -Wformat=2 -O2"

echo -e "\nRunning clang-tidy with the following flags:\n$make_flags\n"

for ext in "${extensions[@]}"; do
    echo -e "Running clang-tidy on all $ext files in $code_dir"
    while IFS= read -r file; do
        total_files=$((total_files + 1))
        echo -e "\nRunning clang-tidy on $file"
        clang-tidy "$file" -- $make_flags -x c++ -I"$code_dir"
        if [ $? -eq 0 ]; then
            passed_files=$((passed_files + 1))
        fi
    done < <(find "$code_dir" -type f -name "*$ext")
done

echo -e "\nTotal files checked: $total_files"
echo "Files passed: $passed_files"
echo "Files failed: $((total_files - passed_files))"
echo "Pass rate: $((passed_files * 100 / total_files))%"
echo "Fail rate: $(((total_files - passed_files) * 100 / total_files))%"
echo "clang-tidy check completed."

if [ $((total_files - passed_files)) -gt 0 ]; then
    echo "Some files failed the clang-tidy check. Please review the output above."
    exit 1
else
    echo "All files passed the clang-tidy check."
    exit 0
fi
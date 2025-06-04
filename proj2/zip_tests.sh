#!/bin/bash

index="ms459531"
replaced_index="ab12345"

temp_dir="zip_tests"

function usage() {
    echo "Usage: $0 [no_delete]"
    echo "If 'no_delete' is provided, the script will delete everything with kuba"
    exit 1
}

if [ $# -ge 2 ]; then
    echo "Error: Too many arguments."
    usage
fi

delete_kuba_files=true

if [ $# -eq 1 ]; then
    if [ "$1" = "no_delete" ]; then
        delete_kuba_files=false
    else
        echo "Error: Invalid argument '$1'."
        usage
    fi
fi

if ! rm -rf "$temp_dir"; then
    echo "Error: Could not remove existing directory '$temp_dir'."
    exit 1
fi

if ! mkdir "$temp_dir"; then
    echo "Error: Could not create directory '$temp_dir'."
    exit 1
fi

tests_dir="tests"

mkdir "$temp_dir/$tests_dir"

if ! $tests_dir/clean.sh; then
    echo "Error: Cleanup script failed."
    exit 1
fi

if ! cp -r "$tests_dir"/* "$temp_dir/$tests_dir/"; then
    echo "Error: Could not copy test files."
    exit 1
fi

if ! cp "prepare.sh" "$temp_dir/"; then
    echo "Error: Could not copy prepare.sh."
    exit 1
fi

if ! cp "TESTS_README.MD" "$temp_dir/README.MD"; then
    echo "Error: Could not copy TESTS_README.MD."
    exit 1
fi

# Replace my initials with the given initials
if ! find "$temp_dir" -type f -exec sed -i "s/$index/$replaced_index/g" {} \;; then
    echo "Error: Could not replace initials in files."
    exit 1
fi

# Remove any folder with kuba in the name
if [ "$delete_kuba_files" = true ]; then
    echo "Removing directories with 'kuba' in the name..."
    find "$temp_dir" -type d -name "*kuba*" -exec rm -rf {} + 2>/dev/null
else
    echo "Skipping removal of directories with 'kuba' in the name."
fi

if [ $? -ne 0 ]; then
    echo "Error: Could not remove directories with 'kuba' in the name."
    exit 1
fi

tests_zip_name="tests.zip"

if ! zip -r "$tests_zip_name" "$temp_dir"; then
    echo "Error: Could not create zip archive."
    exit 1
fi

if ! rm -rf "$temp_dir"; then
    echo "Error: Could not remove temporary directory."
    exit 1
fi
echo "Zip archive '$tests_zip_name' created successfully!"
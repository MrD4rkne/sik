#!/bin/bash

# This script generates a PDF solve.

# Check if pandoc is installed
if ! command -v pandoc &> /dev/null; then
    echo "Pandoc is not installed. Please install it to generate the documentation."
    exit 1
fi

if ! make ; then
    echo "Make command failed. Please check the Makefile for errors."
    exit 1
fi

echo "PDF generated successfully."
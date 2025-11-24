#!/bin/bash

# Check if spim is installed
if ! command -v spim &> /dev/null; then
    echo "Error: spim is not installed or not in PATH"
    echo "Please install spim: sudo apt-get install spim"
    exit 1
fi

TEST_DIR=./test

# Check if test directory exists
if [ ! -d "$TEST_DIR" ]; then
    echo "Error: Test directory '$TEST_DIR' not found"
    exit 1
fi

echo "============================================"
echo "Running MIPS Assembly files with SPIM"
echo "============================================"

# Run all .s files in the test directory
find ${TEST_DIR} -name "*.s" -type f | sort -V | while read -r asm_file; do
    if [ -f "$asm_file" ]; then
        echo ""
        echo "--------------------------------------------"
        echo "Running: ${asm_file}"
        echo "--------------------------------------------"
        
        # Check if there's a corresponding .input file
        base=$(basename "${asm_file}" .s)
        input_file="${TEST_DIR}/${base}.input"
        
        if [ -f "$input_file" ]; then
            echo "Using input file: ${input_file}"
            spim -file "${asm_file}" < "${input_file}"
        else
            spim -file "${asm_file}"
        fi
        
        echo ""
    fi
done

echo "============================================"
echo "All SPIM executions completed!"
echo "============================================"

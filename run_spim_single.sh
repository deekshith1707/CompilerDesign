#!/bin/bash

# Script to run individual MIPS assembly files with SPIM
# Usage: ./run_spim_single.sh <assembly_file> [input_file]

# Check if spim is installed
if ! command -v spim &> /dev/null; then
    echo "Error: spim is not installed or not in PATH"
    echo "Please install spim: sudo apt-get install spim"
    exit 1
fi


# Default test directory for assembly/input files
TEST_DIR=./test

# Check if assembly file is provided
if [ $# -lt 1 ]; then
    echo "Usage: $0 <assembly_file> [input_file]"
    echo "Example: $0 ${TEST_DIR}/1_arithmetic_logical.s"
    echo "Example: $0 ${TEST_DIR}/10_printf_scanf.s ${TEST_DIR}/10_printf_scanf.input"
    echo "Or simply: $0 10_printf_scanf.s"
    exit 1
fi

ASM_FILE=$1
INPUT_FILE=$2

# If user provided a bare filename (no slash), try to resolve it inside the default test dir
if [[ "$ASM_FILE" != */* ]]; then
    # Add .s if missing
    if [[ "$ASM_FILE" != *.s ]]; then
        candidate="${ASM_FILE}.s"
    else
        candidate="$ASM_FILE"
    fi
    if [ -f "$candidate" ]; then
        ASM_FILE="$candidate"
    elif [ -f "${TEST_DIR}/${candidate}" ]; then
        ASM_FILE="${TEST_DIR}/${candidate}"
    fi
fi

# If input file was provided as bare name, resolve it to test dir as well
if [ -n "$INPUT_FILE" ] && [[ "$INPUT_FILE" != */* ]]; then
    if [ -f "$INPUT_FILE" ]; then
        : # keep as-is
    elif [ -f "${TEST_DIR}/${INPUT_FILE}" ]; then
        INPUT_FILE="${TEST_DIR}/${INPUT_FILE}"
    elif [[ "$INPUT_FILE" != *.input ]] && [ -f "${TEST_DIR}/${INPUT_FILE}.input" ]; then
        INPUT_FILE="${TEST_DIR}/${INPUT_FILE}.input"
    fi
fi

# Check if assembly file exists
if [ ! -f "$ASM_FILE" ]; then
    echo "Error: Assembly file '$ASM_FILE' not found"
    exit 1
fi

echo "============================================"
echo "Running: ${ASM_FILE}"
echo "============================================"

# Run with or without input file
if [ -n "$INPUT_FILE" ]; then
    if [ ! -f "$INPUT_FILE" ]; then
        echo "Error: Input file '$INPUT_FILE' not found"
        exit 1
    fi
    echo "Using input file: ${INPUT_FILE}"
    spim -file "${ASM_FILE}" < "${INPUT_FILE}"
else
    # Check if there's a corresponding .input file
    base=$(basename "${ASM_FILE}" .s)
    dir=$(dirname "${ASM_FILE}")
    auto_input="${dir}/${base}.input"
    
    if [ -f "$auto_input" ]; then
        echo "Auto-detected input file: ${auto_input}"
        spim -file "${ASM_FILE}" < "${auto_input}"
    else
        spim -file "${ASM_FILE}"
    fi
fi

echo ""
echo "============================================"
echo "Execution completed"
echo "============================================"

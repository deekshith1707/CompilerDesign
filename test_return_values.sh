#!/bin/bash

# Script to test return values of testreturn cases using SPIM
# Test cases should return 0 on success, non-zero on failure

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Timeout for each test (in seconds)
TIMEOUT=10

# Ensure compiler is built
make clean 2>/dev/null
make 2>/dev/null

if [ ! -f ./ir_generator ]; then
    echo -e "${RED}Compilation failed. Exiting.${NC}"
    exit 1
fi

TEST_DIR=./testreturn

# Check if testreturn directory exists
if [ ! -d "$TEST_DIR" ]; then
    echo -e "${RED}Error: ${TEST_DIR} directory not found${NC}"
    exit 1
fi

# Count test files
total_tests=$(ls -1 ${TEST_DIR}/*.txt 2>/dev/null | wc -l)
if [ $total_tests -eq 0 ]; then
    echo -e "${RED}No test files found in ${TEST_DIR}${NC}"
    exit 1
fi

echo -e "${BLUE}============================================${NC}"
echo -e "${BLUE}Testing Return Values with SPIM${NC}"
echo -e "${BLUE}============================================${NC}"
echo ""

# Initialize counters
passed=0
failed=0
errors=0

# Create arrays to store results
declare -a passed_tests
declare -a failed_tests
declare -a error_tests

# Process each test file
for test_file in $(ls -1v ${TEST_DIR}/*.txt); do
    base=$(basename "${test_file}" .txt)
    echo -e "${YELLOW}--------------------------------------------${NC}"
    echo -e "${YELLOW}Testing: ${base}${NC}"
    echo -e "${YELLOW}--------------------------------------------${NC}"
    
    # Generate IR and MIPS assembly
    echo "Generating MIPS assembly..."
    ./ir_generator "${test_file}" --generate-mips 2>&1
    
    if [ $? -ne 0 ]; then
        echo -e "${RED}✗ Code generation failed${NC}"
        ((errors++))
        error_tests+=("$base (code generation failed)")
        echo ""
        continue
    fi
    
    # Copy output.s to test directory
    if [ -f output.s ]; then
        cp output.s "${TEST_DIR}/${base}.s"
        echo "Created: ${TEST_DIR}/${base}.s"
    else
        echo -e "${RED}✗ Output file not generated${NC}"
        ((errors++))
        error_tests+=("$base (no output file)")
        echo ""
        continue
    fi
    
    # Run with SPIM and capture return value
    echo "Running with SPIM (timeout: ${TIMEOUT}s)..."
    
    # Run SPIM with timeout and capture the output
    # The return value is the last number printed by the program
    spim_output=$(timeout ${TIMEOUT} spim -file "${TEST_DIR}/${base}.s" 2>&1)
    spim_exit=$?
    
    # Check if timeout occurred
    if [ $spim_exit -eq 124 ]; then
        echo -e "${RED}✗ TIMEOUT (exceeded ${TIMEOUT}s)${NC}"
        ((failed++))
        failed_tests+=("$base (timeout)")
    # Check for runtime errors
    elif echo "$spim_output" | grep -qi "exception occurred\|segmentation fault\|bus error"; then
        echo -e "${RED}✗ SPIM runtime error${NC}"
        echo "SPIM output:"
        echo "$spim_output" | tail -10
        ((failed++))
        failed_tests+=("$base (runtime error)")
    else
        # Extract the return value (last line that's a number)
        return_val=$(echo "$spim_output" | grep -E '^[0-9]+$' | tail -1)
        
        if [ -z "$return_val" ]; then
            # No numeric output found - might be an error
            echo -e "${YELLOW}⚠ Could not extract return value${NC}"
            echo "SPIM output:"
            echo "$spim_output" | tail -10
            ((errors++))
            error_tests+=("$base (no return value)")
        elif [ "$return_val" -eq 0 ]; then
            echo -e "${GREEN}✓ Test PASSED (return value: 0)${NC}"
            ((passed++))
            passed_tests+=("$base")
        else
            echo -e "${RED}✗ Test FAILED (return value: $return_val, expected: 0)${NC}"
            echo "  → Test failed at or near line $return_val"
            ((failed++))
            failed_tests+=("$base (returned $return_val)")
        fi
    fi
    
    echo ""
done

# Print summary
echo -e "${BLUE}============================================${NC}"
echo -e "${BLUE}TEST SUMMARY${NC}"
echo -e "${BLUE}============================================${NC}"
echo -e "Total Tests:  ${total_tests}"
echo -e "${GREEN}Passed:       ${passed}${NC}"
echo -e "${RED}Failed:       ${failed}${NC}"
echo -e "${YELLOW}Errors:       ${errors}${NC}"
echo ""

if [ ${#passed_tests[@]} -gt 0 ]; then
    echo -e "${GREEN}Passed Tests:${NC}"
    for test in "${passed_tests[@]}"; do
        echo -e "  ${GREEN}✓${NC} $test"
    done
    echo ""
fi

if [ ${#failed_tests[@]} -gt 0 ]; then
    echo -e "${RED}Failed Tests:${NC}"
    for test in "${failed_tests[@]}"; do
        echo -e "  ${RED}✗${NC} $test"
    done
    echo ""
fi

if [ ${#error_tests[@]} -gt 0 ]; then
    echo -e "${YELLOW}Error Tests:${NC}"
    for test in "${error_tests[@]}"; do
        echo -e "  ${YELLOW}!${NC} $test"
    done
    echo ""
fi

# Exit with appropriate code
if [ $failed -gt 0 ] || [ $errors -gt 0 ]; then
    exit 1
else
    exit 0
fi

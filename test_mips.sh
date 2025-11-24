
#!/bin/bash

# MIPS Code Testing Script for SPIM Simulator
# Tests all generated MIPS assembly files and reports results

echo "============================================"
echo "  MIPS Assembly Testing with SPIM"
echo "  Date: $(date)"                                                                                                                                                                                                                                                                                                                  
echo "============================================"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test directory
TEST_DIR="./test"

# Results tracking
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
TIMEOUT_TESTS=0

# Results file
RESULTS_FILE="test_results.md"

# Initialize results file
cat > "$RESULTS_FILE" << 'EOF'
# MIPS Assembly Test Results

**Test Date:** $(date)
**Simulator:** SPIM

---

## Test Summary

EOF

echo "Starting MIPS tests..."
echo ""

# Function to run a single test
run_test() {
    local test_file=$1
    local base_name=$(basename "$test_file" .s)
    
    echo "--------------------------------------------"
    echo "Testing: $base_name"
    echo "--------------------------------------------"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    # Check if there's an input file for this test
    local input_file="${test_file%.s}.input"
    
    # Run SPIM with timeout (10 seconds)
    # Capture both stdout and stderr
    if [ -f "$input_file" ]; then
        echo "  Using input from: $input_file"
        timeout 10s spim -file "$test_file" < "$input_file" > "${test_file%.s}.output" 2>&1
    else
        timeout 10s spim -file "$test_file" > "${test_file%.s}.output" 2>&1
    fi
    EXIT_CODE=$?
    
    if [ $EXIT_CODE -eq 124 ]; then
        # Timeout occurred
        echo -e "${YELLOW}⏱  TIMEOUT${NC}: Test exceeded 10 seconds"
        TIMEOUT_TESTS=$((TIMEOUT_TESTS + 1))
        echo "| $base_name | ⏱ TIMEOUT | Test exceeded 10 seconds | " >> "$RESULTS_FILE"
    elif [ $EXIT_CODE -eq 0 ]; then
        # Check for common error patterns in output (excluding SPIM's exceptions.s load message)
        if grep -q -i "exception occurred\|unaligned address\|memory address out of bounds\|undefined instruction" "${test_file%.s}.output"; then
            echo -e "${RED}✗ FAILED${NC}: Runtime error detected"
            FAILED_TESTS=$((FAILED_TESTS + 1))
            echo "| $base_name | ❌ FAILED | Runtime error detected | " >> "$RESULTS_FILE"
        else
            echo -e "${GREEN}✓ PASSED${NC}: Executed successfully"
            PASSED_TESTS=$((PASSED_TESTS + 1))
            echo "| $base_name | ✅ PASSED | Executed successfully | " >> "$RESULTS_FILE"
            
            # Show first few lines of output
            echo "Output (first 10 lines):"
            head -10 "${test_file%.s}.output" | sed 's/^/  /'
        fi
    else
        echo -e "${RED}✗ FAILED${NC}: Exit code $EXIT_CODE"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        echo "| $base_name | ❌ FAILED | Exit code: $EXIT_CODE | " >> "$RESULTS_FILE"
    fi
    
    echo ""
}

# Add table header to results file
{
    echo ""
    echo "## Detailed Results"
    echo ""
    echo "| Test Case | Status | Notes |"
    echo "|-----------|--------|-------|"
} >> "$RESULTS_FILE"

# Run all tests
for test_file in $(ls -1v ${TEST_DIR}/*.s); do
    run_test "$test_file"
done

# Print summary
echo "============================================"
echo "  TEST SUMMARY"
echo "============================================"
echo -e "Total Tests:   $TOTAL_TESTS"
echo -e "${GREEN}Passed:        $PASSED_TESTS${NC}"
echo -e "${RED}Failed:        $FAILED_TESTS${NC}"
echo -e "${YELLOW}Timeout:       $TIMEOUT_TESTS${NC}"
echo ""
echo "Pass Rate:     $(awk "BEGIN {printf \"%.1f%%\", ($PASSED_TESTS/$TOTAL_TESTS)*100}")"
echo ""
echo "Detailed results saved to: $RESULTS_FILE"
echo "Individual outputs saved to: test/*.output"
echo "============================================"

# Add summary to results file
{
    echo ""
    echo "---"
    echo ""
    echo "## Summary Statistics"
    echo ""
    echo "- **Total Tests:** $TOTAL_TESTS"
    echo "- **Passed:** $PASSED_TESTS ($(awk "BEGIN {printf \"%.1f%%\", ($PASSED_TESTS/$TOTAL_TESTS)*100}"))"
    echo "- **Failed:** $FAILED_TESTS"
    echo "- **Timeout:** $TIMEOUT_TESTS"
    echo ""
    echo "---"
    echo ""
    echo "## Notes"
    echo ""
    echo "- Tests run with 10-second timeout"
    echo "- SPIM version: $(spim -version 2>&1 | head -1 || echo 'Unknown')"
    echo "- All test outputs saved to test/*.output files"
} >> "$RESULTS_FILE"

# Exit with appropriate code
if [ $FAILED_TESTS -eq 0 ] && [ $TIMEOUT_TESTS -eq 0 ]; then
    exit 0
else
    exit 1
fi

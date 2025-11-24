#!/bin/bash

make clean 2>/dev/null
make 2>/dev/null

if [ ! -f ./ir_generator ]; then
    echo "Compilation failed. Exiting."
    exit 1
fi

TEST_DIR=./test

# Create .txt copies of .c files if they don't exist
for c_file in ${TEST_DIR}/*.c; do
    if [ -f "$c_file" ]; then
        txt_file="${c_file%.c}.txt"
        if [ ! -f "$txt_file" ]; then
            cp "$c_file" "$txt_file"
            echo "Converted $c_file to $txt_file"
        fi
    fi
done

echo "============================================"
echo "Generating IR and MIPS Assembly files..."
echo "============================================"

# Generate both IR and MIPS assembly files for all test cases
find "${TEST_DIR}" -name "*.txt" -type f | sort -V | while IFS= read -r test_file; do
    echo "--------------------------------------------"
    echo "Processing: ${test_file}"
    echo "--------------------------------------------"
    ./ir_generator "${test_file}" --generate-mips
    echo ""
done

echo ""
echo "============================================"
echo "All .ir and .s files generated successfully!"
echo "============================================"

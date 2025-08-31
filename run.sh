#!/bin/bash

make clean
make

if [ ! -f ./syntax_analyzer ]; then
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

# Run syntax analyzer on all .txt files
for test_file in $(ls -1v ${TEST_DIR}/*.txt); do
    echo "--------------------------------------------"
    echo "Running syntax analyzer on: ${test_file}"
    echo "--------------------------------------------"
    ./syntax_analyzer "${test_file}"
    echo ""
done
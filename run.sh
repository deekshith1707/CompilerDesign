#!/bin/bash

make clean
make

if [ ! -f ./lexer ]; then
    echo "Compilation failed. Exiting."
    exit 1
fi

TEST_DIR=./test

for c_file in ${TEST_DIR}/*.c; do
    if [ -f "$c_file" ]; then
        txt_file="${c_file%.c}.txt"
        if [ ! -f "$txt_file" ]; then
            cp "$c_file" "$txt_file"
            echo "Converted $c_file to $txt_file"
        fi
    fi
done

for test_file in ${TEST_DIR}/*.txt; do
    if [ -f "$test_file" ]; then
        echo "--------------------------------------------"
        echo "Running lexer on: ${test_file}"
        echo "--------------------------------------------"
        ./lexer "${test_file}"
        echo ""
    fi
done

#!/bin/bash

# Path to QEMU binary
QEMU="../../build/qemu-system-riscv64"
MACHINE="andes_ae350"
CPU="andes-ax45mpv"

# Ensure we are in the correct directory
if [ ! -f "Makefile" ]; then
    echo "Error: Makefile not found. Please run this script from tests/andes_v5_tests/"
    exit 1
fi

# Build tests
echo "Building tests..."
make || exit 1

# List of tests to run
TESTS=("test_perf" "test_bf16" "test_vpack_fp16" "test_vdot" "test_vsmall_int" "test_vqmac" "test_codense" "test_vfwcvt" "test_vle4" "test_vln8")

# Run each test
for test in "${TESTS[@]}"; do
    echo "----------------------------------------------------------------"
    echo "Running $test..."
    if [ -f "$test" ]; then
        # Run QEMU and capture output
        output=$($QEMU -M $MACHINE -cpu $CPU -nographic -semihosting -readconfig local_andes.cfg -bios ./$test 2>&1)
        echo "$output"
        
        # Simple check for "FAIL" in output
        if echo "$output" | grep -q "FAIL"; then
            echo "Result: FAILED"
        else
            echo "Result: PASSED (Manual verification recommended)"
        fi
    else
        echo "Error: Test binary $test not found!"
    fi
done
echo "----------------------------------------------------------------"

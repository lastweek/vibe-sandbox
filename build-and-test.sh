#!/bin/bash
# Build and test script for nano-kata
# Run this in the Linux environment: ssh -p 2222 ys@localhost
# Then: cd ~/ys/mac/vibe-kata && ./build-and-test.sh

set -e

echo "=== nano-kata Build & Test Script ==="
echo

# Check dependencies
echo "Checking dependencies..."
if ! command -v gcc &> /dev/null; then
    echo "Error: gcc not found. Install: sudo apt-get install build-essential"
    exit 1
fi

if ! pkg-config --exists jansson; then
    echo "Error: libjansson-dev not found."
    echo "Install: sudo apt-get install libjansson-dev"
    exit 1
fi

echo "All dependencies OK!"
echo

# Clean build
echo "Cleaning previous build..."
make clean

# Build
echo "Building nano-kata..."
make

if [ $? -eq 0 ]; then
    echo "Build successful!"
    echo
    echo "Binary: ./bin/nk-runtime"
    echo
    echo "Running basic test..."
    ./bin/nk-runtime --version
    ./bin/nk-runtime --help
    echo
    echo "Build complete! Run tests with: ./tests/integration/test_basic.sh"
else
    echo "Build failed!"
    exit 1
fi

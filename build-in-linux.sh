#!/bin/bash
# Build script for Linux environment

cd ~/ys/mac/vibe-kata || exit 1

echo "=== Building nano-kata in Linux Environment ==="

# Install dependencies if needed
if ! dpkg -l | grep -q libjansson-dev; then
    echo "Installing jansson development library..."
    sudo apt-get update
    sudo apt-get install -y libjansson-dev
fi

# Clean and build
echo "Cleaning previous build..."
make clean

echo "Building..."
make

if [ $? -eq 0 ]; then
    echo ""
    echo "=== Build Successful! ==="
    echo ""
    echo "Running basic tests..."
    ./bin/nk-runtime --version
    ./bin/nk-runtime --help

    echo ""
    echo "Running integration tests..."
    ./tests/integration/test_basic.sh
else
    echo ""
    echo "=== Build Failed ==="
    exit 1
fi

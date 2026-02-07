#!/bin/bash
# Basic integration test for nano-kata

set -e

NK_RUNTIME="./bin/nk-runtime"
TEST_BUNDLE="tests/bundle"
CONTAINER_ID="test-container"

echo "=== nano-kata Basic Integration Test ==="
echo

# Check if binary exists
if [ ! -f "$NK_RUNTIME" ]; then
    echo "Error: $NK_RUNTIME not found. Run 'make' first."
    exit 1
fi

echo "Test 1: Show help"
$NK_RUNTIME --help
echo

echo "Test 2: Show version"
$NK_RUNTIME --version
echo

echo "Test 3: Create container"
$NK_RUNTIME create --bundle=$TEST_BUNDLE $CONTAINER_ID
echo

echo "Test 4: Query container state"
STATE=$($NK_RUNTIME state $CONTAINER_ID)
echo "State: $STATE"
if [ "$STATE" != "created" ]; then
    echo "Error: Expected state 'created', got '$STATE'"
    $NK_RUNTIME delete $CONTAINER_ID
    exit 1
fi
echo

echo "Test 5: Start container"
$NK_RUNTIME start $CONTAINER_ID
echo

echo "Test 6: Query state after start"
STATE=$($NK_RUNTIME state $CONTAINER_ID)
echo "State: $STATE"
if [ "$STATE" != "running" ]; then
    echo "Error: Expected state 'running', got '$STATE'"
    $NK_RUNTIME delete $CONTAINER_ID
    exit 1
fi
echo

echo "Test 7: Delete container"
$NK_RUNTIME delete $CONTAINER_ID
echo

echo "Test 8: Verify deletion"
if $NK_RUNTIME state $CONTAINER_ID 2>&1 | grep -q "not found"; then
    echo "Container successfully deleted"
else
    echo "Error: Container still exists after deletion"
    exit 1
fi
echo

echo "=== All tests passed! ==="

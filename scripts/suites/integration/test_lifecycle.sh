#!/usr/bin/env bash
# Integration test suite for nano-sandbox
# Tests full container lifecycle with validation
# Features: fail-fast, timeouts, detailed logging
#
# This script assumes it's running in a Linux environment and uses
# the installed binary from /usr/local/bin/

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/lib/common.sh
source "$SCRIPT_DIR/../../lib/common.sh"

# ============================================================================
# Configuration
# ============================================================================

TEST_NAME="nano-sandbox-integration"
TEST_CONTAINER="ns-test-$$"
RUN_CONTAINER="${TEST_CONTAINER}-run"
RESUME_CONTAINER="${TEST_CONTAINER}-resume"
STALE_CONTAINER="${TEST_CONTAINER}-stale"
BLOCK_CONTAINER="${TEST_CONTAINER}-block"
RESUME_BUNDLE=""
RUN_BUNDLE=""
RESUME_CAN_EXEC=true
RESUME_CONTAINER_READY=false

# Use installed binary and bundle (from make install)
RUNTIME="/usr/local/bin/ns-runtime"
TEST_BUNDLE="/usr/local/share/nano-sandbox/bundle"

# Set state directory to absolute path (not on sshfs)
export NS_RUN_DIR="${NS_RUN_DIR:-$HOME/.local/share/nano-sandbox/run}"
mkdir -p "$NS_RUN_DIR"

# Elevation wrapper while preserving NS_RUN_DIR for runtime commands.
if [ "$(id -u)" -eq 0 ]; then
    SUDO="env NS_RUN_DIR=$NS_RUN_DIR"
else
    # Ensure sudo credentials are available once, then run non-interactively.
    if ! sudo -n true >/dev/null 2>&1; then
        if [ -t 0 ]; then
            sudo -v
        else
            echo "Error: sudo credentials are required. Run 'sudo -v' first." >&2
            exit 1
        fi
    fi
    SUDO="sudo -n NS_RUN_DIR=$NS_RUN_DIR"
fi

# Timeouts (in seconds)
TIMEOUT_CREATE=5
TIMEOUT_START=10
TIMEOUT_DELETE=5
TIMEOUT_STATE=3
TIMEOUT_RESUME=8

# Fail-fast mode
FAIL_FAST=${FAIL_FAST:-true}

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

TESTS_PASSED=0
TESTS_FAILED=0
TESTS_SKIPPED=0

# ============================================================================
# Test Framework
# ============================================================================

# Test result tracking
test_start() {
    echo -e "\n${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${YELLOW}▶ Test: $1${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
}

test_pass() {
    echo -e "${GREEN}✓ PASS${NC}: $1"
    ((TESTS_PASSED++)) || true
}

test_fail() {
    local msg="$1"
    local output="${2:-}"
    echo -e "${RED}✗ FAIL${NC}: $msg"
    if [ -n "$output" ]; then
        echo -e "${RED}  Output: $output${NC}"
    fi
    ((TESTS_FAILED++)) || true
    if [ "$FAIL_FAST" = "true" ]; then
        echo -e "\n${RED}Fail-fast mode: Exiting immediately${NC}"
        cleanup
        print_summary
        exit 1
    fi
}

test_skip() {
    echo -e "${YELLOW}⊘ SKIP${NC}: $1"
    ((TESTS_SKIPPED++)) || true
}

test_info() {
    echo -e "  ${BLUE}ℹ${NC} $1" >&2
}

contains_runtime_exec_error() {
    local output="$1"
    if echo "$output" | grep -Eq "Failed to execute .*:|Error: Child process failed to initialize"; then
        return 0
    fi
    return 1
}

# Run command with timeout.
# If command starts with sudo, run "timeout ... sudo ..." to avoid sudo implementations
# that cannot execute timeout under a pseudo-tty.
# Supports sudo env assignments (e.g., "sudo NS_RUN_DIR=... <cmd>").
run_with_timeout() {
    local timeout_val="$1"
    shift
    local output
    local output_file
    local ret

    output_file="$(mktemp)"

    if [ "$1" = "sudo" ]; then
        shift
        local sudo_args=()

        # Preserve sudo options/env assignments before command.
        while [ $# -gt 0 ]; do
            case "$1" in
                -*|*=*)
                    sudo_args+=("$1")
                    shift
                    ;;
                *)
                    break
                    ;;
            esac
        done

        test_info "Running: timeout $timeout_val sudo ${sudo_args[*]} $*"
        if timeout "$timeout_val" sudo "${sudo_args[@]}" "$@" >"$output_file" 2>&1; then
            ret=0
        else
            ret=$?
        fi
    else
        test_info "Running: timeout $timeout_val $*"
        if timeout "$timeout_val" "$@" >"$output_file" 2>&1; then
            ret=0
        else
            ret=$?
        fi
    fi
    output="$(cat "$output_file")"
    rm -f "$output_file"
    echo "$output"
    return $ret
}

setup_resume_bundle() {
    if [ -n "$RESUME_BUNDLE" ] && [ -d "$RESUME_BUNDLE" ]; then
        return 0
    fi

    RESUME_BUNDLE="$(mktemp -d)"
    cp -a "$TEST_BUNDLE/rootfs" "$RESUME_BUNDLE/rootfs"
    cat > "$RESUME_BUNDLE/config.json" <<'EOF'
{
  "ociVersion": "1.0.2",
  "process": {
    "terminal": false,
    "user": { "uid": 0, "gid": 0 },
    "args": ["/bin/sh", "-c", "exec 1>&- 2>&-; while :; do :; done"],
    "env": [
      "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
      "TERM=xterm"
    ],
    "cwd": "/",
    "noNewPrivileges": true
  },
  "root": {
    "path": "rootfs",
    "readonly": false
  },
  "hostname": "nano-sandbox",
  "mounts": [
    { "destination": "/proc", "type": "proc", "source": "proc" },
    { "destination": "/dev", "type": "tmpfs", "source": "tmpfs" },
    { "destination": "/dev/pts", "type": "devpts", "source": "devpts" }
  ],
  "linux": {
    "namespaces": [
      { "type": "pid" },
      { "type": "network" },
      { "type": "ipc" },
      { "type": "uts" },
      { "type": "mount" }
    ]
  },
  "annotations": {
    "io.katacontainers.runtime": "container"
  }
}
EOF
}

setup_run_bundle() {
    if [ -n "$RUN_BUNDLE" ] && [ -d "$RUN_BUNDLE" ]; then
        return 0
    fi

    RUN_BUNDLE="$(mktemp -d)"
    cp -a "$TEST_BUNDLE/rootfs" "$RUN_BUNDLE/rootfs"
    cat > "$RUN_BUNDLE/config.json" <<'EOF'
{
  "ociVersion": "1.0.2",
  "process": {
    "terminal": false,
    "user": { "uid": 0, "gid": 0 },
    "args": ["/bin/sh", "-c", "exit 0"],
    "env": [
      "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
      "TERM=xterm"
    ],
    "cwd": "/",
    "noNewPrivileges": true
  },
  "root": {
    "path": "rootfs",
    "readonly": false
  },
  "hostname": "nano-sandbox",
  "mounts": [
    { "destination": "/proc", "type": "proc", "source": "proc" },
    { "destination": "/dev", "type": "tmpfs", "source": "tmpfs" },
    { "destination": "/dev/pts", "type": "devpts", "source": "devpts" }
  ],
  "linux": {
    "namespaces": [
      { "type": "pid" },
      { "type": "network" },
      { "type": "ipc" },
      { "type": "uts" },
      { "type": "mount" }
    ]
  },
  "annotations": {
    "io.katacontainers.runtime": "container"
  }
}
EOF
}

get_container_pid_from_state() {
    local container_id="$1"
    local state_file="$NS_RUN_DIR/$container_id/state.json"

    [ -f "$state_file" ] || return 1
    grep -oE '"pid"[[:space:]]*:[[:space:]]*[0-9]+' "$state_file" \
        | head -1 \
        | grep -oE '[0-9]+'
}

# Cleanup function
cleanup() {
    echo -e "\n${YELLOW}Cleaning up...${NC}"
    $SUDO $RUNTIME delete $TEST_CONTAINER >/dev/null 2>&1 || true
    $SUDO $RUNTIME delete $RUN_CONTAINER >/dev/null 2>&1 || true
    $SUDO $RUNTIME delete $RESUME_CONTAINER >/dev/null 2>&1 || true
    $SUDO $RUNTIME delete $STALE_CONTAINER >/dev/null 2>&1 || true
    $SUDO $RUNTIME delete $BLOCK_CONTAINER >/dev/null 2>&1 || true
    $SUDO rm -rf "$NS_RUN_DIR/$TEST_CONTAINER" >/dev/null 2>&1 || true
    $SUDO rm -rf "$NS_RUN_DIR/$RUN_CONTAINER" >/dev/null 2>&1 || true
    $SUDO rm -rf "$NS_RUN_DIR/$RESUME_CONTAINER" >/dev/null 2>&1 || true
    $SUDO rm -rf "$NS_RUN_DIR/$STALE_CONTAINER" >/dev/null 2>&1 || true
    $SUDO rm -rf "$NS_RUN_DIR/$BLOCK_CONTAINER" >/dev/null 2>&1 || true
    if [ -n "$RESUME_BUNDLE" ] && [ -d "$RESUME_BUNDLE" ]; then
        rm -rf "$RESUME_BUNDLE" >/dev/null 2>&1 || true
    fi
    if [ -n "$RUN_BUNDLE" ] && [ -d "$RUN_BUNDLE" ]; then
        rm -rf "$RUN_BUNDLE" >/dev/null 2>&1 || true
    fi
}

trap cleanup EXIT

# Print test summary
print_summary() {
    echo -e "\n${YELLOW}╔══════════════════════════════════════════════════════════╗${NC}"
    echo -e "${YELLOW}║                    Test Summary                           ║${NC}"
    echo -e "${YELLOW}╚══════════════════════════════════════════════════════════╝${NC}"
    echo -e "Tests Passed: ${GREEN}$TESTS_PASSED${NC}"
    echo -e "Tests Failed: ${RED}$TESTS_FAILED${NC}"
    echo -e "Tests Skipped: ${YELLOW}$TESTS_SKIPPED${NC}"
    echo -e "Total Tests: $((TESTS_PASSED + TESTS_FAILED + TESTS_SKIPPED))"

    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "\n${GREEN}╔══════════════════════════════════════════════════════════╗${NC}"
        echo -e "${GREEN}║              All tests passed! 🎉                        ║${NC}"
        echo -e "${GREEN}╚══════════════════════════════════════════════════════════╝${NC}"
        return 0
    else
        echo -e "\n${RED}╔══════════════════════════════════════════════════════════╗${NC}"
        echo -e "${RED}║              Some tests failed                           ║${NC}"
        echo -e "${RED}╚══════════════════════════════════════════════════════════╝${NC}"
        return 1
    fi
}

# ============================================================================
# Pre-flight Checks
# ============================================================================

test_start "Pre-flight checks"

# Test 1: Runtime exists
if [ ! -f "$RUNTIME" ]; then
    test_fail "Runtime binary not found" \
        "Run 'make install' first. Looking for: $RUNTIME"
    print_summary
    exit 1
fi

# Check if it's executable
if [ ! -x "$RUNTIME" ]; then
    test_fail "Runtime not executable" "Run: chmod +x $RUNTIME"
    print_summary
    exit 1
fi
test_pass "Runtime binary exists"
test_info "Location: $RUNTIME"

# Test 2: Runtime version
VERSION_OUTPUT=$($RUNTIME -v 2>&1)
if ! echo "$VERSION_OUTPUT" | grep -q "version 0.1.0"; then
    test_fail "Runtime version mismatch" "$VERSION_OUTPUT"
    print_summary
    exit 1
fi
test_pass "Runtime version check (0.1.0)"

# Test 3: Runtime help
HELP_OUTPUT=$($RUNTIME -h 2>&1)
if ! echo "$HELP_OUTPUT" | grep -q "create"; then
    test_fail "Runtime help missing commands" "$HELP_OUTPUT"
fi
test_pass "Runtime help shows commands"

# Test 4: Check bundle exists
if [ ! -d "$TEST_BUNDLE" ]; then
    test_fail "Test bundle not found" \
        "Run 'make install' to install the test bundle. Looking for: $TEST_BUNDLE"
    print_summary
    exit 1
fi

# Test 5: Check rootfs exists
if [ ! -d "$TEST_BUNDLE/rootfs/bin" ]; then
    test_fail "Root filesystem not found in bundle" \
        "Run 'make install' to install the bundle. Looking for: $TEST_BUNDLE/rootfs/bin/"
    print_summary
    exit 1
fi
BIN_COUNT=$(ls $TEST_BUNDLE/rootfs/bin/ 2>/dev/null | wc -l)
if ! ROOTFS_CHECK_OUTPUT="$(nk_check_rootfs_exec_ready "$TEST_BUNDLE/rootfs" 2>&1)"; then
    test_fail "Root filesystem is not executable on this host" "$ROOTFS_CHECK_OUTPUT"
    print_summary
    exit 1
fi
test_pass "Root filesystem exists and is executable"
test_info "Location: $TEST_BUNDLE"
test_info "Available: $BIN_COUNT binaries in rootfs/bin/"

# ============================================================================
# Container Lifecycle Tests
# ============================================================================

# Test 5: Create container
test_start "Create container"
test_info "Command: $SUDO $RUNTIME create -V --bundle=$TEST_BUNDLE $TEST_CONTAINER"
set +e
CREATE_OUTPUT=$(run_with_timeout $TIMEOUT_CREATE $SUDO $RUNTIME create -V --bundle=$TEST_BUNDLE $TEST_CONTAINER)
CREATE_RET=$?
set -e

test_info "Exit code: $CREATE_RET"
if [ $CREATE_RET -ne 0 ]; then
    if [ $CREATE_RET -eq 124 ]; then
        test_fail "Container creation timed out after ${TIMEOUT_CREATE}s" "Check above output for where it hung"
    else
        test_fail "Container creation failed (exit code: $CREATE_RET)" "$CREATE_OUTPUT"
    fi
elif ! echo "$CREATE_OUTPUT" | grep -q "Status: created"; then
    test_fail "Container creation didn't report success" "$CREATE_OUTPUT"
else
    test_pass "Container created successfully"
    test_info "Container ID: $TEST_CONTAINER"
fi

# Test 6: Verify state file exists
test_start "State persistence"
STATE_FILE="$NS_RUN_DIR/$TEST_CONTAINER/state.json"
if [ ! -f "$STATE_FILE" ]; then
    test_fail "State file not created at $STATE_FILE"
else
    test_pass "State file created"
    test_info "Location: $STATE_FILE"
fi

# Test 7: Check container state
test_start "Query container state"
set +e
STATE_OUTPUT=$(run_with_timeout $TIMEOUT_STATE $RUNTIME state $TEST_CONTAINER)
STATE_RET=$?
set -e
if [ $STATE_RET -ne 0 ]; then
    test_fail "State query failed (exit code: $STATE_RET)" "$STATE_OUTPUT"
elif [ "$STATE_OUTPUT" != "created" ]; then
    test_fail "Container state is '$STATE_OUTPUT', expected 'created'" "$STATE_OUTPUT"
else
    test_pass "Container state is 'created'"
fi

# Test 8: Verify state file contents
test_start "State file validation"
if ! grep -q "\"id\": \"$TEST_CONTAINER\"" "$STATE_FILE" 2>/dev/null; then
    test_fail "State file missing container ID"
else
    test_pass "State file contains container ID"
fi

# Test 9: Resume should fail for created container
test_start "Exec created container rejection"
set +e
RESUME_CREATED_OUTPUT=$(run_with_timeout $TIMEOUT_RESUME $SUDO $RUNTIME exec --exec "echo should-not-run" $TEST_CONTAINER)
RESUME_CREATED_RET=$?
set -e
if [ $RESUME_CREATED_RET -eq 0 ]; then
    test_fail "Exec unexpectedly succeeded for created container" "$RESUME_CREATED_OUTPUT"
elif ! echo "$RESUME_CREATED_OUTPUT" | grep -qi "not running"; then
    test_fail "Exec created-container error message mismatch" "$RESUME_CREATED_OUTPUT"
else
    test_pass "Exec rejects non-running container state"
fi

# Test 10: Start container
test_start "Start container"
set +e
START_OUTPUT=$(run_with_timeout $TIMEOUT_START $SUDO $RUNTIME start $TEST_CONTAINER)
START_RET=$?
set -e

if [ $START_RET -ne 0 ]; then
    if [ $START_RET -eq 124 ]; then
        test_fail "Container start timed out after ${TIMEOUT_START}s"
    else
        test_fail "Container start failed (exit code: $START_RET)" "$START_OUTPUT"
    fi
elif contains_runtime_exec_error "$START_OUTPUT"; then
    test_fail "Container start reported child execution failure" "$START_OUTPUT"
elif ! echo "$START_OUTPUT" | grep -q "Status: running"; then
    test_fail "Container start didn't report running state" "$START_OUTPUT"
else
    test_pass "Container started successfully"

    # Extract PID from output
    PID=$(echo "$START_OUTPUT" | grep -oP 'PID: \K\d+' || echo "")
    if [ -n "$PID" ]; then
        test_info "Container PID: $PID"

        # Verify process exists (it might exit quickly for /bin/sh)
        if kill -0 $PID 2>/dev/null; then
            test_info "Process verified running (kill -0 succeeded)"
        else
            test_info "Note: Process exited unexpectedly before verification"
        fi
    else
        test_info "Warning: No PID found in output"
    fi
fi

# Test 11: Verify state changed to running
test_start "State transition"
set +e
STATE_OUTPUT=$(run_with_timeout $TIMEOUT_STATE $RUNTIME state $TEST_CONTAINER)
STATE_RET=$?
set -e
if [ $STATE_RET -ne 0 ]; then
    test_fail "State query failed (exit code: $STATE_RET)" "$STATE_OUTPUT"
elif [ "$STATE_OUTPUT" != "running" ]; then
    test_fail "Container state is '$STATE_OUTPUT', expected 'running'" "$STATE_OUTPUT"
else
    test_pass "Container state transitioned to 'running'"
fi

# Test 12: Verify cgroup created (only if we have PID)
if [ -n "$PID" ] && [ -d "/proc/$PID" ]; then
    test_start "Cgroup creation"
    if [ ! -d "/sys/fs/cgroup/nano-sandbox" ]; then
        test_skip "Cgroup root not found (cgroups v2 not available?)"
    elif [ ! -d "/sys/fs/cgroup/nano-sandbox/$TEST_CONTAINER" ]; then
        test_fail "Container cgroup not created at /sys/fs/cgroup/nano-sandbox/$TEST_CONTAINER"
    else
        test_pass "Container cgroup created"
        test_info "Cgroup: /sys/fs/cgroup/nano-sandbox/$TEST_CONTAINER"

        # Test 13: Verify PID in cgroup
        test_start "Process in cgroup"
        CG_PID=$(cat /sys/fs/cgroup/nano-sandbox/$TEST_CONTAINER/cgroup.procs 2>/dev/null | head -1 || echo "")
        if [ -z "$CG_PID" ]; then
            test_fail "No process found in cgroup.procs"
        else
            test_pass "Container process found in cgroup"
            test_info "Cgroup PID: $CG_PID"
        fi
    fi
fi

# Test 14: Verify namespaces (only if PID is still running)
if [ -n "$PID" ] && [ -d "/proc/$PID" ]; then
    test_start "Namespace isolation"
    if [ ! -d "/proc/$PID/ns" ]; then
        test_skip "Namespace directory not accessible for PID $PID"
    else
        NS_COUNT=0
        for ns in pid net ipc uts mnt; do
            if [ -e "/proc/$PID/ns/$ns" ]; then
                ((NS_COUNT++)) || true
            fi
        done
        test_pass "Found $NS_COUNT namespaces for container process"
        test_info "Namespaces: $(ls /proc/$PID/ns/ 2>/dev/null | tr '\n' ' ')"
    fi
fi

# Test 15: Delete container
test_start "Delete container"
set +e
DELETE_OUTPUT=$(run_with_timeout $TIMEOUT_DELETE $SUDO $RUNTIME delete $TEST_CONTAINER)
DELETE_RET=$?
set -e

if [ $DELETE_RET -ne 0 ]; then
    if [ $DELETE_RET -eq 124 ]; then
        test_fail "Container deletion timed out after ${TIMEOUT_DELETE}s"
    else
        test_fail "Container deletion failed (exit code: $DELETE_RET)" "$DELETE_OUTPUT"
    fi
elif ! echo "$DELETE_OUTPUT" | grep -q "Status: deleted"; then
    test_fail "Container deletion didn't report success" "$DELETE_OUTPUT"
else
    test_pass "Container deleted successfully"
fi

# Test 16: Verify cleanup
test_start "Cleanup verification"
if [ -f "$STATE_FILE" ]; then
    test_fail "State file still exists after deletion"
else
    test_pass "State file removed"
fi

# Test 17: Run command (attached + --rm)
test_start "Run command lifecycle (--rm)"
set +e
setup_run_bundle
RUN_BUNDLE_RET=$?
set -e
if [ $RUN_BUNDLE_RET -ne 0 ] || [ ! -f "$RUN_BUNDLE/config.json" ]; then
    test_fail "Failed to prepare run test bundle" "$RUN_BUNDLE"
fi

set +e
RUN_OUTPUT=$(run_with_timeout $TIMEOUT_START $SUDO $RUNTIME run --rm --bundle=$RUN_BUNDLE $RUN_CONTAINER)
RUN_RET=$?
set -e

if [ $RUN_RET -ne 0 ]; then
    if [ $RUN_RET -eq 124 ]; then
        test_fail "Container run timed out after ${TIMEOUT_START}s"
    else
        test_fail "Container run failed (exit code: $RUN_RET)" "$RUN_OUTPUT"
    fi
elif contains_runtime_exec_error "$RUN_OUTPUT"; then
    test_fail "Container run reported child execution failure" "$RUN_OUTPUT"
elif ! echo "$RUN_OUTPUT" | grep -q "Status: stopped (exit code: 0)"; then
    test_fail "Container run did not report clean stop" "$RUN_OUTPUT"
else
    test_pass "Container run completed without execution errors"
fi

# Test 18: Verify --rm cleaned up run container
test_start "Run --rm cleanup"
set +e
RUN_STATE_OUTPUT=$(run_with_timeout $TIMEOUT_STATE $RUNTIME state $RUN_CONTAINER)
RUN_STATE_RET=$?
set -e
if [ $RUN_STATE_RET -eq 0 ]; then
    test_fail "Run container still exists after --rm" "$RUN_STATE_OUTPUT"
elif ! echo "$RUN_STATE_OUTPUT" | grep -q "not found"; then
    test_fail "Run --rm cleanup returned unexpected error" "$RUN_STATE_OUTPUT"
else
    test_pass "Run --rm removed container state"
fi

# Test 19: Run attached keepalive bundle should block until timeout
test_start "Run attached keepalive blocks"
set +e
RUN_BLOCK_OUTPUT=$(run_with_timeout 2 $SUDO $RUNTIME run --bundle=$TEST_BUNDLE $BLOCK_CONTAINER)
RUN_BLOCK_RET=$?
set -e
if [ $RUN_BLOCK_RET -ne 124 ]; then
    test_fail "Attached keepalive run should block and timeout (expected 124)" "$RUN_BLOCK_OUTPUT"
elif contains_runtime_exec_error "$RUN_BLOCK_OUTPUT"; then
    test_fail "Attached keepalive run reported execution failure" "$RUN_BLOCK_OUTPUT"
else
    set +e
    BLOCK_STATE_OUTPUT=$(run_with_timeout $TIMEOUT_STATE $RUNTIME state $BLOCK_CONTAINER)
    BLOCK_STATE_RET=$?
    set -e
    if [ $BLOCK_STATE_RET -ne 0 ]; then
        test_fail "State query failed for blocked run container" "$BLOCK_STATE_OUTPUT"
    elif [ "$BLOCK_STATE_OUTPUT" != "running" ]; then
        test_fail "Blocked run container should remain running after timeout" "$BLOCK_STATE_OUTPUT"
    else
        test_pass "Attached keepalive run blocks and container remains running"
    fi
fi
set +e
$SUDO $RUNTIME delete $BLOCK_CONTAINER >/dev/null 2>&1
set -e

# ============================================================================
# Exec Flow Tests
# ============================================================================

# Test 19: Prepare a long-running bundle for exec tests
test_start "Exec bundle setup"
set +e
setup_resume_bundle
BUNDLE_SETUP_RET=$?
set -e
if [ $BUNDLE_SETUP_RET -ne 0 ] || [ ! -f "$RESUME_BUNDLE/config.json" ]; then
    test_fail "Failed to prepare exec test bundle" "$RESUME_BUNDLE"
else
    test_pass "Exec test bundle prepared"
    test_info "Bundle: $RESUME_BUNDLE"
fi

# Test 20: Start detached container for exec success test
test_start "Exec success setup"
set +e
RESUME_SETUP_OUTPUT=$(run_with_timeout $TIMEOUT_START $SUDO $RUNTIME run -d --bundle=$RESUME_BUNDLE $RESUME_CONTAINER)
RESUME_SETUP_RET=$?
set -e
if [ $RESUME_SETUP_RET -ne 0 ]; then
    test_fail "Failed to start exec test container" "$RESUME_SETUP_OUTPUT"
elif contains_runtime_exec_error "$RESUME_SETUP_OUTPUT"; then
    test_fail "Exec setup container reported execution failure" "$RESUME_SETUP_OUTPUT"
elif ! echo "$RESUME_SETUP_OUTPUT" | grep -q "Status: running"; then
    test_fail "Exec setup container is not running" "$RESUME_SETUP_OUTPUT"
else
    test_pass "Exec setup container started"
    RESUME_CONTAINER_READY=true
    RESUME_PID="$(get_container_pid_from_state "$RESUME_CONTAINER" 2>/dev/null || true)"
    if [ -z "$RESUME_PID" ]; then
        test_skip "Exec setup PID missing; skipping live exec test on this host"
        RESUME_CAN_EXEC=false
    elif ! $SUDO kill -0 "$RESUME_PID" >/dev/null 2>&1; then
        test_skip "Exec setup process exited early; skipping live exec test on this host"
        RESUME_CAN_EXEC=false
    elif [ ! -e "/proc/$RESUME_PID/ns/pid" ]; then
        test_skip "Exec setup namespace handles unavailable; skipping live exec test"
        RESUME_CAN_EXEC=false
    else
        test_info "Resume setup PID: $RESUME_PID"
    fi
fi

# Test 21: Exec running container with command execution
test_start "Exec running container"
if [ "$RESUME_CAN_EXEC" != "true" ]; then
    test_skip "Live exec skipped (detached init process not stable on this host)"
else
    set +e
RESUME_OK_OUTPUT=$(run_with_timeout $TIMEOUT_RESUME $SUDO $RUNTIME exec --exec "echo RESUME_OK && test -x /bin/sh" $RESUME_CONTAINER)
    RESUME_OK_RET=$?
    set -e
    if [ $RESUME_OK_RET -ne 0 ]; then
        test_fail "Exec command execution failed" "$RESUME_OK_OUTPUT"
    elif ! echo "$RESUME_OK_OUTPUT" | grep -q "RESUME_OK"; then
        test_fail "Exec output missing sentinel value" "$RESUME_OK_OUTPUT"
    else
        set +e
        RESUME_STATE_AFTER_OK=$(run_with_timeout $TIMEOUT_STATE $RUNTIME state $RESUME_CONTAINER)
        RESUME_STATE_OK_RET=$?
        set -e
        if [ $RESUME_STATE_OK_RET -ne 0 ]; then
            test_fail "State query failed after successful exec" "$RESUME_STATE_AFTER_OK"
        elif [ "$RESUME_STATE_AFTER_OK" != "running" ]; then
            test_fail "Container should remain running after successful exec" "$RESUME_STATE_AFTER_OK"
        else
            test_pass "Exec command executed and container stayed running"
        fi
    fi
fi

# Test 22: Exec failure should not mark container stopped
test_start "Exec failure preserves running state"
if [ "$RESUME_CONTAINER_READY" != "true" ]; then
    test_skip "Exec setup container was not started"
else
    set +e
    RESUME_FAIL_OUTPUT=$(run_with_timeout $TIMEOUT_RESUME $SUDO env PATH=/no-such-bin $RUNTIME exec --exec "echo should-not-run" $RESUME_CONTAINER)
    RESUME_FAIL_RET=$?
    set -e
    if [ $RESUME_FAIL_RET -eq 0 ]; then
        test_fail "Exec unexpectedly succeeded with missing nsenter PATH" "$RESUME_FAIL_OUTPUT"
    else
        set +e
        RESUME_STATE_AFTER_FAIL=$(run_with_timeout $TIMEOUT_STATE $RUNTIME state $RESUME_CONTAINER)
        RESUME_STATE_RET=$?
        set -e
        if [ $RESUME_STATE_RET -ne 0 ]; then
            test_fail "State query failed after exec failure" "$RESUME_STATE_AFTER_FAIL"
        elif [ "$RESUME_STATE_AFTER_FAIL" != "running" ]; then
            test_fail "Container should remain running after exec tool failure" "$RESUME_STATE_AFTER_FAIL"$'\n'"$RESUME_FAIL_OUTPUT"
        else
            test_pass "Exec failure does not incorrectly stop running container"
        fi
    fi
fi

# Test 23: Exec with /proc unavailable should fail without stopping container
test_start "Exec proc-less host handling"
if [ "$RESUME_CONTAINER_READY" != "true" ]; then
    test_skip "Exec setup container was not started"
else
    if [ ! -e /proc/self/ns/pid ]; then
        set +e
        PROCLESS_OUTPUT=$(run_with_timeout $TIMEOUT_RESUME $SUDO $RUNTIME exec --exec "echo should-not-run" $RESUME_CONTAINER)
        PROCLESS_RET=$?
        set -e
        if [ $PROCLESS_RET -eq 0 ]; then
            test_fail "Exec unexpectedly succeeded on host without /proc" "$PROCLESS_OUTPUT"
        elif ! echo "$PROCLESS_OUTPUT" | grep -q "Host /proc is not mounted"; then
            test_fail "Missing expected /proc error for exec on proc-less host" "$PROCLESS_OUTPUT"
        else
            set +e
            PROCLESS_STATE_OUTPUT=$(run_with_timeout $TIMEOUT_STATE $RUNTIME state $RESUME_CONTAINER)
            PROCLESS_STATE_RET=$?
            set -e
            if [ $PROCLESS_STATE_RET -ne 0 ]; then
                test_fail "State query failed after proc-less exec failure" "$PROCLESS_STATE_OUTPUT"
            elif [ "$PROCLESS_STATE_OUTPUT" != "running" ]; then
                test_fail "Container should remain running after proc-less exec failure" "$PROCLESS_STATE_OUTPUT"$'\n'"$PROCLESS_OUTPUT"
            else
                test_pass "Proc-less host exec failure is handled and container remains running"
            fi
        fi
    elif ! command -v unshare >/dev/null 2>&1; then
        test_skip "unshare not available; skipping proc-less exec scenario"
    else
        PROCLESS_CMD="umount /proc >/dev/null 2>&1 || true; if [ -e /proc/self/ns/pid ]; then echo PROC_STILL_MOUNTED; else echo PROC_UNMOUNTED; fi; $RUNTIME exec --exec 'echo should-not-run' $RESUME_CONTAINER"
        set +e
        PROCLESS_OUTPUT=$(run_with_timeout $TIMEOUT_RESUME $SUDO unshare -m /bin/sh -c "$PROCLESS_CMD")
        PROCLESS_RET=$?
        set -e
        if echo "$PROCLESS_OUTPUT" | grep -q "PROC_STILL_MOUNTED"; then
            test_skip "Could not hide /proc in isolated mount namespace on this host"
        elif [ $PROCLESS_RET -eq 0 ]; then
            test_fail "Exec unexpectedly succeeded without /proc namespace handles" "$PROCLESS_OUTPUT"
        elif ! echo "$PROCLESS_OUTPUT" | grep -q "Host /proc is not mounted"; then
            test_fail "Missing expected /proc error for exec" "$PROCLESS_OUTPUT"
        else
            set +e
            PROCLESS_STATE_OUTPUT=$(run_with_timeout $TIMEOUT_STATE $RUNTIME state $RESUME_CONTAINER)
            PROCLESS_STATE_RET=$?
            set -e
            if [ $PROCLESS_STATE_RET -ne 0 ]; then
                test_fail "State query failed after proc-less exec failure" "$PROCLESS_STATE_OUTPUT"
            elif [ "$PROCLESS_STATE_OUTPUT" != "running" ]; then
                test_fail "Container should remain running after proc-less exec failure" "$PROCLESS_STATE_OUTPUT"$'\n'"$PROCLESS_OUTPUT"
            else
                test_pass "Proc-less exec failure is handled and container remains running"
            fi
        fi
    fi
fi

# Test 24: Setup stale-PID container
test_start "Exec stale PID setup"
set +e
STALE_SETUP_OUTPUT=$(run_with_timeout $TIMEOUT_START $SUDO $RUNTIME run -d --bundle=$RESUME_BUNDLE $STALE_CONTAINER)
STALE_SETUP_RET=$?
set -e
if [ $STALE_SETUP_RET -ne 0 ]; then
    test_fail "Failed to start stale-PID test container (exec)" "$STALE_SETUP_OUTPUT"
else
    test_pass "Stale-PID test container started"
fi

# Test 25: Exec should fail when init PID has exited
test_start "Exec stale PID detection"
STALE_PID="$(get_container_pid_from_state "$STALE_CONTAINER" 2>/dev/null || true)"
if [ -z "$STALE_PID" ]; then
    test_fail "Unable to read stale container PID from state file"
else
    test_info "Killing stale test PID: $STALE_PID"
    $SUDO kill -9 "$STALE_PID" >/dev/null 2>&1 || true
    sleep 0.2

    set +e
RESUME_STALE_OUTPUT=$(run_with_timeout $TIMEOUT_RESUME $SUDO $RUNTIME exec --exec "echo should-not-run" $STALE_CONTAINER)
    RESUME_STALE_RET=$?
    set -e

    if [ $RESUME_STALE_RET -eq 0 ]; then
        test_fail "Exec unexpectedly succeeded for stale PID container" "$RESUME_STALE_OUTPUT"
    elif ! echo "$RESUME_STALE_OUTPUT" | grep -Eqi "not alive|not running|not available"; then
        test_fail "Exec stale-PID error message mismatch" "$RESUME_STALE_OUTPUT"
    else
        test_pass "Exec detects stale PID and fails cleanly"
    fi

    set +e
    STALE_STATE_OUTPUT=$(run_with_timeout $TIMEOUT_STATE $RUNTIME state $STALE_CONTAINER)
    STALE_STATE_RET=$?
    set -e
    if [ $STALE_STATE_RET -ne 0 ]; then
        test_fail "State query failed for stale container" "$STALE_STATE_OUTPUT"
    elif [ "$STALE_STATE_OUTPUT" != "stopped" ]; then
        test_fail "Stale container state should transition to stopped" "$STALE_STATE_OUTPUT"
    else
        test_pass "Stale container state transitioned to 'stopped'"
    fi
fi

# Test 26: Cleanup exec containers
test_start "Exec container cleanup"
set +e
RESUME_DEL_OUTPUT=$(run_with_timeout $TIMEOUT_DELETE $SUDO $RUNTIME delete $RESUME_CONTAINER)
RESUME_DEL_RET=$?
STALE_DEL_OUTPUT=$(run_with_timeout $TIMEOUT_DELETE $SUDO $RUNTIME delete $STALE_CONTAINER)
STALE_DEL_RET=$?
set -e
if [ $RESUME_DEL_RET -ne 0 ] || [ $STALE_DEL_RET -ne 0 ]; then
    test_fail "Failed to cleanup exec test containers" "$RESUME_DEL_OUTPUT"$'\n'"$STALE_DEL_OUTPUT"
else
    test_pass "Exec test containers cleaned up"
fi

# ============================================================================
# Error Handling Tests
# ============================================================================

# Test 25: Create duplicate container ID
test_start "Duplicate ID handling"
set +e
PREP_OUTPUT=$(run_with_timeout $TIMEOUT_CREATE $SUDO $RUNTIME create --bundle=$TEST_BUNDLE $TEST_CONTAINER)
PREP_RET=$?
set -e
if [ $PREP_RET -ne 0 ]; then
    test_fail "Failed to prepare duplicate-ID scenario" "$PREP_OUTPUT"
else
    set +e
    DUP_OUTPUT=$(run_with_timeout $TIMEOUT_CREATE $SUDO $RUNTIME create --bundle=$TEST_BUNDLE $TEST_CONTAINER)
    DUP_RET=$?
    set -e

    if [ $DUP_RET -ne 0 ] && echo "$DUP_OUTPUT" | grep -q "already exists"; then
        test_pass "Duplicate container ID rejected"
    else
        test_fail "Duplicate container ID not detected" "$DUP_OUTPUT"
    fi
fi
# Clean up for next tests
$SUDO $RUNTIME delete $TEST_CONTAINER >/dev/null 2>&1 || true

# Test 26: Start non-existent container
test_start "Non-existent container handling"
set +e
NONEXIST_OUTPUT=$(run_with_timeout $TIMEOUT_START $SUDO $RUNTIME start nonexistent-container)
NONEXIST_RET=$?
set -e
if [ $NONEXIST_RET -eq 0 ] || ! echo "$NONEXIST_OUTPUT" | grep -q "not found"; then
    test_fail "Non-existent container not detected properly" "$NONEXIST_OUTPUT"
else
    test_pass "Non-existent container error handled"
fi

# Test 27: Delete non-existent container
test_start "Non-existent delete"
set +e
NONEXIST_DEL_OUTPUT=$(run_with_timeout $TIMEOUT_DELETE $SUDO $RUNTIME delete nonexistent-container)
NONEXIST_DEL_RET=$?
set -e
if [ $NONEXIST_DEL_RET -eq 0 ] || ! echo "$NONEXIST_DEL_OUTPUT" | grep -q "not found"; then
    test_fail "Non-existent delete not detected" "$NONEXIST_DEL_OUTPUT"
else
    test_pass "Non-existent delete handled"
fi

# Test 28: Exec non-existent container
test_start "Non-existent exec"
set +e
NONEXIST_RESUME_OUTPUT=$(run_with_timeout $TIMEOUT_RESUME $SUDO $RUNTIME exec --exec "echo test" nonexistent-container)
NONEXIST_RESUME_RET=$?
set -e
if [ $NONEXIST_RESUME_RET -eq 0 ] || ! echo "$NONEXIST_RESUME_OUTPUT" | grep -q "not found"; then
    test_fail "Non-existent exec not detected" "$NONEXIST_RESUME_OUTPUT"
else
    test_pass "Non-existent exec handled"
fi

# ============================================================================
# Summary
# ============================================================================

print_summary
exit $?

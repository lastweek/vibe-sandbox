#!/usr/bin/env bash
# Smoke tests: quick validation before integration/perf.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/lib/common.sh
source "$SCRIPT_DIR/../lib/common.sh"

nk_print_header "nano-sandbox Smoke Tests"

printf '[1/6] Checking installed binary...\n'
nk_require_installed_runtime
printf '✓ Binary exists at %s\n' "$NS_RUNTIME_BIN"

printf '\n[2/8] Checking basic commands...\n'
"$NS_RUNTIME_BIN" -v >/dev/null 2>&1
printf '✓ Version command works\n'
"$NS_RUNTIME_BIN" -h >/dev/null 2>&1
printf '✓ Help command works\n'

HELP_OUTPUT="$("$NS_RUNTIME_BIN" -h 2>&1)"
printf '%s\n' "$HELP_OUTPUT" | grep -q "exec" || nk_die "help output missing exec command"
printf '%s\n' "$HELP_OUTPUT" | grep -q -- "--exec" || nk_die "help output missing --exec option"
printf '✓ Help output includes exec command\n'

printf '\n[3/8] Checking exec CLI validation...\n'
if "$NS_RUNTIME_BIN" run --exec "echo test" smoke-cmd >/dev/null 2>&1; then
    nk_die "--exec unexpectedly accepted for non-exec command"
fi
if "$NS_RUNTIME_BIN" exec >/dev/null 2>&1; then
    nk_die "exec unexpectedly accepted without container id"
fi
printf '✓ Exec argument validation works\n'

printf '\n[4/8] Checking state management...\n'
nk_prepare_run_dir
[ -w "$NS_RUN_DIR" ] || nk_die "state directory is not writable: $NS_RUN_DIR"
printf '✓ State directory is writable (%s)\n' "$NS_RUN_DIR"

printf '\n[5/8] Checking installed bundle...\n'
[ -d "$NS_TEST_BUNDLE" ] || nk_die "bundle missing at $NS_TEST_BUNDLE"
printf '✓ Bundle directory exists at %s\n' "$NS_TEST_BUNDLE"

printf '\n[6/8] Checking OCI config...\n'
[ -f "$NS_TEST_BUNDLE/config.json" ] || nk_die "missing OCI config: $NS_TEST_BUNDLE/config.json"
grep -q '"ociVersion"' "$NS_TEST_BUNDLE/config.json" || nk_die "OCI config missing ociVersion"
printf '✓ OCI config is present and valid\n'

printf '\n[7/8] Checking rootfs...\n'
[ -d "$NS_TEST_BUNDLE/rootfs/bin" ] || nk_die "rootfs missing: $NS_TEST_BUNDLE/rootfs/bin"
BIN_COUNT="$(find "$NS_TEST_BUNDLE/rootfs/bin" -mindepth 1 -maxdepth 1 | wc -l | tr -d ' ')"
nk_check_rootfs_exec_ready "$NS_TEST_BUNDLE/rootfs" || nk_die "rootfs is not executable on this host"
printf '✓ Root filesystem exists and is executable (%s binaries)\n' "$BIN_COUNT"

printf '\n[8/8] Checking nsenter dependency...\n'
command -v nsenter >/dev/null 2>&1 || nk_die "nsenter not found in PATH (required for exec)"
printf '✓ nsenter is available\n'

printf '\n=== Smoke Tests Passed ===\n'
printf 'Ready for integration testing.\n'

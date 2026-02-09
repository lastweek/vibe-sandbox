#!/usr/bin/env bash
# Shared helpers for perf benchmarks.

if [ -n "${NK_PERF_COMMON_SH_LOADED:-}" ]; then
    return 0
fi
NK_PERF_COMMON_SH_LOADED=1

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=scripts/lib/common.sh
source "$SCRIPT_DIR/../lib/common.sh"

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

PERF_SUDO=()
PERF_CMD_PREFIX=(env NK_LOG_ENABLED=0 NK_LOG_LEVEL=error NK_LOG_EDUCATIONAL=0)

perf_header() {
    local title="$1"
    echo -e "${BLUE}╔══════════════════════════════════════════════════════════╗${NC}"
    printf "%b║ %-56s ║%b\n" "$BLUE" "$title" "$NC"
    echo -e "${BLUE}╚══════════════════════════════════════════════════════════╝${NC}"
}

perf_section() {
    local title="$1"
    echo -e "${YELLOW}═══════════════════════════════════════════════════════════${NC}"
    echo -e "${YELLOW}${title}${NC}"
    echo -e "${YELLOW}═══════════════════════════════════════════════════════════${NC}"
}

perf_require_env() {
    nk_require_installed_runtime
    nk_require_installed_bundle
    nk_prepare_run_dir

    nk_require_cmd awk
    nk_require_cmd sort
    nk_require_cmd date
    nk_require_cmd bc
    nk_require_cmd sudo

    if [ "$(id -u)" -eq 0 ]; then
        PERF_SUDO=()
        return 0
    fi

    if ! sudo -n true >/dev/null 2>&1; then
        if [ -t 0 ]; then
            sudo -v
        else
            nk_die "sudo credentials are required for perf benchmarks. Run 'sudo -v' first."
        fi
    fi

    PERF_SUDO=(sudo -n)
}

perf_time_us() {
    local start_ns
    local end_ns
    start_ns=$(date +%s%N)
    "$@" >/dev/null 2>&1
    local rc=$?
    end_ns=$(date +%s%N)
    echo $(((end_ns - start_ns) / 1000))
    return $rc
}

perf_progress_dot() {
    local idx="$1"
    local step="$2"
    if [ $((idx % step)) -eq 0 ]; then
        echo -n "."
    fi
}

perf_ms_from_us() {
    local us="$1"
    echo "scale=3; $us / 1000" | bc
}

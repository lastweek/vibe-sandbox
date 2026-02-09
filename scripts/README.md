# Scripts Guide

This directory contains the supported build/test entrypoints.

Build artifacts are placed under `build/` by the Makefile.
Architecture and runtime internals are documented in [`../docs/`](../docs/README.md).

## Primary Commands

### `./scripts/build.sh`

Build the project locally.

```bash
./scripts/build.sh
./scripts/build.sh --clean
./scripts/build.sh --install
./scripts/build.sh --mode release
./scripts/build.sh --sanitize address
./scripts/build.sh --mode debug --sanitize undefined -j 8
```

Useful Make targets behind this script:

```bash
make check-deps
make clean
make distclean
make release
make asan
```

### `./scripts/test.sh`

Run test suites against the installed runtime (`/usr/local/bin/ns-runtime`).

```bash
./scripts/test.sh
./scripts/test.sh smoke
./scripts/test.sh integration
./scripts/test.sh perf
```

Coverage notes:
- `smoke` validates installed bundle/rootfs is executable on current host architecture.
- `smoke` validates CLI semantics for `exec`/`--exec`.
- `integration` validates `create/start/delete`, attached keepalive `run` blocking behavior, `run --rm`, and `exec` lifecycle (success + stale-PID + tool/proc failure paths), and fails if runtime logs child `execve` startup errors.
- `integration` uses `sudo -n` for runtime actions; run `sudo -v` first in non-interactive environments.

### `./scripts/bench.sh`

Run performance benchmarks.

```bash
./scripts/bench.sh
./scripts/bench.sh micro
./scripts/bench.sh start
./scripts/bench.sh latency
./scripts/bench.sh throughput
```

### `./scripts/setup-rootfs.sh`

Download Alpine rootfs into `tests/bundle/rootfs`.

```bash
./scripts/setup-rootfs.sh
./scripts/setup-rootfs.sh --force
```

Notes:
- Architecture is auto-detected from host (`uname -m`)
- Override explicitly with `ALPINE_ARCH=<arch> ./scripts/setup-rootfs.sh --force`

### `./scripts/vm-sync-test.sh`

Recommended for macOS + Ubuntu VM workflow: sync source to VM-native path, build, then run tests there.

```bash
./scripts/vm-sync-test.sh integration
./scripts/vm-sync-test.sh smoke
```

Defaults:
- `VM_USER=ys`
- `VM_HOST=127.0.0.1`
- `VM_PORT=2222`
- `VM_PASS=root`
- `VM_REMOTE_DIR=/home/ys/vibe-sandbox-vm`
- Requires `expect` (for password-based SSH automation)

### `./scripts/ecs-sync-test.sh`

Recommended for key-based remote build/test on ECS servers.

```bash
./scripts/ecs-sync-test.sh integration
./scripts/ecs-sync-test.sh smoke
./scripts/ecs-sync-test.sh --bootstrap integration
```

Defaults:
- `ECS_USER=root`
- `ECS_HOST=118.196.47.98`
- `ECS_PORT=22`
- `ECS_KEY=~/.ssh/id_rsa`
- `ECS_REMOTE_DIR=/root/vibe-sandbox`

## Internal Shared Helpers

- `scripts/lib/common.sh` centralizes shared checks and path defaults for build/test scripts.
- `scripts/perf/common.sh` centralizes perf benchmark helpers.
- `scripts/suites/` contains smoke/integration suite scripts.
- `scripts/perf/` contains direct benchmark scripts.

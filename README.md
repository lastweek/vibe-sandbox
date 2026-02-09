# nano-sandbox: Educational OCI Container Runtime

A minimal OCI-compatible container runtime written in C, supporting both pure container and VM-based execution via Firecracker.

## Project Status

Core runtime flow is implemented and testable end-to-end:
- OCI spec parsing + validation
- lifecycle commands: `create`, `start`, `run`, `exec`, `delete`, `state`
- namespace/mount/cgroup process startup path
- structured state persistence
- smoke/integration/perf script suites

## Documentation

Architecture and internals live in [`docs/`](docs/README.md):
- [`docs/architecture.md`](docs/architecture.md)
- [`docs/container-ecosystem-lifecycle.md`](docs/container-ecosystem-lifecycle.md)
- [`docs/execution-flow.md`](docs/execution-flow.md)
- [`docs/exec-design.md`](docs/exec-design.md)
- [`docs/kernel-mechanisms.md`](docs/kernel-mechanisms.md)
- [`docs/build-and-test.md`](docs/build-and-test.md)

## Building

### Prerequisites
```bash
# On Ubuntu/Debian
sudo apt-get install gcc make libjansson-dev

# On macOS
brew install jansson
```

### Build and Install

```bash
# Check toolchain + headers
make check-deps

# Build and install system-wide (default prefix: /usr/local)
sudo make install

# If you're on an SSHFS/shared mount where sudo cannot read source files:
make install-system

# This installs:
#   /usr/local/bin/ns-runtime           (binary)
#   /usr/local/share/nano-sandbox/bundle   (test bundle)
```

### Build Only

```bash
# Build without installing
make

# Release profile
make release

# Sanitizer profile
make asan

# This produces: ./build/bin/ns-runtime
```

### Packaging-Friendly Install

```bash
# Stage install into a package root
make install DESTDIR=/tmp/pkgroot PREFIX=/usr

# SSHFS-safe staged system install to /usr/local
make install-system
```

### Cleanup

```bash
# Remove only compiled artifacts
make clean

# Remove build + local runtime/test artifacts
make distclean
```

## Usage Examples

### Quick Start (Installed Binary)

```bash
# After running 'make install', you can use the installed binary
/usr/local/bin/ns-runtime create --bundle=/usr/local/share/nano-sandbox/bundle test1
/usr/local/bin/ns-runtime start test1
/usr/local/bin/ns-runtime exec test1
/usr/local/bin/ns-runtime run --bundle=/usr/local/share/nano-sandbox/bundle test2
/usr/local/bin/ns-runtime state test1
/usr/local/bin/ns-runtime delete test1
```

### Using Build Directory Binary

```bash
# Show help
./build/bin/ns-runtime --help

# Show version
./build/bin/ns-runtime --version

# Create a container
./build/bin/ns-runtime create --bundle=./tests/bundle mycontainer

# Start a container
./build/bin/ns-runtime start mycontainer

# Run in one step (create + start, attached by default)
./build/bin/ns-runtime run --bundle=./tests/bundle myrun

# Re-enter a running container (interactive shell)
./build/bin/ns-runtime exec mycontainer

# Run one command in a running container
./build/bin/ns-runtime exec --exec "ps -ef" mycontainer

# Query container state
./build/bin/ns-runtime state mycontainer

# Delete a container
./build/bin/ns-runtime delete mycontainer
```

**Note**: Container state is stored in the `run/` directory (or `~/.local/share/nano-sandbox/run/` for non-root runs, `/run/nano-sandbox` for root).

Command behavior:
- `start` defaults to detached mode (similar to `docker start`).
- `run` defaults to attached mode (similar to `docker run`).
- `exec` re-enters a running container via `nsenter`.
- Use `-a/--attach` or `-d/--detach` to override.
- Use `run --rm` to delete container metadata automatically after attached run exits.
- Default test bundle PID 1 is keepalive (`/bin/busybox sh -c "while :; do /bin/busybox sleep 3600; done"`), so `exec` shell exits do not stop the container.
- `exec` requires host `/proc` to be mounted because `nsenter` reads `/proc/<pid>/ns/*`.

## PID 1 and Exec Best Practices

Practical rule: PID 1 lifecycle controls container lifecycle.

- If PID 1 is an interactive shell (`/bin/sh`, `/bin/bash`), typing `exit` stops the container.
- If PID 1 is a long-running app/keepalive process, `exec` shells can come and go without stopping the container.

Exit-prone sandbox pattern (for learning/debugging):

```bash
# PID 1 is shell, so exit will stop container
sudo ns-runtime start -a my-container
exit
sudo ns-runtime state my-container
```

Preferred stable pattern (for day-to-day use):

1. Make PID 1 a long-running app (or keepalive) in OCI `config.json`.
2. Start container detached.
3. Use `exec` for interactive shells and one-off commands.

Example process args for a stable sandbox:

```json
"process": {
  "args": ["/bin/busybox", "sh", "-c", "while :; do /bin/busybox sleep 3600; done"]
}
```

Then:

```bash
sudo ns-runtime start my-container
sudo ns-runtime exec my-container
# do work, then exit shell
exit
sudo ns-runtime state my-container   # still running
```

Production note:
- Prefer running the actual service as PID 1 (or a tiny init like `tini`) so signals and child reaping behave predictably.

### Testing

```bash
# Build and install
sudo make install

# Run all tests
./scripts/test.sh

# Run specific tests
./scripts/test.sh smoke         # Smoke tests only
./scripts/test.sh integration   # Integration tests only

# Run benchmarks
./scripts/bench.sh
```

## Project Structure

```
nano-sandbox/
├── include/
│   ├── nk.h                 # Main API and data structures
│   ├── nk_oci.h             # OCI spec handling
│   ├── nk_container.h       # Container operations (Phase 2)
│   ├── nk_log.h             # Logging helpers and macros
│   ├── nk_vm.h              # VM operations (Phase 3)
│   └── common/state.h       # State management
├── src/
│   ├── main.c               # CLI entry point
│   ├── oci/
│   │   └── spec.c           # OCI spec parser
│   ├── container/           # Namespaces, mounts, cgroups, process
│   └── common/
│       ├── state.c          # State persistence
│       └── log.c            # Structured logging
├── scripts/
│   ├── build.sh             # Build helper
│   ├── test.sh              # Smoke/integration/perf test entrypoint
│   ├── bench.sh             # Benchmark entrypoint
│   ├── vm-sync-test.sh      # macOS -> VM sync + remote test
│   ├── suites/              # Smoke/integration suites
│   └── perf/                # Perf benchmark scripts
├── docs/
│   ├── architecture.md      # Component boundaries and data paths
│   ├── execution-flow.md    # create/start/run/delete/state flow maps
│   ├── kernel-mechanisms.md # namespaces/mounts/cgroups/process internals
│   └── build-and-test.md    # build/install/test and remote workflows
├── tests/
│   ├── bundle/
│   │   ├── config.json      # OCI spec for testing
│   │   └── rootfs/          # Container rootfs
├── Makefile
└── README.md
```

## Implementation Plan

### ✅ Phase 1: Foundation (Complete)
- Project setup, OCI spec parsing, CLI, state management

### 🚧 Phase 2: Pure Container Execution (Next)
- [ ] Linux namespace setup (pid, net, ipc, uts, mount, user, cgroup)
- [ ] Filesystem operations (pivot root, mount propagation)
- [ ] cgroups v2 integration (memory, CPU, PIDs limits)
- [ ] Process execution with proper isolation
- [ ] Signal handling and forwarding

### Phase 3: VM Mode Architecture
- [ ] Firecracker integration
- [ ] MicroVM root filesystem
- [ ] Guest agent for in-VM container execution

### Phase 4: Advanced Features
- [ ] Container hooks
- [ ] Seccomp filters
- [ ] AppArmor/SELinux integration
- [ ] Device management

### Phase 5: Containerd Shim Integration
- [ ] Shim v2 protocol
- [ ] Runtime registration

### Phase 6: Production Readiness
- [ ] Logging and monitoring
- [ ] Error handling and recovery
- [ ] Comprehensive testing
- [ ] Documentation

## Testing in Linux Environment

Recommended workflow (edit on macOS, test in Ubuntu VM):

```bash
# From macOS project root, sync to VM + run build/tests remotely
./scripts/vm-sync-test.sh integration
# or: ./scripts/vm-sync-test.sh smoke
```

Default VM settings used by `vm-sync-test.sh`:
- `VM_USER=ys`
- `VM_HOST=127.0.0.1`
- `VM_PORT=2222`
- `VM_PASS=root`
- `VM_REMOTE_DIR=/home/ys/vibe-sandbox-vm` (native VM path, not sshfs mount)

You can still test manually via SSH:

```bash
ssh -p 2222 ys@127.0.0.1

cd ~/vibe-sandbox-vm
sudo apt-get install gcc make libjansson-dev
./scripts/setup-rootfs.sh
sudo make install
./scripts/test.sh
./scripts/bench.sh
```

All test scripts assume Linux and use the installed binary from `/usr/local/bin/` by default.
Using the native VM directory avoids sshfs permission issues with `sudo` + container operations.

For ECS/remote server workflow (SSH key auth), use:

```bash
./scripts/ecs-sync-test.sh integration
# or: ./scripts/ecs-sync-test.sh smoke
# first-time deps: ./scripts/ecs-sync-test.sh --bootstrap integration
```

## Learning Outcomes (Phase 1)

By completing Phase 1, you've learned:

1. **OCI Runtime Spec** - Understanding container bundle format and config.json
2. **JSON Parsing** - Using jansson library for spec parsing
3. **State Management** - Persisting container state to disk
4. **CLI Design** - Building a command-line interface with getopt
5. **Project Structure** - Organizing a C project with headers, sources, and tests

## Next Steps

Continue with **Phase 2** to implement:
- Linux namespaces for process isolation
- cgroups for resource management
- Filesystem operations (pivot root, mounts)
- Actual container process execution

This will enable running real containers!

## Resources

- [OCI Runtime Spec](https://github.com/opencontainers/runtime-spec)
- [Linux Namespaces](https://man7.org/linux/man-pages/man7/namespaces.7.html)
- [cgroups v2](https://man7.org/linux/man-pages/man7/cgroups.7.html)
- [Kata Containers](https://katacontainers.io/)
- [Firecracker](https://firecracker-microvm.github.io/)

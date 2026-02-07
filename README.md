# nano-kata: Educational OCI Container Runtime

A minimal OCI-compatible container runtime written in C, supporting both pure container and VM-based execution via Firecracker.

## Project Status: Phase 1 Complete ✅

### What's Implemented

#### Phase 1: Foundation & OCI Spec Handling
- [x] Project structure and build system (Makefile)
- [x] Main header files defining core data structures
- [x] Command-line interface with create/start/delete/state commands
- [x] OCI spec parser (config.json with jansson)
- [x] Container state persistence (JSON files in /run/nano-kata)
- [x] Basic integration tests

## Building

### Prerequisites
```bash
# On Ubuntu/Debian
sudo apt-get install gcc make libjansson-dev

# On macOS
brew install jansson
```

### Compile
```bash
make
```

This produces `bin/nk-runtime` binary.

## Usage Examples

### Basic Commands

```bash
# Show help
./bin/nk-runtime --help

# Show version
./bin/nk-runtime --version

# Create a container
./bin/nk-runtime create --bundle=./tests/bundle mycontainer

# Start a container
./bin/nk-runtime start mycontainer

# Query container state
./bin/nk-runtime state mycontainer

# Delete a container
./bin/nk-runtime delete mycontainer
```

**Note**: Container state is stored in the local `run/` directory (no root required).

### Testing

```bash
# Run integration tests
./tests/integration/test_basic.sh
```

## Project Structure

```
nano-kata/
├── include/
│   ├── nk.h                 # Main API and data structures
│   ├── nk_oci.h             # OCI spec handling
│   ├── nk_container.h       # Container operations (Phase 2)
│   ├── nk_vm.h              # VM operations (Phase 3)
│   └── common/state.h       # State management
├── src/
│   ├── main.c               # CLI entry point
│   ├── oci/
│   │   └── spec.c           # OCI spec parser
│   └── common/
│       └── state.c          # State persistence
├── tests/
│   ├── bundle/
│   │   ├── config.json      # OCI spec for testing
│   │   └── rootfs/          # Container rootfs (empty for now)
│   └── integration/
│       └── test_basic.sh    # Basic integration tests
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

To test on your Linux VM:

```bash
# On Mac, copy files to Linux VM
cd ~/ys/mac/vibe-kata
rsync -avz -e "ssh -p 2222" . ys@localhost:~/ys/mac/vibe-kata/

# Or manually in Linux VM:
ssh -p 2222 ys@localhost
cd ~/ys/mac/vibe-kata

# Install dependencies (if needed)
sudo apt-get install gcc make libjansson-dev

# Build
make clean && make

# Run tests
./tests/integration/test_basic.sh
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

# nano-sandbox Documentation

## Quick Start

**New to containers?** Start with [QUICKSTART.md](../QUICKSTART.md)

**Want to understand the architecture?** Read [Architecture Overview](architecture-overview.md)

**Interested in the ecosystem?** See [Ecosystem Position](ecosystem-position.md)

**Debugging an issue?** Check [Command Reference](command-reference.md)

## Documentation Map

### Core Documentation (NEW - Comprehensive)

| Document | Purpose | Audience |
|----------|---------|----------|
| [Architecture Overview](architecture-overview.md) ⭐ | System design, components, data flow | All users |
| [Command Reference](command-reference.md) ⭐ | Detailed command execution flows | Users, developers |
| [Ecosystem Position](ecosystem-position.md) ⭐ | Where it fits in container stack | Architects, learners |
| [VM Mode Plan](vm-mode-plan.md) ⭐ NEW | VM-based execution design | Advanced learners |
| [Kernel Mechanisms](kernel-mechanisms.md) | Linux primitives used | Systems programmers |
| [Execution Flow](execution-flow.md) | Original execution flow diagrams | Reference |
| [Container Ecosystem](container-ecosystem-lifecycle.md) | Image-to-container lifecycle | Context |
| [Exec Design](exec-design.md) | exec command deep dive | Advanced users |
| [Build and Test](build-and-test.md) | Build system, testing workflows | Developers |

### Supporting Documentation

| Document | Purpose |
|----------|---------|
| [README.md](../README.md) | Project overview, quick start |
| [QUICKSTART.md](../QUICKSTART.md) | Get started in 5 minutes |
| [CLAUDE.md](../CLAUDE.md) | Engineering principles |
| [scripts/README.md](../scripts/README.md) | Build/test scripts |

## Learning Paths

### Path 1: Understanding Container Runtimes (Beginner) ⭐

**Goal:** Learn what container runtimes do and how they work

1. **Start:** [QUICKSTART.md](../QUICKSTART.md) - Run your first container
2. **Learn:** [Ecosystem Position](ecosystem-position.md) - Where runtimes fit
3. **Study:** [Architecture Overview](architecture-overview.md) - System components
4. **Practice:** [Command Reference](command-reference.md) - Try each command
5. **Deep Dive:** [Kernel Mechanisms](kernel-mechanisms.md) - Linux primitives

**Time:** 2-3 hours
**Outcome:** You can explain what a container runtime does and how nano-sandbox works

### Path 2: Systems Programming (Intermediate)

**Goal:** Learn Linux containers from a systems programming perspective

1. **Review:** [Architecture Overview](architecture-overview.md) - Component design
2. **Study:** [Kernel Mechanisms](kernel-mechanisms.md) - clone(), namespaces, cgroups
3. **Analyze:** [Execution Flow](execution-flow.md) - Process lifecycle
4. **Read Code:**
   - `src/main.c` - CLI orchestration
   - `src/container/process.c` - Process creation
   - `src/container/namespaces.c` - Namespace setup
   - `src/container/mounts.c` - Rootfs operations
5. **Experiment:** Modify code, add logging, test changes

**Time:** 6-8 hours
**Outcome:** You understand container internals and can modify the runtime

### Path 3: Container Architecture (Advanced)

**Goal:** Understand how nano-sandbox fits in production container systems

1. **Context:** [Ecosystem Position](ecosystem-position.md) - Full stack view
2. **Comparison:** [Ecosystem Position](ecosystem-position.md) - vs runc, crun
3. **Deep Dive:** [Exec Design](exec-design.md) - Session architecture
4. **Integration:** [Build and Test](build-and-test.md) - Testing workflows
5. **Production:** Consider gaps between nano-sandbox and runc

**Time:** 3-4 hours
**Outcome:** You understand production container architecture and design trade-offs

### Path 4: Contributing (Developer)

**Goal:** Contribute features or fixes to nano-sandbox

1. **Principles:** [CLAUDE.md](../CLAUDE.md) - Engineering guidelines
2. **Build:** [Build and Test](build-and-test.md) - Development workflow
3. **Code:** Read relevant source files
4. **Test:** [scripts/README.md](../scripts/README.md) - Test suite
5. **Contribute:** Make PR, address reviews

**Time:** Ongoing
**Outcome:** You can contribute effectively to the project

## Document Summaries

### Architecture Overview ⭐ NEW

**[architecture-overview.md](architecture-overview.md)**

Comprehensive system architecture document with:
- High-level component diagram with all layers
- Detailed explanation of each component's responsibilities
- Component interaction diagrams
- Installation layout
- Design principles and philosophy
- Comparison to other runtimes (runc, crun, kata-containers)

**Best for:** Understanding the big picture of how nano-sandbox works

**Key Diagrams:**
- High-Level Topology (all components)
- Component Deep Dive (each module)
- Installation Layout
- Data Flow

### Command Reference ⭐ NEW

**[command-reference.md](command-reference.md)**

Complete command execution guide with:
- Detailed sequence diagrams for each command (create, start, run, exec, delete, state)
- State transition diagrams
- Error handling documentation
- Best practices for PID 1 and lifecycle management
- Troubleshooting guide

**Best for:** Learning how to use commands and debug issues

**Key Diagrams:**
- Command Dispatch Map
- Create Flow (sequence diagram)
- Start Flow (sequence diagram)
- Run Flow (flowchart)
- Exec Flow (sequence diagram)
- Delete Flow (flowchart)
- State Machine (complete lifecycle)

### Ecosystem Position ⭐ NEW

**[ecosystem-position.md](ecosystem-position.md)**

Explains where nano-sandbox fits in the container ecosystem:
- Full container technology stack (5 layers)
- End-to-end flow from Docker to running container
- What nano-sandbox does vs higher-level tools (Docker, containerd)
- Detailed comparison to runc, crun, Kata Containers
- Integration options (containerd, Podman, direct)
- When to use each runtime

**Best for:** Understanding architectural context and positioning

**Key Diagrams:**
- Container Technology Stack
- Docker Container Creation (sequence diagram)
- Kubernetes Pod Creation (sequence diagram)
- Runtime Comparison Table

### Kernel Mechanisms

**[kernel-mechanisms.md](kernel-mechanisms.md)**

Deep dive into Linux kernel primitives:
- Process creation model (clone vs fork)
- Namespaces (pid, net, ipc, uts, mount, user, cgroup)
- Filesystem isolation (pivot_root)
- Cgroups (resource limits)
- Capability/limits handling
- Parent-child synchronization
- Signals and lifecycle

**Best for:** Understanding how containers work at the kernel level

### Execution Flow

**[execution-flow.md](execution-flow.md)**

Original command flow diagrams:
- Command dispatch map
- create/start/run/exec/delete/state flows
- Lifecycle state machine
- Child process startup sequence
- Failure visibility model

**Best for:** Quick reference for command behavior

### Container Ecosystem Lifecycle

**[container-ecosystem-lifecycle.md](container-ecosystem-lifecycle.md)**

Explains the journey from Dockerfile to running container:
- Image building (BuildKit)
- Distribution (registries)
- Node preparation (containerd)
- Runtime execution (nano-sandbox)
- Artifact contracts (image, bundle, state)
- Integration directions

**Best for:** Understanding the full container pipeline

### Exec Design

**[exec-design.md](exec-design.md)**

Detailed design of the exec command:
- Mode selection (interactive vs command)
- Lifecycle invariants
- Host prerequisites (/proc, nsenter)
- Failure boundary model
- Implementation notes

**Best for:** Understanding how exec works at the design level

### Build and Test

**[build-and-test.md](build-and-test.md)**

Development workflow documentation:
- Build system (Makefile)
- Build profiles (debug, release, asan)
- Test suites (smoke, integration, perf)
- VM testing workflow (macOS dev → Linux VM)
- Remote testing (ECS)
- Preflight validation

**Best for:** Setting up development environment

### VM Mode Plan ⭐ NEW

**[vm-mode-plan.md](vm-mode-plan.md)**

Comprehensive plan for VM-based container execution:
- Architecture design (nano-sandbox vs Kata Containers)
- API specification for VM lifecycle
- Guest agent protocol design
- Firecracker integration details
- Implementation phases (1-6)
- Educational value and learning exercises
- Complete workflow examples

**Best for:** Understanding VM-based container runtimes and planning implementation

## Diagram Index

### Architecture Diagrams

| Diagram | Location | Shows |
|---------|----------|-------|
| High-Level Topology | [architecture-overview.md](architecture-overview.md) | All components and connections |
| Component Deep Dive | [architecture-overview.md](architecture-overview.md) | Each component's internals |
| Installation Layout | [architecture-overview.md](architecture-overview.md) | File system structure |
| Command Dispatch | [command-reference.md](command-reference.md) | How commands are routed |
| State Machine | [command-reference.md](command-reference.md) | Container lifecycle states |

### Flow Diagrams

| Diagram | Location | Shows |
|---------|----------|-------|
| Create Flow | [command-reference.md](command-reference.md) | create command execution |
| Start Flow | [command-reference.md](command-reference.md) | start command execution |
| Run Flow | [command-reference.md](command-reference.md) | run command execution |
| Exec Flow | [command-reference.md](command-reference.md) | exec command execution |
| Delete Flow | [command-reference.md](command-reference.md) | delete command execution |
| Parent-Child Sync | [command-reference.md](command-reference.md) | Process creation handshake |

### Sequence Diagrams

| Diagram | Location | Shows |
|---------|----------|-------|
| Docker Container Creation | [ecosystem-position.md](ecosystem-position.md) | Docker → containerd → runtime |
| Kubernetes Pod Creation | [ecosystem-position.md](ecosystem-position.md) | K8s → kubelet → runtime |
| Create Command | [command-reference.md](command-reference.md) | CLI → State → OCI → Filesystem |
| Start Command | [command-reference.md](command-reference.md) | CLI → Parent → Child → Kernel |
| Exec Command | [command-reference.md](command-reference.md) | CLI → nsenter → Container |
| Rootfs Setup | [architecture-overview.md](architecture-overview.md) | Child → Kernel mount operations |

### Ecosystem Diagrams

| Diagram | Location | Shows |
|---------|----------|-------|
| Full Stack | [ecosystem-position.md](ecosystem-position.md) | Complete container technology stack |
| Image to Container | [ecosystem-position.md](ecosystem-position.md) | Image layers → rootfs |
| Runtime Comparison | [ecosystem-position.md](ecosystem-position.md) | runc vs crun vs nano-sandbox |

## Key Concepts

### OCI Runtime Specification

The **Open Container Initiative Runtime Specification** defines:
- **Bundle format:** `config.json` + `rootfs/` directory
- **Lifecycle:** create, start, delete, state
- **Config:** process, root, mounts, namespaces, resources
- **Compliance:** All compliant runtimes work the same way

**Learn more:** [OCI Runtime Spec](https://github.com/opencontainers/runtime-spec)

### Namespaces

Linux namespaces provide isolation:
| Namespace | Isolates | Clone Flag |
|-----------|----------|------------|
| PID | Process IDs | `CLONE_NEWPID` |
| Network | Network stack | `CLONE_NEWNET` |
| IPC | IPC mechanisms | `CLONE_NEWIPC` |
| UTS | Hostname | `CLONE_NEWUTS` |
| Mount | Mount table | `CLONE_NEWNS` |
| User | UID/GID mapping | `CLONE_NEWUSER` |
| Cgroup | Cgroup root | `CLONE_NEWCGROUP` |

**Learn more:** [kernel-mechanisms.md](kernel-mechanisms.md)

### cgroups

**Control groups (cgroups)** limit and account resources:
- **cgroups v2** - Unified hierarchy (modern)
- **cgroups v1** - Separate hierarchies (legacy)

nano-sandbox supports cgroups v2:
- Memory limits (memory.max)
- CPU limits (cpu.weight, cpu.max)
- Process limits (pids.max)

**Learn more:** [kernel-mechanisms.md](kernel-mechanisms.md)

### Root Filesystem (rootfs)

The container's filesystem view:
- **pivot_root()** - Switch to new root
- **Pseudo-filesystems** - Mount /proc, /sys, /dev
- **Isolated** - Separate from host filesystem

**Learn more:** [kernel-mechanisms.md](kernel-mechanisms.md)

## Common Tasks

### Run Your First Container

```bash
# Install
make install

# Create
nk-runtime create --bundle=/usr/local/share/nano-sandbox/bundle test1

# Start
nk-runtime start test1

# Enter container
nk-runtime exec test1

# Cleanup
nk-runtime delete test1
```

**Guide:** [QUICKSTART.md](../QUICKSTART.md)

### Understand What's Happening

```bash
# Enable educational mode
nk-runtime -E create --bundle=/usr/local/share/nano-sandbox/bundle test1
nk-runtime -E start test1

# See explanations of each step
```

**Guide:** [Architecture Overview](architecture-overview.md)

### Debug Container Issues

```bash
# Check state
nk-runtime state test1

# Check logs (if running attached)
nk-runtime start -a test1

# Check process
ps aux | grep test1

# Check namespaces
ls -la /proc/<pid>/ns/

# Check cgroup
cat /sys/fs/cgroup/nano-sandbox/test1/cgroup.procs
```

**Guide:** [Command Reference](command-reference.md)

### Build and Test

```bash
# Build
make

# Run tests
./scripts/test.sh

# Run benchmarks
./scripts/bench.sh
```

**Guide:** [Build and Test](build-and-test.md)

## FAQ

### Q: Is nano-sandbox production-ready?

**A:** No. nano-sandbox is an **educational project**. It lacks:
- Comprehensive security hardening
- Extensive testing
- Full feature coverage
- Performance optimization

Use **runc** or **crun** for production.

### Q: Can I use nano-sandbox with Docker?

**A:** Yes, but not recommended. You can configure containerd to use nano-sandbox as the runtime, but you'll miss production features.

**Better:** Use nano-sandbox for learning, then use runc for production.

### Q: How is nano-sandbox different from runc?

**A:** Key differences:

| Feature | nano-sandbox | runc |
|---------|--------------|------|
| Purpose | Educational | Production |
| Language | C | Go |
| LOC | ~4,300 | ~15,000 |
| Logging | Extensive | Minimal |
| Documentation | Extensive | Minimal |
| Features | Basic | Full |

### Q: Can I contribute to nano-sandbox?

**A:** Yes! Please:
1. Read [CLAUDE.md](../CLAUDE.md) for principles
2. Check [Build and Test](build-and-test.md) for workflow
3. Write tests for new features
4. Follow the code style
5. Submit PR with clear description

### Q: Where can I learn more about containers?

**A:** Resources:
- [OCI Runtime Spec](https://github.com/opencontainers/runtime-spec)
- [Linux Namespaces](https://man7.org/linux/man-pages/man7/namespaces.7.html)
- [cgroups v2](https://man7.org/linux/man-pages/man7/cgroups.7.html)
- [runc source code](https://github.com/opencontainers/runc)
- [Kata Containers](https://katacontainers.io/)

## Glossary

**Term** | **Definition**
--------|--------------
**OCI Runtime** | Program that executes containers per OCI spec
**Bundle** | Directory with config.json + rootfs/
**Rootfs** | Container's root filesystem
**Namespace** | Kernel isolation mechanism
**Cgroup** | Kernel resource limiting mechanism
**pivot_root** | System call to switch root filesystem
**Containerd** | High-level container runtime (calls OCI runtime)
**Docker** | Container management platform
**Kubernetes** | Container orchestration platform
**Image** | Packaged application with filesystem layers
**Registry** | Storage for container images
**PID 1** | First process in container
**Exec** | Execute command in running container

## Getting Help

**Documentation didn't answer your question?**

1. Check [README.md](../README.md) for overview
2. Check existing GitHub Issues
3. Read source code - it's well commented!
4. Ask in discussions (if enabled)

**Found a bug?**

1. Check if already reported
2. Create minimal reproduction case
3. File issue with:
   - nano-sandbox version
   - Linux distribution/version
   - Steps to reproduce
   - Expected vs actual behavior
   - Logs/output

**Want to contribute?**

1. Read [CLAUDE.md](../CLAUDE.md)
2. Check [Build and Test](build-and-test.md)
3. Pick a good first issue
4. Follow contribution guidelines

---

**Last Updated:** 2025-02-09
**Documentation Version:** 1.0

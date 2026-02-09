# Documentation

This folder contains deep-dive technical documentation for `nano-sandbox`.

## Reading Order

1. [Architecture Overview](architecture.md)
2. [Container Ecosystem Lifecycle](container-ecosystem-lifecycle.md)
3. [Execution Flow](execution-flow.md)
4. [Exec Design](exec-design.md)
5. [Kernel Mechanisms](kernel-mechanisms.md)
6. [Build and Test Pipeline](build-and-test.md)

## Scope

These docs describe:
- High-level component boundaries
- Ecosystem boundaries (Dockerfile/image/containerd/runtime/kernel)
- Exact command execution paths (`create`, `start`, `run`, `exec`, `delete`, `state`)
- `exec` architecture decisions (session model, invariants, failure boundaries)
- Linux primitives used under the hood (namespaces, mounts, cgroups, process model)
- Local/VM/ECS build-test workflows and reliability checks
- Mermaid diagrams that render directly in GitHub

## Source of Truth

Behavioral truth is always the code in:
- `src/main.c`
- `src/container/*.c`
- `src/common/state.c`
- `scripts/*.sh`
- `Makefile`

Use this folder as the architectural map, and source files as implementation details.

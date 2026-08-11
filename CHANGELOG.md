# Changelog

## 1.1.0 — 2026-08-11

1.1 extends WCM Core from the initial runtime implementation into a deployment-oriented embedded library while preserving the hardware-neutral boundary.

### Runtime hardening

- Runtime-owned monotonic `clock_read()` interface with explicit port-level read failures.
- Actual controller, commit-gate, actuator-dispatch, and whole-step timing observation.
- Internal-only commit authority and per-module actuator authority masks.
- Immutable Snapshot copies and final immutable resource/constraint/safety validation.
- Non-wrapping 64-bit World Epoch, dependency, Witness, and Anchor sequencing.
- Epoch cutover prevents pre-break observations from Rebinding a new World.
- Safe output is established at initialization and re-established on World Break.
- Clock-read failure, actuator failure, and safe-output failure are fail-closed.

### Runtime integration

- Multiple independent SPSC producer ingress queues with deterministic timestamp merge.
- Bounded ingress folding and explicit backlog deferral.
- POSIX port with monotonic clock and commit exclusion.
- Bare-metal/RTOS binding contract with no RTOS dependency in Core.
- Runtime top-level configuration is copied at initialization; descriptor arrays remain immutable integration-owned data.

### Deployment and operations

- Deployment ID and configuration revision carried in operational records.
- Deterministic configuration fingerprint over control-contract metadata.
- Optional periodic configuration-integrity checking.
- Copied runtime health snapshots.
- Fixed-capacity lifecycle/fault event journal.
- Module and actuator execution statistics for target qualification.
- Hardware-neutral A/B Anchor persistence backend with a canonical, endian-defined storage image.
- Deployment manifest and target-qualification record schemas, qualification checklist, operations guide, compatibility policy, and target-port template.
- Versioned configuration-fingerprint and Anchor-format identifiers for deployment traceability.
- CMake Presets, pkg-config metadata, Doxygen input, clang-tidy policy, install-consumer validation, and archive verification.

## 1.0.0 — 2026-08-10

Initial implementation of the Witness–Commit runtime model.

# WCM Core 1.1

WCM Core is a hardware-agnostic C17 control runtime for embedded systems that need an explicit boundary between computation and physical actuation.

The runtime implements the Witness–Commit Model (WCM): observations become time-bounded **Witnesses**, controllers run against immutable **Snapshots**, controllers produce **Intents**, and only the Runtime Executor can commit an Intent after time, dependency, capability, resource, constraint, and safety conditions have been revalidated.

WCM Core is delivered as a static library. It contains no board support package, device driver, vehicle model, navigation stack, network protocol, sensor payload logic, or deployment policy. Those remain application responsibilities behind explicit target interfaces.

## What 1.1 provides

The 1.1 line is structured for integration into long-lived embedded products rather than as a standalone algorithm sample:

- fixed-capacity runtime with no heap allocation in the Core path;
- one serialized Runtime Executor and independent SPSC producer ingress queues;
- runtime-owned monotonic time reads and measured WCET enforcement;
- immutable Snapshot input and private actuator commit authority;
- module-to-actuator authority masks and final immutable validation;
- World Break, safe-output handling, fresh-evidence Rebind, and epoch cutover;
- bounded ingress folding and explicit backlog behavior;
- caller-owned runtime storage with TINY, STANDARD, and EXTENDED profiles;
- runtime health snapshots, lifecycle/fault event journal, module and actuator timing statistics;
- deployment ID, configuration revision, and deterministic configuration fingerprint;
- optional periodic configuration-integrity checking;
- A/B Anchor persistence helpers with a hardware-neutral storage backend;
- compiled POSIX integration port and a bare-metal binding contract;
- CMake package export, pkg-config metadata, CMake Presets, qualification scripts, and source-package verification.

## Build

```sh
cmake --preset release
cmake --build --preset release
ctest --preset release
./build/release/wcm_size_report
./build/release/wcm_reference_loop
```

Without presets:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DWCM_PROFILE=STANDARD
cmake --build build
ctest --test-dir build --output-on-failure
```

Profiles set compile-time capacity limits. Runtime memory remains bounded and caller-owned.

## Install and consume

```sh
cmake --install build/release --prefix /opt/wcm
```

CMake:

```cmake
find_package(WCMCore 1.1 REQUIRED)
target_link_libraries(my_firmware PRIVATE WCM::Core)
```

pkg-config:

```sh
pkg-config --cflags --libs wcm-core
```

On POSIX, `WCM::PortPOSIX` adds a tested monotonic clock and commit-guard adapter. Bare-metal and RTOS integrations bind equivalent primitives without changing Core sources.

## Runtime integration

A production integration supplies immutable descriptors and target services:

1. monotonic `clock_read(user, &time_us)`;
2. commit-guard enter/exit primitives;
3. actuator `apply()` and `safe()` callbacks;
4. observation producers, one producer context per ingress queue;
5. one Runtime Executor context that calls `wcm_runtime_step()`;
6. qualified controller, gate, and actuator timing contracts;
7. operational collection of health, fault, and event records.

The normal control path is:

```text
producer -> SPSC ingress -> Witness -> immutable Snapshot
        -> pre-commit admission -> controller -> Intent
        -> ingress fold -> transforms -> commit guard
        -> immutable final validation -> actual-time Concordance check
        -> actuator dispatch
```

## Operations and deployment

WCM exposes operational data without exposing internal runtime storage:

- `wcm_runtime_get_health()` returns a copied health snapshot;
- `wcm_runtime_get_module_stats()` reports measured controller execution;
- `wcm_runtime_get_actuator_stats()` reports dispatch timing and failures;
- `wcm_runtime_read_event()` exposes a bounded lifecycle/fault journal;
- `wcm_runtime_get_last_fault()` provides the latest typed fault record;
- `wcm_runtime_config_fingerprint()` provides a stable control-contract fingerprint;
- `wcm_anchor_backend_*()` integrates durable A/B Anchor storage without assuming Flash, EEPROM, FRAM, or a filesystem.

Deployment-manifest and target-qualification schemas are provided under `deploy/`. They are intentionally external to the runtime; WCM does not parse configuration files in the control path.

## Qualification boundary

The source release verifies Core semantics and the supplied POSIX port. A target integration must qualify values that only the deployed platform can establish, including controller WCET, commit-guard exclusion time, interrupt latency, monotonic-clock uncertainty, actuator dispatch WCET, physical effect latency, brownout behavior, and electrical safe state.

This is target qualification, not a missing Core feature. WCM provides the runtime contracts, counters, health snapshots, and deployment records needed to carry out that qualification without coupling the library to a specific MCU or board.

Start with:

- [Integration](docs/INTEGRATION.md)
- [Deployment](docs/DEPLOYMENT.md)
- [Qualification](docs/QUALIFICATION.md)
- [Operations](docs/OPERATIONS.md)
- [Timing](docs/TIMING.md)
- [Concurrency](docs/CONCURRENCY.md)
- [Fault model](docs/FAULT_MODEL.md)
- [Persistence](docs/PERSISTENCE.md)
- [Compatibility](docs/COMPATIBILITY.md)
- [Engineering notes for 1.1](docs/ENGINEERING_1_1.md)

License: Apache-2.0.

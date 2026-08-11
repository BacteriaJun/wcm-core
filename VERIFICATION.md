# WCM Core 1.1 verification record

Date: 2026-08-11
Release: 1.1.0
ABI: 0x0101
Configuration fingerprint format: 1
Anchor storage format: 2

This record captures the source-release verification performed for WCM Core 1.1. Target qualification records are separate because target WCET, interrupt behavior, clock properties, electrical safe state, physical effect latency, and brownout behavior are properties of the deployed integration rather than the hardware-neutral Core.

## Toolchain matrix

The complete release pipeline passed with:

- GCC 14.2.0: TINY, STANDARD, EXTENDED;
- Clang 17.0.0: TINY, STANDARD, EXTENDED;
- CMake 3.31.6;
- Ninja 1.12.1;
- warnings enabled: `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`;
- hosted hardening build with `-fstack-protector-strong`.

Every compiler/profile build completed the Core boundary suite, POSIX concurrency stress, integration skeleton, and 100,000-step reference loop where enabled.

## Runtime semantic verification

Core boundary/unit/integration suite:

```text
ALL PASS (61)
```

The suite covers the control-authority and continuity paths, including actual commit-time clock revalidation, controller/gate/actuator WCET enforcement, final immutable validation, module-to-actuator authority, future/out-of-order observations, multi-producer timestamp merge, queue capacity/backlog handling, counter exhaustion, World Break, epoch cutover, evidence-backed Rebind, safe initialization/stop, actuator failure, fail-stop behavior, end-of-step clock failure, immutable Snapshot storage, canonical Anchor encoding, persistence corruption fallback, configuration-integrity checks, operations records, and runtime statistics.

CTest release targets:

- `wcm_core_tests` — pass;
- `wcm_posix_stress` — pass;
- `wcm_integration_skeleton` — pass;
- `wcm_reference_loop` — pass.

Reference loop result on the STANDARD host build:

```text
steps=100000 commits=99999 world_breaks=1 yield=1.000000 runtime_bytes=12808
```

The single non-commit is expected at the injected World Break.

## Concurrency and dynamic analysis

- AddressSanitizer + UndefinedBehaviorSanitizer: pass;
- ThreadSanitizer: pass for Core tests, POSIX concurrent producer stress, and integration skeleton;
- GCC `-fanalyzer`: pass;
- production source heap-use audit: pass;
- public commit-authority audit: pass;
- production fake/mock surface audit: pass.

The supplied POSIX port was exercised with independent concurrent producers and a serialized Runtime Executor/commit guard. The target-port template is compile-checked in every normal build so public API drift is detected by CI.

## Package and consumer verification

- `cmake --install`: pass;
- independent `find_package(WCMCore 1.1)` consumer: pass;
- installed `WCM::Core`: pass;
- installed `WCM::PortPOSIX`: pass when enabled;
- independent pkg-config consumer using `wcm-core.pc`: pass;
- source archive manifest verification: pass in the release package stage;
- source archive clean rebuild: pass in the release package stage.

WCM Core and the POSIX reference port are built as static libraries. The Core does not depend on an RTOS, board support package, filesystem, network stack, or target driver implementation.

## Deployment records and persistence

- deployment-manifest example validates against the shipped Draft 2020-12 schema;
- target-qualification example validates against the shipped Draft 2020-12 schema;
- configuration fingerprint version is exported in `wcm_build_info_t`;
- Anchor format version is exported in `wcm_build_info_t`;
- Anchor format 2 uses the canonical little-endian `WCM_ANCHOR_IMAGE_BYTES` storage image rather than a raw C structure;
- canonical byte-layout test: pass;
- A/B backend round trip: pass;
- corrupted-newest-slot fallback: pass;
- backend read-back verification: pass.

## Host ABI size record

These are GCC 14.2.0 x86-64 host measurements. They are release regression data, not target-MCU size claims.

| Profile | `wcm_runtime_storage_required()` | Reserved caller storage |
|---|---:|---:|
| TINY | 4,712 B | 8,192 B |
| STANDARD | 12,808 B | 20,480 B |
| EXTENDED | 36,112 B | 49,152 B |

Selected STANDARD host ABI measurements:

```text
wcm_value_t=16
wcm_worldstamp_t=24
wcm_witness_t=56
wcm_intent_t=48
wcm_snapshot_t=416
wcm_module_runtime_t=32
wcm_rebind_runtime_t=24
wcm_witness_store_t=4104
wcm_ingress_t=1296
wcm_capability_graph_t=416
wcm_event_record_t=56
wcm_health_snapshot_t=424
wcm_anchor_slot_t=152
runtime_storage_required=12808
runtime_storage_reserved=20480
```

A production target must rerun `wcm_size_report` with its own compiler, ABI, profile, link settings, and optimization flags.

## Release disposition

The source release satisfies the repository criteria for a deployment-oriented WCM Core 1.1 library. A product integration completes the separate target-qualification process in `docs/QUALIFICATION.md` and should archive the resulting `deploy/qualification-record` with its deployment manifest and release digest.

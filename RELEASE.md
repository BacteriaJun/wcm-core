# WCM Core 1.1.0 release notes

WCM Core 1.1 is a deployment-oriented release of the Witness–Commit runtime. The release keeps the Core independent of real hardware while making the integration boundary explicit enough for firmware, platform, verification, and operations teams to use the same runtime contract.

## Integration model

The application owns target-specific clocks, interrupt/critical-section primitives, actuator drivers, storage drivers, sensor acquisition, and product policy. WCM Core owns Witness state, module scheduling, Concordance checks, commit authority, capability/Rebind state, and runtime diagnostics.

The installed package exports:

- `WCM::Core`;
- `WCM::PortPOSIX` when enabled;
- CMake package metadata;
- `wcm-core.pc` for pkg-config;
- bare-metal binding headers.

## Operational additions

1.1 includes copied health snapshots, lifecycle/fault event records, per-module and per-actuator timing statistics, deployment identity, configuration fingerprints, and hardware-neutral Anchor persistence helpers with a canonical storage image. These interfaces are intended for qualification harnesses, supervisory tasks, field diagnostics, and manufacturing/deployment records. Versioned configuration-fingerprint and Anchor-format identifiers are exported for traceability across release tooling.

## Release acceptance

A source package is accepted only after the compiler/profile matrix, boundary suite, POSIX concurrency stress, reference loop, sanitizers, static analysis, public-API audit, install/consumer check, pkg-config consumer check, source manifest validation, and independent archive rebuild pass.

See `VERIFICATION.md` for the recorded release results and `docs/QUALIFICATION.md` for target acceptance work.

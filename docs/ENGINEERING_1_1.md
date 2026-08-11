# 1.1 engineering evolution

The 1.1 engineering line tightens the boundary between the generic runtime and a deployed physical target. The goal is to keep WCM portable while leaving enough explicit contracts for an integration team to qualify it on real hardware without modifying Core internals.

The main changes are grouped around five concerns.

## Time

The Runtime Executor reads target monotonic time through a status-returning `clock_read()` callback. Controller execution, commit-gate work, actuator dispatch, and whole-step execution are observable separately. Physical effect latency remains a declared target property and is used in Commit Horizon calculations.

## Authority

Controllers receive immutable Snapshots and return Intents. They do not receive commit or actuator handles. Each module has a declared actuator mask and the runtime validates the final transformed value immediately before dispatch.

## Continuity

World Epoch cutover invalidates transient physical evidence. A new epoch accepts only observations captured at or after the cutover. Capability recovery is evidence-backed through Rebind requirements. Durable non-physical state is kept outside the Witness store and can use the Anchor persistence interface.

## Operations

The runtime now exposes copied health, event, fault, module, and actuator records. These are intended to feed product-specific telemetry or service systems without making those systems part of Core.

## Deployment identity

Each runtime configuration can carry a deployment ID and configuration revision. A deterministic configuration fingerprint covers the scalar control contract and descriptor metadata. The fingerprint deliberately excludes callback addresses and user pointers; it is a deployment traceability aid, not a memory-integrity primitive.

## Hardware boundary

No MCU, board, transport, sensor, actuator technology, or operating system is encoded in WCM semantics. The supplied POSIX port is a tested integration reference. The bare-metal binding and target-port template describe the required primitives for a production target.

## Persistence and release records

Anchor persistence now separates the logical in-memory slot from a fixed canonical storage image, avoiding compiler padding and target-endianness dependence. Deployment and qualification schemas are kept outside the runtime so release engineering can record target evidence without introducing JSON, filesystem, or signing dependencies into Core.

## Measurement semantics

Whole-step timing is treated as an operational measurement, not a control-authority primitive. If a step crosses a World Epoch cutover, elapsed timing is explicitly marked invalid instead of comparing timestamps from different clock domains. Clock read failures at step completion use the same fail-closed path as other control-time reads.

# Concurrency contract

WCM Core 1.1 is a serialized control runtime.

## Runtime Executor

Exactly one context calls executor-owned APIs such as:

- `wcm_runtime_step()`
- `wcm_runtime_world_break()`
- `wcm_runtime_stop()`
- diagnostic getters

Module callbacks run in that executor context. Two module callbacks are never run in parallel by Core 1.1.

## Observation producers

`wcm_runtime_post_observation()` is safe for the single producer assigned to that queue index while the Runtime Executor consumes the queue. One Core queue is SPSC.

Multiple producers use different queue indices. The Runtime Executor merges queue heads by `observed_at`, then producer index for equal timestamps.

If an application needs multiple tasks to share one producer index, serialize them outside Core. The POSIX wrapper uses the port mutex for this purpose.

## Asynchronous World Break

`wcm_runtime_request_world_break()` is the only lifecycle call intended for a non-executor task/ISR. Requests coalesce. The Runtime Executor consumes the pending request at a safe boundary.

## Commit guard

The commit guard makes the final commit boundary linearizable against configured observation publication.

A correct port must ensure that a producer cannot complete a publication while the guard is held. Core checks for unexpected ingress appearing inside the guarded window; such an event is recorded as a port-contract fault and causes a World Break.

The guard must be short. WCM keeps World Break evidence cutover inside the guard but performs safe-output and module-reset callbacks outside it. Actuator `apply()` remains inside the final commit guard and therefore must be bounded and non-blocking.

## Callback restrictions

Controller, transform, validator, actuator, safe, and reset callbacks are trusted same-address-space code.

Callbacks must not:

- allocate from an unbounded/general heap in a control-critical path;
- block waiting for another task;
- call `wcm_runtime_step()` recursively;
- call a producer wrapper that acquires the currently-held commit guard;
- mutate descriptor arrays;
- retain pointers to Snapshot storage after the callback returns.

A callback may request an asynchronous World Break with `wcm_runtime_request_world_break()`.

## Memory isolation

The authority split is structural, not process isolation. C code with arbitrary access to the runtime's address space can corrupt memory. Use an MPU, process boundary, or a higher-level isolation mechanism when module code is not trusted.

## Operational reads

Health, event, metrics, module-stat, and actuator-stat getters copy data from Runtime state but are not a second lock-free telemetry subsystem. Call them from the Runtime Executor or serialize them according to the product integration model. A product that needs concurrent telemetry should copy these records in its executor/service handoff and publish the copy outside Core.

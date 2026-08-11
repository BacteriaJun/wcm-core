# Architecture

WCM Core is organized around a single authority transition: a controller may propose an actuator value, but only the Runtime Executor may commit it.

## Data path

```text
producer context
    |
    v
per-producer SPSC ingress
    |
    v
Witness admission
    |  source policy / timestamp policy / epoch cutover
    v
Witness store
    |
    v
Dependency Lens
    |  module-relative dependency clock
    v
immutable Snapshot + Worldstamp
    |
    v
Pre-Commit Admission
    |
    v
controller
    |
    v
Intent
    |
    v
candidate transforms
    |
    v
commit guard
    |
    +--> fold observations that raced computation
    |
    +--> actual-time Concordance
    |
    +--> final resource/constraint/safety validators
    |
    +--> actual-time Concordance again
    |
    v
actuator apply
```

The runtime is serialized. Asynchronous producers can enqueue observations, but they do not mutate the Witness store. This keeps Core state single-writer while still allowing interrupt/task producers.

## Witness

A Witness is an accepted physical observation with:

- value;
- observation time;
- Livebound;
- World Epoch;
- 64-bit generation;
- quality;
- source-authorized flags.

A Witness is decision-grade evidence, not a generic message. Rejectable messages remain transport events and never enter the Witness store.

## Dependency Lens

A module declares how a Witness transition affects that module's view of the world:

- `WCM_LENS_SAMPLE`: every accepted sample changes the dependency clock;
- `WCM_LENS_EDGE`: valid/invalid transitions change the dependency clock;
- `WCM_LENS_VALIDITY`: validity/quality-class changes change the dependency clock.

The resulting dependency clock is module-relative. It avoids a global generation counter that would invalidate unrelated or lower-rate control decisions.

## Snapshot and Worldstamp

A Snapshot copies the admitted values needed by one controller release. It contains a Worldstamp:

```text
World Epoch
module dependency clock
minimum Livebound of the admitted evidence
```

The controller cannot read the live Witness store through the public API.

## Intent

An Intent contains a proposed value, actuator ID, source module ID, and the Snapshot's Worldstamp. It is not an actuator command.

There is no public commit API. The internal commit path checks that the source module matches the scheduled module and that the target actuator is in that module's `allowed_actuators` set.

## Concordance Gate

The gate rejects an Intent when any of these no longer hold:

1. World Epoch matches;
2. module dependency clock matches;
3. Commit Horizon has not passed;
4. required capabilities remain bound;
5. final resource policy admits the value;
6. final constraint validator accepts the value;
7. final safety validator accepts the value;
8. gate WCET has not been exceeded.

Transforms run before immutable final validation. A later transform therefore cannot silently invalidate an earlier invariant.

## World Break and Rebind

World Break is continuity loss, not just a fault event. The new epoch gets a cutover timestamp. All physical Witnesses are invalidated and capability Rebind state restarts.

Evidence captured before the cutover is not accepted into the new epoch even if it arrives late. This prevents queued pre-break samples from satisfying post-break Rebind requirements.

Anchor persistence is separate from Witness storage. Configuration, calibration, and application identity may be persisted; transient physical belief is not restored as current evidence.

## Resource bounds

Core uses fixed-capacity arrays selected by profile. There is no runtime allocation. Ingress folding is also bounded by `ingress_fold_limit`, so a continuous producer flood cannot turn one `wcm_runtime_step()` call into an unbounded drain.

`wcm_size_report` must be run with the actual target compiler because structure padding and alignment are ABI-dependent.

## Supervision plane

The control path does not call an external logger or telemetry backend. Instead, the runtime maintains bounded operational state that can be copied by the integration layer:

```text
Runtime Executor
    +--> health snapshot
    +--> lifecycle/fault journal
    +--> module timing statistics
    +--> actuator timing statistics
    +--> cumulative metrics
```

This keeps protocol, storage, fleet management, and service tooling outside Core while giving those systems a stable integration surface.

## Deployment metadata

The top-level runtime configuration is copied during initialization. Integration-owned descriptor arrays remain immutable for the runtime lifetime. WCM records a deterministic fingerprint over the scalar control contract and descriptor metadata so a deployed binary can be associated with the configuration that was qualified.

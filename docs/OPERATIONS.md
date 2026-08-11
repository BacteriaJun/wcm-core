# Operations

WCM Core does not include a telemetry protocol or field-service daemon. It exposes copied operational records so the product can integrate them into its own supervision path.

## Health snapshot

`wcm_runtime_get_health()` returns deployment identity, configuration fingerprint, World state, bound capabilities, step timing, ingress backlog, latest fault, and cumulative metrics.

The call is intended for the Runtime Executor or another integration context that follows the product's serialization policy. It does not create an independent concurrent telemetry subsystem inside Core.

Recommended service data:

- runtime version and ABI;
- deployment ID, config revision, config fingerprint;
- World Epoch and World state;
- required/bound capability masks;
- last fault code and sequence;
- maximum observed controller, actuator-dispatch, and runtime-step execution;
- ingress drops/backlog deferrals;
- Concordance yield;
- World Break and fail-stop counts.

## Event journal

`wcm_runtime_read_event()` reads the fixed-capacity lifecycle/fault journal by sequence number. If the requested cursor is older than retained history, the call returns `WCM_ERR_GAP` and supplies the oldest retained record. Consumers should persist their own cursor outside Core.

The journal is for low-rate operational transitions and faults, not high-rate control tracing.

## Qualification statistics

`wcm_runtime_get_module_stats()` and `wcm_runtime_get_actuator_stats()` expose observed execution maxima. These values support qualification and regression monitoring; they do not replace static WCET analysis or target-specific measurement methodology.

## Fail-stop

When the runtime reports `WCM_HEALTH_FAIL_STOP`, the product must not attempt to resume control through normal step calls. Place the surrounding system into its product-defined recovery path and reinitialize WCM only after the target-level cause has been addressed.

## Step timing validity

`wcm_health_snapshot_t.last_step_timing_valid` states whether `last_step_elapsed_us` was measured entirely inside one World Epoch. If a step spans a World Break or clock-domain cutover, the start and completion timestamps are retained for diagnosis but elapsed time is marked invalid rather than subtracting timestamps from different clock domains. Whole-step budget accounting is applied only when the elapsed measurement is valid.

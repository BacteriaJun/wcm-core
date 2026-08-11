# API notes

Include the public surface with:

```c
#include <wcm/wcm.h>
```

## Configuration

Always initialize configuration with:

```c
wcm_runtime_config_t cfg;
wcm_runtime_config_init(&cfg);
```

This sets `struct_size`, `abi_version`, and the default ingress fold limit. `wcm_runtime_init()` rejects an ABI mismatch.

The configuration object and all descriptor arrays referenced by it are immutable integration data for the runtime lifetime.

## Runtime storage

```c
wcm_runtime_t runtime = {0};
WCM_RUNTIME_STORAGE(storage);

wcm_status_t rc = wcm_runtime_init(
    &runtime,
    storage,
    sizeof(storage),
    &cfg);
```

`wcm_runtime_storage_required()` returns the implementation size for the compiled profile and target ABI. `WCM_RUNTIME_STORAGE_BYTES` is the public reservation bound.

## Observation publication

```c
wcm_observation_t obs = {0};
obs.witness_id = 0;
obs.observed_at = timestamp_us;
obs.requested_livebound = timestamp_us + 5000u;
obs.quality = 200u;
obs.value.f32[0] = measurement;

rc = wcm_runtime_post_observation(&runtime, producer_index, &obs);
```

A successful post means the record entered ingress. It is not yet a Witness. Source, timestamp, quality, and epoch policy are applied when the Runtime Executor folds ingress.

`WCM_ERR_FULL` means the producer queue has no free slot. The queue's drop counter is reflected in runtime metrics.

## Controller callback

```c
static wcm_status_t controller(
    const wcm_snapshot_t *snapshot,
    void *user,
    wcm_intent_t *out)
{
    wcm_value_t state = {0};
    wcm_value_t command = {0};

    if (wcm_snapshot_read_value(snapshot, 0u, &state) != WCM_OK)
        return WCM_ERR_STATE;

    command.f32[0] = -0.5f * state.f32[0];
    return wcm_intent_from_snapshot(out, snapshot, 0u, &command);
}
```

The module has no API for physical commit. Returning `WCM_OK` only proposes the Intent.

## Actuator descriptor

```c
static const wcm_actuator_desc_t actuator = {
    .id = 0u,
    .dispatch_wcet_us = 50u,
    .effect_latency_us = 300u,
    .constraint_transform = clamp_candidate,
    .constraint_validate = validate_constraint,
    .safety_transform = shape_safe_candidate,
    .safety_validate = validate_safety,
    .apply = apply_nonblocking,
    .safe = drive_safe_state,
    .user = &driver,
};
```

`effect_latency_us` is the worst-case time from `apply()` invocation to physical effect. `dispatch_wcet_us` is the callback execution bound. They are deliberately separate.

## Executor

```c
rc = wcm_runtime_step(&runtime);
```

The function performs bounded ingress work and due module releases. It may legally return `WCM_OK` without running a controller when capabilities are unbound, evidence is non-viable, or ingress backlog needs to be drained first.

## Continuity

Synchronous executor-owned break:

```c
rc = wcm_runtime_world_break(&runtime, WCM_BREAK_BROWNOUT);
```

Asynchronous task/ISR request:

```c
rc = wcm_runtime_request_world_break(&runtime, WCM_BREAK_BROWNOUT);
```

Controlled shutdown:

```c
rc = wcm_runtime_stop(&runtime);
```

After stop, control and observation publication return `WCM_ERR_STOPPED`.

## Diagnostics

```c
wcm_metrics_t metrics;
wcm_fault_record_t fault;
wcm_module_stats_t module;
wcm_cap_state_t capability;

wcm_runtime_get_metrics(&runtime, &metrics);
wcm_runtime_get_last_fault(&runtime, &fault);
wcm_runtime_get_module_stats(&runtime, module_id, &module);
wcm_runtime_get_capability_state(&runtime, capability_id, &capability);
```

Getters copy data out of the runtime. In 1.1 they are executor-owned calls; if another task needs telemetry, copy the data in the executor and publish it through the application's normal telemetry path.

## Error strings

`wcm_status_string()`, `wcm_commit_code_string()`, `wcm_world_break_reason_string()`, and `wcm_fault_code_string()` are available for diagnostics outside hard real-time paths.

## Deployment and build identity

`wcm_get_build_info()` reports the runtime version, ABI, selected profile, pointer width, storage reservation, and build tag.

`wcm_runtime_config_fingerprint()` hashes control-contract metadata from the runtime config and descriptors. Callback addresses and user pointers are deliberately excluded. `wcm_runtime_check_configuration()` compares the live descriptor metadata with the initialization fingerprint.

## Health and event records

`wcm_runtime_get_health()` copies the runtime health state, deployment identity, World state, capability mask, step timing, metrics, and latest fault.

`wcm_runtime_read_event(runtime, after_sequence, &record)` returns the next retained low-rate lifecycle/fault event. `WCM_ERR_GAP` means the consumer cursor fell behind the fixed-capacity journal; the returned record is the oldest retained event.

`wcm_runtime_get_actuator_stats()` provides dispatch count, failure/overrun counts, maximum observed dispatch time, and configured dispatch/effect bounds.

## Persistence

`wcm_anchor_backend_load()` and `wcm_anchor_backend_commit()` provide a storage-neutral A/B persistence pattern. The backend owns the actual storage medium and synchronization semantics.

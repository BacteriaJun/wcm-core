# Integration guide

This document describes the minimum work required to put WCM Core 1.1 into an embedded application.

## 1. Choose a profile

Start with `STANDARD`. Use `wcm_size_report` with the target compiler before reducing or increasing capacities.

The profile sets maximum Witnesses, modules, capabilities, producer queues, actuators, and Snapshot items. These are compile-time capacities; WCM does not grow them at runtime.

## 2. Allocate the runtime

```c
wcm_runtime_t runtime = {0};
WCM_RUNTIME_STORAGE(runtime_storage);
```

Both objects must remain alive for the runtime lifetime. Do not copy an initialized `wcm_runtime_t` handle and use the copies concurrently.

## 3. Provide immutable descriptors

Create static or otherwise lifetime-stable arrays for:

- `wcm_witness_source_desc_t`
- `wcm_module_desc_t`
- `wcm_capability_desc_t`
- `wcm_actuator_desc_t`

The arrays, dependency lists, Rebind requirements, and callback contexts referenced by them must remain valid and must not be modified while the runtime is active.

`wcm_runtime_init()` validates descriptor ranges, duplicate IDs, graph cycles, dependency references, actuator authority, transform/validator pairing, and ABI version.

## 4. Provide a clock

`clock_read(user, &time_us)` returns a status and writes monotonic microseconds for the current World Epoch.

```c
static wcm_status_t target_clock_read(void *ctx, wcm_time_t *out);
```

Use the same time domain for observation timestamps. If sensor hardware has another clock, convert it before posting.

Set `clock_uncertainty_us` to a conservative upper bound for clock-read/translation uncertainty. The runtime adds this value when making time-sensitive admission and commit decisions.

## 5. Provide a commit guard

The guard establishes the final observation/actuation linearization boundary:

```c
static void commit_enter(void *ctx);
static void commit_exit(void *ctx);
```

All asynchronous producer paths must respect the same exclusion mechanism. The POSIX port does this through `wcm_posix_post_observation()`.

On bare metal, this will commonly be a narrowly scoped interrupt mask. On an RTOS it may be a short scheduler/IRQ-aware critical section. Do not place network I/O, logging, allocation, or other unbounded work in this guard.

## 6. Define Witness sources

A source owns admission policy:

```c
static const wcm_witness_source_desc_t sources[] = {
    {
        .max_live_us = 5000,
        .max_future_skew_us = 20,
        .id = 0,
        .min_quality = 160,
        .timestamp_policy = WCM_TS_STRICT_INCREASING,
        .allowed_flags = WCM_WITNESS_ESTIMATED,
    },
};
```

Observation producers cannot promote themselves beyond `allowed_flags`.

After a World Break, an observation whose `observed_at` precedes the new epoch's cutover is rejected even if it arrives later.

## 7. Define capabilities and Rebind evidence

Capabilities express which control functions are currently backed by enough fresh evidence. A World Break clears the binding state and replays the declared Rebind requirements from fresh observations.

Avoid using capabilities as generic feature flags. They should represent runtime authority that is meaningful to control admission.

## 8. Define modules

A module is run-to-completion and non-blocking. It receives an immutable Snapshot and may return one Intent.

```c
static wcm_status_t controller(
    const wcm_snapshot_t *snapshot,
    void *user,
    wcm_intent_t *intent);
```

Set:

- `period_us`
- `deadline_us`
- measured/conservative `wcet_us`
- required capabilities
- allowed actuators
- dependency list and Dependency Lens

A module that exceeds `wcet_us` does not get its Intent committed.

## 9. Define actuator contracts

Each actuator provides:

- `dispatch_wcet_us`: maximum CPU/driver callback duration;
- `effect_latency_us`: maximum time from invocation of `apply()` until the commanded physical effect is established;
- optional candidate transforms;
- immutable final validators;
- `apply()`;
- `safe()`.

`apply()` should hand work to hardware and return promptly. Do not use it for a long blocking bus transaction. The final commit guard remains held across `apply()`.

If a transform exists, its matching validator is mandatory. Final resource, constraint, and safety validation sees the final transformed value.

## 10. Initialize and run

Use `wcm_runtime_config_init()` before filling the configuration.

```c
wcm_runtime_config_t cfg;
wcm_runtime_config_init(&cfg);
/* assign descriptors, clock, guard, policy callbacks */

wcm_status_t rc = wcm_runtime_init(
    &runtime,
    runtime_storage,
    sizeof(runtime_storage),
    &cfg);
```

Initialization drives every configured actuator through `safe()`. Initialization fails if any safe output cannot be established.

Call `wcm_runtime_step()` from exactly one executor context. Call it often enough to satisfy the periods and deadlines configured for the modules.

## 11. Feed observations

Each queue index is SPSC at the Core boundary. Assign one producer context per index, or serialize multiple application producers before the Core queue.

For POSIX use `wcm_posix_post_observation()`. For an ISR producer on a bare-metal target, post directly only if the target's atomic/critical-section contract is satisfied.

Queue overflow returns `WCM_ERR_FULL` and increments the queue drop counter. Sustained ingress backlog is bounded by `ingress_fold_limit`; control is deferred rather than allowing an unbounded fold to consume the executor cycle.

## 12. Handle lifecycle and faults

Use `wcm_runtime_request_world_break()` from an ISR/task to request an asynchronous continuity cut. It is the concurrency-safe break API.

Use `wcm_runtime_world_break()` from the executor context for a synchronous break.

Use `wcm_runtime_stop()` for controlled shutdown. It establishes safe output and rejects subsequent control and observation calls.

Collect:

- `wcm_runtime_get_metrics()`
- `wcm_runtime_get_last_fault()`
- `wcm_runtime_get_module_stats()`
- `wcm_runtime_get_capability_state()`

Do not use logging callbacks in the hard commit path. Pull diagnostics from the runtime outside the control-critical section.

## 13. Target qualification

Before deployment, measure on the final target:

- controller WCET under worst interrupt/cache/bus conditions;
- commit-gate WCET;
- `apply()` callback WCET;
- end-to-end actuator effect latency;
- clock uncertainty and wrap/reset behavior;
- producer burst rate and queue margin;
- World Break safe-output latency;
- brownout behavior during actuator dispatch.

Use conservative values in descriptors. If the measured bound cannot be made credible, do not encode it as a hard WCM contract.

## 14. Add deployment identity and supervision

Assign `deployment_id` and `config_revision` before initialization. Record the result of `wcm_runtime_config_fingerprint()` in the product's release or manufacturing record.

If descriptor memory is susceptible to accidental writes and the additional periodic work is acceptable, configure `config_check_period_steps`. The runtime then compares the current descriptor fingerprint with the initialization fingerprint and cuts the current World on mismatch.

Pull operational state through copied APIs rather than exposing private runtime memory:

- `wcm_runtime_get_health()`;
- `wcm_runtime_read_event()`;
- `wcm_runtime_get_module_stats()`;
- `wcm_runtime_get_actuator_stats()`.

`step_budget_us` is an operational budget for the complete `wcm_runtime_step()` call. It records an overrun after the step; controller/gate/dispatch WCET contracts remain the pre-dispatch fail-closed timing mechanisms.

## 15. Persistent application state

Use `wcm_anchor_backend_t` when the product needs durable A/B Anchor storage. The backend is intentionally storage-neutral. Flash erase/program rules, filesystem semantics, FRAM access, EEPROM wear, ECC, and power-fail guarantees remain target responsibilities.

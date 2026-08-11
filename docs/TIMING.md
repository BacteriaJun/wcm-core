# Timing contract

WCM uses one 64-bit microsecond time domain per World Epoch.

## Livebound

A Witness is usable only while:

```text
world epoch matches
observed_at <= now <= livebound
```

The source descriptor limits the maximum Livebound extension from `observed_at`.

## Epoch cutover

`wcm_world_t.started_at` records the clock value at the current epoch cutover. An observation captured before that value cannot become evidence in the new epoch. This prevents a delayed pre-break sample from clearing Rebind Debt after continuity has been cut.

For a clock-discontinuity break, the new `started_at` is expressed in the new clock domain.

## Commit Horizon

For an Intent:

```text
Commit Horizon = Livebound - actuator.effect_latency_us
```

`effect_latency_us` is end-to-end from invocation of `apply()` to the commanded physical effect. It is not the same as callback execution time.

## Pre-Commit Admission

Before a controller runs, the runtime checks whether the Snapshot has enough remaining time for:

```text
controller WCET + gate WCET + worst permitted actuator effect latency
```

A non-viable module release is skipped instead of consuming compute on an Intent that cannot legally commit.

## Actual-time validation

WCET is a planning bound, not a substitute for an actual clock read. The runtime measures the controller and rereads its clock through the commit path. It checks Concordance again immediately before actuator dispatch.

A controller WCET overrun fails closed for that Intent. A commit-gate WCET overrun fails closed before dispatch. An actuator callback WCET overrun causes a World Break because dispatch timing integrity is no longer trusted.

## Clock uncertainty

`clock_uncertainty_us` is added conservatively to decision time. Use it for timestamp conversion error, timer read granularity, or other bounded uncertainty that can make the real physical time later than the returned clock value.

## Counter exhaustion

World Epoch, dependency clocks, and Witness generations use 64-bit counters. Semantic counters do not wrap. Exhaustion is treated as a state-integrity failure rather than allowing an ABA equality match.

Ingress ring indices are different: they are transport counters and intentionally use unsigned modular arithmetic with a compile-time capacity bound.

## Clock service failure

The port clock returns a `wcm_status_t` and writes time through an output pointer. This keeps a valid time value of zero distinct from an unavailable timer. A read failure removes timing authority, triggers best-effort safe output, and latches the runtime fail-stop until reinitialization.

## Whole-step budget

`step_budget_us` is optional and measures the full executor call. It is useful for integration-level scheduling regression and watchdog policy. Unlike controller/gate/dispatch WCET checks, it is evaluated after the step and therefore does not authorize or revoke an individual commit.

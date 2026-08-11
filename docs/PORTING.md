# Porting

WCM Core has no scheduler or RTOS dependency. A port supplies time and final-commit exclusion; the application supplies the executor context and device drivers.

## Required services

A production port must provide:

- status-returning monotonic microsecond `clock_read`;
- `commit_guard_enter` / `commit_guard_exit`;
- a publication path in which every asynchronous observation producer respects the same commit guard;
- bounded, non-blocking actuator `apply()` and `safe()` callbacks.

## POSIX port

`WCM::PortPOSIX` is built with the repository. It provides:

- `CLOCK_MONOTONIC` microsecond time;
- a pthread mutex commit guard;
- `wcm_posix_post_observation()` which serializes publication with the final commit boundary.

This port is useful for Linux integration, hardware-in-the-loop hosts, and driver-process validation. It is not intended to supply hard real-time scheduling by itself.

## Bare-metal binding

The installed `wcm/ports/baremetal.h` contains `wcm_baremetal_port_t` and `wcm_baremetal_bind()`.

A typical MCU port maps:

```text
clock_read           -> free-running hardware timer + explicit read status
commit_guard_enter   -> save/mask relevant producer IRQ state
commit_guard_exit    -> restore IRQ state
runtime_step         -> main loop or timer-driven executive
producer queue       -> one sensor ISR/task per producer index
```

Mask only the producer sources that can publish into this runtime. Do not hold a global interrupt mask longer than required by the commit boundary.

## RTOS integration

Run `wcm_runtime_step()` from one task/work item. Other tasks may produce observations using separate producer indices. If task publication can race final commit, the producer's publication wrapper must participate in the same guard used by Core.

Do not run multiple Runtime Executor workers for one runtime instance in 1.1.

## Clock requirements

The control clock need not be UTC and need not survive reset. It must be monotonic within an epoch. A backwards read causes a clock-discontinuity World Break.

Observation timestamps must be translated into this domain before publication. Configure `max_future_skew_us` per source and `clock_uncertainty_us` globally.

## World Break and reset

Core does not treat RAM checkpoint restoration as evidence restoration. After reset/brownout:

1. restore configuration/Anchor data owned by the application;
2. initialize a new runtime;
3. establish safe outputs;
4. feed fresh observations;
5. allow Rebind requirements to restore capabilities.

## Target size

Do not copy host structure byte counts into an MCU budget. Run `wcm_size_report` with the target compiler/profile. Aggregate structure padding is intentionally not frozen across ABIs.

## C atomics

Ingress uses C17 atomics for SPSC publication. The current Core requires always-lock-free `unsigned int` atomics at compile time because producer publication may occur in interrupt context. A target whose compiler implements these operations through a lock or runtime helper needs a port-specific ingress implementation before it can be considered ISR-safe.

## Port error semantics

`clock_read()` must return a WCM status and must not encode failure as a timestamp sentinel. If the timer peripheral or underlying OS service cannot provide a trustworthy monotonic value, return an error.

Commit-guard callbacks are intentionally infallible at the WCM API boundary. A port must establish their prerequisites during platform initialization. If the underlying primitive can fail at runtime, the port should treat that as a platform integrity failure rather than silently returning from the guard callback without exclusion.

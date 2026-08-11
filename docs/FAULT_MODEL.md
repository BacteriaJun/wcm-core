# Fault model

WCM separates bad input, module failure, timing failure, actuator failure, and continuity loss because they require different containment behavior.

## Rejectable observation

Examples:

- quality below source threshold;
- future timestamp beyond allowed skew;
- timestamp regression;
- flags not allowed by source policy;
- observation captured before the current World Epoch cutover;
- unknown source.

Action: drop the record, increment a typed metric, continue the control cycle when bounded ingress work permits.

## Module callback error

A controller returning an error produces no commit. The module error counter is incremented. WCET overrun is separately fault-recorded.

## Timing contract failure

- controller WCET overrun: discard Intent; module remains scheduled;
- commit-gate WCET overrun: fail closed before dispatch and record a fault;
- clock moves backwards: World Break;
- semantic counter exhaustion: World Break/cold state.

An integrator may use diagnostics to escalate repeated module overruns outside Core.

## Actuator dispatch failure

If `apply()` returns an error, Core records `WCM_FAULT_ACTUATOR_DISPATCH_FAILED`, performs an actuator-failure World Break, and drives configured safe outputs.

If the callback exceeds `dispatch_wcet_us`, timing integrity of the actuator domain is treated as lost and a World Break is performed.

## Safe-output failure

If any `safe()` callback fails, Core records the failure and latches the runtime cold/stopped. Normal control cannot resume by merely clearing a metric or Rebinding evidence. Reinitialize only after the application has dealt with the underlying hardware condition.

## World Break

A World Break:

1. increments the World Epoch and records its cutover time;
2. requests safe output;
3. atomically clears queued pre-break ingress at the producer boundary;
4. invalidates all physical Witnesses;
5. resets capability Rebind state;
6. resets module transient runtime state and release phase.

Only Anchor/configuration state managed outside physical Witness storage may be carried across a break.

## Diagnostics

`wcm_runtime_get_last_fault()` returns the latest fault record with sequence, time, World Epoch, subject ID, status, and detail field. Metrics provide cumulative counters. The runtime intentionally does not invoke a logging backend from the hard control path.

## Clock service failure

If the target `clock_read()` callback returns an error, timing authority is unavailable. Core records `WCM_FAULT_CLOCK_READ_FAILED`, attempts safe output, latches fail-stop, and returns `WCM_ERR_CLOCK`. A real timestamp value of zero remains valid and is not used as an error sentinel.

## Configuration contract mutation

When periodic configuration checking is enabled, a change in fingerprinted descriptor metadata records `WCM_FAULT_CONFIG_MUTATION` and cuts the current World as state corruption. This is intended to catch accidental contract mutation; it is not a substitute for MPU/process memory isolation.

## Whole-step budget

`step_budget_us` is an operational diagnostic. `WCM_FAULT_STEP_BUDGET_OVERRUN` is recorded after a completed step when the configured budget is exceeded. Because physical commits may already have occurred during that step, this fault is not itself a pre-dispatch safety barrier.

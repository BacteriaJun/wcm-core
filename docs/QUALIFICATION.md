# Target qualification

WCM Core is hardware-neutral. A product release therefore qualifies the target integration rather than relying on host measurements.

## Required evidence

A production target should record at least:

- compiler, version, flags, and WCM profile;
- `wcm_size_report` output for the target ABI;
- runtime/deployment/config identifiers;
- controller WCET for every registered module;
- commit-gate worst-case time under representative contention;
- commit-guard exclusion time and interrupt masking impact;
- actuator `dispatch_wcet_us` and physical `effect_latency_us`;
- monotonic clock resolution, wrap behavior, failure behavior, and `clock_uncertainty_us`;
- maximum expected producer burst and ingress backlog recovery;
- safe-output behavior at startup, World Break, watchdog/reset, and brownout;
- actuator failure injection results;
- delayed/out-of-order observation behavior;
- stack high-water measurement for Runtime Executor and producer contexts;
- long-duration counter/timestamp soak results appropriate to product lifetime.

## Acceptance tests

Target CI/HIL should include:

1. normal closed-loop operation;
2. controller WCET overrun;
3. commit boundary observation arrival;
4. queue burst/overflow;
5. clock-read failure and discontinuity;
6. World Break and evidence-backed Rebind;
7. actuator dispatch failure/overrun;
8. safe-output failure;
9. brownout/reset around actuator dispatch;
10. persistent Anchor interruption/corruption;
11. configuration-integrity mismatch when enabled;
12. controlled shutdown and cold restart.

## Timing margins

Do not configure declared WCET or effect latency from average measurements. Use a qualification method appropriate to the target, scheduling environment, interrupt configuration, cache/memory system, and product safety requirements.

`step_budget_us` is an operational whole-step budget. An overrun is recorded after the step and is not a substitute for controller/gate pre-dispatch fail-closed timing checks.

## Qualification record

`deploy/qualification-record.schema.json` defines a hardware-neutral record for target evidence. The supplied example is intentionally unqualified and contains placeholders. A product release should archive the completed record together with the deployment manifest, source archive digest, target `wcm_size_report`, and HIL/bench evidence referenced by the record.

The record is not consumed by WCM at runtime. It exists so firmware, controls, validation, and release teams can exchange the same timing and safety contract without adding file parsing or a configuration subsystem to the control path.

# Security and safety reporting

Treat the following as high-severity WCM defects:

- any public or callback path that bypasses runtime-owned actuator commit;
- stale, old-epoch, or pre-cutover evidence accepted as current;
- timestamp/counter behavior that can recreate an old Worldstamp;
- a final actuator value that bypasses configured immutable validators;
- actuator dispatch after a detected WCET/clock/continuity failure;
- failure to establish safe output during initialization, stop, or World Break without latching the runtime cold;
- data races or producer/commit ordering defects in a supplied port;
- descriptor validation errors that permit authority outside configured module/capability/actuator sets.

Do not include operational secrets or live-system credentials in a public report. Use the repository host's private vulnerability reporting channel when available.

WCM's authority boundary is an in-process API/runtime boundary. It is not memory isolation against arbitrary hostile native code in the same address space.

Additional high-severity integration defects include a clock service failure that does not fail closed, configuration-contract mutation that escapes an enabled integrity check, or a persistence backend that reports a successful Anchor commit without durable read-back verification.

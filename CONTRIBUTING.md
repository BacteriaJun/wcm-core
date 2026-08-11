# Contributing

Run the full release checks before sending a change:

```sh
./scripts/verify.sh
```

Changes to timing, authority, continuity, ingress ordering, or World Break behavior require a boundary test that fails on the old behavior and passes on the new behavior.

Core rules:

- no runtime heap allocation;
- one serialized Runtime Executor per runtime instance;
- module and actuator callbacks are bounded and non-blocking;
- controllers return Intents and never receive commit/actuator handles;
- no public commit function;
- actual commit decisions use the runtime clock, not a cached caller timestamp;
- source policy owns observation admission flags;
- physical Witnesses do not survive World Break;
- all candidate modifications are followed by immutable final validation;
- rejectable ingress data is isolated and measured;
- semantic consistency counters never wrap silently;
- platform-specific IRQ/task policy belongs in a port, not in Core.

Keep comments focused on contracts, invariants, and reasons that are not obvious from the code. Avoid commentary that restates the implementation line by line.

Public operational interfaces should return copied data and must not expose pointers into Runtime storage. Changes to persistence format, deployment fingerprint semantics, or installed package metadata require release-note and qualification-document updates.

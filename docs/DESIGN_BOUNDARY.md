# Design boundary

WCM Core deliberately uses ordinary embedded mechanisms where ordinary mechanisms are sufficient: static storage, fixed-capacity queues, SPSC atomics, monotonic clocks, run-to-completion callbacks, explicit WCET contracts, and final actuator validators.

The project-specific design is the control-authority chain that joins those mechanisms:

```text
physical observation
    -> Witness + Livebound
    -> module-relative Dependency Lens
    -> immutable Snapshot + Worldstamp
    -> controller-produced Intent
    -> actual-time Concordance revalidation
    -> runtime-only physical commit
    -> World Break / evidence-backed Rebind when continuity is lost
```

The implementation treats completion of computation and authority to actuate as separate events. A completed controller result can become void because its evidence, dependency view, time budget, capability basis, or World Epoch changed before commit.

The repository does not claim historical priority for the individual ingredients or terminology. Engineering claims in release documentation are limited to behavior implemented and verified by the code in that release.

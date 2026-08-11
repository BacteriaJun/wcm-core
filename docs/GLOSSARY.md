# Glossary

**Witness**  
An observation accepted by source policy and admitted as physical evidence for the current World Epoch.

**Livebound**  
The latest time at which a Witness may still support a physical control decision.

**World Epoch**  
A continuity interval in which runtime physical-evidence assumptions are allowed to relate to each other.

**Epoch cutover (`started_at`)**  
The control-clock time at which the current World Epoch began. Evidence captured before this value cannot enter the epoch.

**Dependency Lens**  
A module-specific rule defining which transitions of a dependency advance that module's dependency clock.

**Worldstamp**  
The World Epoch, module-relative dependency clock, and minimum Livebound attached to a Snapshot/Intent.

**Snapshot**  
An immutable copy of the Witness values admitted for one module release.

**Intent**  
A controller proposal for an actuator. An Intent has no direct physical commit authority.

**Commit Horizon**  
Latest admissible dispatch time after accounting for the target actuator's physical effect latency.

**Pre-Commit Admission**  
The check performed before controller execution to determine whether enough time remains for the declared controller, gate, and physical-effect bounds.

**Concordance**  
The condition that an Intent still refers to the current World Epoch, current dependency view, live evidence, and currently-bound capabilities at commit time.

**Concordance Gate**  
The internal final authority path that revalidates Concordance, final policy invariants, timing, and actuator authority before dispatch.

**World Break**  
An explicit cut in the runtime's physical-continuity assumption. It invalidates Witnesses and starts Rebind for the new epoch.

**Rebind**  
The process by which fresh post-break evidence restores a capability.

**Rebind Debt**  
The outstanding Witness requirements that must be satisfied before a capability can become bound.

**Anchor**  
Application-managed persistent state that may legally survive a World Break, such as configuration or calibration. Anchor data is not a physical Witness.

**Concordance Yield**  
Ratio of committed Intents to proposed Intents. It is a runtime efficiency/validity metric, not a safety proof.

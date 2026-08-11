# Release criteria

A WCM Core 1.1 source package is releaseable only when all of the following pass from a clean tree.

## Build

- GCC and Clang build `TINY`, `STANDARD`, and `EXTENDED` with project warnings treated as errors.
- `wcm_runtime_storage_required()` remains within the selected profile reservation.
- no production Core source depends on the host fake clock or a test-support header.

## Runtime semantics

- boundary suite passes;
- deterministic reference control loop passes 100,000 steps with an injected World Break;
- concurrent POSIX producer stress passes with zero port-contract and internal-commit errors;
- controller, gate, and actuator WCET fail-closed cases are covered;
- clock discontinuity, old-epoch evidence, pre-cutover late evidence, queue overflow/backlog, actuator failure, safe-output failure, and filter composition are covered.

## Dynamic/static analysis

- AddressSanitizer + UndefinedBehaviorSanitizer suite passes;
- ThreadSanitizer passes the Core and concurrent POSIX stress suite;
- GCC `-fanalyzer` build passes;
- repository heap-use search finds no `malloc/calloc/realloc/free` in Core/port code.

## Package integration

- `cmake --install` succeeds;
- an independent consumer resolves `find_package(WCMCore 1.1)` and links `WCM::Core`;
- when the POSIX port is packaged, the independent consumer also resolves `WCM::PortPOSIX`;
- the source manifest verifies after archive extraction;
- archive contains no build trees, compiled objects, caches, or local editor state.

## Documentation

- API, timing, concurrency, fault, and port contracts match the shipped headers;
- release notes identify source-incompatible changes;
- target-specific timing/electrical qualification requirements are explicit and are not replaced by host measurements.

## Deployment interfaces

- health/event/module/actuator diagnostic APIs are covered by tests;
- Anchor backend round-trip, canonical storage-image encoding, corruption fallback, and read-back verification pass;
- status-returning target clock failure is covered by a fail-closed test;
- configuration fingerprint and integrity-check behavior are covered;
- pkg-config metadata is installed and consumed by an independent build;
- deployment and qualification-record schemas parse and their supplied examples validate against the schemas when the verifier is available;
- compatibility documentation records ABI, configuration-fingerprint, and Anchor-format version domains.

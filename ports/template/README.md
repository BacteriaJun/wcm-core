# Target port template

A target port should bind WCM Core to platform primitives without changing Core sources.

The minimum target contract is:

- a status-returning monotonic microsecond clock;
- commit-guard enter/exit functions that serialize configured observation producers with final physical commit;
- one Runtime Executor context;
- one producer owner per configured SPSC ingress queue;
- target actuator `apply()` and `safe()` callbacks;
- product-specific persistence backend when Anchor storage is used.

Keep target driver headers out of public WCM headers. Platform code may wrap RTOS, interrupt-controller, timer, or storage APIs in a private board/application layer and pass only WCM callbacks into `wcm_runtime_config_t`.

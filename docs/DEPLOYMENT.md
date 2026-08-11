# Deployment

A WCM deployment consists of a Core build, immutable control descriptors, target-service callbacks, a qualification record, and product-specific firmware around the Runtime Executor.

## Deployment identity

Set `deployment_id` to a stable product/integration identifier and increment `config_revision` whenever a deployed control contract changes. Record `wcm_runtime_config_fingerprint()` after descriptors are finalized.

The repository includes `deploy/deployment-manifest.schema.json` and an example manifest. The manifest is outside the runtime and may be produced by CI, manufacturing, or release tooling.

## Configuration lifetime

`wcm_runtime_init()` copies the top-level `wcm_runtime_config_t`. Descriptor arrays referenced by the config, dependency lists, Rebind requirements, callbacks, and callback context objects remain integration-owned and must stay valid and immutable for the runtime lifetime.

For deployments that want periodic detection of accidental descriptor mutation, set `config_check_period_steps`. A mismatch is treated as a control-contract integrity failure and cuts the current World.

## Startup

A deployment is ready for normal execution only after:

1. target services are initialized;
2. immutable descriptors have been constructed;
3. `wcm_runtime_init()` succeeds;
4. all actuator `safe()` callbacks have succeeded;
5. required capabilities have Rebound from fresh evidence.

Do not infer readiness from process startup alone. Use `wcm_runtime_get_health()` and required-capability checks.

## Controlled shutdown

Call `wcm_runtime_stop()` from the Runtime Executor context. WCM performs an application World Break, requests safe actuator states, and latches the runtime stopped. Reuse requires reinitialization.

## Upgrade

Treat the ABI version, runtime version, deployment manifest, configuration revision, and Anchor format as separate compatibility fields. Do not restore transient Witness or Intent state across an application/runtime upgrade.

## Release records

A deployment should ship with two external records:

- `deployment-manifest`: identifies the exact runtime/configuration build installed in the product;
- `qualification-record`: records target timing, safe-state, ingress, fault-injection, and verification evidence used to approve that build.

Schemas and non-qualified examples are provided in `deploy/`. These files deliberately remain outside Core and may be generated or signed by the product's own release system.

#include <string.h>
#include "wcm/wcm.h"

typedef struct {
    wcm_time_t now_us;
    wcm_value_t applied;
    unsigned safe_count;
} target_context_t;

static wcm_status_t target_clock_read(void *user, wcm_time_t *out) {
    target_context_t *target = (target_context_t *)user;
    if (!target || !out) return WCM_ERR_ARG;
    *out = target->now_us;
    return WCM_OK;
}

static void target_guard_enter(void *user) { (void)user; }
static void target_guard_exit(void *user) { (void)user; }

static wcm_status_t target_apply(void *user, const wcm_value_t *value) {
    target_context_t *target = (target_context_t *)user;
    if (!target || !value) return WCM_ERR_ARG;
    target->applied = *value;
    return WCM_OK;
}

static wcm_status_t target_safe(void *user) {
    target_context_t *target = (target_context_t *)user;
    if (!target) return WCM_ERR_ARG;
    memset(&target->applied, 0, sizeof(target->applied));
    target->safe_count++;
    return WCM_OK;
}

static wcm_status_t control_step(
    const wcm_snapshot_t *snapshot,
    void *user,
    wcm_intent_t *out_intent) {
    (void)user;
    wcm_value_t observed;
    if (wcm_snapshot_read_value(snapshot, 0u, &observed) != WCM_OK) return WCM_ERR_STATE;

    wcm_value_t command = {0};
    command.f32[0] = observed.f32[0];
    return wcm_intent_from_snapshot(out_intent, snapshot, 0u, &command);
}

int main(void) {
    target_context_t target = {.now_us = 1000u};

    static const wcm_witness_source_desc_t sources[] = {{
        .max_live_us = 5000u,
        .max_future_skew_us = 0u,
        .id = 0u,
        .min_quality = 100u,
        .timestamp_policy = WCM_TS_STRICT_INCREASING,
        .allowed_flags = 0u,
    }};
    static const wcm_dependency_desc_t dependencies[] = {{
        .witness_id = 0u,
        .lens = WCM_LENS_SAMPLE,
        .quality_floor = 100u,
    }};
    static const wcm_rebind_requirement_t rebind[] = {{
        .witness_id = 0u,
        .min_quality = 100u,
        .consecutive_samples = 1u,
        .max_gap_us = 0u,
    }};
    static const wcm_capability_desc_t capabilities[] = {{
        .capability_id = 0u,
        .requirements = rebind,
        .requires_mask = 0u,
        .requirement_count = 1u,
    }};
    static const wcm_module_desc_t modules[] = {{
        .id = 1u,
        .period_us = 1000u,
        .deadline_us = 1000u,
        .wcet_us = 100u,
        .required_capabilities = UINT64_C(1),
        .allowed_actuators = UINT64_C(1),
        .dependencies = dependencies,
        .dependency_count = 1u,
        .step = control_step,
        .reset = NULL,
        .user = NULL,
    }};
    wcm_actuator_desc_t actuators[] = {{
        .id = 0u,
        .dispatch_wcet_us = 100u,
        .effect_latency_us = 100u,
        .constraint_transform = NULL,
        .safety_transform = NULL,
        .constraint_validate = NULL,
        .safety_validate = NULL,
        .apply = target_apply,
        .safe = target_safe,
        .user = &target,
    }};

    wcm_runtime_config_t config;
    wcm_runtime_config_init(&config);
    config.deployment_id = UINT64_C(0x11000001);
    config.config_revision = 1u;
    config.sources = sources;
    config.source_count = 1u;
    config.modules = modules;
    config.module_count = 1u;
    config.capabilities = capabilities;
    config.capability_count = 1u;
    config.actuators = actuators;
    config.actuator_count = 1u;
    config.ingress_producer_count = 1u;
    config.gate_wcet_us = 100u;
    config.clock_read = target_clock_read;
    config.clock_user = &target;
    config.commit_guard_enter = target_guard_enter;
    config.commit_guard_exit = target_guard_exit;

    wcm_runtime_t runtime = {0};
    WCM_RUNTIME_STORAGE(storage);
    if (wcm_runtime_init(&runtime, storage, sizeof(storage), &config) != WCM_OK) return 1;

    wcm_observation_t observation;
    memset(&observation, 0, sizeof(observation));
    observation.witness_id = 0u;
    observation.observed_at = target.now_us;
    observation.requested_livebound = target.now_us + 5000u;
    observation.quality = 200u;
    observation.value.f32[0] = 0.25f;

    if (wcm_runtime_post_observation(&runtime, 0u, &observation) != WCM_OK) return 2;
    if (wcm_runtime_step(&runtime) != WCM_OK) return 3;

    wcm_health_snapshot_t health;
    if (wcm_runtime_get_health(&runtime, &health) != WCM_OK) return 4;
    if (health.state != WCM_HEALTH_NOMINAL || target.applied.f32[0] != 0.25f) return 5;
    return wcm_runtime_stop(&runtime) == WCM_OK ? 0 : 6;
}

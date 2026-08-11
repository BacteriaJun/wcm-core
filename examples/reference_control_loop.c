#include <stdio.h>
#include <string.h>

#include "wcm/wcm.h"

typedef struct {
    wcm_time_t now_us;
    float output;
    uint64_t applies;
    uint64_t safe_calls;
} app_t;

static wcm_status_t control_step(const wcm_snapshot_t *snapshot, void *user, wcm_intent_t *intent) {
    (void)user;
    wcm_value_t state = {0};
    if (wcm_snapshot_read_value(snapshot, 0u, &state) != WCM_OK) return WCM_ERR_STATE;

    wcm_value_t command = {0};
    command.f32[0] = -0.25f * state.f32[0];
    return wcm_intent_from_snapshot(intent, snapshot, 0u, &command);
}

static wcm_status_t apply_output(void *user, const wcm_value_t *value) {
    app_t *app = (app_t *)user;
    app->output = value->f32[0];
    app->applies++;
    return WCM_OK;
}

static wcm_status_t safe_output(void *user) {
    app_t *app = (app_t *)user;
    app->output = 0.0f;
    app->safe_calls++;
    return WCM_OK;
}

static wcm_status_t reference_clock_read(void *user, wcm_time_t *out) {
    app_t *app = (app_t *)user;
    if (!app || !out) return WCM_ERR_ARG;
    *out = app->now_us;
    return WCM_OK;
}

static void guard_noop(void *user) {
    (void)user;
}

int main(void) {
    app_t app;
    memset(&app, 0, sizeof(app));
    app.now_us = 1000u;

    const wcm_witness_source_desc_t sources[] = {{
        .max_live_us = 5000u,
        .max_future_skew_us = 0u,
        .id = 0u,
        .min_quality = 100u,
        .timestamp_policy = WCM_TS_STRICT_INCREASING,
        .allowed_flags = 0u,
    }};
    const wcm_dependency_desc_t dependencies[] = {{
        .witness_id = 0u,
        .lens = WCM_LENS_SAMPLE,
        .quality_floor = 100u,
    }};
    const wcm_rebind_requirement_t requirements[] = {{
        .witness_id = 0u,
        .min_quality = 100u,
        .consecutive_samples = 1u,
        .max_gap_us = 0u,
    }};
    const wcm_capability_desc_t capabilities[] = {{
        .capability_id = 0u,
        .requirements = requirements,
        .requirement_count = 1u,
    }};
    const wcm_module_desc_t modules[] = {{
        .id = 1u,
        .period_us = 1000u,
        .deadline_us = 1000u,
        .wcet_us = 200u,
        .required_capabilities = wcm_capability_bit(0u),
        .allowed_actuators = wcm_actuator_bit(0u),
        .dependencies = dependencies,
        .dependency_count = 1u,
        .step = control_step,
        .reset = NULL,
        .user = &app,
    }};
    const wcm_actuator_desc_t actuators[] = {{
        .id = 0u,
        .dispatch_wcet_us = 100u,
        .effect_latency_us = 100u,
        .constraint_transform = NULL,
        .safety_transform = NULL,
        .constraint_validate = NULL,
        .safety_validate = NULL,
        .apply = apply_output,
        .safe = safe_output,
        .user = &app,
    }};

    wcm_runtime_config_t config;
    wcm_runtime_config_init(&config);
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
    config.clock_read = reference_clock_read;
    config.clock_user = &app;
    config.commit_guard_enter = guard_noop;
    config.commit_guard_exit = guard_noop;

    wcm_runtime_t runtime = {0};
    WCM_RUNTIME_STORAGE(storage);
    if (wcm_runtime_init(&runtime, storage, sizeof(storage), &config) != WCM_OK) return 1;

    const uint32_t steps = 100000u;
    for (uint32_t i = 0u; i < steps; ++i) {
        const wcm_time_t now = UINT64_C(1000) + (wcm_time_t)i * UINT64_C(1000);
        app.now_us = now;

        wcm_observation_t observation;
        memset(&observation, 0, sizeof(observation));
        observation.witness_id = 0u;
        observation.observed_at = now;
        observation.requested_livebound = now + UINT64_C(5000);
        observation.quality = 200u;
        observation.value.f32[0] = (float)(i % 20u) - 10.0f;

        if (wcm_runtime_post_observation(&runtime, 0u, &observation) != WCM_OK) return 2;
        if (i == 50000u && wcm_runtime_world_break(&runtime, WCM_BREAK_APPLICATION) != WCM_OK) return 3;
        if (wcm_runtime_step(&runtime) != WCM_OK) return 4;
    }

    wcm_metrics_t metrics;
    if (wcm_runtime_get_metrics(&runtime, &metrics) != WCM_OK) return 5;
    if (metrics.commits_ok == 0u || metrics.commit_internal_error != 0u || metrics.port_violations != 0u) return 6;

    printf("steps=%u commits=%llu world_breaks=%llu yield=%.6f runtime_bytes=%zu\n",
           steps,
           (unsigned long long)metrics.commits_ok,
           (unsigned long long)metrics.world_breaks,
           wcm_metrics_concordance_yield(&metrics),
           wcm_runtime_storage_required());
    return 0;
}

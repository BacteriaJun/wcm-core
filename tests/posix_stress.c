#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "wcm/wcm.h"
#include "wcm/ports/posix.h"

typedef struct {
    wcm_posix_port_t port;
    wcm_runtime_t runtime;
    WCM_RUNTIME_STORAGE(storage);
    atomic_uint producers_done;
    atomic_ullong applies;
} stress_t;

typedef struct {
    stress_t *stress;
    uint8_t producer;
    uint16_t witness;
    unsigned count;
} producer_arg_t;

static wcm_status_t control_step(const wcm_snapshot_t *snapshot, void *user, wcm_intent_t *intent) {
    (void)user;
    wcm_value_t a = {0};
    wcm_value_t b = {0};
    if (wcm_snapshot_read_value(snapshot, 0u, &a) != WCM_OK ||
        wcm_snapshot_read_value(snapshot, 1u, &b) != WCM_OK) return WCM_ERR_STATE;
    wcm_value_t command = {0};
    command.f32[0] = (a.f32[0] + b.f32[0]) * 0.5f;
    return wcm_intent_from_snapshot(intent, snapshot, 0u, &command);
}

static wcm_status_t apply_output(void *user, const wcm_value_t *value) {
    stress_t *stress = (stress_t *)user;
    (void)value;
    (void)atomic_fetch_add_explicit(&stress->applies, 1u, memory_order_relaxed);
    return WCM_OK;
}

static wcm_status_t safe_output(void *user) {
    (void)user;
    return WCM_OK;
}

static void *producer_main(void *opaque) {
    producer_arg_t *arg = (producer_arg_t *)opaque;
    for (unsigned i = 0u; i < arg->count; ++i) {
        wcm_observation_t obs;
        memset(&obs, 0, sizeof(obs));
        obs.witness_id = arg->witness;
        obs.quality = 220u;
        obs.value.f32[0] = (float)i;

        for (;;) {
            const wcm_time_t now = wcm_posix_clock_now();
            obs.observed_at = now;
            obs.requested_livebound = now + UINT64_C(1000000);
            const wcm_status_t rc = wcm_posix_post_observation(
                &arg->stress->port, &arg->stress->runtime, arg->producer, &obs);
            if (rc == WCM_OK) break;
            if (rc != WCM_ERR_FULL) return (void *)1;
            sched_yield();
        }
    }
    (void)atomic_fetch_add_explicit(&arg->stress->producers_done, 1u, memory_order_release);
    return NULL;
}

int main(void) {
    stress_t stress;
    memset(&stress, 0, sizeof(stress));
    atomic_init(&stress.producers_done, 0u);
    atomic_init(&stress.applies, 0u);
    if (wcm_posix_port_init(&stress.port) != WCM_OK) return 1;

    const wcm_witness_source_desc_t sources[] = {
        {.max_live_us = 1000000u, .max_future_skew_us = 100u, .id = 0u, .min_quality = 100u,
         .timestamp_policy = WCM_TS_NONDECREASING, .allowed_flags = 0u},
        {.max_live_us = 1000000u, .max_future_skew_us = 100u, .id = 1u, .min_quality = 100u,
         .timestamp_policy = WCM_TS_NONDECREASING, .allowed_flags = 0u},
    };
    const wcm_dependency_desc_t dependencies[] = {
        {.witness_id = 0u, .lens = WCM_LENS_VALIDITY, .quality_floor = 100u},
        {.witness_id = 1u, .lens = WCM_LENS_VALIDITY, .quality_floor = 100u},
    };
    const wcm_rebind_requirement_t requirements[] = {
        {.witness_id = 0u, .min_quality = 100u, .consecutive_samples = 1u, .max_gap_us = 0u},
        {.witness_id = 1u, .min_quality = 100u, .consecutive_samples = 1u, .max_gap_us = 0u},
    };
    const wcm_capability_desc_t capabilities[] = {{
        .capability_id = 0u,
        .requirements = requirements,
        .requirement_count = 2u,
    }};
    const wcm_module_desc_t modules[] = {{
        .id = 11u,
        .period_us = 250u,
        .deadline_us = 250u,
        .wcet_us = 100u,
        .required_capabilities = wcm_capability_bit(0u),
        .allowed_actuators = wcm_actuator_bit(0u),
        .dependencies = dependencies,
        .dependency_count = 2u,
        .step = control_step,
        .reset = NULL,
        .user = &stress,
    }};
    const wcm_actuator_desc_t actuators[] = {{
        .id = 0u,
        .dispatch_wcet_us = 1000u,
        .effect_latency_us = 0u,
        .apply = apply_output,
        .safe = safe_output,
        .user = &stress,
    }};

    wcm_runtime_config_t config;
    wcm_runtime_config_init(&config);
    config.sources = sources;
    config.source_count = 2u;
    config.modules = modules;
    config.module_count = 1u;
    config.capabilities = capabilities;
    config.capability_count = 1u;
    config.actuators = actuators;
    config.actuator_count = 1u;
    config.ingress_producer_count = 2u;
    config.ingress_fold_limit = (uint16_t)WCM_MAX_INGRESS;
    config.gate_wcet_us = 2000u;
    config.clock_uncertainty_us = 10u;
    config.clock_read = wcm_posix_clock_read;
    config.commit_guard_enter = wcm_posix_commit_guard_enter;
    config.commit_guard_exit = wcm_posix_commit_guard_exit;
    config.commit_guard_user = &stress.port;

    if (wcm_runtime_init(&stress.runtime, stress.storage, sizeof(stress.storage), &config) != WCM_OK) {
        wcm_posix_port_destroy(&stress.port);
        return 2;
    }

    producer_arg_t args[2] = {
        {.stress = &stress, .producer = 0u, .witness = 0u, .count = 5000u},
        {.stress = &stress, .producer = 1u, .witness = 1u, .count = 5000u},
    };
    pthread_t threads[2];
    if (pthread_create(&threads[0], NULL, producer_main, &args[0]) != 0 ||
        pthread_create(&threads[1], NULL, producer_main, &args[1]) != 0) {
        wcm_posix_port_destroy(&stress.port);
        return 3;
    }

    const wcm_time_t deadline = wcm_posix_clock_now() + UINT64_C(3000000);
    while (atomic_load_explicit(&stress.producers_done, memory_order_acquire) < 2u &&
           wcm_posix_clock_now() < deadline) {
        if (wcm_runtime_step(&stress.runtime) != WCM_OK) return 4;
        sched_yield();
    }
    void *r0 = NULL;
    void *r1 = NULL;
    (void)pthread_join(threads[0], &r0);
    (void)pthread_join(threads[1], &r1);
    if (r0 != NULL || r1 != NULL) return 5;

    const wcm_time_t drain_deadline = wcm_posix_clock_now() + UINT64_C(1000000);
    while (wcm_posix_clock_now() < drain_deadline &&
           atomic_load_explicit(&stress.applies, memory_order_relaxed) == 0u) {
        if (wcm_runtime_step(&stress.runtime) != WCM_OK) return 6;
        sched_yield();
    }

    wcm_metrics_t metrics;
    if (wcm_runtime_get_metrics(&stress.runtime, &metrics) != WCM_OK) return 7;
    const int ok = metrics.port_violations == 0u && metrics.commit_internal_error == 0u &&
                   metrics.safe_apply_failures == 0u && metrics.observations_published > 0u &&
                   atomic_load_explicit(&stress.applies, memory_order_relaxed) > 0u;

    (void)wcm_runtime_stop(&stress.runtime);
    wcm_posix_port_destroy(&stress.port);
    if (ok) {
        printf("published=%llu applies=%llu dropped=%llu port_violations=%llu internal_errors=%llu\n",
               (unsigned long long)metrics.observations_published,
               (unsigned long long)atomic_load_explicit(&stress.applies, memory_order_relaxed),
               (unsigned long long)metrics.ingress_dropped,
               (unsigned long long)metrics.port_violations,
               (unsigned long long)metrics.commit_internal_error);
    }
    if (!ok) {
        fprintf(stderr, "stress failed: published=%llu applies=%llu port=%llu internal=%llu safe=%llu\n",
                (unsigned long long)metrics.observations_published,
                (unsigned long long)atomic_load_explicit(&stress.applies, memory_order_relaxed),
                (unsigned long long)metrics.port_violations,
                (unsigned long long)metrics.commit_internal_error,
                (unsigned long long)metrics.safe_apply_failures);
        return 8;
    }
    return 0;
}

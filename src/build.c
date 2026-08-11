#include "wcm/build.h"
#include "wcm/config.h"
#include "wcm/anchor.h"
#include "wcm/runtime.h"

#include <string.h>

#ifndef WCM_BUILD_TAG
#define WCM_BUILD_TAG "release"
#endif

static uint64_t hash_bytes(uint64_t hash, const void *data, size_t len) {
    const unsigned char *p = (const unsigned char *)data;
    for (size_t i = 0; i < len; ++i) {
        hash ^= (uint64_t)p[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t hash_u64(uint64_t hash, uint64_t value) {
    unsigned char canonical[8];
    for (unsigned i = 0u; i < 8u; ++i) {
        const unsigned shift = (7u - i) * 8u;
        canonical[i] = (unsigned char)((value >> shift) & UINT64_C(0xFF));
    }
    return hash_bytes(hash, canonical, sizeof(canonical));
}

void wcm_get_build_info(wcm_build_info_t *out) {
    if (!out) return;
    out->abi_version = WCM_ABI_VERSION;
    out->config_fingerprint_version = WCM_CONFIG_FINGERPRINT_VERSION;
    out->anchor_format_version = WCM_ANCHOR_FORMAT;
    out->profile = (uint8_t)WCM_PROFILE;
    out->pointer_bits = (uint8_t)(sizeof(void *) * 8u);
    out->runtime_storage_bytes = WCM_RUNTIME_STORAGE_BYTES;
    out->version = WCM_VERSION_STRING;
    out->build_tag = WCM_BUILD_TAG;
}

uint64_t wcm_runtime_config_fingerprint(const wcm_runtime_config_t *config) {
    if (!config) return 0u;
    uint64_t h = UINT64_C(1469598103934665603);
    h = hash_u64(h, config->deployment_id);
    h = hash_u64(h, config->config_revision);
    h = hash_u64(h, config->ingress_fold_limit);
    h = hash_u64(h, config->gate_wcet_us);
    h = hash_u64(h, config->clock_uncertainty_us);
    h = hash_u64(h, config->step_budget_us);
    h = hash_u64(h, config->config_check_period_steps);
    h = hash_u64(h, config->source_count);
    h = hash_u64(h, config->module_count);
    h = hash_u64(h, config->capability_count);
    h = hash_u64(h, config->actuator_count);
    h = hash_u64(h, config->ingress_producer_count);
    h = hash_u64(h, config->resource_admit != NULL ? 1u : 0u);

    for (uint16_t i = 0; i < config->source_count; ++i) {
        const wcm_witness_source_desc_t *d = &config->sources[i];
        h = hash_u64(h, d->id);
        h = hash_u64(h, d->max_live_us);
        h = hash_u64(h, d->max_future_skew_us);
        h = hash_u64(h, d->min_quality);
        h = hash_u64(h, d->timestamp_policy);
        h = hash_u64(h, d->allowed_flags);
    }
    for (uint8_t i = 0; i < config->module_count; ++i) {
        const wcm_module_desc_t *d = &config->modules[i];
        h = hash_u64(h, d->id);
        h = hash_u64(h, d->period_us);
        h = hash_u64(h, d->deadline_us);
        h = hash_u64(h, d->wcet_us);
        h = hash_u64(h, d->required_capabilities);
        h = hash_u64(h, d->allowed_actuators);
        h = hash_u64(h, d->dependency_count);
        h = hash_u64(h, d->reset != NULL ? 1u : 0u);
        for (uint8_t j = 0; j < d->dependency_count; ++j) {
            h = hash_u64(h, d->dependencies[j].witness_id);
            h = hash_u64(h, d->dependencies[j].lens);
            h = hash_u64(h, d->dependencies[j].quality_floor);
        }
    }
    for (uint8_t i = 0; i < config->capability_count; ++i) {
        const wcm_capability_desc_t *d = &config->capabilities[i];
        h = hash_u64(h, d->capability_id);
        h = hash_u64(h, d->requires_mask);
        h = hash_u64(h, d->requirement_count);
        for (uint8_t j = 0; j < d->requirement_count; ++j) {
            h = hash_u64(h, d->requirements[j].witness_id);
            h = hash_u64(h, d->requirements[j].min_quality);
            h = hash_u64(h, d->requirements[j].consecutive_samples);
            h = hash_u64(h, d->requirements[j].max_gap_us);
        }
    }
    for (uint8_t i = 0; i < config->actuator_count; ++i) {
        const wcm_actuator_desc_t *d = &config->actuators[i];
        h = hash_u64(h, d->id);
        h = hash_u64(h, d->dispatch_wcet_us);
        h = hash_u64(h, d->effect_latency_us);
        h = hash_u64(h, d->constraint_transform != NULL ? 1u : 0u);
        h = hash_u64(h, d->safety_transform != NULL ? 1u : 0u);
        h = hash_u64(h, d->constraint_validate != NULL ? 1u : 0u);
        h = hash_u64(h, d->safety_validate != NULL ? 1u : 0u);
    }
    return h == 0u ? 1u : h;
}

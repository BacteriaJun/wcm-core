#include "wcm/concordance.h"

wcm_time_t wcm_commit_horizon(const wcm_intent_t *intent, uint32_t effect_latency_us) {
    if (!intent) return 0u;
    if (intent->stamp.livebound <= (wcm_time_t)effect_latency_us) return 0u;
    return intent->stamp.livebound - (wcm_time_t)effect_latency_us;
}

int64_t wcm_viability_margin(
    const wcm_intent_t *intent,
    wcm_time_t now,
    uint32_t module_wcet_us,
    uint32_t gate_wcet_us,
    uint32_t effect_latency_us) {
    const wcm_time_t horizon = wcm_commit_horizon(intent, effect_latency_us);
    const uint64_t cost = (uint64_t)module_wcet_us + (uint64_t)gate_wcet_us;
    if (horizon < now) {
        const uint64_t overdue = now - horizon;
        if (overdue > (uint64_t)INT64_MAX || cost > (uint64_t)INT64_MAX) return INT64_MIN;
        const uint64_t total = overdue + cost;
        if (total > (uint64_t)INT64_MAX) return INT64_MIN;
        return -(int64_t)total;
    }
    const uint64_t remaining = horizon - now;
    if (remaining > (uint64_t)INT64_MAX) return INT64_MAX;
    if (cost > (uint64_t)INT64_MAX) return INT64_MIN;
    return (int64_t)remaining - (int64_t)cost;
}

wcm_commit_code_t wcm_concordance_commit(
    const wcm_world_t *world,
    wcm_counter_t current_dependency_clock,
    wcm_time_t now,
    const wcm_commit_context_t *ctx,
    const wcm_intent_t *intent) {
    if (!world || !ctx || !intent) return WCM_COMMIT_INTERNAL_ERROR;
    if (intent->stamp.world_epoch != world->epoch) return WCM_VOID_WORLD;
    if (intent->stamp.dependency_clock != current_dependency_clock) return WCM_VOID_STALE;
    if (intent->stamp.livebound < (wcm_time_t)ctx->effect_latency_us) return WCM_VOID_EXPIRED;
    if (now > wcm_commit_horizon(intent, ctx->effect_latency_us)) return WCM_VOID_EXPIRED;
    if (!ctx->capability_ok) return WCM_DENY_CAPABILITY;
    return WCM_COMMIT_OK;
}

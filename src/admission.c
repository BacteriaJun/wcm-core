#include "wcm/admission.h"

int64_t wcm_snapshot_viability_margin(
    const wcm_snapshot_t *snapshot,
    wcm_time_t now,
    uint32_t module_wcet_us,
    uint32_t gate_wcet_us,
    uint32_t effect_latency_us) {
    if (!snapshot || !snapshot->sealed) return INT64_MIN;
    const uint64_t total_cost = (uint64_t)module_wcet_us + (uint64_t)gate_wcet_us + (uint64_t)effect_latency_us;
    if (snapshot->stamp.livebound >= now) {
        const uint64_t remaining = snapshot->stamp.livebound - now;
        if (remaining > (uint64_t)INT64_MAX) return INT64_MAX;
        return (int64_t)remaining - (int64_t)total_cost;
    }
    const uint64_t overdue = now - snapshot->stamp.livebound;
    if (overdue > (uint64_t)INT64_MAX) return INT64_MIN;
    return -(int64_t)overdue - (int64_t)total_cost;
}

wcm_admission_code_t wcm_precommit_admit(
    const wcm_snapshot_t *snapshot,
    const wcm_world_t *world,
    wcm_time_t now,
    uint32_t module_wcet_us,
    uint32_t gate_wcet_us,
    uint32_t effect_latency_us) {
    if (!snapshot || !snapshot->sealed || !world) return WCM_ADMIT_NONVIABLE;
    if (snapshot->stamp.world_epoch != world->epoch) return WCM_ADMIT_WORLD_MISMATCH;
    if (now > snapshot->stamp.livebound) return WCM_ADMIT_EXPIRED;
    return wcm_snapshot_viability_margin(snapshot, now, module_wcet_us, gate_wcet_us, effect_latency_us) > 0
        ? WCM_ADMIT_OK
        : WCM_ADMIT_NONVIABLE;
}

#include "wcm/rebind.h"
#include <string.h>

void wcm_rebind_begin(wcm_rebind_runtime_t *rt, const wcm_capability_desc_t *desc, wcm_time_t now) {
    if (!rt || !desc) return;
    memset(rt, 0, sizeof(*rt));
    rt->binding_started_at = now;
    const uint8_t n = desc->requirement_count > WCM_MAX_REBIND_REQS ? WCM_MAX_REBIND_REQS : desc->requirement_count;
    uint16_t debt = 0u;
    for (uint8_t i = 0; i < n; ++i) debt |= (uint16_t)(UINT16_C(1) << i);
    rt->debt_mask = debt;
    rt->state = n ? WCM_CAP_BINDING : WCM_CAP_BOUND;
}

wcm_status_t wcm_rebind_observe(
    wcm_rebind_runtime_t *rt,
    const wcm_capability_desc_t *desc,
    uint16_t witness_id,
    uint8_t quality,
    wcm_time_t observed_at,
    wcm_time_t previous_observed_at) {
    if (!rt || !desc) return WCM_ERR_ARG;
    const uint8_t n = desc->requirement_count > WCM_MAX_REBIND_REQS ? WCM_MAX_REBIND_REQS : desc->requirement_count;
    for (uint8_t i = 0; i < n; ++i) {
        const wcm_rebind_requirement_t *req = &desc->requirements[i];
        if (req->witness_id != witness_id || quality < req->min_quality) continue;
        if (!(rt->debt_mask & (uint16_t)(1u << i))) continue;

        const bool previous_is_binding_sample = previous_observed_at >= rt->binding_started_at;
        const bool gap_ok = req->max_gap_us == 0u ||
            (previous_is_binding_sample && observed_at >= previous_observed_at &&
             observed_at - previous_observed_at <= (wcm_time_t)req->max_gap_us);

        if (rt->sample_count[i] > 0u && !gap_ok) {
            rt->sample_count[i] = 0u;
        }
        if (rt->sample_count[i] < UINT8_MAX) rt->sample_count[i]++;

        const uint8_t needed = req->consecutive_samples == 0u ? 1u : req->consecutive_samples;
        if (rt->sample_count[i] >= needed) {
            rt->debt_mask &= (uint16_t)~(1u << i);
        }
    }
    if (rt->debt_mask == 0u) rt->state = WCM_CAP_BOUND;
    return WCM_OK;
}

bool wcm_rebind_is_clear(const wcm_rebind_runtime_t *rt) {
    return rt && rt->debt_mask == 0u;
}

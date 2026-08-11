#ifndef WCM_REBIND_H
#define WCM_REBIND_H

#include "wcm/config.h"
#include "wcm/types.h"

typedef enum {
    WCM_CAP_UNBOUND = 0,
    WCM_CAP_BINDING,
    WCM_CAP_BOUND,
    WCM_CAP_DEGRADED,
} wcm_cap_state_t;

typedef struct {
    uint16_t witness_id;
    uint8_t min_quality;
    uint8_t consecutive_samples;
    uint32_t max_gap_us;
} wcm_rebind_requirement_t;

typedef struct {
    uint16_t capability_id;
    const wcm_rebind_requirement_t *requirements;
    wcm_capability_set_t requires_mask;
    uint8_t requirement_count;
} wcm_capability_desc_t;

typedef struct {
    wcm_time_t binding_started_at;
    uint16_t debt_mask;
    uint8_t sample_count[WCM_MAX_REBIND_REQS];
    uint8_t state;
} wcm_rebind_runtime_t;

void wcm_rebind_begin(wcm_rebind_runtime_t *rt, const wcm_capability_desc_t *desc, wcm_time_t now);
wcm_status_t wcm_rebind_observe(
    wcm_rebind_runtime_t *rt,
    const wcm_capability_desc_t *desc,
    uint16_t witness_id,
    uint8_t quality,
    wcm_time_t observed_at,
    wcm_time_t previous_observed_at);
bool wcm_rebind_is_clear(const wcm_rebind_runtime_t *rt);

#endif

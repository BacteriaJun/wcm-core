#ifndef WCM_HEALTH_H
#define WCM_HEALTH_H

#include "wcm/diagnostics.h"
#include "wcm/metrics.h"
#include "wcm/world.h"

typedef enum {
    WCM_HEALTH_STARTING = 0,
    WCM_HEALTH_NOMINAL,
    WCM_HEALTH_DEGRADED,
    WCM_HEALTH_REBINDING,
    WCM_HEALTH_STOPPED,
    WCM_HEALTH_FAIL_STOP,
} wcm_health_state_t;

typedef struct {
    uint64_t deployment_id;
    uint64_t config_fingerprint;
    uint64_t step_sequence;
    uint32_t config_revision;
    uint32_t pending_ingress;
    wcm_counter_t world_epoch;
    wcm_capability_set_t bound_capabilities;
    wcm_time_t epoch_started_at;
    wcm_time_t last_step_started_us;
    wcm_time_t last_step_completed_us;
    uint32_t last_step_elapsed_us;
    uint32_t max_step_elapsed_us;
    bool last_step_timing_valid;
    wcm_health_state_t state;
    wcm_world_state_t world_state;
    wcm_fault_record_t last_fault;
    wcm_metrics_t metrics;
} wcm_health_snapshot_t;

const char *wcm_health_state_string(wcm_health_state_t state);

#endif

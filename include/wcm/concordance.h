#ifndef WCM_CONCORDANCE_H
#define WCM_CONCORDANCE_H

#include "wcm/intent.h"
#include "wcm/world.h"

typedef enum {
    WCM_COMMIT_OK = 0,
    WCM_COMMIT_MODIFIED,
    WCM_VOID_WORLD,
    WCM_VOID_STALE,
    WCM_VOID_EXPIRED,
    WCM_VOID_INGRESS_BACKLOG,
    WCM_DENY_CAPABILITY,
    WCM_DENY_RESOURCE,
    WCM_DENY_AUTHORITY,
    WCM_REJECT_CONSTRAINT,
    WCM_REJECT_SAFETY,
    WCM_COMMIT_GATE_OVERRUN,
    WCM_COMMIT_DISPATCH_FAILED,
    WCM_COMMIT_DISPATCH_OVERRUN,
    WCM_COMMIT_PORT_VIOLATION,
    WCM_COMMIT_INTERNAL_ERROR,
} wcm_commit_code_t;

typedef struct {
    uint32_t effect_latency_us;
    bool capability_ok;
} wcm_commit_context_t;

wcm_time_t wcm_commit_horizon(const wcm_intent_t *intent, uint32_t effect_latency_us);
int64_t wcm_viability_margin(
    const wcm_intent_t *intent,
    wcm_time_t now,
    uint32_t module_wcet_us,
    uint32_t gate_wcet_us,
    uint32_t effect_latency_us);

wcm_commit_code_t wcm_concordance_commit(
    const wcm_world_t *world,
    wcm_counter_t current_dependency_clock,
    wcm_time_t now,
    const wcm_commit_context_t *ctx,
    const wcm_intent_t *intent);

#endif

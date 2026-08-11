#ifndef WCM_WORLD_H
#define WCM_WORLD_H

#include "wcm/types.h"

typedef enum {
    WCM_WORLD_COLD = 0,
    WCM_WORLD_REBINDING,
    WCM_WORLD_BOUND,
    WCM_WORLD_DEGRADED,
} wcm_world_state_t;

typedef enum {
    WCM_BREAK_RESET = 1,
    WCM_BREAK_BROWNOUT,
    WCM_BREAK_CLOCK_DISCONTINUITY,
    WCM_BREAK_STATE_CORRUPTION,
    WCM_BREAK_ACTUATOR_DOMAIN_RESET,
    WCM_BREAK_ACTUATOR_FAILURE,
    WCM_BREAK_APPLICATION,
} wcm_world_break_reason_t;

typedef struct {
    wcm_counter_t epoch;
    wcm_time_t started_at;
    wcm_world_state_t state;
    wcm_world_break_reason_t last_break_reason;
} wcm_world_t;

void wcm_world_init(wcm_world_t *world, wcm_time_t now);
wcm_status_t wcm_world_break(wcm_world_t *world, wcm_world_break_reason_t reason, wcm_time_t now);

#endif

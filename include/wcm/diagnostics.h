#ifndef WCM_DIAGNOSTICS_H
#define WCM_DIAGNOSTICS_H

#include "wcm/concordance.h"
#include "wcm/dependency.h"
#include "wcm/types.h"
#include "wcm/world.h"

typedef enum {
    WCM_FAULT_NONE = 0,
    WCM_FAULT_CLOCK_DISCONTINUITY,
    WCM_FAULT_CLOCK_READ_FAILED,
    WCM_FAULT_COUNTER_EXHAUSTED,
    WCM_FAULT_MODULE_CALLBACK,
    WCM_FAULT_MODULE_WCET_OVERRUN,
    WCM_FAULT_GATE_WCET_OVERRUN,
    WCM_FAULT_ACTUATOR_DISPATCH_FAILED,
    WCM_FAULT_ACTUATOR_DISPATCH_OVERRUN,
    WCM_FAULT_SAFE_OUTPUT_FAILED,
    WCM_FAULT_PORT_CONTRACT,
    WCM_FAULT_INGRESS_BACKLOG,
    WCM_FAULT_STEP_BUDGET_OVERRUN,
    WCM_FAULT_CONFIG_MUTATION,
} wcm_fault_code_t;

typedef struct {
    uint64_t sequence;
    wcm_time_t at_us;
    wcm_counter_t world_epoch;
    wcm_fault_code_t code;
    wcm_status_t status;
    uint16_t subject_id;
    uint32_t detail;
} wcm_fault_record_t;

typedef struct {
    uint16_t module_id;
    uint32_t runs;
    uint32_t errors;
    wcm_time_t next_release_us;
    uint32_t max_effect_latency_us;
    wcm_module_runtime_t runtime;
} wcm_module_stats_t;

typedef struct {
    uint16_t actuator_id;
    uint64_t dispatches;
    uint64_t failures;
    uint64_t overruns;
    uint32_t max_dispatch_us;
    uint32_t configured_dispatch_wcet_us;
    uint32_t configured_effect_latency_us;
} wcm_actuator_stats_t;

const char *wcm_status_string(wcm_status_t status);
const char *wcm_commit_code_string(wcm_commit_code_t code);
const char *wcm_world_break_reason_string(wcm_world_break_reason_t reason);
const char *wcm_fault_code_string(wcm_fault_code_t code);

#endif

#ifndef WCM_DEPENDENCY_H
#define WCM_DEPENDENCY_H

#include "wcm/types.h"
#include "wcm/witness.h"

typedef enum {
    WCM_LENS_EDGE = 0,
    WCM_LENS_SAMPLE,
    WCM_LENS_VALIDITY,
} wcm_dependency_lens_t;

typedef struct {
    uint16_t witness_id;
    uint8_t lens;
    uint8_t quality_floor;
} wcm_dependency_desc_t;

typedef struct {
    wcm_counter_t dependency_clock;
    uint32_t last_exec_us;
    uint32_t deadline_misses;
    uint32_t viability_skips;
    uint32_t wcet_overruns;
    uint32_t release_skips;
    uint32_t max_exec_us;
} wcm_module_runtime_t;

bool wcm_dependency_transition_matters(
    const wcm_dependency_desc_t *dep,
    const wcm_witness_t *before,
    const wcm_witness_t *after);

wcm_status_t wcm_dependency_apply_transition(
    wcm_module_runtime_t *module,
    const wcm_dependency_desc_t *dep,
    const wcm_witness_t *before,
    const wcm_witness_t *after);

#endif

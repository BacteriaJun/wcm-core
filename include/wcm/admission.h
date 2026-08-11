#ifndef WCM_ADMISSION_H
#define WCM_ADMISSION_H
#include "wcm/snapshot.h"

typedef enum {
    WCM_ADMIT_OK = 0,
    WCM_ADMIT_WORLD_MISMATCH,
    WCM_ADMIT_EXPIRED,
    WCM_ADMIT_NONVIABLE,
} wcm_admission_code_t;

int64_t wcm_snapshot_viability_margin(
    const wcm_snapshot_t *snapshot,
    wcm_time_t now,
    uint32_t module_wcet_us,
    uint32_t gate_wcet_us,
    uint32_t effect_latency_us);

wcm_admission_code_t wcm_precommit_admit(
    const wcm_snapshot_t *snapshot,
    const wcm_world_t *world,
    wcm_time_t now,
    uint32_t module_wcet_us,
    uint32_t gate_wcet_us,
    uint32_t effect_latency_us);

#endif

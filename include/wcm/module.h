#ifndef WCM_MODULE_H
#define WCM_MODULE_H

#include "wcm/dependency.h"
#include "wcm/intent.h"

typedef wcm_status_t (*wcm_module_step_fn)(
    const wcm_snapshot_t *snapshot,
    void *user,
    wcm_intent_t *out_intent);

typedef void (*wcm_module_reset_fn)(void *user);

typedef struct {
    uint16_t id;
    uint32_t period_us;
    uint32_t deadline_us;
    uint32_t wcet_us;
    wcm_capability_set_t required_capabilities;
    wcm_actuator_set_t allowed_actuators;
    const wcm_dependency_desc_t *dependencies;
    uint8_t dependency_count;
    wcm_module_step_fn step;
    wcm_module_reset_fn reset;
    void *user;
} wcm_module_desc_t;

#endif

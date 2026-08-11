#ifndef WCM_ACTUATOR_H
#define WCM_ACTUATOR_H

#include "wcm/types.h"

typedef enum {
    WCM_FILTER_ACCEPT = 0,
    WCM_FILTER_MODIFY,
    WCM_FILTER_REJECT,
} wcm_filter_result_t;

typedef wcm_filter_result_t (*wcm_actuator_transform_fn)(void *user, wcm_value_t *value);
typedef bool (*wcm_actuator_validate_fn)(void *user, const wcm_value_t *value);
typedef wcm_status_t (*wcm_actuator_apply_fn)(void *user, const wcm_value_t *value);
typedef wcm_status_t (*wcm_actuator_safe_fn)(void *user);

typedef struct {
    uint16_t id;
    uint32_t dispatch_wcet_us;
    uint32_t effect_latency_us;
    wcm_actuator_transform_fn constraint_transform;
    wcm_actuator_transform_fn safety_transform;
    wcm_actuator_validate_fn constraint_validate;
    wcm_actuator_validate_fn safety_validate;
    wcm_actuator_apply_fn apply;
    wcm_actuator_safe_fn safe;
    void *user;
} wcm_actuator_desc_t;

#endif

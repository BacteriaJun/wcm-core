#ifndef WCM_INTENT_H
#define WCM_INTENT_H

#include "wcm/snapshot.h"
#include "wcm/types.h"

typedef struct {
    wcm_value_t value;
    wcm_worldstamp_t stamp;
    uint16_t source_module;
    uint16_t actuator;
} wcm_intent_t;

wcm_status_t wcm_intent_from_snapshot(
    wcm_intent_t *intent,
    const wcm_snapshot_t *snapshot,
    uint16_t actuator,
    const wcm_value_t *value);

#endif

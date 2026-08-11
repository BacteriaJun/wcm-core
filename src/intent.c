#include "wcm/intent.h"

wcm_status_t wcm_intent_from_snapshot(
    wcm_intent_t *intent,
    const wcm_snapshot_t *snapshot,
    uint16_t actuator,
    const wcm_value_t *value) {
    if (!intent || !snapshot || !value || !snapshot->sealed) return WCM_ERR_ARG;
    intent->value = *value;
    intent->stamp = snapshot->stamp;
    intent->source_module = snapshot->module_id;
    intent->actuator = actuator;
    return WCM_OK;
}

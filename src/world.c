#include "wcm/world.h"

void wcm_world_init(wcm_world_t *world, wcm_time_t now) {
    if (!world) return;
    world->epoch = 1u;
    world->started_at = now;
    world->state = WCM_WORLD_REBINDING;
    world->last_break_reason = 0;
}

wcm_status_t wcm_world_break(wcm_world_t *world, wcm_world_break_reason_t reason, wcm_time_t now) {
    if (!world) return WCM_ERR_ARG;
    if (world->epoch == UINT64_MAX) {
        world->state = WCM_WORLD_COLD;
        return WCM_ERR_COUNTER;
    }
    world->epoch++;
    world->started_at = now;
    world->state = WCM_WORLD_REBINDING;
    world->last_break_reason = reason;
    return WCM_OK;
}

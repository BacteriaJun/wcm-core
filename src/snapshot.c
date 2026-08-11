#include "wcm/snapshot.h"
#include <string.h>

wcm_status_t wcm_snapshot_begin(
    wcm_snapshot_t *snapshot,
    const wcm_world_t *world,
    uint16_t module_id,
    wcm_counter_t dependency_clock) {
    if (!snapshot || !world) return WCM_ERR_ARG;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->stamp.livebound = UINT64_MAX;
    snapshot->stamp.world_epoch = world->epoch;
    snapshot->stamp.dependency_clock = dependency_clock;
    snapshot->module_id = module_id;
    return WCM_OK;
}

wcm_status_t wcm_snapshot_observe(
    wcm_snapshot_t *snapshot,
    uint16_t witness_id,
    const wcm_witness_t *witness) {
    if (!snapshot || !witness || snapshot->sealed) return WCM_ERR_STATE;
    if (witness_id >= WCM_MAX_WITNESSES) return WCM_ERR_RANGE;
    if ((witness->flags & WCM_WITNESS_VALID) == 0u) return WCM_ERR_STATE;
    if (witness->world_epoch != snapshot->stamp.world_epoch) return WCM_ERR_STATE;

    for (uint8_t i = 0; i < snapshot->item_count; ++i) {
        if (snapshot->items[i].witness_id == witness_id) return WCM_ERR_STATE;
    }
    if (snapshot->item_count >= WCM_MAX_SNAPSHOT_ITEMS) return WCM_ERR_FULL;

    wcm_snapshot_item_t *item = &snapshot->items[snapshot->item_count++];
    item->value = witness->value;
    item->witness_id = witness_id;
    item->reserved = 0u;
    if (witness->livebound < snapshot->stamp.livebound) snapshot->stamp.livebound = witness->livebound;
    return WCM_OK;
}

wcm_status_t wcm_snapshot_seal(wcm_snapshot_t *snapshot) {
    if (!snapshot) return WCM_ERR_ARG;
    if (snapshot->sealed || snapshot->item_count == 0u || snapshot->stamp.livebound == UINT64_MAX) {
        return WCM_ERR_STATE;
    }
    snapshot->sealed = 1u;
    return WCM_OK;
}

wcm_status_t wcm_snapshot_read_value(
    const wcm_snapshot_t *snapshot,
    uint16_t witness_id,
    wcm_value_t *out) {
    if (!snapshot || !out || !snapshot->sealed) return WCM_ERR_ARG;
    for (uint8_t i = 0; i < snapshot->item_count; ++i) {
        if (snapshot->items[i].witness_id == witness_id) {
            *out = snapshot->items[i].value;
            return WCM_OK;
        }
    }
    return WCM_ERR_NOT_FOUND;
}

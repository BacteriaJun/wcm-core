#ifndef WCM_SNAPSHOT_H
#define WCM_SNAPSHOT_H

#include "wcm/config.h"
#include "wcm/types.h"
#include "wcm/witness.h"

typedef struct {
    wcm_time_t livebound;
    wcm_counter_t world_epoch;
    wcm_counter_t dependency_clock;
} wcm_worldstamp_t;

typedef struct {
    wcm_value_t value;
    uint16_t witness_id;
    uint16_t reserved;
} wcm_snapshot_item_t;

typedef struct {
    wcm_worldstamp_t stamp;
    wcm_snapshot_item_t items[WCM_MAX_SNAPSHOT_ITEMS];
    uint16_t module_id;
    uint8_t item_count;
    uint8_t sealed;
} wcm_snapshot_t;

wcm_status_t wcm_snapshot_begin(
    wcm_snapshot_t *snapshot,
    const wcm_world_t *world,
    uint16_t module_id,
    wcm_counter_t dependency_clock);
wcm_status_t wcm_snapshot_observe(wcm_snapshot_t *snapshot, uint16_t witness_id, const wcm_witness_t *witness);
wcm_status_t wcm_snapshot_seal(wcm_snapshot_t *snapshot);
wcm_status_t wcm_snapshot_read_value(const wcm_snapshot_t *snapshot, uint16_t witness_id, wcm_value_t *out);

#endif

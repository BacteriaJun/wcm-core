#ifndef WCM_WITNESS_H
#define WCM_WITNESS_H

#include "wcm/config.h"
#include "wcm/types.h"
#include "wcm/world.h"

enum {
    WCM_WITNESS_VALID = 1u << 0,
    WCM_WITNESS_ESTIMATED = 1u << 1,
    WCM_WITNESS_PREDICTED = 1u << 2,
    WCM_WITNESS_DEGRADED = 1u << 3,
};

#define WCM_WITNESS_OBSERVATION_FLAGS \
    (WCM_WITNESS_ESTIMATED | WCM_WITNESS_PREDICTED | WCM_WITNESS_DEGRADED)

typedef enum {
    WCM_TS_STRICT_INCREASING = 0,
    WCM_TS_NONDECREASING = 1,
} wcm_timestamp_policy_t;

typedef struct {
    wcm_value_t value;
    wcm_time_t observed_at;
    wcm_time_t livebound;
    wcm_counter_t world_epoch;
    wcm_counter_t generation;
    uint8_t quality;
    uint8_t flags;
} wcm_witness_t;

typedef struct {
    uint32_t max_live_us;
    uint32_t max_future_skew_us;
    uint16_t id;
    uint8_t min_quality;
    uint8_t timestamp_policy;
    uint8_t allowed_flags;
} wcm_witness_source_desc_t;

#define WCM_WITNESS_USED_BYTES ((WCM_MAX_WITNESSES + 7u) / 8u)

typedef struct {
    wcm_witness_t slots[WCM_MAX_WITNESSES];
    wcm_time_t previous_observed_at[WCM_MAX_WITNESSES];
    uint8_t used_bits[WCM_WITNESS_USED_BYTES];
} wcm_witness_store_t;

void wcm_witness_store_init(wcm_witness_store_t *store);
wcm_status_t wcm_witness_publish(
    wcm_witness_store_t *store,
    const wcm_world_t *world,
    uint16_t id,
    const wcm_value_t *value,
    wcm_time_t now,
    wcm_time_t observed_at,
    wcm_time_t requested_livebound,
    const wcm_witness_source_desc_t *source,
    uint8_t quality,
    uint8_t flags);
const wcm_witness_t *wcm_witness_get(const wcm_witness_store_t *store, uint16_t id);
wcm_time_t wcm_witness_previous_observed_at(const wcm_witness_store_t *store, uint16_t id);
bool wcm_witness_is_live(const wcm_witness_t *w, const wcm_world_t *world, wcm_time_t now);
void wcm_witness_invalidate_all(wcm_witness_store_t *store);

#endif

#include "wcm/witness.h"
#include <string.h>

static bool used_get(const wcm_witness_store_t *store, uint16_t id) {
    return (store->used_bits[id >> 3u] & (uint8_t)(1u << (id & 7u))) != 0u;
}

static void used_set(wcm_witness_store_t *store, uint16_t id) {
    store->used_bits[id >> 3u] |= (uint8_t)(1u << (id & 7u));
}

void wcm_witness_store_init(wcm_witness_store_t *store) {
    if (!store) return;
    memset(store, 0, sizeof(*store));
}

static bool timestamp_order_ok(
    const wcm_witness_source_desc_t *source,
    bool has_previous,
    wcm_time_t previous,
    wcm_time_t current) {
    if (!has_previous) return true;
    if ((wcm_timestamp_policy_t)source->timestamp_policy == WCM_TS_NONDECREASING) {
        return current >= previous;
    }
    return current > previous;
}

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
    uint8_t flags) {
    if (!store || !world || !value || !source) return WCM_ERR_ARG;
    if (id >= WCM_MAX_WITNESSES || source->id != id) return WCM_ERR_RANGE;
    if (quality < source->min_quality) return WCM_ERR_QUALITY;
    if ((flags & (uint8_t)~source->allowed_flags) != 0u) return WCM_ERR_POLICY;
    if (observed_at < world->started_at) return WCM_ERR_TIMESTAMP;

    const wcm_time_t future_limit =
        UINT64_MAX - now < (wcm_time_t)source->max_future_skew_us
            ? UINT64_MAX
            : now + (wcm_time_t)source->max_future_skew_us;
    if (observed_at > future_limit) return WCM_ERR_TIMESTAMP;

    wcm_time_t previous = 0u;
    const bool same_epoch = used_get(store, id) && store->slots[id].world_epoch == world->epoch;
    if (same_epoch) {
        previous = store->slots[id].observed_at;
        if (!timestamp_order_ok(source, true, previous, observed_at)) return WCM_ERR_TIMESTAMP;
        if (store->slots[id].generation == UINT64_MAX) return WCM_ERR_COUNTER;
    }

    const wcm_time_t delta = (wcm_time_t)source->max_live_us;
    const wcm_time_t max_livebound =
        UINT64_MAX - observed_at < delta ? UINT64_MAX : observed_at + delta;
    wcm_time_t livebound = requested_livebound < max_livebound ? requested_livebound : max_livebound;
    if (livebound < observed_at) livebound = observed_at;

    store->previous_observed_at[id] = previous;
    wcm_witness_t *w = &store->slots[id];
    w->value = *value;
    w->observed_at = observed_at;
    w->livebound = livebound;
    w->world_epoch = world->epoch;
    w->generation = same_epoch ? w->generation + 1u : 1u;
    w->quality = quality;
    w->flags = (uint8_t)((flags & source->allowed_flags) | WCM_WITNESS_VALID);
    used_set(store, id);
    return WCM_OK;
}

const wcm_witness_t *wcm_witness_get(const wcm_witness_store_t *store, uint16_t id) {
    if (!store || id >= WCM_MAX_WITNESSES || !used_get(store, id)) return NULL;
    return &store->slots[id];
}

wcm_time_t wcm_witness_previous_observed_at(const wcm_witness_store_t *store, uint16_t id) {
    if (!store || id >= WCM_MAX_WITNESSES || !used_get(store, id)) return 0u;
    return store->previous_observed_at[id];
}

bool wcm_witness_is_live(const wcm_witness_t *w, const wcm_world_t *world, wcm_time_t now) {
    if (!w || !world) return false;
    return (w->flags & WCM_WITNESS_VALID) != 0u &&
           w->world_epoch == world->epoch &&
           now >= w->observed_at &&
           now <= w->livebound;
}

void wcm_witness_invalidate_all(wcm_witness_store_t *store) {
    if (!store) return;
    for (uint16_t i = 0; i < WCM_MAX_WITNESSES; ++i) {
        if (!used_get(store, i)) continue;
        store->slots[i].flags &= (uint8_t)~WCM_WITNESS_VALID;
        store->previous_observed_at[i] = 0u;
    }
}

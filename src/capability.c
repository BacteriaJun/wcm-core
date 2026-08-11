#include "wcm/capability.h"
#include <string.h>

static wcm_status_t validate_graph(const wcm_capability_desc_t *descs, uint8_t count, wcm_capability_set_t *present_out) {
    if (count > WCM_MAX_CAPABILITIES) return WCM_ERR_RANGE;
    wcm_capability_set_t present = 0u;
    for (uint8_t i = 0; i < count; ++i) {
        const uint16_t id = descs[i].capability_id;
        if (id >= 64u) return WCM_ERR_RANGE;
        const wcm_capability_set_t bit = wcm_capability_bit(id);
        if ((present & bit) != 0u) return WCM_ERR_GRAPH;
        if (descs[i].requirement_count > WCM_MAX_REBIND_REQS) return WCM_ERR_RANGE;
        if (descs[i].requirement_count > 0u && !descs[i].requirements) return WCM_ERR_ARG;
        if ((descs[i].requires_mask & bit) != 0u) return WCM_ERR_GRAPH;
        present |= bit;
    }
    for (uint8_t i = 0; i < count; ++i) {
        if ((descs[i].requires_mask & ~present) != 0u) return WCM_ERR_GRAPH;
    }

    wcm_capability_set_t resolved = 0u;
    for (uint8_t pass = 0; pass < count; ++pass) {
        bool progress = false;
        for (uint8_t i = 0; i < count; ++i) {
            const wcm_capability_set_t bit = wcm_capability_bit(descs[i].capability_id);
            if ((resolved & bit) != 0u) continue;
            if ((descs[i].requires_mask & ~resolved) == 0u) {
                resolved |= bit;
                progress = true;
            }
        }
        if (!progress) break;
    }
    if (resolved != present) return WCM_ERR_GRAPH;
    *present_out = present;
    return WCM_OK;
}

wcm_status_t wcm_capability_graph_init(
    wcm_capability_graph_t *graph,
    const wcm_capability_desc_t *descs,
    uint8_t count,
    wcm_time_t now) {
    if (!graph || (count > 0u && !descs)) return WCM_ERR_ARG;
    memset(graph, 0, sizeof(*graph));
    wcm_status_t rc = validate_graph(descs, count, &graph->present_mask);
    if (rc != WCM_OK) return rc;
    graph->descs = descs;
    graph->count = count;
    for (uint8_t i = 0; i < count; ++i) wcm_rebind_begin(&graph->runtime[i], &descs[i], now);
    wcm_capability_graph_refresh(graph);
    return WCM_OK;
}

wcm_status_t wcm_capability_graph_observe(
    wcm_capability_graph_t *graph,
    uint16_t witness_id,
    uint8_t quality,
    wcm_time_t observed_at,
    wcm_time_t previous_observed_at) {
    if (!graph) return WCM_ERR_ARG;
    for (uint8_t i = 0; i < graph->count; ++i) {
        wcm_status_t rc = wcm_rebind_observe(&graph->runtime[i], &graph->descs[i], witness_id, quality,
                                             observed_at, previous_observed_at);
        if (rc != WCM_OK) return rc;
    }
    wcm_capability_graph_refresh(graph);
    return WCM_OK;
}

void wcm_capability_graph_refresh(wcm_capability_graph_t *graph) {
    if (!graph) return;
    graph->bound_mask = 0u;
    for (uint8_t pass = 0; pass <= graph->count; ++pass) {
        bool changed = false;
        for (uint8_t i = 0; i < graph->count; ++i) {
            const wcm_capability_desc_t *d = &graph->descs[i];
            const wcm_capability_set_t bit = wcm_capability_bit(d->capability_id);
            if ((graph->bound_mask & bit) != 0u) continue;
            if (!wcm_rebind_is_clear(&graph->runtime[i])) continue;
            if ((d->requires_mask & ~graph->bound_mask) != 0u) continue;
            graph->runtime[i].state = WCM_CAP_BOUND;
            graph->bound_mask |= bit;
            changed = true;
        }
        if (!changed) break;
    }
    for (uint8_t i = 0; i < graph->count; ++i) {
        const wcm_capability_set_t bit = wcm_capability_bit(graph->descs[i].capability_id);
        if ((graph->bound_mask & bit) == 0u && graph->runtime[i].state == WCM_CAP_BOUND) {
            graph->runtime[i].state = WCM_CAP_BINDING;
        }
    }
}

void wcm_capability_graph_world_break(wcm_capability_graph_t *graph, wcm_time_t now) {
    if (!graph) return;
    graph->bound_mask = 0u;
    for (uint8_t i = 0; i < graph->count; ++i) wcm_rebind_begin(&graph->runtime[i], &graph->descs[i], now);
    wcm_capability_graph_refresh(graph);
}

bool wcm_capability_graph_has(const wcm_capability_graph_t *graph, wcm_capability_set_t required) {
    if (!graph) return required == 0u;
    return (required & ~graph->bound_mask) == 0u;
}

void wcm_capability_graph_update_world(const wcm_capability_graph_t *graph, wcm_world_t *world) {
    if (!graph || !world) return;
    if (graph->count == 0u || graph->bound_mask == graph->present_mask) {
        world->state = WCM_WORLD_BOUND;
    } else if (graph->bound_mask != 0u) {
        world->state = WCM_WORLD_DEGRADED;
    } else {
        world->state = WCM_WORLD_REBINDING;
    }
}

#ifndef WCM_CAPABILITY_H
#define WCM_CAPABILITY_H
#include "wcm/rebind.h"
#include "wcm/world.h"

typedef struct {
    const wcm_capability_desc_t *descs;
    uint8_t count;
    wcm_rebind_runtime_t runtime[WCM_MAX_CAPABILITIES];
    wcm_capability_set_t present_mask;
    wcm_capability_set_t bound_mask;
} wcm_capability_graph_t;

wcm_status_t wcm_capability_graph_init(
    wcm_capability_graph_t *graph,
    const wcm_capability_desc_t *descs,
    uint8_t count,
    wcm_time_t now);

wcm_status_t wcm_capability_graph_observe(
    wcm_capability_graph_t *graph,
    uint16_t witness_id,
    uint8_t quality,
    wcm_time_t observed_at,
    wcm_time_t previous_observed_at);

void wcm_capability_graph_world_break(wcm_capability_graph_t *graph, wcm_time_t now);
void wcm_capability_graph_refresh(wcm_capability_graph_t *graph);
bool wcm_capability_graph_has(const wcm_capability_graph_t *graph, wcm_capability_set_t required);
void wcm_capability_graph_update_world(const wcm_capability_graph_t *graph, wcm_world_t *world);

#endif

#ifndef WCM_RUNTIME_INTERNAL_H
#define WCM_RUNTIME_INTERNAL_H

#include <stdatomic.h>
#include "wcm/runtime.h"
#include "wcm/witness.h"
#include "wcm/event.h"

typedef struct {
    wcm_module_runtime_t core;
    wcm_time_t next_release_us;
    uint32_t max_effect_latency_us;
    uint32_t runs;
    uint32_t errors;
} wcm_module_slot_t;


typedef struct {
    uint64_t dispatches;
    uint64_t failures;
    uint64_t overruns;
    uint32_t max_dispatch_us;
} wcm_actuator_runtime_t;

typedef struct {
    wcm_runtime_config_t config_storage;
    const wcm_runtime_config_t *config;
    wcm_world_t world;
    wcm_witness_store_t witnesses;
    wcm_ingress_t ingress[WCM_MAX_INGRESS_PRODUCERS];
    wcm_capability_graph_t capabilities;
    wcm_module_slot_t modules[WCM_MAX_MODULES];
    wcm_actuator_runtime_t actuator_runtime[WCM_MAX_ACTUATORS];
    wcm_event_record_t events[WCM_MAX_EVENTS];
    wcm_metrics_t metrics;
    wcm_fault_record_t last_fault;
    wcm_time_t last_clock;
    wcm_time_t last_step_started_us;
    wcm_time_t last_step_completed_us;
    wcm_counter_t step_start_epoch;
    uint64_t config_fingerprint;
    uint64_t step_sequence;
    uint64_t event_sequence;
    uint32_t last_step_elapsed_us;
    uint32_t max_step_elapsed_us;
    uint16_t event_count;
    uint16_t event_head;
    atomic_uint pending_break_reason;
    atomic_uint actuation_fault_latched;
    atomic_uint stopped;
    uint8_t producer_count;
    uint8_t initialized;
    uint8_t in_step;
    uint8_t in_break;
    uint8_t guard_active;
    uint8_t last_step_timing_valid;
} wcm_runtime_impl_t;

_Static_assert(sizeof(wcm_runtime_impl_t) <= WCM_RUNTIME_STORAGE_BYTES,
               "WCM_RUNTIME_STORAGE_BYTES is too small for this profile");

#endif

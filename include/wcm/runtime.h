#ifndef WCM_RUNTIME_H
#define WCM_RUNTIME_H

#include <stdalign.h>
#include <stdatomic.h>
#include "wcm/actuator.h"
#include "wcm/capability.h"
#include "wcm/config.h"
#include "wcm/diagnostics.h"
#include "wcm/ingress.h"
#include "wcm/metrics.h"
#include "wcm/event.h"
#include "wcm/health.h"
#include "wcm/module.h"
#include "wcm/version.h"
#include "wcm/world.h"

typedef wcm_status_t (*wcm_clock_read_fn)(void *user, wcm_time_t *out);
typedef void (*wcm_commit_guard_fn)(void *user);

typedef bool (*wcm_resource_admit_fn)(
    void *user,
    uint16_t source_module,
    uint16_t actuator,
    const wcm_value_t *value);

typedef struct wcm_runtime_config {
    uint32_t struct_size;
    uint16_t abi_version;
    uint16_t ingress_fold_limit;
    uint32_t gate_wcet_us;
    uint32_t clock_uncertainty_us;
    uint32_t step_budget_us;
    uint32_t config_check_period_steps;
    uint64_t deployment_id;
    uint32_t config_revision;
    uint32_t reserved0;

    const wcm_witness_source_desc_t *sources;
    uint16_t source_count;
    const wcm_module_desc_t *modules;
    uint8_t module_count;
    const wcm_capability_desc_t *capabilities;
    uint8_t capability_count;
    const wcm_actuator_desc_t *actuators;
    uint8_t actuator_count;
    uint8_t ingress_producer_count;

    wcm_clock_read_fn clock_read;
    void *clock_user;
    wcm_resource_admit_fn resource_admit;
    void *resource_user;
    wcm_commit_guard_fn commit_guard_enter;
    wcm_commit_guard_fn commit_guard_exit;
    void *commit_guard_user;
} wcm_runtime_config_t;

typedef struct {
    void *impl;
} wcm_runtime_t;

#define WCM_RUNTIME_STORAGE(name) \
    _Alignas(max_align_t) unsigned char name[WCM_RUNTIME_STORAGE_BYTES]

void wcm_runtime_config_init(wcm_runtime_config_t *config);
size_t wcm_runtime_storage_required(void);
wcm_status_t wcm_runtime_init(
    wcm_runtime_t *runtime,
    void *storage,
    size_t storage_size,
    const wcm_runtime_config_t *config);

/* SPSC producer-safe. All other runtime APIs are executor-owned unless noted. */
wcm_status_t wcm_runtime_post_observation(
    wcm_runtime_t *runtime,
    uint8_t producer_index,
    const wcm_observation_t *observation);

/* May be called by an ISR/task that is not the Runtime Executor. Requests coalesce. */
wcm_status_t wcm_runtime_request_world_break(
    wcm_runtime_t *runtime,
    wcm_world_break_reason_t reason);

wcm_status_t wcm_runtime_step(wcm_runtime_t *runtime);
wcm_status_t wcm_runtime_world_break(wcm_runtime_t *runtime, wcm_world_break_reason_t reason);
wcm_status_t wcm_runtime_stop(wcm_runtime_t *runtime);

wcm_status_t wcm_runtime_get_metrics(const wcm_runtime_t *runtime, wcm_metrics_t *out);
wcm_status_t wcm_runtime_get_last_fault(const wcm_runtime_t *runtime, wcm_fault_record_t *out);
wcm_status_t wcm_runtime_get_module_stats(
    const wcm_runtime_t *runtime,
    uint16_t module_id,
    wcm_module_stats_t *out);
wcm_status_t wcm_runtime_get_capability_state(
    const wcm_runtime_t *runtime,
    uint16_t capability_id,
    wcm_cap_state_t *out);
wcm_world_state_t wcm_runtime_world_state(const wcm_runtime_t *runtime);
wcm_counter_t wcm_runtime_world_epoch(const wcm_runtime_t *runtime);
bool wcm_runtime_has_capabilities(const wcm_runtime_t *runtime, wcm_capability_set_t required);
wcm_status_t wcm_runtime_get_health(const wcm_runtime_t *runtime, wcm_health_snapshot_t *out);
wcm_status_t wcm_runtime_read_event(
    const wcm_runtime_t *runtime,
    uint64_t after_sequence,
    wcm_event_record_t *out);
wcm_status_t wcm_runtime_get_actuator_stats(
    const wcm_runtime_t *runtime,
    uint16_t actuator_id,
    wcm_actuator_stats_t *out);
uint64_t wcm_runtime_config_fingerprint(const wcm_runtime_config_t *config);
wcm_status_t wcm_runtime_check_configuration(const wcm_runtime_t *runtime);

#endif

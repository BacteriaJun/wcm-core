#ifndef WCM_METRICS_H
#define WCM_METRICS_H

#include <stdint.h>

typedef struct {
    uint64_t observations_published;
    uint64_t ingress_dropped;
    uint64_t ingress_unknown_source;
    uint64_t ingress_rejected_quality;
    uint64_t ingress_rejected_timestamp;
    uint64_t ingress_rejected_policy;
    uint64_t ingress_backlog_deferrals;
    uint64_t module_runs;
    uint64_t module_errors;
    uint64_t deadline_misses;
    uint64_t release_skips;
    uint64_t viability_skips;
    uint64_t wcet_overruns;
    uint64_t gate_wcet_overruns;
    uint64_t intents_proposed;
    uint64_t commits_ok;
    uint64_t commits_modified;
    uint64_t void_world;
    uint64_t void_stale;
    uint64_t void_expired;
    uint64_t void_ingress_backlog;
    uint64_t deny_capability;
    uint64_t deny_resource;
    uint64_t deny_authority;
    uint64_t reject_constraint;
    uint64_t reject_safety;
    uint64_t dispatch_failures;
    uint64_t dispatch_overruns;
    uint64_t port_violations;
    uint64_t safe_apply_failures;
    uint64_t commit_internal_error;
    uint64_t world_breaks;
    uint64_t clock_discontinuities;
    uint64_t stops;
    uint64_t step_budget_overruns;
    uint64_t event_overwrites;
} wcm_metrics_t;

double wcm_metrics_concordance_yield(const wcm_metrics_t *m);

#endif

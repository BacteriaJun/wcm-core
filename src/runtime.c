#include "wcm/runtime.h"
#include "wcm/admission.h"
#include "wcm/concordance.h"
#include "wcm/build.h"
#include "runtime_internal.h"

#include <stdint.h>
#include <string.h>

static wcm_runtime_impl_t *impl_of(wcm_runtime_t *runtime) {
    return runtime ? (wcm_runtime_impl_t *)runtime->impl : NULL;
}

static const wcm_runtime_impl_t *const_impl_of(const wcm_runtime_t *runtime) {
    return runtime ? (const wcm_runtime_impl_t *)runtime->impl : NULL;
}

static wcm_time_t add_saturating(wcm_time_t value, uint64_t delta) {
    return UINT64_MAX - value < delta ? UINT64_MAX : value + delta;
}

static wcm_time_t decision_now(const wcm_runtime_impl_t *rt, wcm_time_t raw_now) {
    return add_saturating(raw_now, (uint64_t)rt->config->clock_uncertainty_us);
}

static void append_event(
    wcm_runtime_impl_t *rt,
    wcm_event_type_t type,
    uint16_t subject_id,
    wcm_status_t status,
    uint32_t detail,
    wcm_time_t now) {
    if (!rt || !rt->config || WCM_MAX_EVENTS == 0u) return;
    if (rt->event_sequence == UINT64_MAX) {
        rt->metrics.event_overwrites++;
        return;
    }
    rt->event_sequence++;
    wcm_event_record_t *record = &rt->events[rt->event_head];
    memset(record, 0, sizeof(*record));
    record->sequence = rt->event_sequence;
    record->at_us = now;
    record->world_epoch = rt->world.epoch;
    record->deployment_id = rt->config->deployment_id;
    record->config_revision = rt->config->config_revision;
    record->detail = detail;
    record->subject_id = subject_id;
    record->status = (int16_t)status;
    record->type = (uint8_t)type;
    rt->event_head = (uint16_t)((rt->event_head + 1u) % WCM_MAX_EVENTS);
    if (rt->event_count < WCM_MAX_EVENTS) rt->event_count++;
    else rt->metrics.event_overwrites++;
}

static void append_capability_changes(
    wcm_runtime_impl_t *rt,
    wcm_capability_set_t before,
    wcm_capability_set_t after,
    wcm_time_t now) {
    const wcm_capability_set_t changed = before ^ after;
    if (changed == 0u) return;
    for (uint16_t id = 0u; id < 64u; ++id) {
        const wcm_capability_set_t bit = wcm_capability_bit(id);
        if ((changed & bit) == 0u) continue;
        append_event(
            rt,
            (after & bit) != 0u ? WCM_EVENT_CAPABILITY_BOUND : WCM_EVENT_CAPABILITY_UNBOUND,
            id,
            WCM_OK,
            0u,
            now);
    }
}

static void record_fault(
    wcm_runtime_impl_t *rt,
    wcm_fault_code_t code,
    wcm_status_t status,
    uint16_t subject_id,
    uint32_t detail,
    wcm_time_t now) {
    if (!rt) return;
    if (rt->last_fault.sequence != UINT64_MAX) rt->last_fault.sequence++;
    rt->last_fault.at_us = now;
    rt->last_fault.world_epoch = rt->world.epoch;
    rt->last_fault.code = code;
    rt->last_fault.status = status;
    rt->last_fault.subject_id = subject_id;
    rt->last_fault.detail = detail;
    append_event(rt, WCM_EVENT_FAULT, subject_id, status, (uint32_t)code, now);
}

static const wcm_witness_source_desc_t *find_source(const wcm_runtime_impl_t *rt, uint16_t id) {
    for (uint16_t i = 0; i < rt->config->source_count; ++i) {
        if (rt->config->sources[i].id == id) return &rt->config->sources[i];
    }
    return NULL;
}

static int find_actuator_index(const wcm_runtime_impl_t *rt, uint16_t id) {
    for (uint8_t i = 0; i < rt->config->actuator_count; ++i) {
        if (rt->config->actuators[i].id == id) return (int)i;
    }
    return -1;
}

static int find_module_index(const wcm_runtime_impl_t *rt, uint16_t id) {
    for (uint8_t i = 0; i < rt->config->module_count; ++i) {
        if (rt->config->modules[i].id == id) return (int)i;
    }
    return -1;
}

static bool valid_break_reason(wcm_world_break_reason_t reason) {
    return reason >= WCM_BREAK_RESET && reason <= WCM_BREAK_APPLICATION;
}

static void metrics_commit(wcm_metrics_t *m, wcm_commit_code_t code) {
    switch (code) {
        case WCM_COMMIT_OK: m->commits_ok++; break;
        case WCM_COMMIT_MODIFIED: m->commits_modified++; break;
        case WCM_VOID_WORLD: m->void_world++; break;
        case WCM_VOID_STALE: m->void_stale++; break;
        case WCM_VOID_EXPIRED: m->void_expired++; break;
        case WCM_VOID_INGRESS_BACKLOG: m->void_ingress_backlog++; break;
        case WCM_DENY_CAPABILITY: m->deny_capability++; break;
        case WCM_DENY_RESOURCE: m->deny_resource++; break;
        case WCM_DENY_AUTHORITY: m->deny_authority++; break;
        case WCM_REJECT_CONSTRAINT: m->reject_constraint++; break;
        case WCM_REJECT_SAFETY: m->reject_safety++; break;
        case WCM_COMMIT_GATE_OVERRUN: m->gate_wcet_overruns++; break;
        case WCM_COMMIT_DISPATCH_FAILED: m->dispatch_failures++; break;
        case WCM_COMMIT_DISPATCH_OVERRUN: m->dispatch_overruns++; break;
        case WCM_COMMIT_PORT_VIOLATION: m->port_violations++; break;
        default: m->commit_internal_error++; break;
    }
}

size_t wcm_runtime_storage_required(void) {
    return sizeof(wcm_runtime_impl_t);
}

void wcm_runtime_config_init(wcm_runtime_config_t *config) {
    if (!config) return;
    memset(config, 0, sizeof(*config));
    config->struct_size = (uint32_t)sizeof(*config);
    config->abi_version = WCM_ABI_VERSION;
    config->ingress_fold_limit = (uint16_t)WCM_MAX_INGRESS;
}

static wcm_status_t force_safe_outputs(wcm_runtime_impl_t *rt, wcm_time_t now) {
    wcm_status_t result = WCM_OK;
    for (uint8_t i = 0; i < rt->config->actuator_count; ++i) {
        const wcm_actuator_desc_t *ad = &rt->config->actuators[i];
        if (!ad->safe || ad->safe(ad->user) != WCM_OK) {
            rt->metrics.safe_apply_failures++;
            atomic_store_explicit(&rt->actuation_fault_latched, 1u, memory_order_release);
            record_fault(rt, WCM_FAULT_SAFE_OUTPUT_FAILED, WCM_ERR_IO, ad->id, 0u, now);
            result = WCM_ERR_IO;
        }
    }
    return result;
}

static void guard_enter(wcm_runtime_impl_t *rt) {
    rt->config->commit_guard_enter(rt->config->commit_guard_user);
    rt->guard_active = 1u;
}

static void guard_exit(wcm_runtime_impl_t *rt) {
    rt->guard_active = 0u;
    rt->config->commit_guard_exit(rt->config->commit_guard_user);
}

static wcm_status_t perform_world_break(
    wcm_runtime_impl_t *rt,
    wcm_world_break_reason_t reason,
    wcm_time_t now) {
    if (!rt) return WCM_ERR_ARG;
    if (rt->in_break) return WCM_ERR_BUSY;
    rt->in_break = 1u;

    const wcm_status_t world_rc = wcm_world_break(&rt->world, reason, now);
    if (world_rc != WCM_OK) {
        record_fault(rt, WCM_FAULT_COUNTER_EXHAUSTED, world_rc, 0u, 0u, now);
        (void)force_safe_outputs(rt, now);
        atomic_store_explicit(&rt->actuation_fault_latched, 1u, memory_order_release);
        rt->world.state = WCM_WORLD_COLD;
        guard_enter(rt);
        for (uint8_t p = 0; p < rt->producer_count; ++p) wcm_ingress_clear(&rt->ingress[p]);
        wcm_witness_invalidate_all(&rt->witnesses);
        guard_exit(rt);
        atomic_store_explicit(&rt->pending_break_reason, 0u, memory_order_release);
        rt->in_break = 0u;
        return world_rc;
    }

    rt->metrics.world_breaks++;
    append_event(rt, WCM_EVENT_WORLD_BREAK, 0u, WCM_OK, (uint32_t)reason, now);
    const wcm_status_t safe_rc = force_safe_outputs(rt, now);

    /* Keep the port critical section short: only the evidence cutover is guarded. */
    guard_enter(rt);
    for (uint8_t p = 0; p < rt->producer_count; ++p) wcm_ingress_clear(&rt->ingress[p]);
    wcm_witness_invalidate_all(&rt->witnesses);
    const wcm_capability_set_t cap_before = rt->capabilities.bound_mask;
    wcm_capability_graph_world_break(&rt->capabilities, now);
    append_capability_changes(rt, cap_before, rt->capabilities.bound_mask, now);
    wcm_capability_graph_update_world(&rt->capabilities, &rt->world);
    atomic_store_explicit(&rt->pending_break_reason, 0u, memory_order_release);
    guard_exit(rt);

    for (uint8_t i = 0; i < rt->config->module_count; ++i) {
        rt->modules[i].core.dependency_clock = 1u;
        rt->modules[i].next_release_us = now;
        if (rt->config->modules[i].reset) rt->config->modules[i].reset(rt->config->modules[i].user);
    }

    if (atomic_load_explicit(&rt->actuation_fault_latched, memory_order_acquire) != 0u) {
        rt->world.state = WCM_WORLD_COLD;
    }
    rt->in_break = 0u;
    return safe_rc;
}

static wcm_status_t read_clock(wcm_runtime_impl_t *rt, wcm_time_t *out) {
    if (!rt || !out || !rt->config->clock_read) return WCM_ERR_ARG;
    wcm_time_t now = 0u;
    const wcm_status_t clock_rc = rt->config->clock_read(rt->config->clock_user, &now);
    if (clock_rc != WCM_OK) {
        record_fault(rt, WCM_FAULT_CLOCK_READ_FAILED, clock_rc, 0u, 0u, rt->last_clock);
        (void)force_safe_outputs(rt, rt->last_clock);
        atomic_store_explicit(&rt->actuation_fault_latched, 1u, memory_order_release);
        rt->world.state = WCM_WORLD_COLD;
        return WCM_ERR_CLOCK;
    }
    if (now < rt->last_clock) {
        rt->metrics.clock_discontinuities++;
        record_fault(rt, WCM_FAULT_CLOCK_DISCONTINUITY, WCM_ERR_CLOCK, 0u, 0u, now);
        rt->last_clock = now;
        unsigned expected = 0u;
        (void)atomic_compare_exchange_strong_explicit(
            &rt->pending_break_reason,
            &expected,
            (unsigned)WCM_BREAK_CLOCK_DISCONTINUITY,
            memory_order_release,
            memory_order_relaxed);
        *out = now;
        if (rt->guard_active == 0u) {
            const wcm_world_break_reason_t pending = (wcm_world_break_reason_t)atomic_exchange_explicit(
                &rt->pending_break_reason, 0u, memory_order_acq_rel);
            if ((unsigned)pending != 0u) (void)perform_world_break(rt, pending, now);
        }
        return WCM_ERR_CLOCK;
    }
    rt->last_clock = now;
    *out = now;
    return WCM_OK;
}

static void update_drop_metric(wcm_runtime_impl_t *rt) {
    uint64_t total = 0u;
    for (uint8_t p = 0; p < rt->producer_count; ++p) total += wcm_ingress_dropped(&rt->ingress[p]);
    rt->metrics.ingress_dropped = total;
}

static uint32_t pending_ingress(const wcm_runtime_impl_t *rt) {
    uint32_t total = 0u;
    for (uint8_t p = 0; p < rt->producer_count; ++p) total += wcm_ingress_count(&rt->ingress[p]);
    return total;
}

static wcm_status_t finish_step(wcm_runtime_impl_t *rt) {
    if (!rt || (rt->last_step_started_us == 0u && rt->step_sequence == 0u)) return WCM_OK;
    wcm_time_t end = 0u;
    const wcm_status_t clock_rc = read_clock(rt, &end);
    if (clock_rc != WCM_OK) return clock_rc;
    rt->last_step_completed_us = end;
    if (rt->world.epoch != rt->step_start_epoch) {
        rt->last_step_elapsed_us = 0u;
        rt->last_step_timing_valid = 0u;
        return WCM_OK;
    }
    if (end < rt->last_step_started_us) return WCM_ERR_CLOCK;
    const wcm_time_t elapsed = end - rt->last_step_started_us;
    rt->last_step_timing_valid = 1u;
    rt->last_step_elapsed_us = (uint32_t)(elapsed > UINT32_MAX ? UINT32_MAX : elapsed);
    if (rt->last_step_elapsed_us > rt->max_step_elapsed_us) rt->max_step_elapsed_us = rt->last_step_elapsed_us;
    if (rt->config->step_budget_us > 0u && elapsed > (wcm_time_t)rt->config->step_budget_us) {
        rt->metrics.step_budget_overruns++;
        record_fault(
            rt,
            WCM_FAULT_STEP_BUDGET_OVERRUN,
            WCM_ERR_WCET,
            0u,
            rt->last_step_elapsed_us,
            end);
    }
    return WCM_OK;
}

static int select_ingress(const wcm_runtime_impl_t *rt, wcm_observation_t *selected) {
    int selected_index = -1;
    wcm_observation_t best;
    for (uint8_t p = 0; p < rt->producer_count; ++p) {
        wcm_observation_t candidate;
        if (wcm_ingress_peek(&rt->ingress[p], &candidate) != WCM_OK) continue;
        if (selected_index < 0 || candidate.observed_at < best.observed_at ||
            (candidate.observed_at == best.observed_at && p < (uint8_t)selected_index)) {
            selected_index = (int)p;
            best = candidate;
        }
    }
    if (selected_index >= 0 && selected) *selected = best;
    return selected_index;
}

static wcm_status_t process_ingress(wcm_runtime_impl_t *rt, uint32_t limit, bool *drained) {
    uint32_t processed = 0u;
    while (processed < limit) {
        wcm_observation_t peeked;
        const int producer = select_ingress(rt, &peeked);
        if (producer < 0) break;

        wcm_observation_t obs;
        if (wcm_ingress_pop(&rt->ingress[producer], &obs) != WCM_OK) return WCM_ERR_STATE;
        processed++;

        const wcm_witness_source_desc_t *source = find_source(rt, obs.witness_id);
        if (!source) {
            rt->metrics.ingress_unknown_source++;
            continue;
        }

        wcm_time_t now;
        wcm_status_t rc = read_clock(rt, &now);
        if (rc != WCM_OK) return rc;

        wcm_witness_t before_storage;
        const wcm_witness_t *before_ptr = wcm_witness_get(&rt->witnesses, obs.witness_id);
        const wcm_witness_t *before = NULL;
        if (before_ptr) {
            before_storage = *before_ptr;
            before = &before_storage;
        }

        rc = wcm_witness_publish(
            &rt->witnesses,
            &rt->world,
            obs.witness_id,
            &obs.value,
            now,
            obs.observed_at,
            obs.requested_livebound,
            source,
            obs.quality,
            obs.flags);
        if (rc == WCM_ERR_QUALITY) {
            rt->metrics.ingress_rejected_quality++;
            continue;
        }
        if (rc == WCM_ERR_TIMESTAMP) {
            rt->metrics.ingress_rejected_timestamp++;
            continue;
        }
        if (rc == WCM_ERR_POLICY) {
            rt->metrics.ingress_rejected_policy++;
            continue;
        }
        if (rc == WCM_ERR_COUNTER) {
            record_fault(rt, WCM_FAULT_COUNTER_EXHAUSTED, rc, obs.witness_id, 0u, now);
            (void)perform_world_break(rt, WCM_BREAK_STATE_CORRUPTION, now);
            return rc;
        }
        if (rc != WCM_OK) return rc;

        rt->metrics.observations_published++;
        const wcm_witness_t *after = wcm_witness_get(&rt->witnesses, obs.witness_id);
        for (uint8_t m = 0; m < rt->config->module_count; ++m) {
            const wcm_module_desc_t *md = &rt->config->modules[m];
            for (uint8_t d = 0; d < md->dependency_count; ++d) {
                if (md->dependencies[d].witness_id != obs.witness_id) continue;
                rc = wcm_dependency_apply_transition(
                    &rt->modules[m].core,
                    &md->dependencies[d],
                    before,
                    after);
                if (rc == WCM_ERR_COUNTER) {
                    record_fault(rt, WCM_FAULT_COUNTER_EXHAUSTED, rc, md->id, 0u, now);
                    (void)perform_world_break(rt, WCM_BREAK_STATE_CORRUPTION, now);
                    return rc;
                }
                if (rc != WCM_OK) return rc;
            }
        }

        const wcm_capability_set_t cap_before = rt->capabilities.bound_mask;
        rc = wcm_capability_graph_observe(
            &rt->capabilities,
            obs.witness_id,
            obs.quality,
            obs.observed_at,
            wcm_witness_previous_observed_at(&rt->witnesses, obs.witness_id));
        if (rc != WCM_OK) return rc;
        append_capability_changes(rt, cap_before, rt->capabilities.bound_mask, now);
    }

    update_drop_metric(rt);
    wcm_capability_graph_update_world(&rt->capabilities, &rt->world);
    if (drained) *drained = pending_ingress(rt) == 0u;
    return WCM_OK;
}

static bool dependency_snapshot(
    wcm_runtime_impl_t *rt,
    uint8_t module_index,
    wcm_time_t raw_now,
    wcm_snapshot_t *snapshot) {
    const wcm_module_desc_t *md = &rt->config->modules[module_index];
    if (wcm_snapshot_begin(
            snapshot,
            &rt->world,
            md->id,
            rt->modules[module_index].core.dependency_clock) != WCM_OK) {
        return false;
    }

    const wcm_time_t now = decision_now(rt, raw_now);
    for (uint8_t d = 0; d < md->dependency_count; ++d) {
        const wcm_dependency_desc_t *dep = &md->dependencies[d];
        const wcm_witness_t *w = wcm_witness_get(&rt->witnesses, dep->witness_id);
        if (!w || !wcm_witness_is_live(w, &rt->world, now) || w->quality < dep->quality_floor) return false;
        if (wcm_snapshot_observe(snapshot, dep->witness_id, w) != WCM_OK) return false;
    }
    return wcm_snapshot_seal(snapshot) == WCM_OK;
}

static wcm_commit_code_t concordance_now(
    wcm_runtime_impl_t *rt,
    uint8_t module_index,
    const wcm_actuator_desc_t *ad,
    const wcm_intent_t *intent,
    wcm_time_t raw_now) {
    const wcm_module_desc_t *md = &rt->config->modules[module_index];
    const wcm_commit_context_t ctx = {
        .effect_latency_us = ad->effect_latency_us,
        .capability_ok = wcm_capability_graph_has(&rt->capabilities, md->required_capabilities),
    };
    return wcm_concordance_commit(
        &rt->world,
        rt->modules[module_index].core.dependency_clock,
        decision_now(rt, raw_now),
        &ctx,
        intent);
}

static bool pending_break(const wcm_runtime_impl_t *rt) {
    return atomic_load_explicit(&rt->pending_break_reason, memory_order_acquire) != 0u;
}

static wcm_world_break_reason_t take_pending_break(wcm_runtime_impl_t *rt) {
    return (wcm_world_break_reason_t)atomic_exchange_explicit(
        &rt->pending_break_reason,
        0u,
        memory_order_acq_rel);
}

static wcm_status_t execute_pending_break(wcm_runtime_impl_t *rt, wcm_time_t now) {
    const wcm_world_break_reason_t reason = take_pending_break(rt);
    if ((unsigned)reason == 0u) return WCM_OK;
    return perform_world_break(rt, reason, now);
}

static wcm_commit_code_t commit_intent(
    wcm_runtime_impl_t *rt,
    uint8_t module_index,
    const wcm_intent_t *intent) {
    if (!rt || !intent || module_index >= rt->config->module_count) return WCM_COMMIT_INTERNAL_ERROR;
    const wcm_module_desc_t *md = &rt->config->modules[module_index];
    if (intent->source_module != md->id) return WCM_COMMIT_INTERNAL_ERROR;

    const wcm_actuator_set_t actuator_bit = wcm_actuator_bit(intent->actuator);
    if (actuator_bit == 0u || (md->allowed_actuators & actuator_bit) == 0u) return WCM_DENY_AUTHORITY;

    const int actuator_index = find_actuator_index(rt, intent->actuator);
    if (actuator_index < 0) return WCM_COMMIT_INTERNAL_ERROR;
    const wcm_actuator_desc_t *ad = &rt->config->actuators[actuator_index];

    wcm_time_t gate_start;
    if (read_clock(rt, &gate_start) != WCM_OK) return WCM_VOID_WORLD;
    wcm_commit_code_t code = concordance_now(rt, module_index, ad, intent, gate_start);
    if (code != WCM_COMMIT_OK) return code;

    wcm_value_t value = intent->value;
    bool modified = false;
    if (ad->constraint_transform) {
        const wcm_filter_result_t result = ad->constraint_transform(ad->user, &value);
        if (result == WCM_FILTER_REJECT) return WCM_REJECT_CONSTRAINT;
        modified = modified || result == WCM_FILTER_MODIFY;
    }
    if (ad->safety_transform) {
        const wcm_filter_result_t result = ad->safety_transform(ad->user, &value);
        if (result == WCM_FILTER_REJECT) return WCM_REJECT_SAFETY;
        modified = modified || result == WCM_FILTER_MODIFY;
    }

    bool drained = false;
    wcm_status_t rc = process_ingress(rt, rt->config->ingress_fold_limit, &drained);
    if (rc != WCM_OK) return rc == WCM_ERR_CLOCK ? WCM_VOID_WORLD : WCM_COMMIT_INTERNAL_ERROR;
    if (!drained) return WCM_VOID_INGRESS_BACKLOG;

    wcm_time_t after_transform;
    if (read_clock(rt, &after_transform) != WCM_OK) return WCM_VOID_WORLD;
    code = concordance_now(rt, module_index, ad, intent, after_transform);
    if (code != WCM_COMMIT_OK) return code;

    if (pending_break(rt)) {
        (void)execute_pending_break(rt, after_transform);
        return WCM_VOID_WORLD;
    }

    guard_enter(rt);

    const uint32_t guarded_pending = pending_ingress(rt);
    drained = guarded_pending == 0u;
    if (guarded_pending > 0u) {
        rc = process_ingress(rt, guarded_pending, &drained);
        if (rc != WCM_OK) {
            guard_exit(rt);
            if (rc == WCM_ERR_CLOCK && pending_break(rt)) (void)execute_pending_break(rt, rt->last_clock);
            return rc == WCM_ERR_CLOCK ? WCM_VOID_WORLD : WCM_COMMIT_INTERNAL_ERROR;
        }
    }
    if (!drained || pending_ingress(rt) != 0u) {
        wcm_time_t now = rt->last_clock;
        record_fault(rt, WCM_FAULT_PORT_CONTRACT, WCM_ERR_STATE, intent->actuator, 0u, now);
        guard_exit(rt);
        (void)perform_world_break(rt, WCM_BREAK_STATE_CORRUPTION, now);
        return WCM_COMMIT_PORT_VIOLATION;
    }

    if (pending_break(rt)) {
        const wcm_world_break_reason_t reason = take_pending_break(rt);
        guard_exit(rt);
        wcm_time_t now;
        if (read_clock(rt, &now) == WCM_OK) (void)perform_world_break(rt, reason, now);
        return WCM_VOID_WORLD;
    }

    wcm_time_t before_validation;
    if (read_clock(rt, &before_validation) != WCM_OK) {
        guard_exit(rt);
        if (pending_break(rt)) (void)execute_pending_break(rt, rt->last_clock);
        return WCM_VOID_WORLD;
    }
    code = concordance_now(rt, module_index, ad, intent, before_validation);
    if (code != WCM_COMMIT_OK) {
        guard_exit(rt);
        return code;
    }

    if (rt->config->resource_admit &&
        !rt->config->resource_admit(rt->config->resource_user, md->id, intent->actuator, &value)) {
        guard_exit(rt);
        return WCM_DENY_RESOURCE;
    }
    if (ad->constraint_validate && !ad->constraint_validate(ad->user, &value)) {
        guard_exit(rt);
        return WCM_REJECT_CONSTRAINT;
    }
    if (ad->safety_validate && !ad->safety_validate(ad->user, &value)) {
        guard_exit(rt);
        return WCM_REJECT_SAFETY;
    }

    const uint32_t callback_pending = pending_ingress(rt);
    drained = callback_pending == 0u;
    if (callback_pending > 0u) {
        rc = process_ingress(rt, callback_pending, &drained);
        if (rc != WCM_OK) {
            guard_exit(rt);
            if (rc == WCM_ERR_CLOCK && pending_break(rt)) (void)execute_pending_break(rt, rt->last_clock);
            return rc == WCM_ERR_CLOCK ? WCM_VOID_WORLD : WCM_COMMIT_INTERNAL_ERROR;
        }
    }
    if (!drained || pending_ingress(rt) != 0u) {
        wcm_time_t now = rt->last_clock;
        record_fault(rt, WCM_FAULT_PORT_CONTRACT, WCM_ERR_STATE, intent->actuator, 1u, now);
        guard_exit(rt);
        (void)perform_world_break(rt, WCM_BREAK_STATE_CORRUPTION, now);
        return WCM_COMMIT_PORT_VIOLATION;
    }

    if (pending_break(rt)) {
        const wcm_world_break_reason_t reason = take_pending_break(rt);
        guard_exit(rt);
        wcm_time_t now;
        if (read_clock(rt, &now) == WCM_OK) (void)perform_world_break(rt, reason, now);
        return WCM_VOID_WORLD;
    }

    wcm_time_t pre_apply;
    if (read_clock(rt, &pre_apply) != WCM_OK) {
        guard_exit(rt);
        if (pending_break(rt)) (void)execute_pending_break(rt, rt->last_clock);
        return WCM_VOID_WORLD;
    }
    code = concordance_now(rt, module_index, ad, intent, pre_apply);
    if (code != WCM_COMMIT_OK) {
        guard_exit(rt);
        return code;
    }

    if (pre_apply - gate_start > (wcm_time_t)rt->config->gate_wcet_us) {
        record_fault(
            rt,
            WCM_FAULT_GATE_WCET_OVERRUN,
            WCM_ERR_WCET,
            intent->actuator,
            (uint32_t)((pre_apply - gate_start) > UINT32_MAX ? UINT32_MAX : (pre_apply - gate_start)),
            pre_apply);
        guard_exit(rt);
        return WCM_COMMIT_GATE_OVERRUN;
    }

    const wcm_status_t dispatch_rc = ad->apply(ad->user, &value);
    wcm_time_t dispatch_end = 0u;
    const wcm_status_t dispatch_clock_rc = read_clock(rt, &dispatch_end);
    wcm_actuator_runtime_t *act_rt = &rt->actuator_runtime[actuator_index];
    act_rt->dispatches++;
    guard_exit(rt);
    if (dispatch_clock_rc != WCM_OK) return WCM_VOID_WORLD;

    if (dispatch_rc != WCM_OK) {
        act_rt->failures++;
        record_fault(rt, WCM_FAULT_ACTUATOR_DISPATCH_FAILED, dispatch_rc, intent->actuator, 0u, dispatch_end);
        (void)perform_world_break(rt, WCM_BREAK_ACTUATOR_FAILURE, dispatch_end);
        return WCM_COMMIT_DISPATCH_FAILED;
    }

    const wcm_time_t dispatch_elapsed = dispatch_end - pre_apply;
    const uint32_t dispatch_elapsed_u32 = (uint32_t)(dispatch_elapsed > UINT32_MAX ? UINT32_MAX : dispatch_elapsed);
    if (dispatch_elapsed_u32 > act_rt->max_dispatch_us) act_rt->max_dispatch_us = dispatch_elapsed_u32;
    if (dispatch_elapsed > (wcm_time_t)ad->dispatch_wcet_us) {
        act_rt->overruns++;
        record_fault(
            rt,
            WCM_FAULT_ACTUATOR_DISPATCH_OVERRUN,
            WCM_ERR_WCET,
            intent->actuator,
            (uint32_t)(dispatch_elapsed > UINT32_MAX ? UINT32_MAX : dispatch_elapsed),
            dispatch_end);
        (void)perform_world_break(rt, WCM_BREAK_ACTUATOR_DOMAIN_RESET, dispatch_end);
        return WCM_COMMIT_DISPATCH_OVERRUN;
    }

    if (pending_break(rt)) (void)execute_pending_break(rt, dispatch_end);
    return modified ? WCM_COMMIT_MODIFIED : WCM_COMMIT_OK;
}

static bool config_has_source(const wcm_runtime_config_t *config, uint16_t id) {
    for (uint16_t i = 0; i < config->source_count; ++i) {
        if (config->sources[i].id == id) return true;
    }
    return false;
}

static wcm_capability_set_t configured_capability_mask(const wcm_runtime_config_t *config) {
    wcm_capability_set_t mask = 0u;
    for (uint8_t i = 0; i < config->capability_count; ++i) {
        mask |= wcm_capability_bit(config->capabilities[i].capability_id);
    }
    return mask;
}

static wcm_actuator_set_t configured_actuator_mask(const wcm_runtime_config_t *config) {
    wcm_actuator_set_t mask = 0u;
    for (uint8_t i = 0; i < config->actuator_count; ++i) {
        mask |= wcm_actuator_bit(config->actuators[i].id);
    }
    return mask;
}

static wcm_status_t validate_config(const wcm_runtime_config_t *config) {
    if (!config) return WCM_ERR_ARG;
    if (config->struct_size != sizeof(*config) || config->abi_version != WCM_ABI_VERSION) return WCM_ERR_ABI;
    if (!config->clock_read) return WCM_ERR_ARG;
    if ((config->source_count && !config->sources) ||
        (config->module_count && !config->modules) ||
        (config->capability_count && !config->capabilities) ||
        (config->actuator_count && !config->actuators)) {
        return WCM_ERR_ARG;
    }
    if (config->source_count > WCM_MAX_WITNESSES ||
        config->module_count > WCM_MAX_MODULES ||
        config->capability_count > WCM_MAX_CAPABILITIES ||
        config->actuator_count > WCM_MAX_ACTUATORS ||
        config->ingress_producer_count == 0u ||
        config->ingress_producer_count > WCM_MAX_INGRESS_PRODUCERS ||
        config->ingress_fold_limit == 0u) {
        return WCM_ERR_RANGE;
    }
    if (config->module_count > 0u && config->gate_wcet_us == 0u) return WCM_ERR_ARG;
    if (!config->commit_guard_enter || !config->commit_guard_exit) return WCM_ERR_ARG;

    for (uint16_t i = 0; i < config->source_count; ++i) {
        const wcm_witness_source_desc_t *source = &config->sources[i];
        if (source->id >= WCM_MAX_WITNESSES) return WCM_ERR_RANGE;
        if (source->timestamp_policy > WCM_TS_NONDECREASING) return WCM_ERR_ARG;
        if ((source->allowed_flags & (uint8_t)~WCM_WITNESS_OBSERVATION_FLAGS) != 0u) return WCM_ERR_POLICY;
        for (uint16_t j = (uint16_t)(i + 1u); j < config->source_count; ++j) {
            if (source->id == config->sources[j].id) return WCM_ERR_STATE;
        }
    }

    const wcm_capability_set_t capability_mask = configured_capability_mask(config);
    const wcm_actuator_set_t actuator_mask = configured_actuator_mask(config);

    for (uint8_t i = 0; i < config->module_count; ++i) {
        const wcm_module_desc_t *module = &config->modules[i];
        if (!module->step || !module->dependencies || module->dependency_count == 0u ||
            module->dependency_count > WCM_MAX_SNAPSHOT_ITEMS || module->wcet_us == 0u ||
            module->period_us == 0u) {
            return WCM_ERR_ARG;
        }
        if ((module->required_capabilities & ~capability_mask) != 0u ||
            (module->allowed_actuators & ~actuator_mask) != 0u || module->allowed_actuators == 0u) {
            return WCM_ERR_POLICY;
        }
        for (uint8_t d = 0; d < module->dependency_count; ++d) {
            if (!config_has_source(config, module->dependencies[d].witness_id)) return WCM_ERR_NOT_FOUND;
            if (module->dependencies[d].lens > WCM_LENS_VALIDITY) return WCM_ERR_ARG;
            for (uint8_t e = (uint8_t)(d + 1u); e < module->dependency_count; ++e) {
                if (module->dependencies[d].witness_id == module->dependencies[e].witness_id) return WCM_ERR_STATE;
            }
        }
        for (uint8_t j = (uint8_t)(i + 1u); j < config->module_count; ++j) {
            if (module->id == config->modules[j].id) return WCM_ERR_STATE;
        }
    }

    for (uint8_t i = 0; i < config->capability_count; ++i) {
        const wcm_capability_desc_t *cap = &config->capabilities[i];
        if (cap->capability_id >= 64u || cap->requirement_count > WCM_MAX_REBIND_REQS) return WCM_ERR_RANGE;
        if (cap->requirement_count > 0u && !cap->requirements) return WCM_ERR_ARG;
        for (uint8_t r = 0; r < cap->requirement_count; ++r) {
            if (!config_has_source(config, cap->requirements[r].witness_id)) return WCM_ERR_NOT_FOUND;
        }
    }

    if (config->module_count > 0u && config->actuator_count == 0u) return WCM_ERR_ARG;
    for (uint8_t i = 0; i < config->actuator_count; ++i) {
        const wcm_actuator_desc_t *actuator = &config->actuators[i];
        if (actuator->id >= 64u) return WCM_ERR_RANGE;
        if (!actuator->apply || !actuator->safe || actuator->dispatch_wcet_us == 0u) return WCM_ERR_ARG;
        if (actuator->constraint_transform && !actuator->constraint_validate) return WCM_ERR_ARG;
        if (actuator->safety_transform && !actuator->safety_validate) return WCM_ERR_ARG;
        for (uint8_t j = (uint8_t)(i + 1u); j < config->actuator_count; ++j) {
            if (actuator->id == config->actuators[j].id) return WCM_ERR_STATE;
        }
    }
    return WCM_OK;
}

static uint32_t module_max_effect_latency(
    const wcm_runtime_config_t *config,
    const wcm_module_desc_t *module) {
    uint32_t max_latency = 0u;
    for (uint8_t i = 0; i < config->actuator_count; ++i) {
        const wcm_actuator_desc_t *actuator = &config->actuators[i];
        if ((module->allowed_actuators & wcm_actuator_bit(actuator->id)) != 0u &&
            actuator->effect_latency_us > max_latency) {
            max_latency = actuator->effect_latency_us;
        }
    }
    return max_latency;
}

wcm_status_t wcm_runtime_init(
    wcm_runtime_t *runtime,
    void *storage,
    size_t storage_size,
    const wcm_runtime_config_t *config) {
    if (!runtime || !storage) return WCM_ERR_ARG;
    runtime->impl = NULL;
    if (storage_size < sizeof(wcm_runtime_impl_t)) return WCM_ERR_RANGE;
    if (((uintptr_t)storage % _Alignof(wcm_runtime_impl_t)) != 0u) return WCM_ERR_ARG;

    wcm_status_t rc = validate_config(config);
    if (rc != WCM_OK) return rc;

    wcm_runtime_impl_t *rt = (wcm_runtime_impl_t *)storage;
    memset(rt, 0, sizeof(*rt));
    rt->config_storage = *config;
    rt->config = &rt->config_storage;
    rt->config_fingerprint = wcm_runtime_config_fingerprint(config);
    rt->producer_count = config->ingress_producer_count;
    atomic_init(&rt->pending_break_reason, 0u);
    atomic_init(&rt->actuation_fault_latched, 0u);
    atomic_init(&rt->stopped, 0u);
    wcm_witness_store_init(&rt->witnesses);
    for (uint8_t p = 0; p < rt->producer_count; ++p) wcm_ingress_init(&rt->ingress[p]);

    wcm_time_t now = 0u;
    const wcm_status_t init_clock_rc = config->clock_read(config->clock_user, &now);
    if (init_clock_rc != WCM_OK) return WCM_ERR_CLOCK;
    rt->last_clock = now;
    wcm_world_init(&rt->world, now);
    rc = wcm_capability_graph_init(&rt->capabilities, config->capabilities, config->capability_count, now);
    if (rc != WCM_OK) return rc;
    wcm_capability_graph_update_world(&rt->capabilities, &rt->world);

    for (uint8_t i = 0; i < config->module_count; ++i) {
        rt->modules[i].core.dependency_clock = 1u;
        rt->modules[i].next_release_us = now;
        rt->modules[i].max_effect_latency_us = module_max_effect_latency(config, &config->modules[i]);
        if (config->modules[i].reset) config->modules[i].reset(config->modules[i].user);
    }

    rt->initialized = 1u;
    if (force_safe_outputs(rt, now) != WCM_OK) {
        rt->world.state = WCM_WORLD_COLD;
        rt->initialized = 0u;
        return WCM_ERR_IO;
    }

    runtime->impl = rt;
    append_event(rt, WCM_EVENT_RUNTIME_READY, 0u, WCM_OK, 0u, now);
    return WCM_OK;
}

wcm_status_t wcm_runtime_post_observation(
    wcm_runtime_t *runtime,
    uint8_t producer_index,
    const wcm_observation_t *observation) {
    wcm_runtime_impl_t *rt = impl_of(runtime);
    if (!rt || !rt->initialized || !observation) return WCM_ERR_ARG;
    if (atomic_load_explicit(&rt->stopped, memory_order_acquire) != 0u ||
        atomic_load_explicit(&rt->actuation_fault_latched, memory_order_acquire) != 0u) return WCM_ERR_STOPPED;
    if (producer_index >= rt->producer_count) return WCM_ERR_RANGE;
    return wcm_ingress_push(&rt->ingress[producer_index], observation);
}

wcm_status_t wcm_runtime_request_world_break(
    wcm_runtime_t *runtime,
    wcm_world_break_reason_t reason) {
    wcm_runtime_impl_t *rt = impl_of(runtime);
    if (!rt || !rt->initialized || !valid_break_reason(reason)) return WCM_ERR_ARG;
    if (atomic_load_explicit(&rt->stopped, memory_order_acquire) != 0u) return WCM_ERR_STOPPED;
    unsigned expected = 0u;
    (void)atomic_compare_exchange_strong_explicit(
        &rt->pending_break_reason,
        &expected,
        (unsigned)reason,
        memory_order_release,
        memory_order_relaxed);
    return WCM_OK;
}

static wcm_status_t advance_release(
    wcm_runtime_impl_t *rt,
    wcm_module_slot_t *slot,
    const wcm_module_desc_t *md,
    wcm_time_t now) {
    const wcm_time_t scheduled = slot->next_release_us;
    if (now < scheduled) return WCM_OK;

    const uint64_t elapsed = now - scheduled;
    const uint64_t releases = elapsed / (uint64_t)md->period_us + 1u;
    if (releases > 1u) {
        const uint64_t skipped = releases - 1u;
        slot->core.release_skips += skipped > UINT32_MAX ? UINT32_MAX : (uint32_t)skipped;
        rt->metrics.release_skips += skipped;
    }
    if (releases > UINT64_MAX / (uint64_t)md->period_us) return WCM_ERR_COUNTER;
    const uint64_t delta = releases * (uint64_t)md->period_us;
    if (UINT64_MAX - scheduled < delta) return WCM_ERR_COUNTER;
    slot->next_release_us = scheduled + delta;

    if (md->deadline_us > 0u && elapsed > (uint64_t)md->deadline_us) {
        slot->core.deadline_misses++;
        rt->metrics.deadline_misses++;
    }
    return WCM_OK;
}

wcm_status_t wcm_runtime_step(wcm_runtime_t *runtime) {
    wcm_runtime_impl_t *rt = impl_of(runtime);
    if (!rt || !rt->initialized) return WCM_ERR_ARG;
    if (atomic_load_explicit(&rt->stopped, memory_order_acquire) != 0u ||
        atomic_load_explicit(&rt->actuation_fault_latched, memory_order_acquire) != 0u) return WCM_ERR_STOPPED;
    if (rt->in_step || rt->in_break) return WCM_ERR_BUSY;
    rt->in_step = 1u;

    wcm_status_t rc = WCM_OK;
    wcm_time_t now;
    if (read_clock(rt, &now) != WCM_OK) {
        rt->in_step = 0u;
        return WCM_ERR_CLOCK;
    }
    rt->step_sequence++;
    rt->last_step_started_us = now;
    rt->step_start_epoch = rt->world.epoch;
    rt->last_step_timing_valid = 0u;
    if (rt->config->config_check_period_steps > 0u &&
        (rt->step_sequence % (uint64_t)rt->config->config_check_period_steps) == 0u &&
        wcm_runtime_config_fingerprint(rt->config) != rt->config_fingerprint) {
        record_fault(rt, WCM_FAULT_CONFIG_MUTATION, WCM_ERR_STATE, 0u, 0u, now);
        rc = perform_world_break(rt, WCM_BREAK_STATE_CORRUPTION, now);
        const wcm_status_t finish_rc = finish_step(rt);
        rt->in_step = 0u;
        if (finish_rc != WCM_OK) return finish_rc;
        return rc == WCM_OK ? WCM_ERR_STATE : rc;
    }
    if (pending_break(rt)) {
        rc = execute_pending_break(rt, now);
        const wcm_status_t finish_rc = finish_step(rt);
        rt->in_step = 0u;
        return finish_rc != WCM_OK ? finish_rc : rc;
    }

    bool drained = false;
    rc = process_ingress(rt, rt->config->ingress_fold_limit, &drained);
    if (rc != WCM_OK) {
        const wcm_status_t finish_rc = finish_step(rt);
        rt->in_step = 0u;
        return finish_rc != WCM_OK ? finish_rc : rc;
    }
    if (!drained) {
        rt->metrics.ingress_backlog_deferrals++;
        const uint32_t backlog = pending_ingress(rt);
        record_fault(rt, WCM_FAULT_INGRESS_BACKLOG, WCM_ERR_BACKLOG, 0u, backlog, now);
        append_event(rt, WCM_EVENT_INGRESS_BACKLOG, 0u, WCM_ERR_BACKLOG, backlog, now);
        const wcm_status_t finish_rc = finish_step(rt);
        rt->in_step = 0u;
        return finish_rc;
    }

    for (uint8_t i = 0; i < rt->config->module_count; ++i) {
        rc = read_clock(rt, &now);
        if (rc != WCM_OK) break;

        if (pending_break(rt)) {
            rc = execute_pending_break(rt, now);
            break;
        }

        const wcm_module_desc_t *md = &rt->config->modules[i];
        wcm_module_slot_t *slot = &rt->modules[i];
        if (now < slot->next_release_us) continue;

        rc = advance_release(rt, slot, md, now);
        if (rc != WCM_OK) {
            record_fault(rt, WCM_FAULT_COUNTER_EXHAUSTED, rc, md->id, 0u, now);
            (void)perform_world_break(rt, WCM_BREAK_STATE_CORRUPTION, now);
            break;
        }

        if (!wcm_capability_graph_has(&rt->capabilities, md->required_capabilities)) {
            rt->metrics.deny_capability++;
            continue;
        }

        wcm_snapshot_t snapshot;
        if (!dependency_snapshot(rt, i, now, &snapshot)) continue;

        const wcm_admission_code_t admit = wcm_precommit_admit(
            &snapshot,
            &rt->world,
            decision_now(rt, now),
            md->wcet_us,
            rt->config->gate_wcet_us,
            slot->max_effect_latency_us);
        if (admit != WCM_ADMIT_OK) {
            slot->core.viability_skips++;
            rt->metrics.viability_skips++;
            continue;
        }

        wcm_time_t controller_start;
        if (read_clock(rt, &controller_start) != WCM_OK) {
            rc = WCM_ERR_CLOCK;
            break;
        }

        wcm_intent_t intent;
        memset(&intent, 0, sizeof(intent));
        const wcm_status_t step_rc = md->step(&snapshot, md->user, &intent);
        slot->runs++;
        rt->metrics.module_runs++;

        wcm_time_t controller_end;
        if (read_clock(rt, &controller_end) != WCM_OK) {
            rc = WCM_ERR_CLOCK;
            break;
        }
        const wcm_time_t elapsed = controller_end - controller_start;
        slot->core.last_exec_us = elapsed > UINT32_MAX ? UINT32_MAX : (uint32_t)elapsed;
        if (slot->core.last_exec_us > slot->core.max_exec_us) slot->core.max_exec_us = slot->core.last_exec_us;
        if (elapsed > (wcm_time_t)md->wcet_us) {
            slot->core.wcet_overruns++;
            slot->errors++;
            rt->metrics.wcet_overruns++;
            rt->metrics.module_errors++;
            record_fault(
                rt,
                WCM_FAULT_MODULE_WCET_OVERRUN,
                WCM_ERR_WCET,
                md->id,
                (uint32_t)(elapsed > UINT32_MAX ? UINT32_MAX : elapsed),
                controller_end);
            continue;
        }
        if (step_rc != WCM_OK) {
            slot->errors++;
            rt->metrics.module_errors++;
            record_fault(rt, WCM_FAULT_MODULE_CALLBACK, step_rc, md->id, 0u, controller_end);
            continue;
        }

        if (pending_break(rt)) {
            rc = execute_pending_break(rt, controller_end);
            break;
        }

        if (intent.source_module != md->id ||
            intent.stamp.world_epoch != snapshot.stamp.world_epoch ||
            intent.stamp.dependency_clock != snapshot.stamp.dependency_clock ||
            intent.stamp.livebound != snapshot.stamp.livebound) {
            slot->errors++;
            rt->metrics.module_errors++;
            continue;
        }
        rt->metrics.intents_proposed++;

        rc = process_ingress(rt, rt->config->ingress_fold_limit, &drained);
        if (rc != WCM_OK) break;
        if (!drained) {
            metrics_commit(&rt->metrics, WCM_VOID_INGRESS_BACKLOG);
            continue;
        }

        const wcm_counter_t epoch_before_commit = rt->world.epoch;
        const wcm_commit_code_t code = commit_intent(rt, i, &intent);
        metrics_commit(&rt->metrics, code);
        if (rt->world.epoch != epoch_before_commit ||
            atomic_load_explicit(&rt->actuation_fault_latched, memory_order_acquire) != 0u) break;
    }

    const wcm_status_t finish_rc = finish_step(rt);
    if (rc == WCM_OK && finish_rc != WCM_OK) rc = finish_rc;
    rt->in_step = 0u;
    return rc;
}

wcm_status_t wcm_runtime_world_break(wcm_runtime_t *runtime, wcm_world_break_reason_t reason) {
    wcm_runtime_impl_t *rt = impl_of(runtime);
    if (!rt || !rt->initialized || !valid_break_reason(reason)) return WCM_ERR_ARG;
    if (atomic_load_explicit(&rt->stopped, memory_order_acquire) != 0u) return WCM_ERR_STOPPED;
    if (rt->in_break) return WCM_OK;
    if (rt->in_step) return wcm_runtime_request_world_break(runtime, reason);

    wcm_time_t now;
    const wcm_status_t rc = read_clock(rt, &now);
    if (rc != WCM_OK) return rc;
    return perform_world_break(rt, reason, now);
}

wcm_status_t wcm_runtime_stop(wcm_runtime_t *runtime) {
    wcm_runtime_impl_t *rt = impl_of(runtime);
    if (!rt || !rt->initialized) return WCM_ERR_ARG;
    if (atomic_load_explicit(&rt->stopped, memory_order_acquire) != 0u) return WCM_OK;
    if (rt->in_step || rt->in_break) return WCM_ERR_BUSY;

    wcm_time_t now;
    const wcm_status_t clock_rc = read_clock(rt, &now);
    wcm_status_t break_rc = WCM_OK;
    if (clock_rc == WCM_OK) {
        break_rc = perform_world_break(rt, WCM_BREAK_APPLICATION, now);
    } else {
        /* read_clock() already performs the clock-discontinuity World Break. */
        now = rt->last_clock;
        (void)now;
    }
    atomic_store_explicit(&rt->stopped, 1u, memory_order_release);
    rt->world.state = WCM_WORLD_COLD;
    rt->metrics.stops++;
    append_event(rt, WCM_EVENT_RUNTIME_STOPPED, 0u, WCM_OK, 0u, now);
    return clock_rc == WCM_OK ? break_rc : clock_rc;
}

wcm_status_t wcm_runtime_get_metrics(const wcm_runtime_t *runtime, wcm_metrics_t *out) {
    const wcm_runtime_impl_t *rt = const_impl_of(runtime);
    if (!rt || !rt->initialized || !out) return WCM_ERR_ARG;
    *out = rt->metrics;
    return WCM_OK;
}

wcm_status_t wcm_runtime_get_last_fault(const wcm_runtime_t *runtime, wcm_fault_record_t *out) {
    const wcm_runtime_impl_t *rt = const_impl_of(runtime);
    if (!rt || !rt->initialized || !out) return WCM_ERR_ARG;
    *out = rt->last_fault;
    return WCM_OK;
}

wcm_status_t wcm_runtime_get_module_stats(
    const wcm_runtime_t *runtime,
    uint16_t module_id,
    wcm_module_stats_t *out) {
    const wcm_runtime_impl_t *rt = const_impl_of(runtime);
    if (!rt || !rt->initialized || !out) return WCM_ERR_ARG;
    const int index = find_module_index(rt, module_id);
    if (index < 0) return WCM_ERR_NOT_FOUND;
    const wcm_module_slot_t *slot = &rt->modules[index];
    out->module_id = module_id;
    out->runs = slot->runs;
    out->errors = slot->errors;
    out->next_release_us = slot->next_release_us;
    out->max_effect_latency_us = slot->max_effect_latency_us;
    out->runtime = slot->core;
    return WCM_OK;
}

wcm_status_t wcm_runtime_get_capability_state(
    const wcm_runtime_t *runtime,
    uint16_t capability_id,
    wcm_cap_state_t *out) {
    const wcm_runtime_impl_t *rt = const_impl_of(runtime);
    if (!rt || !rt->initialized || !out) return WCM_ERR_ARG;
    for (uint8_t i = 0; i < rt->capabilities.count; ++i) {
        if (rt->capabilities.descs[i].capability_id == capability_id) {
            *out = (wcm_cap_state_t)rt->capabilities.runtime[i].state;
            return WCM_OK;
        }
    }
    return WCM_ERR_NOT_FOUND;
}

wcm_world_state_t wcm_runtime_world_state(const wcm_runtime_t *runtime) {
    const wcm_runtime_impl_t *rt = const_impl_of(runtime);
    if (!rt || !rt->initialized ||
        atomic_load_explicit(&rt->actuation_fault_latched, memory_order_acquire) != 0u ||
        atomic_load_explicit(&rt->stopped, memory_order_acquire) != 0u) return WCM_WORLD_COLD;
    return rt->world.state;
}

wcm_counter_t wcm_runtime_world_epoch(const wcm_runtime_t *runtime) {
    const wcm_runtime_impl_t *rt = const_impl_of(runtime);
    return rt && rt->initialized ? rt->world.epoch : 0u;
}

bool wcm_runtime_has_capabilities(const wcm_runtime_t *runtime, wcm_capability_set_t required) {
    const wcm_runtime_impl_t *rt = const_impl_of(runtime);
    return rt && rt->initialized &&
           atomic_load_explicit(&rt->actuation_fault_latched, memory_order_acquire) == 0u &&
           atomic_load_explicit(&rt->stopped, memory_order_acquire) == 0u &&
           wcm_capability_graph_has(&rt->capabilities, required);
}


wcm_status_t wcm_runtime_get_actuator_stats(
    const wcm_runtime_t *runtime,
    uint16_t actuator_id,
    wcm_actuator_stats_t *out) {
    const wcm_runtime_impl_t *rt = const_impl_of(runtime);
    if (!rt || !rt->initialized || !out) return WCM_ERR_ARG;
    const int index = find_actuator_index(rt, actuator_id);
    if (index < 0) return WCM_ERR_NOT_FOUND;
    const wcm_actuator_desc_t *desc = &rt->config->actuators[index];
    const wcm_actuator_runtime_t *stats = &rt->actuator_runtime[index];
    out->actuator_id = actuator_id;
    out->dispatches = stats->dispatches;
    out->failures = stats->failures;
    out->overruns = stats->overruns;
    out->max_dispatch_us = stats->max_dispatch_us;
    out->configured_dispatch_wcet_us = desc->dispatch_wcet_us;
    out->configured_effect_latency_us = desc->effect_latency_us;
    return WCM_OK;
}

wcm_status_t wcm_runtime_read_event(
    const wcm_runtime_t *runtime,
    uint64_t after_sequence,
    wcm_event_record_t *out) {
    const wcm_runtime_impl_t *rt = const_impl_of(runtime);
    if (!rt || !rt->initialized || !out) return WCM_ERR_ARG;
    if (rt->event_count == 0u) return WCM_ERR_NOT_FOUND;

    const uint64_t oldest_sequence = rt->event_sequence - (uint64_t)rt->event_count + 1u;
    uint64_t target = after_sequence == UINT64_MAX ? UINT64_MAX : after_sequence + 1u;
    wcm_status_t result = WCM_OK;
    if (target < oldest_sequence) {
        target = oldest_sequence;
        result = WCM_ERR_GAP;
    }
    if (target > rt->event_sequence) return WCM_ERR_NOT_FOUND;

    const uint16_t oldest_index = (uint16_t)((rt->event_head + WCM_MAX_EVENTS - rt->event_count) % WCM_MAX_EVENTS);
    const uint64_t offset = target - oldest_sequence;
    if (offset >= rt->event_count) return WCM_ERR_NOT_FOUND;
    const uint16_t index = (uint16_t)((oldest_index + (uint16_t)offset) % WCM_MAX_EVENTS);
    *out = rt->events[index];
    return result;
}

wcm_status_t wcm_runtime_get_health(const wcm_runtime_t *runtime, wcm_health_snapshot_t *out) {
    const wcm_runtime_impl_t *rt = const_impl_of(runtime);
    if (!rt || !rt->initialized || !out) return WCM_ERR_ARG;
    memset(out, 0, sizeof(*out));
    out->deployment_id = rt->config->deployment_id;
    out->config_fingerprint = rt->config_fingerprint;
    out->step_sequence = rt->step_sequence;
    out->config_revision = rt->config->config_revision;
    out->pending_ingress = pending_ingress(rt);
    out->world_epoch = rt->world.epoch;
    out->bound_capabilities = rt->capabilities.bound_mask;
    out->epoch_started_at = rt->world.started_at;
    out->last_step_started_us = rt->last_step_started_us;
    out->last_step_completed_us = rt->last_step_completed_us;
    out->last_step_elapsed_us = rt->last_step_elapsed_us;
    out->max_step_elapsed_us = rt->max_step_elapsed_us;
    out->last_step_timing_valid = rt->last_step_timing_valid != 0u;
    out->world_state = rt->world.state;
    out->last_fault = rt->last_fault;
    out->metrics = rt->metrics;

    if (atomic_load_explicit(&rt->actuation_fault_latched, memory_order_acquire) != 0u) {
        out->state = WCM_HEALTH_FAIL_STOP;
    } else if (atomic_load_explicit(&rt->stopped, memory_order_acquire) != 0u) {
        out->state = WCM_HEALTH_STOPPED;
    } else if (rt->world.state == WCM_WORLD_BOUND) {
        out->state = WCM_HEALTH_NOMINAL;
    } else if (rt->world.state == WCM_WORLD_DEGRADED) {
        out->state = WCM_HEALTH_DEGRADED;
    } else if (rt->world.state == WCM_WORLD_REBINDING) {
        out->state = WCM_HEALTH_REBINDING;
    } else {
        out->state = WCM_HEALTH_STARTING;
    }
    return WCM_OK;
}


wcm_status_t wcm_runtime_check_configuration(const wcm_runtime_t *runtime) {
    const wcm_runtime_impl_t *rt = const_impl_of(runtime);
    if (!rt || !rt->initialized) return WCM_ERR_ARG;
    return wcm_runtime_config_fingerprint(rt->config) == rt->config_fingerprint ? WCM_OK : WCM_ERR_STATE;
}

#include <stdio.h>
#include <string.h>
#include "wcm/abi.h"
#include "wcm/wcm.h"
#include "wcm_fake_clock.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

static const wcm_witness_source_desc_t k_source = {
    .id = 0,
    .max_live_us = 5000,
    .max_future_skew_us = 0,
    .min_quality = 100,
    .timestamp_policy = WCM_TS_STRICT_INCREASING,
    .allowed_flags = WCM_WITNESS_ESTIMATED | WCM_WITNESS_DEGRADED,
};

static wcm_observation_t observation_at(wcm_time_t t, float value) {
    wcm_observation_t obs;
    memset(&obs, 0, sizeof(obs));
    obs.witness_id = 0;
    obs.observed_at = t;
    obs.requested_livebound = t + 5000u;
    obs.quality = 200;
    obs.value.f32[0] = value;
    return obs;
}


static int test_witness_timestamp_policy(void) {
    wcm_world_t world;
    wcm_world_init(&world, 0u);
    wcm_witness_store_t store;
    wcm_witness_store_init(&store);
    wcm_value_t value = {0};

    CHECK(wcm_witness_publish(&store, &world, 0, &value, 1000, 1000, 2000,
                              &k_source, 200, 0) == WCM_OK);
    CHECK(wcm_witness_publish(&store, &world, 0, &value, 1100, 900, 2000,
                              &k_source, 200, 0) == WCM_ERR_TIMESTAMP);
    CHECK(wcm_witness_publish(&store, &world, 0, &value, 1100, 1200, 2000,
                              &k_source, 200, 0) == WCM_ERR_TIMESTAMP);
    CHECK(wcm_witness_publish(&store, &world, 0, &value, 1100, 1100, 2000,
                              &k_source, 99, 0) == WCM_ERR_QUALITY);
    CHECK(wcm_witness_publish(&store, &world, 0, &value, 1100, 1100, 2000,
                              &k_source, 200, WCM_WITNESS_PREDICTED) == WCM_ERR_POLICY);

    const wcm_witness_t *w = wcm_witness_get(&store, 0);
    CHECK(w != NULL);
    CHECK(w->observed_at == 1000u);
    CHECK(wcm_witness_is_live(w, &world, 1000u));
    CHECK(!wcm_witness_is_live(w, &world, 999u));
    return 0;
}


static int test_strict_timestamp_zero_is_not_a_sentinel(void) {
    wcm_world_t world;
    wcm_world_init(&world, 0u);
    wcm_witness_store_t store;
    wcm_witness_store_init(&store);
    wcm_value_t value = {0};
    CHECK(wcm_witness_publish(&store, &world, 0, &value, 0, 0, 100,
                              &k_source, 200, 0) == WCM_OK);
    CHECK(wcm_witness_publish(&store, &world, 0, &value, 0, 0, 100,
                              &k_source, 200, 0) == WCM_ERR_TIMESTAMP);
    return 0;
}

static int test_impossible_commit_horizon_expires(void) {
    wcm_world_t world;
    wcm_world_init(&world, 0u);
    wcm_intent_t intent;
    memset(&intent, 0, sizeof(intent));
    intent.stamp.world_epoch = world.epoch;
    intent.stamp.dependency_clock = 1u;
    intent.stamp.livebound = 50u;
    const wcm_commit_context_t ctx = {
        .effect_latency_us = 100u,
        .capability_ok = true,
    };
    CHECK(wcm_concordance_commit(&world, 1u, 0u, &ctx, &intent) == WCM_VOID_EXPIRED);
    return 0;
}

static int test_witness_counter_no_wrap(void) {
    wcm_world_t world;
    wcm_world_init(&world, 0u);
    wcm_witness_store_t store;
    wcm_witness_store_init(&store);
    wcm_value_t value = {0};

    CHECK(wcm_witness_publish(&store, &world, 0, &value, 1, 1, 100,
                              &k_source, 200, 0) == WCM_OK);
    store.slots[0].generation = UINT64_MAX;
    CHECK(wcm_witness_publish(&store, &world, 0, &value, 2, 2, 100,
                              &k_source, 200, 0) == WCM_ERR_COUNTER);
    return 0;
}

static int test_world_counter_no_wrap(void) {
    wcm_world_t world;
    wcm_world_init(&world, 0u);
    world.epoch = UINT64_MAX;
    CHECK(wcm_world_break(&world, WCM_BREAK_APPLICATION, 0u) == WCM_ERR_COUNTER);
    CHECK(world.state == WCM_WORLD_COLD);
    return 0;
}

static int test_dependency_counter_no_wrap(void) {
    wcm_dependency_desc_t dep = {.witness_id = 0, .lens = WCM_LENS_SAMPLE, .quality_floor = 1};
    wcm_module_runtime_t module;
    memset(&module, 0, sizeof(module));
    module.dependency_clock = UINT64_MAX;
    wcm_witness_t before;
    wcm_witness_t after;
    memset(&before, 0, sizeof(before));
    memset(&after, 0, sizeof(after));
    before.flags = WCM_WITNESS_VALID;
    after.flags = WCM_WITNESS_VALID;
    before.generation = 1;
    after.generation = 2;
    CHECK(wcm_dependency_apply_transition(&module, &dep, &before, &after) == WCM_ERR_COUNTER);
    return 0;
}

static int test_snapshot_is_immutable_copy(void) {
    wcm_world_t world;
    wcm_world_init(&world, 0u);
    wcm_witness_store_t store;
    wcm_witness_store_init(&store);
    wcm_value_t value = {0};
    value.f32[0] = 1.0f;
    CHECK(wcm_witness_publish(&store, &world, 0, &value, 100, 100, 1000,
                              &k_source, 200, 0) == WCM_OK);

    wcm_snapshot_t snapshot;
    CHECK(wcm_snapshot_begin(&snapshot, &world, 7, 10) == WCM_OK);
    CHECK(wcm_snapshot_observe(&snapshot, 0, wcm_witness_get(&store, 0)) == WCM_OK);
    CHECK(wcm_snapshot_seal(&snapshot) == WCM_OK);

    value.f32[0] = 2.0f;
    CHECK(wcm_witness_publish(&store, &world, 0, &value, 200, 200, 1000,
                              &k_source, 200, 0) == WCM_OK);
    wcm_value_t readback = {0};
    CHECK(wcm_snapshot_read_value(&snapshot, 0, &readback) == WCM_OK);
    CHECK(readback.f32[0] == 1.0f);
    return 0;
}

typedef struct fixture fixture_t;

struct fixture {
    wcm_runtime_t runtime;
    WCM_RUNTIME_STORAGE(storage);
    wcm_fake_clock_t clock;
    float command;
    unsigned applies;
    unsigned safe_calls;
    unsigned guard_enter;
    unsigned guard_exit;
    unsigned controller_calls;
    uint32_t controller_advance_us;
    uint32_t transform_advance_us;
    uint32_t apply_advance_us;
    uint32_t validator_advance_us;
    bool forge_source;
    bool apply_fail;
    bool safe_fail;
    bool safety_set_two;
    bool post_during_controller;
    bool post_during_safety;
    bool break_during_controller;
    bool break_during_validator;
    bool rewind_during_validator;
    bool break_during_apply;
    bool post_once;
    bool resource_limit_one;
    bool try_reentrant;
    wcm_status_t reentrant_rc;
    uint16_t intent_actuator;
    wcm_observation_t injected;
    const wcm_witness_source_desc_t *sources;
    const wcm_dependency_desc_t *deps;
    const wcm_rebind_requirement_t *reqs;
    const wcm_capability_desc_t *caps;
    const wcm_module_desc_t *modules;
    const wcm_actuator_desc_t *actuators;
    wcm_runtime_config_t config;
};

static wcm_status_t controller_step(const wcm_snapshot_t *snapshot, void *user, wcm_intent_t *out) {
    fixture_t *f = (fixture_t *)user;
    f->controller_calls++;
    if (f->try_reentrant) f->reentrant_rc = wcm_runtime_step(&f->runtime);
    if (f->break_during_controller) {
        if (wcm_runtime_world_break(&f->runtime, WCM_BREAK_APPLICATION) != WCM_OK) {
            return WCM_ERR_STATE;
        }
    }
    wcm_value_t x = {0};
    if (wcm_snapshot_read_value(snapshot, 0, &x) != WCM_OK) return WCM_ERR_STATE;
    if (f->controller_advance_us) wcm_fake_clock_advance(&f->clock, f->controller_advance_us);
    if (f->post_during_controller && !f->post_once) {
        f->post_once = true;
        f->injected = observation_at(wcm_fake_clock_now(&f->clock), x.f32[0] + 1.0f);
        if (wcm_runtime_post_observation(&f->runtime, 0, &f->injected) != WCM_OK) return WCM_ERR_IO;
    }
    wcm_value_t u = {0};
    u.f32[0] = 0.5f;
    const wcm_status_t rc = wcm_intent_from_snapshot(out, snapshot, f->intent_actuator, &u);
    if (rc == WCM_OK && f->forge_source) out->source_module = 999u;
    return rc;
}

static wcm_filter_result_t constraint_clamp(void *user, wcm_value_t *value) {
    fixture_t *f = (fixture_t *)user;
    if (f->transform_advance_us) wcm_fake_clock_advance(&f->clock, f->transform_advance_us);
    if (value->f32[0] > 1.0f) {
        value->f32[0] = 1.0f;
        return WCM_FILTER_MODIFY;
    }
    return WCM_FILTER_ACCEPT;
}

static wcm_filter_result_t safety_transform(void *user, wcm_value_t *value) {
    fixture_t *f = (fixture_t *)user;
    if (f->transform_advance_us) wcm_fake_clock_advance(&f->clock, f->transform_advance_us);
    if (f->post_during_safety && !f->post_once) {
        f->post_once = true;
        f->injected = observation_at(wcm_fake_clock_now(&f->clock), 3.0f);
        if (wcm_runtime_post_observation(&f->runtime, 0, &f->injected) != WCM_OK) {
            return WCM_FILTER_REJECT;
        }
    }
    if (f->safety_set_two) {
        value->f32[0] = 2.0f;
        return WCM_FILTER_MODIFY;
    }
    return WCM_FILTER_ACCEPT;
}

static bool range_one(void *user, const wcm_value_t *value) {
    (void)user;
    return value->f32[0] >= -1.0f && value->f32[0] <= 1.0f;
}

static bool safety_any(void *user, const wcm_value_t *value) {
    fixture_t *f = (fixture_t *)user;
    (void)value;
    if (f->validator_advance_us) wcm_fake_clock_advance(&f->clock, f->validator_advance_us);
    if (f->rewind_during_validator) {
        f->rewind_during_validator = false;
        wcm_fake_clock_set(&f->clock, 10u);
    }
    if (f->break_during_validator) {
        f->break_during_validator = false;
        (void)wcm_runtime_world_break(&f->runtime, WCM_BREAK_APPLICATION);
    }
    return true;
}

static bool resource_admit(void *user, uint16_t source_module, uint16_t actuator, const wcm_value_t *value) {
    fixture_t *f = (fixture_t *)user;
    (void)source_module;
    (void)actuator;
    return !f->resource_limit_one || (value->f32[0] >= -1.0f && value->f32[0] <= 1.0f);
}

static wcm_status_t actuator_apply(void *user, const wcm_value_t *value) {
    fixture_t *f = (fixture_t *)user;
    f->applies++;
    f->command = value->f32[0];
    if (f->apply_advance_us) wcm_fake_clock_advance(&f->clock, f->apply_advance_us);
    if (f->break_during_apply) {
        f->break_during_apply = false;
        (void)wcm_runtime_world_break(&f->runtime, WCM_BREAK_APPLICATION);
    }
    return f->apply_fail ? WCM_ERR_IO : WCM_OK;
}

static wcm_status_t actuator_safe(void *user) {
    fixture_t *f = (fixture_t *)user;
    f->safe_calls++;
    f->command = 0.0f;
    return f->safe_fail ? WCM_ERR_IO : WCM_OK;
}

static void guard_enter(void *user) {
    fixture_t *f = (fixture_t *)user;
    f->guard_enter++;
}

static void guard_exit(void *user) {
    fixture_t *f = (fixture_t *)user;
    f->guard_exit++;
}

static int fixture_init_options(fixture_t *f, bool use_constraint, bool use_safety, uint32_t step_budget_us, uint32_t config_check_period_steps) {
    memset(f, 0, sizeof(*f));
    wcm_fake_clock_init(&f->clock, 1000);

    static wcm_witness_source_desc_t sources[1];
    static wcm_dependency_desc_t deps[1];
    static wcm_rebind_requirement_t reqs[1];
    static wcm_capability_desc_t caps[1];
    static wcm_module_desc_t modules[1];
    static wcm_actuator_desc_t actuators[2];

    sources[0] = k_source;
    deps[0] = (wcm_dependency_desc_t){.witness_id = 0, .lens = WCM_LENS_SAMPLE, .quality_floor = 100};
    reqs[0] = (wcm_rebind_requirement_t){.witness_id = 0, .min_quality = 100, .consecutive_samples = 1, .max_gap_us = 0};
    caps[0] = (wcm_capability_desc_t){.capability_id = 0, .requirements = reqs, .requirement_count = 1};
    modules[0] = (wcm_module_desc_t){
        .id = 7,
        .period_us = 1000,
        .deadline_us = 1000,
        .wcet_us = 500,
        .required_capabilities = UINT64_C(1),
        .allowed_actuators = UINT64_C(1),
        .dependencies = deps,
        .dependency_count = 1,
        .step = controller_step,
        .user = f,
    };
    actuators[0] = (wcm_actuator_desc_t){
        .id = 0,
        .dispatch_wcet_us = 100,
        .effect_latency_us = 100,
        .constraint_transform = use_constraint ? constraint_clamp : NULL,
        .safety_transform = use_safety ? safety_transform : NULL,
        .constraint_validate = use_constraint ? range_one : NULL,
        .safety_validate = use_safety ? safety_any : NULL,
        .apply = actuator_apply,
        .safe = actuator_safe,
        .user = f,
    };
    actuators[1] = actuators[0];
    actuators[1].id = 1;

    f->sources = sources;
    f->deps = deps;
    f->reqs = reqs;
    f->caps = caps;
    f->modules = modules;
    f->actuators = actuators;
    wcm_runtime_config_init(&f->config);
    f->config.sources = sources;
    f->config.source_count = 1;
    f->config.modules = modules;
    f->config.module_count = 1;
    f->config.capabilities = caps;
    f->config.capability_count = 1;
    f->config.actuators = actuators;
    f->config.actuator_count = 2;
    f->config.ingress_producer_count = 2;
    f->config.gate_wcet_us = 100;
    f->config.step_budget_us = step_budget_us;
    f->config.config_check_period_steps = config_check_period_steps;
    f->config.deployment_id = UINT64_C(0x57434D110001);
    f->config.config_revision = 11u;
    f->config.clock_read = wcm_fake_clock_read;
    f->config.clock_user = &f->clock;
    f->config.resource_admit = resource_admit;
    f->config.resource_user = f;
    f->config.commit_guard_enter = guard_enter;
    f->config.commit_guard_exit = guard_exit;
    f->config.commit_guard_user = f;

    CHECK(wcm_runtime_init(&f->runtime, f->storage, sizeof(f->storage), &f->config) == WCM_OK);
    return 0;
}

static int fixture_init_ex(fixture_t *f, bool use_constraint, bool use_safety, uint32_t step_budget_us) {
    return fixture_init_options(f, use_constraint, use_safety, step_budget_us, 0u);
}

static int fixture_init(fixture_t *f, bool use_constraint, bool use_safety) {
    return fixture_init_options(f, use_constraint, use_safety, 0u, 0u);
}

static int publish_and_step(fixture_t *f, uint8_t producer, wcm_time_t t, float value) {
    wcm_fake_clock_set(&f->clock, t);
    wcm_observation_t obs = observation_at(t, value);
    CHECK(wcm_runtime_post_observation(&f->runtime, producer, &obs) == WCM_OK);
    CHECK(wcm_runtime_step(&f->runtime) == WCM_OK);
    return 0;
}

static wcm_metrics_t metrics_of(const fixture_t *f) {
    wcm_metrics_t m;
    memset(&m, 0, sizeof(m));
    (void)wcm_runtime_get_metrics(&f->runtime, &m);
    return m;
}

static int test_runtime_end_to_end(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    CHECK(publish_and_step(&f, 0, 1000, 1.0f) == 0);
    CHECK(f.applies == 1u);
    CHECK(f.command == 0.5f);
    CHECK(f.guard_enter == 1u && f.guard_exit == 1u);
    const wcm_metrics_t m = metrics_of(&f);
    CHECK(m.commits_ok == 1u);
    return 0;
}

static int test_controller_wcet_overrun_fails_closed(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    f.controller_advance_us = 501;
    CHECK(publish_and_step(&f, 0, 1000, 1.0f) == 0);
    CHECK(f.applies == 0u);
    const wcm_metrics_t m = metrics_of(&f);
    CHECK(m.wcet_overruns == 1u);
    return 0;
}

static int test_commit_rechecks_actual_clock(void) {
    fixture_t f;
    CHECK(fixture_init(&f, true, false) == 0);
    f.transform_advance_us = 5000;
    CHECK(publish_and_step(&f, 0, 1000, 1.0f) == 0);
    CHECK(f.applies == 0u);
    const wcm_metrics_t m = metrics_of(&f);
    CHECK(m.void_expired == 1u);
    return 0;
}


static int test_commit_rechecks_after_final_validation(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, true) == 0);
    f.validator_advance_us = 5000;
    CHECK(publish_and_step(&f, 0, 1000, 1.0f) == 0);
    CHECK(f.applies == 0u);
    CHECK(metrics_of(&f).void_expired == 1u);
    return 0;
}

static int test_forged_source_rejected(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    f.forge_source = true;
    CHECK(publish_and_step(&f, 0, 1000, 1.0f) == 0);
    CHECK(f.applies == 0u);
    CHECK(metrics_of(&f).module_errors == 1u);
    return 0;
}

static int test_filter_composition_final_validation(void) {
    fixture_t f;
    CHECK(fixture_init(&f, true, true) == 0);
    f.safety_set_two = true;
    CHECK(publish_and_step(&f, 0, 1000, 1.0f) == 0);
    CHECK(f.applies == 0u);
    CHECK(metrics_of(&f).reject_constraint == 1u);
    return 0;
}

static int test_resource_checks_final_value(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, true) == 0);
    f.safety_set_two = true;
    f.resource_limit_one = true;
    CHECK(publish_and_step(&f, 0, 1000, 1.0f) == 0);
    CHECK(f.applies == 0u);
    CHECK(metrics_of(&f).deny_resource == 1u);
    return 0;
}

static int test_ingress_rejection_is_local(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    wcm_fake_clock_set(&f.clock, 1000);
    wcm_observation_t bad = observation_at(1000, 9.0f);
    bad.quality = 10;
    wcm_observation_t good = observation_at(1001, 1.0f);
    wcm_fake_clock_set(&f.clock, 1001);
    CHECK(wcm_runtime_post_observation(&f.runtime, 0, &bad) == WCM_OK);
    CHECK(wcm_runtime_post_observation(&f.runtime, 1, &good) == WCM_OK);
    CHECK(wcm_runtime_step(&f.runtime) == WCM_OK);
    CHECK(f.applies == 1u);
    CHECK(metrics_of(&f).ingress_rejected_quality == 1u);
    return 0;
}

static int test_unknown_source_is_local(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    wcm_fake_clock_set(&f.clock, 1001);
    wcm_observation_t unknown = observation_at(1000, 9.0f);
    unknown.witness_id = 9;
    wcm_observation_t good = observation_at(1001, 1.0f);
    CHECK(wcm_runtime_post_observation(&f.runtime, 0, &unknown) == WCM_OK);
    CHECK(wcm_runtime_post_observation(&f.runtime, 1, &good) == WCM_OK);
    CHECK(wcm_runtime_step(&f.runtime) == WCM_OK);
    CHECK(f.applies == 1u);
    CHECK(metrics_of(&f).ingress_unknown_source == 1u);
    return 0;
}

static int test_multi_producer_timestamp_merge(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    wcm_fake_clock_set(&f.clock, 1200);
    wcm_observation_t later = observation_at(1200, 2.0f);
    wcm_observation_t earlier = observation_at(1100, 1.0f);
    CHECK(wcm_runtime_post_observation(&f.runtime, 0, &later) == WCM_OK);
    CHECK(wcm_runtime_post_observation(&f.runtime, 1, &earlier) == WCM_OK);
    CHECK(wcm_runtime_step(&f.runtime) == WCM_OK);
    CHECK(metrics_of(&f).observations_published == 2u);
    CHECK(metrics_of(&f).ingress_rejected_timestamp == 0u);
    return 0;
}

static int test_out_of_order_observation_dropped(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    CHECK(publish_and_step(&f, 0, 1000, 1.0f) == 0);
    wcm_fake_clock_set(&f.clock, 2000);
    wcm_observation_t old = observation_at(900, 2.0f);
    CHECK(wcm_runtime_post_observation(&f.runtime, 0, &old) == WCM_OK);
    CHECK(wcm_runtime_step(&f.runtime) == WCM_OK);
    CHECK(metrics_of(&f).ingress_rejected_timestamp == 1u);
    return 0;
}

static int test_future_observation_dropped(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    wcm_fake_clock_set(&f.clock, 1000);
    wcm_observation_t future = observation_at(1100, 1.0f);
    CHECK(wcm_runtime_post_observation(&f.runtime, 0, &future) == WCM_OK);
    CHECK(wcm_runtime_step(&f.runtime) == WCM_OK);
    CHECK(metrics_of(&f).ingress_rejected_timestamp == 1u);
    CHECK(f.applies == 0u);
    return 0;
}

static int test_clock_discontinuity_breaks_world(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    CHECK(publish_and_step(&f, 0, 1000, 1.0f) == 0);
    const wcm_counter_t before = wcm_runtime_world_epoch(&f.runtime);
    wcm_fake_clock_set(&f.clock, 10);
    CHECK(wcm_runtime_step(&f.runtime) == WCM_ERR_CLOCK);
    CHECK(wcm_runtime_world_epoch(&f.runtime) == before + 1u);
    CHECK(f.safe_calls >= 1u);
    CHECK(metrics_of(&f).clock_discontinuities == 1u);
    return 0;
}

static int test_world_break_requires_fresh_evidence(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    CHECK(publish_and_step(&f, 0, 1000, 1.0f) == 0);
    const unsigned applies = f.applies;
    wcm_fake_clock_set(&f.clock, 1500);
    CHECK(wcm_runtime_world_break(&f.runtime, WCM_BREAK_APPLICATION) == WCM_OK);
    CHECK(f.safe_calls >= 1u);
    wcm_fake_clock_set(&f.clock, 2000);
    CHECK(wcm_runtime_step(&f.runtime) == WCM_OK);
    CHECK(f.applies == applies);
    CHECK(publish_and_step(&f, 0, 2100, 1.0f) == 0);
    CHECK(f.applies == applies);
    wcm_fake_clock_set(&f.clock, 2500u);
    CHECK(wcm_runtime_step(&f.runtime) == WCM_OK);
    CHECK(f.applies == applies + 1u);
    return 0;
}

static int test_arrival_during_controller_voids_intent(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    f.post_during_controller = true;
    f.controller_advance_us = 10;
    CHECK(publish_and_step(&f, 0, 1000, 1.0f) == 0);
    CHECK(f.applies == 0u);
    CHECK(metrics_of(&f).void_stale == 1u);
    return 0;
}

static int test_arrival_during_filter_voids_intent(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, true) == 0);
    f.post_during_safety = true;
    f.transform_advance_us = 10;
    CHECK(publish_and_step(&f, 0, 1000, 1.0f) == 0);
    CHECK(f.applies == 0u);
    CHECK(metrics_of(&f).void_stale == 1u);
    return 0;
}

static int test_actuator_failure_breaks_world(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    f.apply_fail = true;
    const wcm_counter_t before = wcm_runtime_world_epoch(&f.runtime);
    CHECK(publish_and_step(&f, 0, 1000, 1.0f) == 0);
    CHECK(wcm_runtime_world_epoch(&f.runtime) == before + 1u);
    CHECK(metrics_of(&f).dispatch_failures == 1u);
    CHECK(f.safe_calls >= 1u);
    return 0;
}

static int test_actuator_overrun_breaks_world(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    f.apply_advance_us = 101;
    const wcm_counter_t before = wcm_runtime_world_epoch(&f.runtime);
    CHECK(publish_and_step(&f, 0, 1000, 1.0f) == 0);
    CHECK(wcm_runtime_world_epoch(&f.runtime) == before + 1u);
    CHECK(metrics_of(&f).dispatch_overruns == 1u);
    CHECK(f.safe_calls >= 1u);
    return 0;
}


static int test_world_break_from_controller_is_deferred(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    f.break_during_controller = true;
    const wcm_counter_t before = wcm_runtime_world_epoch(&f.runtime);
    CHECK(publish_and_step(&f, 0, 1000, 1.0f) == 0);
    CHECK(f.applies == 0u);
    CHECK(wcm_runtime_world_epoch(&f.runtime) == before + 1u);
    CHECK(f.safe_calls >= 1u);
    return 0;
}


static int test_unauthorized_actuator_is_rejected(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    f.intent_actuator = 1u;
    CHECK(publish_and_step(&f, 0, 1000, 1.0f) == 0);
    CHECK(f.applies == 0u);
    CHECK(metrics_of(&f).deny_authority == 1u);
    return 0;
}

static int test_safe_output_failure_latches_runtime(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    f.safe_fail = true;
    wcm_fake_clock_set(&f.clock, 1100u);
    CHECK(wcm_runtime_world_break(&f.runtime, WCM_BREAK_APPLICATION) == WCM_ERR_IO);
    CHECK(metrics_of(&f).safe_apply_failures >= 1u);
    CHECK(wcm_runtime_world_state(&f.runtime) == WCM_WORLD_COLD);
    CHECK(!wcm_runtime_has_capabilities(&f.runtime, UINT64_C(1)));
    CHECK(wcm_runtime_step(&f.runtime) == WCM_ERR_STOPPED);
    return 0;
}

static int test_world_break_from_final_validator_voids_intent(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, true) == 0);
    f.break_during_validator = true;
    const wcm_counter_t before = wcm_runtime_world_epoch(&f.runtime);
    CHECK(publish_and_step(&f, 0, 1000, 1.0f) == 0);
    CHECK(f.applies == 0u);
    CHECK(wcm_runtime_world_epoch(&f.runtime) == before + 1u);
    CHECK(metrics_of(&f).void_world == 1u);
    return 0;
}

static int test_world_break_from_apply_occurs_after_commit(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    f.break_during_apply = true;
    const wcm_counter_t before = wcm_runtime_world_epoch(&f.runtime);
    CHECK(publish_and_step(&f, 0, 1000, 1.0f) == 0);
    CHECK(f.applies == 1u);
    CHECK(wcm_runtime_world_epoch(&f.runtime) == before + 1u);
    CHECK(metrics_of(&f).commits_ok == 1u);
    CHECK(f.safe_calls >= 1u);
    return 0;
}

static int test_anchor_slots(void) {
    wcm_anchor_slot_t a;
    wcm_anchor_slot_t b;
    const char first[] = "alpha";
    const char second[] = "beta";
    CHECK(wcm_anchor_write(&a, first, (uint16_t)sizeof(first), 10u) == WCM_OK);
    CHECK(wcm_anchor_write(&b, second, (uint16_t)sizeof(second), 11u) == WCM_OK);
    CHECK(wcm_anchor_validate(&a));
    CHECK(wcm_anchor_validate(&b));
    CHECK(wcm_anchor_choose_latest(&a, &b) == &b);
    b.payload[0] ^= 1u;
    CHECK(!wcm_anchor_validate(&b));
    CHECK(wcm_anchor_choose_latest(&a, &b) == &a);
    return 0;
}

static int test_capability_cycle_rejected(void) {
    const wcm_capability_desc_t caps[] = {
        {.capability_id = 0, .requires_mask = UINT64_C(2)},
        {.capability_id = 1, .requires_mask = UINT64_C(1)},
    };
    wcm_capability_graph_t graph;
    CHECK(wcm_capability_graph_init(&graph, caps, 2, 0) == WCM_ERR_GRAPH);
    return 0;
}

static int test_queue_overflow_is_counted(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    wcm_fake_clock_set(&f.clock, 2000);
    unsigned full = 0u;
    for (unsigned i = 0; i < WCM_MAX_INGRESS + 4u; ++i) {
        wcm_observation_t obs = observation_at(1000u + i, (float)i);
        const wcm_status_t rc = wcm_runtime_post_observation(&f.runtime, 0, &obs);
        if (rc == WCM_ERR_FULL) full++;
    }
    CHECK(full > 0u);
    CHECK(wcm_runtime_step(&f.runtime) == WCM_OK);
    CHECK(metrics_of(&f).ingress_dropped == full);
    return 0;
}


static int test_config_rejects_unbound_references(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);

    wcm_runtime_t runtime = {0};
    WCM_RUNTIME_STORAGE(storage);

    wcm_runtime_config_t config = f.config;
    wcm_module_desc_t module = f.modules[0];
    wcm_dependency_desc_t dependency = f.deps[0];
    dependency.witness_id = 3u;
    module.dependencies = &dependency;
    config.modules = &module;
    CHECK(wcm_runtime_init(&runtime, storage, sizeof(storage), &config) == WCM_ERR_NOT_FOUND);

    module = f.modules[0];
    module.allowed_actuators = wcm_actuator_bit(9u);
    config = f.config;
    config.modules = &module;
    CHECK(wcm_runtime_init(&runtime, storage, sizeof(storage), &config) == WCM_ERR_POLICY);

    wcm_capability_desc_t capability = f.caps[0];
    wcm_rebind_requirement_t requirement = f.reqs[0];
    requirement.witness_id = 3u;
    capability.requirements = &requirement;
    config = f.config;
    config.capabilities = &capability;
    CHECK(wcm_runtime_init(&runtime, storage, sizeof(storage), &config) == WCM_ERR_NOT_FOUND);
    return 0;
}

static int test_runtime_reentrancy_guard(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    f.try_reentrant = true;
    CHECK(publish_and_step(&f, 0, 1000, 1.0f) == 0);
    CHECK(f.reentrant_rc == WCM_ERR_BUSY);
    CHECK(f.applies == 1u);
    return 0;
}

static int test_init_forces_safe_output(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    CHECK(f.safe_calls == 2u);
    CHECK(f.command == 0.0f);
    return 0;
}

static int test_init_fails_if_safe_output_fails(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    f.safe_fail = true;
    wcm_runtime_t runtime = {0};
    WCM_RUNTIME_STORAGE(storage);
    CHECK(wcm_runtime_init(&runtime, storage, sizeof(storage), &f.config) == WCM_ERR_IO);
    CHECK(runtime.impl == NULL);
    return 0;
}

static int test_gate_wcet_overrun_fails_closed(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, true) == 0);
    f.validator_advance_us = 101u;
    CHECK(publish_and_step(&f, 0, 1000u, 1.0f) == 0);
    CHECK(f.applies == 0u);
    CHECK(metrics_of(&f).gate_wcet_overruns == 1u);
    wcm_fault_record_t fault;
    CHECK(wcm_runtime_get_last_fault(&f.runtime, &fault) == WCM_OK);
    CHECK(fault.code == WCM_FAULT_GATE_WCET_OVERRUN);
    return 0;
}

static int test_scheduler_does_not_hot_loop_when_unbound(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    for (unsigned i = 0u; i < 10u; ++i) {
        wcm_fake_clock_set(&f.clock, 1000u);
        CHECK(wcm_runtime_step(&f.runtime) == WCM_OK);
    }
    CHECK(metrics_of(&f).deny_capability == 1u);
    CHECK(f.controller_calls == 0u);
    return 0;
}

static int test_ingress_exact_capacity(void) {
    wcm_ingress_t q;
    wcm_ingress_init(&q);
    for (unsigned i = 0u; i < WCM_MAX_INGRESS; ++i) {
        wcm_observation_t obs = observation_at((wcm_time_t)i, (float)i);
        CHECK(wcm_ingress_push(&q, &obs) == WCM_OK);
    }
    wcm_observation_t extra = observation_at(9999u, 1.0f);
    CHECK(wcm_ingress_push(&q, &extra) == WCM_ERR_FULL);
    CHECK(wcm_ingress_count(&q) == WCM_MAX_INGRESS);
    return 0;
}

static int test_ingress_fold_budget_defers_control(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    wcm_runtime_config_t config = f.config;
    config.ingress_fold_limit = 1u;
    wcm_runtime_t runtime = {0};
    WCM_RUNTIME_STORAGE(storage);
    CHECK(wcm_runtime_init(&runtime, storage, sizeof(storage), &config) == WCM_OK);

    wcm_fake_clock_set(&f.clock, 2000u);
    for (unsigned i = 0u; i < 3u; ++i) {
        wcm_observation_t obs = observation_at(1000u + i, (float)i);
        CHECK(wcm_runtime_post_observation(&runtime, 0u, &obs) == WCM_OK);
    }
    CHECK(wcm_runtime_step(&runtime) == WCM_OK);
    wcm_metrics_t m;
    CHECK(wcm_runtime_get_metrics(&runtime, &m) == WCM_OK);
    CHECK(m.ingress_backlog_deferrals == 1u);
    CHECK(f.controller_calls == 0u);
    return 0;
}

static int test_async_world_break_request(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    const wcm_counter_t before = wcm_runtime_world_epoch(&f.runtime);
    CHECK(wcm_runtime_request_world_break(&f.runtime, WCM_BREAK_APPLICATION) == WCM_OK);
    CHECK(wcm_runtime_step(&f.runtime) == WCM_OK);
    CHECK(wcm_runtime_world_epoch(&f.runtime) == before + 1u);
    CHECK(f.safe_calls >= 2u);
    return 0;
}

static int test_runtime_stop_is_fail_closed(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    const unsigned safe_before = f.safe_calls;
    CHECK(wcm_runtime_stop(&f.runtime) == WCM_OK);
    CHECK(f.safe_calls > safe_before);
    CHECK(wcm_runtime_world_state(&f.runtime) == WCM_WORLD_COLD);
    CHECK(wcm_runtime_step(&f.runtime) == WCM_ERR_STOPPED);
    wcm_observation_t obs = observation_at(2000u, 1.0f);
    CHECK(wcm_runtime_post_observation(&f.runtime, 0u, &obs) == WCM_ERR_STOPPED);
    CHECK(wcm_runtime_request_world_break(&f.runtime, WCM_BREAK_APPLICATION) == WCM_ERR_STOPPED);
    return 0;
}

static int test_config_abi_mismatch(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    wcm_runtime_config_t config = f.config;
    config.abi_version ^= 1u;
    wcm_runtime_t runtime = {0};
    WCM_RUNTIME_STORAGE(storage);
    CHECK(wcm_runtime_init(&runtime, storage, sizeof(storage), &config) == WCM_ERR_ABI);
    return 0;
}

static int test_module_stats_and_fault_record(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    f.controller_advance_us = 501u;
    CHECK(publish_and_step(&f, 0, 1000u, 1.0f) == 0);
    wcm_module_stats_t stats;
    CHECK(wcm_runtime_get_module_stats(&f.runtime, 7u, &stats) == WCM_OK);
    CHECK(stats.runs == 1u);
    CHECK(stats.runtime.wcet_overruns == 1u);
    wcm_fault_record_t fault;
    CHECK(wcm_runtime_get_last_fault(&f.runtime, &fault) == WCM_OK);
    CHECK(fault.code == WCM_FAULT_MODULE_WCET_OVERRUN);
    CHECK(fault.subject_id == 7u);
    return 0;
}

static int test_clock_discontinuity_inside_commit_guard(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, true) == 0);
    f.rewind_during_validator = true;
    const wcm_counter_t before = wcm_runtime_world_epoch(&f.runtime);
    CHECK(publish_and_step(&f, 0, 1000u, 1.0f) == 0);
    CHECK(f.applies == 0u);
    CHECK(wcm_runtime_world_epoch(&f.runtime) == before + 1u);
    CHECK(metrics_of(&f).clock_discontinuities == 1u);
    wcm_health_snapshot_t health;
    CHECK(wcm_runtime_get_health(&f.runtime, &health) == WCM_OK);
    CHECK(!health.last_step_timing_valid);
    return 0;
}

static int test_release_skip_accounting(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    wcm_fake_clock_set(&f.clock, 5500u);
    CHECK(wcm_runtime_step(&f.runtime) == WCM_OK);
    wcm_module_stats_t stats;
    CHECK(wcm_runtime_get_module_stats(&f.runtime, 7u, &stats) == WCM_OK);
    CHECK(stats.runtime.release_skips == 4u);
    CHECK(metrics_of(&f).release_skips == 4u);
    return 0;
}

static int test_pre_break_observation_cannot_rebind_new_epoch(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    CHECK(publish_and_step(&f, 0u, 1000u, 1.0f) == 0);
    wcm_fake_clock_set(&f.clock, 1500u);
    CHECK(wcm_runtime_world_break(&f.runtime, WCM_BREAK_APPLICATION) == WCM_OK);

    wcm_observation_t late = observation_at(1400u, 2.0f);
    wcm_fake_clock_set(&f.clock, 1600u);
    CHECK(wcm_runtime_post_observation(&f.runtime, 0u, &late) == WCM_OK);
    CHECK(wcm_runtime_step(&f.runtime) == WCM_OK);
    wcm_cap_state_t state;
    CHECK(wcm_runtime_get_capability_state(&f.runtime, 0u, &state) == WCM_OK);
    CHECK(state != WCM_CAP_BOUND);
    CHECK(metrics_of(&f).ingress_rejected_timestamp == 1u);

    wcm_observation_t fresh = observation_at(1600u, 3.0f);
    CHECK(wcm_runtime_post_observation(&f.runtime, 0u, &fresh) == WCM_OK);
    CHECK(wcm_runtime_step(&f.runtime) == WCM_OK);
    CHECK(wcm_runtime_get_capability_state(&f.runtime, 0u, &state) == WCM_OK);
    CHECK(state == WCM_CAP_BOUND);
    return 0;
}


typedef struct {
    uint8_t images[2][WCM_ANCHOR_IMAGE_BYTES];
    bool present[2];
    unsigned syncs;
} memory_anchor_backend_t;

static wcm_status_t memory_anchor_read(void *user, uint8_t slot, void *buffer, size_t size) {
    memory_anchor_backend_t *b = (memory_anchor_backend_t *)user;
    if (!b || !buffer || size != WCM_ANCHOR_IMAGE_BYTES || slot > 1u) return WCM_ERR_ARG;
    if (!b->present[slot]) return WCM_ERR_NOT_FOUND;
    memcpy(buffer, b->images[slot], size);
    return WCM_OK;
}

static wcm_status_t memory_anchor_write(void *user, uint8_t slot, const void *buffer, size_t size) {
    memory_anchor_backend_t *b = (memory_anchor_backend_t *)user;
    if (!b || !buffer || size != WCM_ANCHOR_IMAGE_BYTES || slot > 1u) return WCM_ERR_ARG;
    memcpy(b->images[slot], buffer, size);
    b->present[slot] = true;
    return WCM_OK;
}

static wcm_status_t memory_anchor_sync(void *user) {
    memory_anchor_backend_t *b = (memory_anchor_backend_t *)user;
    if (!b) return WCM_ERR_ARG;
    b->syncs++;
    return WCM_OK;
}

static int test_persistence_backend_roundtrip(void) {
    memory_anchor_backend_t memory;
    memset(&memory, 0, sizeof(memory));
    const wcm_anchor_backend_t backend = {
        .read = memory_anchor_read,
        .write = memory_anchor_write,
        .sync = memory_anchor_sync,
        .user = &memory,
    };
    const uint8_t first[] = {1u, 2u, 3u, 4u};
    const uint8_t second[] = {8u, 7u, 6u};
    CHECK(wcm_anchor_backend_commit(&backend, first, (uint16_t)sizeof(first), 1u) == WCM_OK);
    CHECK(wcm_anchor_backend_commit(&backend, second, (uint16_t)sizeof(second), 2u) == WCM_OK);
    CHECK(memory.syncs == 2u);
    wcm_anchor_slot_t loaded;
    CHECK(wcm_anchor_backend_load(&backend, &loaded) == WCM_OK);
    CHECK(loaded.sequence == 2u);
    CHECK(loaded.payload_size == sizeof(second));
    CHECK(memcmp(loaded.payload, second, sizeof(second)) == 0);
    return 0;
}

static int test_runtime_health_and_build_identity(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    wcm_build_info_t build;
    memset(&build, 0, sizeof(build));
    wcm_get_build_info(&build);
    CHECK(build.abi_version == WCM_ABI_VERSION);
    CHECK(build.config_fingerprint_version == WCM_CONFIG_FINGERPRINT_VERSION);
    CHECK(build.anchor_format_version == WCM_ANCHOR_FORMAT);
    CHECK(strcmp(build.version, WCM_VERSION_STRING) == 0);
    CHECK(build.runtime_storage_bytes == WCM_RUNTIME_STORAGE_BYTES);

    wcm_health_snapshot_t health;
    CHECK(wcm_runtime_get_health(&f.runtime, &health) == WCM_OK);
    CHECK(health.deployment_id == UINT64_C(0x57434D110001));
    CHECK(health.config_revision == 11u);
    CHECK(health.config_fingerprint != 0u);
    CHECK(health.step_sequence == 0u);
    CHECK(wcm_runtime_check_configuration(&f.runtime) == WCM_OK);

    CHECK(publish_and_step(&f, 0u, 1000u, 1.0f) == 0);
    CHECK(wcm_runtime_get_health(&f.runtime, &health) == WCM_OK);
    CHECK(health.step_sequence == 1u);
    CHECK(health.state == WCM_HEALTH_NOMINAL);
    CHECK((health.bound_capabilities & UINT64_C(1)) != 0u);
    return 0;
}

static int test_event_journal_lifecycle(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    wcm_event_record_t event;
    CHECK(wcm_runtime_read_event(&f.runtime, 0u, &event) == WCM_OK);
    CHECK(event.type == WCM_EVENT_RUNTIME_READY);
    uint64_t cursor = event.sequence;

    CHECK(publish_and_step(&f, 0u, 1000u, 1.0f) == 0);
    CHECK(wcm_runtime_read_event(&f.runtime, cursor, &event) == WCM_OK);
    CHECK(event.type == WCM_EVENT_CAPABILITY_BOUND);
    cursor = event.sequence;

    wcm_fake_clock_set(&f.clock, 1200u);
    CHECK(wcm_runtime_world_break(&f.runtime, WCM_BREAK_APPLICATION) == WCM_OK);
    CHECK(wcm_runtime_read_event(&f.runtime, cursor, &event) == WCM_OK);
    CHECK(event.type == WCM_EVENT_WORLD_BREAK);
    return 0;
}

static int test_actuator_runtime_stats(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    f.apply_advance_us = 25u;
    CHECK(publish_and_step(&f, 0u, 1000u, 1.0f) == 0);
    wcm_actuator_stats_t stats;
    CHECK(wcm_runtime_get_actuator_stats(&f.runtime, 0u, &stats) == WCM_OK);
    CHECK(stats.dispatches == 1u);
    CHECK(stats.failures == 0u);
    CHECK(stats.overruns == 0u);
    CHECK(stats.max_dispatch_us == 25u);
    CHECK(stats.configured_dispatch_wcet_us == 100u);
    CHECK(stats.configured_effect_latency_us == 100u);
    return 0;
}

static int test_step_budget_is_observable(void) {
    fixture_t f;
    CHECK(fixture_init_ex(&f, false, false, 10u) == 0);
    f.controller_advance_us = 20u;
    CHECK(publish_and_step(&f, 0u, 1000u, 1.0f) == 0);
    wcm_health_snapshot_t health;
    CHECK(wcm_runtime_get_health(&f.runtime, &health) == WCM_OK);
    CHECK(health.metrics.step_budget_overruns == 1u);
    CHECK(health.last_fault.code == WCM_FAULT_STEP_BUDGET_OVERRUN);
    CHECK(health.max_step_elapsed_us >= 20u);
    return 0;
}

static int test_configuration_fingerprint_detects_descriptor_change(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    CHECK(wcm_runtime_check_configuration(&f.runtime) == WCM_OK);
    wcm_module_desc_t *modules = (wcm_module_desc_t *)(uintptr_t)f.modules;
    const uint32_t saved = modules[0].period_us;
    modules[0].period_us = saved + 1u;
    CHECK(wcm_runtime_check_configuration(&f.runtime) == WCM_ERR_STATE);
    modules[0].period_us = saved;
    CHECK(wcm_runtime_check_configuration(&f.runtime) == WCM_OK);
    return 0;
}



static int test_periodic_config_integrity_check_cuts_world(void) {
    fixture_t f;
    CHECK(fixture_init_options(&f, false, false, 0u, 1u) == 0);
    const wcm_counter_t before = wcm_runtime_world_epoch(&f.runtime);
    wcm_module_desc_t *modules = (wcm_module_desc_t *)(uintptr_t)f.modules;
    const uint32_t saved = modules[0].period_us;
    modules[0].period_us = saved + 1u;
    CHECK(wcm_runtime_step(&f.runtime) == WCM_ERR_STATE);
    CHECK(wcm_runtime_world_epoch(&f.runtime) == before + 1u);
    wcm_fault_record_t fault;
    CHECK(wcm_runtime_get_last_fault(&f.runtime, &fault) == WCM_OK);
    CHECK(fault.code == WCM_FAULT_CONFIG_MUTATION);
    modules[0].period_us = saved;
    return 0;
}

static int test_runtime_copies_top_level_config(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    f.config.deployment_id = UINT64_C(0xDEADBEEF);
    f.config.config_revision = 999u;
    wcm_health_snapshot_t health;
    CHECK(wcm_runtime_get_health(&f.runtime, &health) == WCM_OK);
    CHECK(health.deployment_id == UINT64_C(0x57434D110001));
    CHECK(health.config_revision == 11u);
    return 0;
}

static int test_event_journal_reports_gap_after_overwrite(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    for (unsigned i = 0u; i < (unsigned)WCM_MAX_EVENTS + 4u; ++i) {
        wcm_fake_clock_advance(&f.clock, 1u);
        CHECK(wcm_runtime_world_break(&f.runtime, WCM_BREAK_APPLICATION) == WCM_OK);
    }
    wcm_event_record_t event;
    CHECK(wcm_runtime_read_event(&f.runtime, 0u, &event) == WCM_ERR_GAP);
    CHECK(event.sequence > 1u);
    wcm_health_snapshot_t health;
    CHECK(wcm_runtime_get_health(&f.runtime, &health) == WCM_OK);
    CHECK(health.metrics.event_overwrites > 0u);
    return 0;
}

static int test_persistence_falls_back_to_older_valid_slot(void) {
    memory_anchor_backend_t memory;
    memset(&memory, 0, sizeof(memory));
    const wcm_anchor_backend_t backend = {
        .read = memory_anchor_read,
        .write = memory_anchor_write,
        .sync = memory_anchor_sync,
        .user = &memory,
    };
    const uint8_t first[] = {1u};
    const uint8_t second[] = {2u};
    CHECK(wcm_anchor_backend_commit(&backend, first, (uint16_t)sizeof(first), 10u) == WCM_OK);
    CHECK(wcm_anchor_backend_commit(&backend, second, (uint16_t)sizeof(second), 11u) == WCM_OK);
    wcm_anchor_slot_t slot0;
    wcm_anchor_slot_t slot1;
    CHECK(wcm_anchor_decode(memory.images[0], &slot0) == WCM_OK);
    CHECK(wcm_anchor_decode(memory.images[1], &slot1) == WCM_OK);
    const unsigned newest = slot0.sequence > slot1.sequence ? 0u : 1u;
    memory.images[newest][16] ^= 1u;
    wcm_anchor_slot_t loaded;
    CHECK(wcm_anchor_backend_load(&backend, &loaded) == WCM_OK);
    CHECK(loaded.sequence == 10u);
    CHECK(loaded.payload[0] == 1u);
    return 0;
}


static int test_anchor_canonical_storage_image(void) {
    const uint8_t payload[] = {0xA1u, 0xB2u, 0xC3u};
    wcm_anchor_slot_t slot;
    CHECK(wcm_anchor_write(&slot, payload, (uint16_t)sizeof(payload), UINT64_C(0x0102030405060708)) == WCM_OK);

    uint8_t image[WCM_ANCHOR_IMAGE_BYTES];
    CHECK(wcm_anchor_encode(&slot, image) == WCM_OK);
    CHECK(WCM_ANCHOR_IMAGE_BYTES == 148u);
    CHECK(image[0] == 0x41u && image[1] == 0x4Du && image[2] == 0x43u && image[3] == 0x57u);
    CHECK(image[4] == 0x02u && image[5] == 0x00u);
    CHECK(image[6] == 0x03u && image[7] == 0x00u);
    CHECK(image[8] == 0x08u && image[9] == 0x07u && image[10] == 0x06u && image[11] == 0x05u);
    CHECK(image[12] == 0x04u && image[13] == 0x03u && image[14] == 0x02u && image[15] == 0x01u);
    CHECK(image[20] == 0xA1u && image[21] == 0xB2u && image[22] == 0xC3u);

    wcm_anchor_slot_t decoded;
    CHECK(wcm_anchor_decode(image, &decoded) == WCM_OK);
    CHECK(decoded.sequence == UINT64_C(0x0102030405060708));
    CHECK(decoded.payload_size == sizeof(payload));
    CHECK(memcmp(decoded.payload, payload, sizeof(payload)) == 0);
    return 0;
}



static int test_end_of_step_clock_failure_is_fail_closed(void) {
    fixture_t f;
    memset(&f, 0, sizeof(f));
    wcm_fake_clock_init(&f.clock, 1000u);

    wcm_runtime_config_t config;
    wcm_runtime_config_init(&config);
    config.ingress_producer_count = 1u;
    config.clock_read = wcm_fake_clock_read;
    config.clock_user = &f.clock;
    config.commit_guard_enter = guard_enter;
    config.commit_guard_exit = guard_exit;
    config.commit_guard_user = &f;

    CHECK(wcm_runtime_init(&f.runtime, f.storage, sizeof(f.storage), &config) == WCM_OK);
    CHECK(f.clock.read_count == 1u);
    wcm_fake_clock_fail_on_read(&f.clock, 3u); /* step entry is #2, end-of-step accounting is #3 */
    CHECK(wcm_runtime_step(&f.runtime) == WCM_ERR_CLOCK);
    CHECK(wcm_runtime_step(&f.runtime) == WCM_ERR_STOPPED);
    wcm_health_snapshot_t health;
    CHECK(wcm_runtime_get_health(&f.runtime, &health) == WCM_OK);
    CHECK(health.state == WCM_HEALTH_FAIL_STOP);
    CHECK(health.last_fault.code == WCM_FAULT_CLOCK_READ_FAILED);
    return 0;
}


static int test_clock_read_failure_is_fail_closed(void) {
    fixture_t f;
    CHECK(fixture_init(&f, false, false) == 0);
    wcm_fake_clock_fail(&f.clock, true);
    CHECK(wcm_runtime_step(&f.runtime) == WCM_ERR_CLOCK);
    CHECK(f.safe_calls >= 4u); /* two at init, then both actuators on clock failure */
    CHECK(wcm_runtime_step(&f.runtime) == WCM_ERR_STOPPED);
    wcm_health_snapshot_t health;
    CHECK(wcm_runtime_get_health(&f.runtime, &health) == WCM_OK);
    CHECK(health.state == WCM_HEALTH_FAIL_STOP);
    CHECK(health.last_fault.code == WCM_FAULT_CLOCK_READ_FAILED ||
          health.last_fault.code == WCM_FAULT_SAFE_OUTPUT_FAILED);
    return 0;
}

int main(void) {
    struct test_case {
        const char *name;
        int (*run)(void);
    } tests[] = {
        {"periodic_config_integrity_check_cuts_world", test_periodic_config_integrity_check_cuts_world},
        {"runtime_copies_top_level_config", test_runtime_copies_top_level_config},
        {"event_journal_reports_gap_after_overwrite", test_event_journal_reports_gap_after_overwrite},
        {"persistence_falls_back_to_older_valid_slot", test_persistence_falls_back_to_older_valid_slot},
        {"anchor_canonical_storage_image", test_anchor_canonical_storage_image},
        {"end_of_step_clock_failure_is_fail_closed", test_end_of_step_clock_failure_is_fail_closed},
        {"clock_read_failure_is_fail_closed", test_clock_read_failure_is_fail_closed},
        {"persistence_backend_roundtrip", test_persistence_backend_roundtrip},
        {"runtime_health_and_build_identity", test_runtime_health_and_build_identity},
        {"event_journal_lifecycle", test_event_journal_lifecycle},
        {"actuator_runtime_stats", test_actuator_runtime_stats},
        {"step_budget_is_observable", test_step_budget_is_observable},
        {"configuration_fingerprint_detects_descriptor_change", test_configuration_fingerprint_detects_descriptor_change},
        {"init_forces_safe_output", test_init_forces_safe_output},
        {"init_fails_if_safe_output_fails", test_init_fails_if_safe_output_fails},
        {"gate_wcet_overrun_fails_closed", test_gate_wcet_overrun_fails_closed},
        {"scheduler_does_not_hot_loop_when_unbound", test_scheduler_does_not_hot_loop_when_unbound},
        {"ingress_exact_capacity", test_ingress_exact_capacity},
        {"ingress_fold_budget_defers_control", test_ingress_fold_budget_defers_control},
        {"async_world_break_request", test_async_world_break_request},
        {"runtime_stop_is_fail_closed", test_runtime_stop_is_fail_closed},
        {"config_abi_mismatch", test_config_abi_mismatch},
        {"module_stats_and_fault_record", test_module_stats_and_fault_record},
        {"clock_discontinuity_inside_commit_guard", test_clock_discontinuity_inside_commit_guard},
        {"release_skip_accounting", test_release_skip_accounting},
        {"pre_break_observation_cannot_rebind_new_epoch", test_pre_break_observation_cannot_rebind_new_epoch},
        {"witness_timestamp_policy", test_witness_timestamp_policy},
        {"strict_timestamp_zero_is_not_a_sentinel", test_strict_timestamp_zero_is_not_a_sentinel},
        {"impossible_commit_horizon_expires", test_impossible_commit_horizon_expires},
        {"witness_counter_no_wrap", test_witness_counter_no_wrap},
        {"world_counter_no_wrap", test_world_counter_no_wrap},
        {"dependency_counter_no_wrap", test_dependency_counter_no_wrap},
        {"snapshot_is_immutable_copy", test_snapshot_is_immutable_copy},
        {"runtime_end_to_end", test_runtime_end_to_end},
        {"controller_wcet_overrun_fails_closed", test_controller_wcet_overrun_fails_closed},
        {"commit_rechecks_actual_clock", test_commit_rechecks_actual_clock},
        {"commit_rechecks_after_final_validation", test_commit_rechecks_after_final_validation},
        {"forged_source_rejected", test_forged_source_rejected},
        {"filter_composition_final_validation", test_filter_composition_final_validation},
        {"resource_checks_final_value", test_resource_checks_final_value},
        {"ingress_rejection_is_local", test_ingress_rejection_is_local},
        {"unknown_source_is_local", test_unknown_source_is_local},
        {"multi_producer_timestamp_merge", test_multi_producer_timestamp_merge},
        {"out_of_order_observation_dropped", test_out_of_order_observation_dropped},
        {"future_observation_dropped", test_future_observation_dropped},
        {"clock_discontinuity_breaks_world", test_clock_discontinuity_breaks_world},
        {"world_break_requires_fresh_evidence", test_world_break_requires_fresh_evidence},
        {"arrival_during_controller_voids_intent", test_arrival_during_controller_voids_intent},
        {"arrival_during_filter_voids_intent", test_arrival_during_filter_voids_intent},
        {"actuator_failure_breaks_world", test_actuator_failure_breaks_world},
        {"actuator_overrun_breaks_world", test_actuator_overrun_breaks_world},
        {"world_break_from_controller_is_deferred", test_world_break_from_controller_is_deferred},
        {"unauthorized_actuator_is_rejected", test_unauthorized_actuator_is_rejected},
        {"safe_output_failure_latches_runtime", test_safe_output_failure_latches_runtime},
        {"world_break_from_final_validator_voids_intent", test_world_break_from_final_validator_voids_intent},
        {"world_break_from_apply_occurs_after_commit", test_world_break_from_apply_occurs_after_commit},
        {"anchor_slots", test_anchor_slots},
        {"capability_cycle_rejected", test_capability_cycle_rejected},
        {"queue_overflow_is_counted", test_queue_overflow_is_counted},
        {"config_rejects_unbound_references", test_config_rejects_unbound_references},
        {"runtime_reentrancy_guard", test_runtime_reentrancy_guard},
    };

    const size_t count = sizeof(tests) / sizeof(tests[0]);
    for (size_t i = 0; i < count; ++i) {
        if (tests[i].run() != 0) return 1;
        printf("PASS %s\n", tests[i].name);
    }
    printf("ALL PASS (%zu)\n", count);
    return 0;
}

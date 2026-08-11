#include "wcm/dependency.h"

static bool is_valid(const wcm_witness_t *w) {
    return w && (w->flags & WCM_WITNESS_VALID) != 0u;
}

static bool quality_admissible(const wcm_dependency_desc_t *dep, const wcm_witness_t *w) {
    return is_valid(w) && w->quality >= dep->quality_floor;
}

bool wcm_dependency_transition_matters(
    const wcm_dependency_desc_t *dep,
    const wcm_witness_t *before,
    const wcm_witness_t *after) {
    if (!dep) return false;

    const bool before_valid = is_valid(before);
    const bool after_valid = is_valid(after);

    switch ((wcm_dependency_lens_t)dep->lens) {
        case WCM_LENS_SAMPLE:
            if (!before && after) return true;
            if (before && !after) return true;
            if (!before && !after) return false;
            return before->generation != after->generation ||
                   before->world_epoch != after->world_epoch ||
                   before_valid != after_valid;
        case WCM_LENS_EDGE:
            return before_valid != after_valid ||
                   (before && after && before->world_epoch != after->world_epoch);
        case WCM_LENS_VALIDITY:
            return before_valid != after_valid ||
                   quality_admissible(dep, before) != quality_admissible(dep, after) ||
                   (before && after && before->world_epoch != after->world_epoch);
        default:
            return true;
    }
}

wcm_status_t wcm_dependency_apply_transition(
    wcm_module_runtime_t *module,
    const wcm_dependency_desc_t *dep,
    const wcm_witness_t *before,
    const wcm_witness_t *after) {
    if (!module || !dep) return WCM_ERR_ARG;
    if (!wcm_dependency_transition_matters(dep, before, after)) return WCM_OK;
    if (module->dependency_clock == UINT64_MAX) return WCM_ERR_COUNTER;
    module->dependency_clock++;
    return WCM_OK;
}

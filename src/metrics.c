#include "wcm/metrics.h"

double wcm_metrics_concordance_yield(const wcm_metrics_t *m) {
    if (!m || m->intents_proposed == 0u) return 0.0;
    return (double)(m->commits_ok + m->commits_modified) / (double)m->intents_proposed;
}

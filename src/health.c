#include "wcm/health.h"

const char *wcm_health_state_string(wcm_health_state_t state) {
    switch (state) {
        case WCM_HEALTH_STARTING: return "starting";
        case WCM_HEALTH_NOMINAL: return "nominal";
        case WCM_HEALTH_DEGRADED: return "degraded";
        case WCM_HEALTH_REBINDING: return "rebinding";
        case WCM_HEALTH_STOPPED: return "stopped";
        case WCM_HEALTH_FAIL_STOP: return "fail_stop";
        default: return "unknown";
    }
}

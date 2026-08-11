#include "wcm/event.h"

const char *wcm_event_type_string(wcm_event_type_t type) {
    switch (type) {
        case WCM_EVENT_RUNTIME_READY: return "runtime_ready";
        case WCM_EVENT_RUNTIME_STOPPED: return "runtime_stopped";
        case WCM_EVENT_WORLD_BREAK: return "world_break";
        case WCM_EVENT_FAULT: return "fault";
        case WCM_EVENT_CAPABILITY_BOUND: return "capability_bound";
        case WCM_EVENT_CAPABILITY_UNBOUND: return "capability_unbound";
        case WCM_EVENT_INGRESS_BACKLOG: return "ingress_backlog";
        default: return "unknown";
    }
}

#include "wcm/diagnostics.h"

const char *wcm_status_string(wcm_status_t status) {
    switch (status) {
        case WCM_OK: return "ok";
        case WCM_ERR_ARG: return "invalid argument";
        case WCM_ERR_RANGE: return "out of range";
        case WCM_ERR_STATE: return "invalid state";
        case WCM_ERR_FULL: return "full";
        case WCM_ERR_NOT_FOUND: return "not found";
        case WCM_ERR_GRAPH: return "invalid capability graph";
        case WCM_ERR_IO: return "I/O failure";
        case WCM_ERR_QUALITY: return "quality rejected";
        case WCM_ERR_TIMESTAMP: return "timestamp rejected";
        case WCM_ERR_POLICY: return "policy rejected";
        case WCM_ERR_COUNTER: return "counter exhausted";
        case WCM_ERR_BUSY: return "busy";
        case WCM_ERR_CLOCK: return "clock discontinuity";
        case WCM_ERR_WCET: return "timing contract exceeded";
        case WCM_ERR_ABI: return "ABI mismatch";
        case WCM_ERR_STOPPED: return "runtime stopped";
        case WCM_ERR_BACKLOG: return "ingress backlog";
        case WCM_ERR_GAP: return "event history gap";
        default: return "unknown status";
    }
}

const char *wcm_commit_code_string(wcm_commit_code_t code) {
    switch (code) {
        case WCM_COMMIT_OK: return "committed";
        case WCM_COMMIT_MODIFIED: return "committed after transform";
        case WCM_VOID_WORLD: return "void: world changed";
        case WCM_VOID_STALE: return "void: dependency changed";
        case WCM_VOID_EXPIRED: return "void: livebound expired";
        case WCM_VOID_INGRESS_BACKLOG: return "void: ingress backlog";
        case WCM_DENY_CAPABILITY: return "denied: capability";
        case WCM_DENY_RESOURCE: return "denied: resource";
        case WCM_DENY_AUTHORITY: return "denied: authority";
        case WCM_REJECT_CONSTRAINT: return "rejected: constraint";
        case WCM_REJECT_SAFETY: return "rejected: safety";
        case WCM_COMMIT_GATE_OVERRUN: return "rejected: gate WCET overrun";
        case WCM_COMMIT_DISPATCH_FAILED: return "dispatch failed";
        case WCM_COMMIT_DISPATCH_OVERRUN: return "dispatch WCET overrun";
        case WCM_COMMIT_PORT_VIOLATION: return "commit guard violation";
        case WCM_COMMIT_INTERNAL_ERROR: return "internal commit error";
        default: return "unknown commit code";
    }
}

const char *wcm_world_break_reason_string(wcm_world_break_reason_t reason) {
    switch (reason) {
        case WCM_BREAK_RESET: return "reset";
        case WCM_BREAK_BROWNOUT: return "brownout";
        case WCM_BREAK_CLOCK_DISCONTINUITY: return "clock discontinuity";
        case WCM_BREAK_STATE_CORRUPTION: return "state corruption";
        case WCM_BREAK_ACTUATOR_DOMAIN_RESET: return "actuator domain reset";
        case WCM_BREAK_ACTUATOR_FAILURE: return "actuator failure";
        case WCM_BREAK_APPLICATION: return "application request";
        default: return "unknown world break";
    }
}

const char *wcm_fault_code_string(wcm_fault_code_t code) {
    switch (code) {
        case WCM_FAULT_NONE: return "none";
        case WCM_FAULT_CLOCK_DISCONTINUITY: return "clock discontinuity";
        case WCM_FAULT_CLOCK_READ_FAILED: return "clock read failed";
        case WCM_FAULT_COUNTER_EXHAUSTED: return "counter exhausted";
        case WCM_FAULT_MODULE_CALLBACK: return "module callback error";
        case WCM_FAULT_MODULE_WCET_OVERRUN: return "module WCET overrun";
        case WCM_FAULT_GATE_WCET_OVERRUN: return "commit gate WCET overrun";
        case WCM_FAULT_ACTUATOR_DISPATCH_FAILED: return "actuator dispatch failed";
        case WCM_FAULT_ACTUATOR_DISPATCH_OVERRUN: return "actuator dispatch WCET overrun";
        case WCM_FAULT_SAFE_OUTPUT_FAILED: return "safe output failed";
        case WCM_FAULT_PORT_CONTRACT: return "port commit-guard contract violated";
        case WCM_FAULT_INGRESS_BACKLOG: return "ingress backlog";
        case WCM_FAULT_STEP_BUDGET_OVERRUN: return "runtime step budget overrun";
        case WCM_FAULT_CONFIG_MUTATION: return "configuration contract changed";
        default: return "unknown fault";
    }
}

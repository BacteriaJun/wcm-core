#ifndef WCM_PORTS_BAREMETAL_H
#define WCM_PORTS_BAREMETAL_H

#include "wcm/runtime.h"

typedef struct {
    wcm_clock_read_fn clock_read;
    void *clock_user;
    wcm_commit_guard_fn commit_guard_enter;
    wcm_commit_guard_fn commit_guard_exit;
    void *commit_guard_user;
} wcm_baremetal_port_t;

static inline wcm_status_t wcm_baremetal_bind(
    wcm_runtime_config_t *config,
    const wcm_baremetal_port_t *port) {
    if (!config || !port || !port->clock_read ||
        !port->commit_guard_enter || !port->commit_guard_exit) {
        return WCM_ERR_ARG;
    }
    config->clock_read = port->clock_read;
    config->clock_user = port->clock_user;
    config->commit_guard_enter = port->commit_guard_enter;
    config->commit_guard_exit = port->commit_guard_exit;
    config->commit_guard_user = port->commit_guard_user;
    return WCM_OK;
}

#endif

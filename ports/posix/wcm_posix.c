#define _POSIX_C_SOURCE 200809L
#include "wcm/ports/posix.h"

#include <errno.h>
#include <stdint.h>
#include <time.h>

wcm_status_t wcm_posix_port_init(wcm_posix_port_t *port) {
    if (!port) return WCM_ERR_ARG;
    if (pthread_mutex_init(&port->commit_lock, NULL) != 0) return WCM_ERR_IO;
    port->initialized = 1;
    return WCM_OK;
}

void wcm_posix_port_destroy(wcm_posix_port_t *port) {
    if (!port || !port->initialized) return;
    (void)pthread_mutex_destroy(&port->commit_lock);
    port->initialized = 0;
}

wcm_status_t wcm_posix_clock_read(void *user, wcm_time_t *out) {
    (void)user;
    if (!out) return WCM_ERR_ARG;
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return WCM_ERR_IO;
    const uint64_t sec = (uint64_t)ts.tv_sec;
    const uint64_t usec = (uint64_t)ts.tv_nsec / UINT64_C(1000);
    if (sec > (UINT64_MAX - usec) / UINT64_C(1000000)) return WCM_ERR_RANGE;
    *out = sec * UINT64_C(1000000) + usec;
    return WCM_OK;
}

wcm_time_t wcm_posix_clock_now(void) {
    wcm_time_t now = 0u;
    return wcm_posix_clock_read(NULL, &now) == WCM_OK ? now : 0u;
}

void wcm_posix_commit_guard_enter(void *user) {
    wcm_posix_port_t *port = (wcm_posix_port_t *)user;
    if (!port || !port->initialized) return;
    (void)pthread_mutex_lock(&port->commit_lock);
}

void wcm_posix_commit_guard_exit(void *user) {
    wcm_posix_port_t *port = (wcm_posix_port_t *)user;
    if (!port || !port->initialized) return;
    (void)pthread_mutex_unlock(&port->commit_lock);
}

wcm_status_t wcm_posix_post_observation(
    wcm_posix_port_t *port,
    wcm_runtime_t *runtime,
    uint8_t producer_index,
    const wcm_observation_t *observation) {
    if (!port || !port->initialized || !runtime || !observation) return WCM_ERR_ARG;
    if (pthread_mutex_lock(&port->commit_lock) != 0) return WCM_ERR_IO;
    const wcm_status_t rc = wcm_runtime_post_observation(runtime, producer_index, observation);
    if (pthread_mutex_unlock(&port->commit_lock) != 0 && rc == WCM_OK) return WCM_ERR_IO;
    return rc;
}

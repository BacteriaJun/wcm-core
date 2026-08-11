#ifndef WCM_PORTS_POSIX_H
#define WCM_PORTS_POSIX_H

#include <pthread.h>
#include "wcm/runtime.h"

typedef struct {
    pthread_mutex_t commit_lock;
    int initialized;
} wcm_posix_port_t;

wcm_status_t wcm_posix_port_init(wcm_posix_port_t *port);
void wcm_posix_port_destroy(wcm_posix_port_t *port);
wcm_status_t wcm_posix_clock_read(void *user, wcm_time_t *out);
wcm_time_t wcm_posix_clock_now(void);
void wcm_posix_commit_guard_enter(void *user);
void wcm_posix_commit_guard_exit(void *user);
wcm_status_t wcm_posix_post_observation(
    wcm_posix_port_t *port,
    wcm_runtime_t *runtime,
    uint8_t producer_index,
    const wcm_observation_t *observation);

#endif

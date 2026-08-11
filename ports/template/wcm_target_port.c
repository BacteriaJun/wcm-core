#include "wcm/runtime.h"

/*
 * Integration template only. Replace these functions in the target layer and
 * do not add board or RTOS dependencies to WCM Core.
 */

wcm_status_t target_clock_read(void *user, wcm_time_t *out) {
    (void)user;
    (void)out;
    return WCM_ERR_IO;
}

void target_commit_guard_enter(void *user) {
    (void)user;
}

void target_commit_guard_exit(void *user) {
    (void)user;
}

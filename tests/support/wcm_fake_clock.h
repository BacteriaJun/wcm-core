#ifndef WCM_TEST_FAKE_CLOCK_H
#define WCM_TEST_FAKE_CLOCK_H

#include "wcm/types.h"

typedef struct {
    wcm_time_t now;
    uint32_t read_count;
    uint32_t fail_on_read;
    bool fail_read;
} wcm_fake_clock_t;

static inline void wcm_fake_clock_init(wcm_fake_clock_t *clock, wcm_time_t start) {
    if (clock) {
        clock->now = start;
        clock->read_count = 0u;
        clock->fail_on_read = 0u;
        clock->fail_read = false;
    }
}

static inline wcm_time_t wcm_fake_clock_now(const wcm_fake_clock_t *clock) {
    return clock ? clock->now : 0u;
}

static inline wcm_status_t wcm_fake_clock_read(void *user, wcm_time_t *out) {
    if (!user || !out) return WCM_ERR_ARG;
    wcm_fake_clock_t *clock = (wcm_fake_clock_t *)user;
    clock->read_count++;
    if (clock->fail_read || (clock->fail_on_read != 0u && clock->read_count == clock->fail_on_read)) {
        return WCM_ERR_IO;
    }
    *out = wcm_fake_clock_now(clock);
    return WCM_OK;
}

static inline void wcm_fake_clock_advance(wcm_fake_clock_t *clock, uint64_t delta_us) {
    if (clock) clock->now += delta_us;
}

static inline void wcm_fake_clock_set(wcm_fake_clock_t *clock, wcm_time_t now) {
    if (clock) clock->now = now;
}

static inline void wcm_fake_clock_fail(wcm_fake_clock_t *clock, bool fail) {
    if (clock) clock->fail_read = fail;
}

static inline void wcm_fake_clock_fail_on_read(wcm_fake_clock_t *clock, uint32_t read_number) {
    if (clock) clock->fail_on_read = read_number;
}

#endif

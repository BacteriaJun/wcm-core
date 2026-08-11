#ifndef WCM_INGRESS_H
#define WCM_INGRESS_H

#include <limits.h>
#include <stdatomic.h>
#include "wcm/config.h"
#include "wcm/types.h"

#if ATOMIC_INT_LOCK_FREE != 2
#error "WCM ingress requires always-lock-free atomic unsigned operations"
#endif

_Static_assert(UINT_MAX > (2u * WCM_MAX_INGRESS), "unsigned is too small for ingress counters");

typedef struct {
    wcm_value_t value;
    wcm_time_t observed_at;
    wcm_time_t requested_livebound;
    uint16_t witness_id;
    uint8_t quality;
    uint8_t flags;
} wcm_observation_t;

typedef struct {
    wcm_observation_t records[WCM_MAX_INGRESS];
    atomic_uint head;
    atomic_uint tail;
    atomic_uint dropped;
} wcm_ingress_t;

void wcm_ingress_init(wcm_ingress_t *q);
wcm_status_t wcm_ingress_push(wcm_ingress_t *q, const wcm_observation_t *observation);
wcm_status_t wcm_ingress_peek(const wcm_ingress_t *q, wcm_observation_t *out);
wcm_status_t wcm_ingress_pop(wcm_ingress_t *q, wcm_observation_t *out);
void wcm_ingress_clear(wcm_ingress_t *q);
uint32_t wcm_ingress_dropped(const wcm_ingress_t *q);
uint32_t wcm_ingress_count(const wcm_ingress_t *q);

#endif

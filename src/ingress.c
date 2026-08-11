#include "wcm/ingress.h"
#include <string.h>

static unsigned queue_count(unsigned head, unsigned tail) {
    return head - tail;
}

static void dropped_inc_saturating(atomic_uint *value) {
    unsigned current = atomic_load_explicit(value, memory_order_relaxed);
    while (current != UINT_MAX &&
           !atomic_compare_exchange_weak_explicit(
               value,
               &current,
               current + 1u,
               memory_order_relaxed,
               memory_order_relaxed)) {
    }
}

void wcm_ingress_init(wcm_ingress_t *q) {
    if (!q) return;
    memset(q->records, 0, sizeof(q->records));
    atomic_init(&q->head, 0u);
    atomic_init(&q->tail, 0u);
    atomic_init(&q->dropped, 0u);
}

wcm_status_t wcm_ingress_push(wcm_ingress_t *q, const wcm_observation_t *observation) {
    if (!q || !observation) return WCM_ERR_ARG;
    const unsigned head = atomic_load_explicit(&q->head, memory_order_relaxed);
    const unsigned tail = atomic_load_explicit(&q->tail, memory_order_acquire);
    if (queue_count(head, tail) >= WCM_MAX_INGRESS) {
        dropped_inc_saturating(&q->dropped);
        return WCM_ERR_FULL;
    }
    q->records[head % WCM_MAX_INGRESS] = *observation;
    atomic_store_explicit(&q->head, head + 1u, memory_order_release);
    return WCM_OK;
}

wcm_status_t wcm_ingress_peek(const wcm_ingress_t *q, wcm_observation_t *out) {
    if (!q || !out) return WCM_ERR_ARG;
    const unsigned tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
    const unsigned head = atomic_load_explicit(&q->head, memory_order_acquire);
    if (tail == head) return WCM_ERR_NOT_FOUND;
    *out = q->records[tail % WCM_MAX_INGRESS];
    return WCM_OK;
}

wcm_status_t wcm_ingress_pop(wcm_ingress_t *q, wcm_observation_t *out) {
    if (!q || !out) return WCM_ERR_ARG;
    const unsigned tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
    const unsigned head = atomic_load_explicit(&q->head, memory_order_acquire);
    if (tail == head) return WCM_ERR_NOT_FOUND;
    *out = q->records[tail % WCM_MAX_INGRESS];
    atomic_store_explicit(&q->tail, tail + 1u, memory_order_release);
    return WCM_OK;
}

void wcm_ingress_clear(wcm_ingress_t *q) {
    if (!q) return;
    const unsigned head = atomic_load_explicit(&q->head, memory_order_acquire);
    atomic_store_explicit(&q->tail, head, memory_order_release);
}

uint32_t wcm_ingress_dropped(const wcm_ingress_t *q) {
    if (!q) return 0u;
    return (uint32_t)atomic_load_explicit(&q->dropped, memory_order_relaxed);
}

uint32_t wcm_ingress_count(const wcm_ingress_t *q) {
    if (!q) return 0u;
    const unsigned tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
    const unsigned head = atomic_load_explicit(&q->head, memory_order_acquire);
    return (uint32_t)queue_count(head, tail);
}

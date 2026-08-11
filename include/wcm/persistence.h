#ifndef WCM_PERSISTENCE_H
#define WCM_PERSISTENCE_H

#include "wcm/anchor.h"

typedef wcm_status_t (*wcm_persistence_read_fn)(
    void *user,
    uint8_t slot,
    void *buffer,
    size_t size);

typedef wcm_status_t (*wcm_persistence_write_fn)(
    void *user,
    uint8_t slot,
    const void *buffer,
    size_t size);

typedef wcm_status_t (*wcm_persistence_sync_fn)(void *user);

typedef struct {
    wcm_persistence_read_fn read;
    wcm_persistence_write_fn write;
    wcm_persistence_sync_fn sync;
    void *user;
} wcm_anchor_backend_t;

wcm_status_t wcm_anchor_backend_load(
    const wcm_anchor_backend_t *backend,
    wcm_anchor_slot_t *out);

wcm_status_t wcm_anchor_backend_commit(
    const wcm_anchor_backend_t *backend,
    const void *payload,
    uint16_t payload_size,
    uint64_t sequence);

#endif

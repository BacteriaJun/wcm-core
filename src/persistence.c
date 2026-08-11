#include "wcm/persistence.h"

#include <string.h>

static wcm_status_t read_slot(
    const wcm_anchor_backend_t *backend,
    uint8_t index,
    wcm_anchor_slot_t *slot) {
    uint8_t image[WCM_ANCHOR_IMAGE_BYTES];
    const wcm_status_t rc = backend->read(backend->user, index, image, sizeof(image));
    if (rc != WCM_OK) return rc;
    return wcm_anchor_decode(image, slot);
}

wcm_status_t wcm_anchor_backend_load(
    const wcm_anchor_backend_t *backend,
    wcm_anchor_slot_t *out) {
    if (!backend || !backend->read || !out) return WCM_ERR_ARG;

    wcm_anchor_slot_t a;
    wcm_anchor_slot_t b;
    const bool va = read_slot(backend, 0u, &a) == WCM_OK;
    const bool vb = read_slot(backend, 1u, &b) == WCM_OK;
    if (!va && !vb) return WCM_ERR_NOT_FOUND;

    const wcm_anchor_slot_t *latest = va && vb ? wcm_anchor_choose_latest(&a, &b) : (va ? &a : &b);
    if (!latest) return WCM_ERR_STATE;
    *out = *latest;
    return WCM_OK;
}

wcm_status_t wcm_anchor_backend_commit(
    const wcm_anchor_backend_t *backend,
    const void *payload,
    uint16_t payload_size,
    uint64_t sequence) {
    if (!backend || !backend->read || !backend->write || !payload) return WCM_ERR_ARG;

    wcm_anchor_slot_t a;
    wcm_anchor_slot_t b;
    const bool va = read_slot(backend, 0u, &a) == WCM_OK;
    const bool vb = read_slot(backend, 1u, &b) == WCM_OK;
    uint8_t target = 0u;
    if (va && vb) target = (uint8_t)(a.sequence <= b.sequence ? 0u : 1u);
    else if (va) target = 1u;

    wcm_anchor_slot_t slot;
    wcm_status_t rc = wcm_anchor_write(&slot, payload, payload_size, sequence);
    if (rc != WCM_OK) return rc;
    uint8_t image[WCM_ANCHOR_IMAGE_BYTES];
    rc = wcm_anchor_encode(&slot, image);
    if (rc != WCM_OK) return rc;

    rc = backend->write(backend->user, target, image, sizeof(image));
    if (rc != WCM_OK) return rc;
    if (backend->sync) {
        rc = backend->sync(backend->user);
        if (rc != WCM_OK) return rc;
    }

    wcm_anchor_slot_t verify;
    rc = read_slot(backend, target, &verify);
    if (rc != WCM_OK) return rc;
    if (verify.sequence != sequence || verify.payload_size != payload_size ||
        memcmp(verify.payload, payload, payload_size) != 0) {
        return WCM_ERR_IO;
    }
    return WCM_OK;
}

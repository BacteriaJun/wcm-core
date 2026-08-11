#ifndef WCM_ANCHOR_H
#define WCM_ANCHOR_H

#include "wcm/config.h"
#include "wcm/types.h"

#define WCM_ANCHOR_MAGIC UINT32_C(0x57434D41) /* WCMA */
#define WCM_ANCHOR_FORMAT 2u
#define WCM_ANCHOR_IMAGE_BYTES (20u + WCM_ANCHOR_PAYLOAD_SIZE)

typedef struct {
    uint32_t magic;
    uint16_t format_version;
    uint16_t payload_size;
    uint64_t sequence;
    uint32_t crc32;
    uint8_t payload[WCM_ANCHOR_PAYLOAD_SIZE];
} wcm_anchor_slot_t;

wcm_status_t wcm_anchor_write(
    wcm_anchor_slot_t *slot,
    const void *payload,
    uint16_t payload_size,
    uint64_t sequence);
bool wcm_anchor_validate(const wcm_anchor_slot_t *slot);
const wcm_anchor_slot_t *wcm_anchor_choose_latest(
    const wcm_anchor_slot_t *a,
    const wcm_anchor_slot_t *b);

wcm_status_t wcm_anchor_encode(
    const wcm_anchor_slot_t *slot,
    uint8_t image[WCM_ANCHOR_IMAGE_BYTES]);
wcm_status_t wcm_anchor_decode(
    const uint8_t image[WCM_ANCHOR_IMAGE_BYTES],
    wcm_anchor_slot_t *slot);

#endif

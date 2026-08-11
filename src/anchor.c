#include "wcm/anchor.h"

#include <string.h>

#define WCM_ANCHOR_OFF_MAGIC 0u
#define WCM_ANCHOR_OFF_FORMAT 4u
#define WCM_ANCHOR_OFF_PAYLOAD_SIZE 6u
#define WCM_ANCHOR_OFF_SEQUENCE 8u
#define WCM_ANCHOR_OFF_CRC 16u
#define WCM_ANCHOR_OFF_PAYLOAD 20u

static void put_u16_le(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)(value & UINT16_C(0xFF));
    p[1] = (uint8_t)((value >> 8u) & UINT16_C(0xFF));
}

static void put_u32_le(uint8_t *p, uint32_t value) {
    for (unsigned i = 0u; i < 4u; ++i) p[i] = (uint8_t)((value >> (i * 8u)) & UINT32_C(0xFF));
}

static void put_u64_le(uint8_t *p, uint64_t value) {
    for (unsigned i = 0u; i < 8u; ++i) p[i] = (uint8_t)((value >> (i * 8u)) & UINT64_C(0xFF));
}

static uint16_t get_u16_le(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8u));
}

static uint32_t get_u32_le(const uint8_t *p) {
    uint32_t value = 0u;
    for (unsigned i = 0u; i < 4u; ++i) value |= (uint32_t)p[i] << (i * 8u);
    return value;
}

static uint64_t get_u64_le(const uint8_t *p) {
    uint64_t value = 0u;
    for (unsigned i = 0u; i < 8u; ++i) value |= (uint64_t)p[i] << (i * 8u);
    return value;
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        crc ^= data[i];
        for (unsigned b = 0; b < 8u; ++b) {
            const uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
            crc = (crc >> 1u) ^ (UINT32_C(0xEDB88320) & mask);
        }
    }
    return crc;
}

static uint32_t image_crc(const uint8_t image[WCM_ANCHOR_IMAGE_BYTES], uint16_t payload_size) {
    uint32_t crc = UINT32_C(0xFFFFFFFF);
    crc = crc32_update(crc, image, WCM_ANCHOR_OFF_CRC);
    crc = crc32_update(crc, &image[WCM_ANCHOR_OFF_PAYLOAD], payload_size);
    return ~crc;
}

static void encode_without_crc(
    const wcm_anchor_slot_t *slot,
    uint8_t image[WCM_ANCHOR_IMAGE_BYTES]) {
    memset(image, 0, WCM_ANCHOR_IMAGE_BYTES);
    put_u32_le(&image[WCM_ANCHOR_OFF_MAGIC], slot->magic);
    put_u16_le(&image[WCM_ANCHOR_OFF_FORMAT], slot->format_version);
    put_u16_le(&image[WCM_ANCHOR_OFF_PAYLOAD_SIZE], slot->payload_size);
    put_u64_le(&image[WCM_ANCHOR_OFF_SEQUENCE], slot->sequence);
    if (slot->payload_size > 0u) {
        memcpy(&image[WCM_ANCHOR_OFF_PAYLOAD], slot->payload, slot->payload_size);
    }
}

wcm_status_t wcm_anchor_write(
    wcm_anchor_slot_t *slot,
    const void *payload,
    uint16_t payload_size,
    uint64_t sequence) {
    if (!slot || (!payload && payload_size != 0u)) return WCM_ERR_ARG;
    if (payload_size > WCM_ANCHOR_PAYLOAD_SIZE) return WCM_ERR_RANGE;
    memset(slot, 0, sizeof(*slot));
    slot->magic = WCM_ANCHOR_MAGIC;
    slot->format_version = WCM_ANCHOR_FORMAT;
    slot->payload_size = payload_size;
    slot->sequence = sequence;
    if (payload_size > 0u) memcpy(slot->payload, payload, payload_size);

    uint8_t image[WCM_ANCHOR_IMAGE_BYTES];
    encode_without_crc(slot, image);
    slot->crc32 = image_crc(image, slot->payload_size);
    return WCM_OK;
}

wcm_status_t wcm_anchor_encode(
    const wcm_anchor_slot_t *slot,
    uint8_t image[WCM_ANCHOR_IMAGE_BYTES]) {
    if (!slot || !image) return WCM_ERR_ARG;
    if (!wcm_anchor_validate(slot)) return WCM_ERR_STATE;
    encode_without_crc(slot, image);
    put_u32_le(&image[WCM_ANCHOR_OFF_CRC], slot->crc32);
    return WCM_OK;
}

wcm_status_t wcm_anchor_decode(
    const uint8_t image[WCM_ANCHOR_IMAGE_BYTES],
    wcm_anchor_slot_t *slot) {
    if (!image || !slot) return WCM_ERR_ARG;
    memset(slot, 0, sizeof(*slot));
    slot->magic = get_u32_le(&image[WCM_ANCHOR_OFF_MAGIC]);
    slot->format_version = get_u16_le(&image[WCM_ANCHOR_OFF_FORMAT]);
    slot->payload_size = get_u16_le(&image[WCM_ANCHOR_OFF_PAYLOAD_SIZE]);
    slot->sequence = get_u64_le(&image[WCM_ANCHOR_OFF_SEQUENCE]);
    slot->crc32 = get_u32_le(&image[WCM_ANCHOR_OFF_CRC]);
    if (slot->payload_size > WCM_ANCHOR_PAYLOAD_SIZE) return WCM_ERR_RANGE;
    if (slot->payload_size > 0u) {
        memcpy(slot->payload, &image[WCM_ANCHOR_OFF_PAYLOAD], slot->payload_size);
    }
    return wcm_anchor_validate(slot) ? WCM_OK : WCM_ERR_STATE;
}

bool wcm_anchor_validate(const wcm_anchor_slot_t *slot) {
    if (!slot) return false;
    if (slot->magic != WCM_ANCHOR_MAGIC || slot->format_version != WCM_ANCHOR_FORMAT) return false;
    if (slot->payload_size > WCM_ANCHOR_PAYLOAD_SIZE) return false;
    uint8_t image[WCM_ANCHOR_IMAGE_BYTES];
    encode_without_crc(slot, image);
    return slot->crc32 == image_crc(image, slot->payload_size);
}

const wcm_anchor_slot_t *wcm_anchor_choose_latest(
    const wcm_anchor_slot_t *a,
    const wcm_anchor_slot_t *b) {
    const bool av = wcm_anchor_validate(a);
    const bool bv = wcm_anchor_validate(b);
    if (!av && !bv) return NULL;
    if (av && !bv) return a;
    if (!av && bv) return b;
    return a->sequence >= b->sequence ? a : b;
}

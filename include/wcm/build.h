#ifndef WCM_BUILD_H
#define WCM_BUILD_H

#include "wcm/types.h"
#include "wcm/version.h"

#define WCM_CONFIG_FINGERPRINT_VERSION 1u

struct wcm_runtime_config;

typedef struct {
    uint16_t abi_version;
    uint16_t config_fingerprint_version;
    uint16_t anchor_format_version;
    uint8_t profile;
    uint8_t pointer_bits;
    uint32_t runtime_storage_bytes;
    const char *version;
    const char *build_tag;
} wcm_build_info_t;

void wcm_get_build_info(wcm_build_info_t *out);
uint64_t wcm_runtime_config_fingerprint(const struct wcm_runtime_config *config);

#endif

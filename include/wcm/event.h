#ifndef WCM_EVENT_H
#define WCM_EVENT_H

#include "wcm/types.h"
#include "wcm/world.h"

typedef enum {
    WCM_EVENT_RUNTIME_READY = 1,
    WCM_EVENT_RUNTIME_STOPPED,
    WCM_EVENT_WORLD_BREAK,
    WCM_EVENT_FAULT,
    WCM_EVENT_CAPABILITY_BOUND,
    WCM_EVENT_CAPABILITY_UNBOUND,
    WCM_EVENT_INGRESS_BACKLOG,
} wcm_event_type_t;

typedef struct {
    uint64_t sequence;
    wcm_time_t at_us;
    wcm_counter_t world_epoch;
    uint64_t deployment_id;
    uint32_t config_revision;
    uint32_t detail;
    uint16_t subject_id;
    int16_t status;
    uint8_t type;
    uint8_t reserved[7];
} wcm_event_record_t;

const char *wcm_event_type_string(wcm_event_type_t type);

#endif

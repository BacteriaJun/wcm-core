#ifndef WCM_TYPES_H
#define WCM_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint64_t wcm_time_t;
typedef uint64_t wcm_counter_t;
typedef uint64_t wcm_capability_set_t;
typedef uint64_t wcm_actuator_set_t;

typedef union {
    uint8_t u8[16];
    int8_t i8[16];
    uint16_t u16[8];
    int16_t i16[8];
    uint32_t u32[4];
    int32_t i32[4];
    uint64_t u64[2];
    int64_t i64[2];
    float f32[4];
    double f64[2];
} wcm_value_t;

typedef enum {
    WCM_OK = 0,
    WCM_ERR_ARG = -1,
    WCM_ERR_RANGE = -2,
    WCM_ERR_STATE = -3,
    WCM_ERR_FULL = -4,
    WCM_ERR_NOT_FOUND = -5,
    WCM_ERR_GRAPH = -6,
    WCM_ERR_IO = -7,
    WCM_ERR_QUALITY = -8,
    WCM_ERR_TIMESTAMP = -9,
    WCM_ERR_POLICY = -10,
    WCM_ERR_COUNTER = -11,
    WCM_ERR_BUSY = -12,
    WCM_ERR_CLOCK = -13,
    WCM_ERR_WCET = -14,
    WCM_ERR_ABI = -15,
    WCM_ERR_STOPPED = -16,
    WCM_ERR_BACKLOG = -17,
    WCM_ERR_GAP = -18,
} wcm_status_t;

static inline wcm_capability_set_t wcm_capability_bit(uint16_t id) {
    return id < 64u ? (UINT64_C(1) << id) : UINT64_C(0);
}

static inline wcm_actuator_set_t wcm_actuator_bit(uint16_t id) {
    return id < 64u ? (UINT64_C(1) << id) : UINT64_C(0);
}

#endif

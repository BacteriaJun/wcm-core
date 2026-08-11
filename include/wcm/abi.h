#ifndef WCM_ABI_H
#define WCM_ABI_H

#include "wcm/dependency.h"
#include "wcm/intent.h"
#include "wcm/snapshot.h"
#include "wcm/witness.h"

/* Scalar widths are part of the source-level contract. Aggregate padding is target ABI specific. */
_Static_assert(sizeof(wcm_time_t) == 8u, "wcm_time_t must be 64-bit");
_Static_assert(sizeof(wcm_counter_t) == 8u, "wcm_counter_t must be 64-bit");
_Static_assert(sizeof(wcm_capability_set_t) == 8u, "capability set must be 64-bit");
_Static_assert(sizeof(wcm_actuator_set_t) == 8u, "actuator set must be 64-bit");
_Static_assert(sizeof(wcm_value_t) == 16u, "wcm_value_t must remain 16 bytes");

#endif

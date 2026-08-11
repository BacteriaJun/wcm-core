#ifndef WCM_VERSION_H
#define WCM_VERSION_H

#include <stdint.h>

#define WCM_VERSION_MAJOR 1
#define WCM_VERSION_MINOR 1
#define WCM_VERSION_PATCH 0
#define WCM_VERSION_STRING "1.1.0"

/* Increment when public structure/layout contracts change incompatibly. */
#define WCM_ABI_VERSION UINT16_C(0x0101)

#endif

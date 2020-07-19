/*
 * Copyright (c) 2012 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * $ATH_LICENSE_TARGET_C$
 */

/*
 * Data-types used in rate-control on host.
 */

#ifndef _RATECTRL_TYPES_H_
#define _RATECTRL_TYPES_H_

#include <a_types.h>
#include <a_osapi.h>

#define A_MILLISECONDS()      CONVERT_SYSTEM_TIME_TO_MS((OS_GET_TICKS()))

#if 0
#ifndef A_RATEMASK
#define A_RATEMASK            A_UINT32
#endif
#endif

#define A_RSSI                A_UINT8 //Verify, not u

#ifndef WIN32
#ifndef A_MAX
#define A_MAX(_x, _y) \
        ({ __typeof__ (_x) __x = (_x); __typeof__ (_y) __y = (_y); __x > __y ? __x : __y; })
#endif
#endif

#ifdef WIN32
 #define A_MIN(_x, _y) ((_x) < (_y) ? (_x) : (_y))
#else
#define A_MIN(_x, _y) \
        ({ __typeof__ (_x) __x = (_x); __typeof__ (_y) __y = (_y); __x < __y ? __x : __y; })
#endif

#endif /* _RATECTRL_TYPES_H_ */

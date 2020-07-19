/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

                       QuIPC Common Types Used In OS Abstraction Layer

GENERAL DESCRIPTION
  This file contains the definition of common types that are used by various
  QuIPC modules.

Copyright (c) 2010, 2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/
#ifndef __OSAL_COMMON_TYPES_H
#define __OSAL_COMMON_TYPES_H

/*--------------------------------------------------------------------------
 * Include Files
 * -----------------------------------------------------------------------*/
#define __STDC_FORMAT_MACROS
#include <ctype.h>
#include <inttypes.h>
#include <time.h>

#include <stdint.h>

/*--------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -----------------------------------------------------------------------*/

#define UUID_LENGTH 16

/* MAC ID size in bytes. */
#define WIFI_MAC_ID_SIZE 6

/* ------------------------------------------------------------------------
** Constants
** ------------------------------------------------------------------------ */

#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif

#define TRUE   1   /* Boolean true value. */
#define FALSE  0   /* Boolean false value. */

#ifndef __ANDROID__
#define ALOGE printf
#define ALOGW printf
#define ALOGI printf
#define ALOGD printf
#define ALOGV printf
#endif


/*********************** BEGIN PACK() Definition ***************************/
#if defined __GNUC__
#define PACK(x)       x __attribute__((__packed__))
#elif defined __arm
#define PACK(x)       __packed x
#else
#error No PACK() macro defined for this compiler
#endif
/********************** END PACK() Definition *****************************/

/*--------------------------------------------------------------------------
 * Type Declarations
 * -----------------------------------------------------------------------*/

typedef  uint8_t            boolean;     /* Boolean value type. */
typedef  uint32_t           uint32;      /* Unsigned 32 bit value */
typedef  uint16_t           uint16;      /* Unsigned 16 bit value */
typedef  uint8_t            uint8;       /* Unsigned 8  bit value */
typedef  int32_t            int32;       /* Signed 32 bit value */
typedef  int16_t            int16;       /* Signed 16 bit value */
typedef  int8_t             int8;        /* Signed 8  bit value */
typedef  int64_t            int64;       /* Signed 64 bit value */
typedef  uint64_t           uint64;      /* Unsigned 64 bit value */

#endif /* __OSAL_COMMON_TYPES_H */

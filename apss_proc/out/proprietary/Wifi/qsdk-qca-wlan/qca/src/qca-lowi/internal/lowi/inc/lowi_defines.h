#ifndef __LOWI_DEFINES_H__
#define __LOWI_DEFINES_H__

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Defines Interface Header file

GENERAL DESCRIPTION
  This file contains the structure definitions and function prototypes for
  LOWIDefines

Copyright (c) 2012 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

Copyright (c) 2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/

#include <ctype.h>
#include <inttypes.h>
#include <stdint.h>
#include <osal/commontypes.h>
typedef double DOUBLE;
typedef float FLOAT;
typedef long long INT64;
typedef unsigned long long UINT64;
typedef int INT32;
typedef unsigned int UINT32;
typedef short int INT16;
typedef unsigned short int UINT16;
typedef char INT8;
typedef unsigned char UINT8;
typedef bool BOOL;

// This is here because for some reason strlcat and strlcpy
// are not available on Ubuntu but on Android
#ifndef __ANDROID__
#include <base_util/string_routines.h>
#endif

// Config file
#define CONFIG_NAME "/etc/lowi/lowi.conf"

#endif //#ifndef __LOWI_DEFINES_H__

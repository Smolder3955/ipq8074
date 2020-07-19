/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

                  QUIPC modules common include files

GENERAL DESCRIPTION
  This file contains the common include files for all QUIPC modules.

Copyright (c) 2010, 2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/
#ifndef QUIPC_OSAL_COMMON_INC_H
#define QUIPC_OSAL_COMMON_INC_H

#ifdef __cplusplus
extern "C"
{
#endif

// QUIPC common internal header files
#include "commontypes.h"
#include "os_api.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

#ifdef __cplusplus
}
#endif
#endif // OSAL_COMMON_INC_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __LOWI_TIME_H__
#define __LOWI_TIME_H__

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Time module

GENERAL DESCRIPTION
  This file contains the declaration and some global constants for Time
  module.

Copyright (c) 2013 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/
#include <stdint.h>
#include <osal/common_inc.h>

/**
 * Initializes lowi time module.
 * Should only be called once.
 * returns 0 for success, err number for failure
 */
int lowi_time_init();

/**
 * Closes lowi time module.
 * Should only be called once.
 * returns 0 for success, err number for failure
 */
int lowi_time_close();

/**
 * Returns the time from android boot.
 * Uses android boot clock which some times jump backwards
 * To deal with such issues, the time that's returned last time
 * is maintained and returned if the current time jump backwards.
 *
 * @returns Time from android boot clock
 */
uint64 lowi_get_time_from_boot();

#endif // __LOWI_TIME_H__

#ifdef __cplusplus
}
#endif

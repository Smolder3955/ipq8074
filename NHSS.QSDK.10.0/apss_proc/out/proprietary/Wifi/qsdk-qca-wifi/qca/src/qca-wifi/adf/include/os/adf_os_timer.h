/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

/**
 * @ingroup adf_os_public
 * @file adf_os_timer.h
 * This file abstracts OS timers.
 */

#ifndef _ADF_OS_TIMER_H
#define _ADF_OS_TIMER_H

#include <adf_os_types.h>
#include <adf_os_timer_pvt.h>


/**
 * @brief Platform timer object
 */
typedef __adf_os_timer_t           adf_os_timer_t;


/**
 * @brief Initialize a timer
 * 
 * @param[in] hdl       OS handle
 * @param[in] timer     timer object pointer
 * @param[in] func      timer function
 * @param[in] context   context of timer function
 */
static inline void
adf_os_timer_init(adf_os_handle_t      hdl,
                  adf_os_timer_t      *timer,
                  adf_os_timer_func_t  func,
                  void                *arg)
{
    __adf_os_timer_init(hdl, timer, func, arg);
}

/**
 * @brief Start a one-shot timer
 * 
 * @param[in] timer     timer object pointer
 * @param[in] msec      expiration period in milliseconds
 */
static inline void
adf_os_timer_start(adf_os_timer_t *timer, int msec)
{
    __adf_os_timer_start(timer, msec);
}

/**
 * @brief Modify existing timer to new timeout value
 * 
 * @param[in] timer     timer object pointer
 * @param[in] msec      expiration period in milliseconds
 */
static inline void
adf_os_timer_mod(adf_os_timer_t *timer, int msec)
{
    __adf_os_timer_mod(timer, msec);
}

/**
 * @brief Cancel a timer
 * The function will return after any running timer completes.
 *
 * @param[in] timer     timer object pointer
 * 
 * @retval    TRUE      timer was cancelled and deactived
 * @retval    FALSE     timer was cancelled but already got fired.
 */
static inline a_bool_t
adf_os_timer_cancel(adf_os_timer_t *timer)
{
    return __adf_os_timer_cancel(timer);
}

/**
 * @brief Cancel a timer synchronously
 * The function will return after any running timer completes.
 *
 * @param[in] timer     timer object pointer
 * 
 * @retval    TRUE      timer was cancelled and deactived
 * @retval    FALSE     timer was not cancelled
 */
static inline a_bool_t
adf_os_timer_sync_cancel(adf_os_timer_t *timer)
{
    return __adf_os_timer_sync_cancel(timer);
}

/**
 * @brief Free a timer
 * The function will return after any running timer completes.
 *
 * @param[in] timer     timer object pointer
 * 
 */
static inline void
adf_os_timer_free(adf_os_timer_t *timer)
{
    __adf_os_timer_free(timer);
}

#endif


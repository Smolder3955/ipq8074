/*
* Copyright (c) 2013, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef __ADF_OS_WAITQ_PVT_H
#define __ADF_OS_WAITQ_PVT_H

#include <linux/wait.h>

typedef struct __adf_os_waitq {
    wait_queue_head_t    waitq;
    volatile int         cond;
} __adf_os_waitq_t;

/**
 * @brief Initialize the waitq
 * 
 * @param wq
 */
static inline void
__adf_os_init_waitq(__adf_os_waitq_t *wq)
{
    /* Not used in Linux */
    return;
}

/**
 * @brief Sleep on the waitq, the thread is woken up if somebody
 *        wakes it or the timeout occurs, whichever is earlier
 * @Note  Locks should be taken by the driver
 * 
 * @param wq
 * @param timeout
 * 
 * @return a_status_t
 */
static inline a_status_t
__adf_os_sleep_waitq(__adf_os_waitq_t *wq, uint32_t timeout)
{
    /* Not used in Linux */
    return 0;
}

/**
 * @brief Wake the first guy sleeping in the queue
 * 
 * @param wq
 * 
 * @return a_status_t
 */
static inline a_status_t
__adf_os_wake_waitq(__adf_os_waitq_t *wq)
{
    /* Not used in Linux */
    return A_STATUS_OK;
}

static inline a_status_t
__adf_os_reset_waitq(__adf_os_waitq_t *wq)
{
    /* Not used in Linux */
    return A_STATUS_OK;
}

#endif

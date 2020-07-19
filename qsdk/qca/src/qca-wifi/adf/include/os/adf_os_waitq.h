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

#ifndef __ADF_OS_WAITQ_H
#define __ADF_OS_WAITQ_H

#include <adf_os_waitq_pvt.h>

typedef __adf_os_waitq_t      adf_os_waitq_t;

/**
 * @brief Initialize the waitq
 * 
 * @param wq
 */
static inline void
adf_os_init_waitq(adf_os_waitq_t *wq)
{
    __adf_os_init_waitq(wq);
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
adf_os_sleep_waitq(adf_os_waitq_t *wq, a_uint32_t timeout)
{
    return __adf_os_sleep_waitq(wq, timeout);
}
/**
 * @brief Wake the first guy sleeping in the queue
 * 
 * @param wq
 * 
 * @return a_status_t
 */
static inline a_status_t
adf_os_wake_waitq(adf_os_waitq_t *wq)
{
    return __adf_os_wake_waitq(wq);
}


#endif

/*
 * Copyright (c) 2009, Atheros Communications Inc.
 * All Rights Reserved.
 *
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 *
 *  Implementation of Timer module.
 */

#include "ath_internal.h"
#include "ath_timer.h"

#define TIMER_SIGNATURE    0xABCD9876

static
OS_TIMER_FUNC(_ath_internal_timer_handler)
{
    struct ath_timer    *timer_object;

    OS_GET_TIMER_ARG(timer_object, struct ath_timer *);

    if (!timer_object->active_flag) {
        printk("%s: Timer is not active!! Investigate!!\n", __func__);
        return;
    }

    if (timer_object->timer_handler(timer_object->context) == 0) {
        // rearm timer only if handler function returned 0
        OS_SET_TIMER(&timer_object->os_timer, timer_object->timer_period);
    } else {
        // In case cancel_timer is not called.
        timer_object->active_flag = 0;
    }
}

u_int8_t ath_initialize_timer_module (osdev_t sc_osdev)
{
    UNREFERENCED_PARAMETER(sc_osdev);

    return 1;
}

u_int8_t ath_initialize_timer_int (osdev_t                osdev,
                               struct ath_timer*      timer_object,
                               u_int32_t              timer_period,
                               timer_handler_function timer_handler,
                               void*                  context)
{
    ASSERT(timer_object != NULL);
    ASSERT(osdev != NULL);

    if (timer_object != NULL && timer_handler != NULL) {
        OS_INIT_TIMER(osdev, &timer_object->os_timer, _ath_internal_timer_handler,
                timer_object, QDF_TIMER_TYPE_WAKE_APPS);

        timer_object->timer_period  = timer_period;
        timer_object->context       = context;
        timer_object->timer_handler = timer_handler;
        timer_object->signature     = TIMER_SIGNATURE;

        return 1;
    }

    return 0;
}

void ath_set_timer_period (struct ath_timer* timer_object, u_int32_t timer_period)
{
    timer_object->timer_period  = timer_period;
}

u_int8_t ath_start_timer (struct ath_timer* timer_object)
{
    // arm timer for the first time
    timer_object->active_flag = 1;
    OS_SET_TIMER(&timer_object->os_timer, timer_object->timer_period);

    return 1;
}

/*
 * ath_cancelTimer: Argument busywait_flag indicates whether the
 * delay function used by _ath_internal_cancelTimer will perform a
 * "busy wait" or relinquish access to the CPU. The former can
 * be called at IRQL <= DISPATCH_LEVEL, while the latter is only
 * valid at IRQL < DISPATCH_LEVEL.
 */
u_int8_t ath_cancel_timer (struct ath_timer* timer_object, enum timer_flags flags)
{
    u_int8_t    canceled = 0;

    if (OS_CANCEL_TIMER(&timer_object->os_timer)) {
        timer_object->active_flag = 0;
        canceled = 1;
    }

    return canceled;
}

u_int8_t ath_timer_is_active (struct ath_timer* timer_object)
{
    return (timer_object->active_flag);
}

u_int8_t ath_timer_is_initialized (struct ath_timer* timer_object)
{
    return (timer_object->signature == TIMER_SIGNATURE);
}

void ath_free_timer_int(struct ath_timer* timer_object)
{
    if(timer_object && ath_timer_is_initialized(timer_object)) {
        OS_FREE_TIMER(&timer_object->os_timer);
        timer_object->active_flag   = 0;
        timer_object->timer_period  = 0;
        timer_object->context       = NULL;
        timer_object->timer_handler = NULL;
        timer_object->signature     = 0;
    }
}


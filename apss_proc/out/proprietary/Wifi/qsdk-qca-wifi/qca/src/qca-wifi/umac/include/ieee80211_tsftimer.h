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

#ifndef _IEEE80211_TSFTIMER_H_
#define _IEEE80211_TSFTIMER_H_

#include <ieee80211_options.h>

/*
 * Some hardware can support multiple TSF Timers. For parameter will specific which
 * TSF Clock to use. For now, just use 0.
 */
typedef int tsftimer_clk_id;

struct ieee80211_tsf_timer_mgr;
typedef struct ieee80211_tsf_timer_mgr *ieee80211_tsf_timer_mgr_t;

typedef void (*ieee80211_tsf_timer_function)(void *arg);


/*
 * This TSF Timer module allows the VAP to schedule timer events based on the current TSF
 * clock. The callback functions are done at DPC level. The timer events can be single-shot or
 * periodic.
 * Note that the TSF clock can be changed due to the Infra BSS AP beacons or internal resets.
 * When the TSF has changed, the callback function, fp_tsf_changed, will be called and the
 * vaps should cancel all timers and reschedule them based on the new TSF clock.
 */
struct tsftimer;
typedef struct tsftimer *tsftimer_handle_t;

typedef void (*tsftimer_function)(tsftimer_handle_t h_tsftime, int arg1, void *arg2, u_int32_t curr_tsf);

/* Note that we must use signed int32 operators to compare. We need the wraparound arithmetic */
#define TSFTIME_IS_GREATER(a, b)        ((int32_t)(a - b) > 0)
#define TSFTIME_IS_GREATER_EQ(a, b)     ((int32_t)(a - b) >= 0)
#define TSFTIME_IS_SMALLER(a, b)        ((int32_t)(a - b) < 0)
#define TSFTIME_IS_SMALLER_EQ(a, b)     ((int32_t)(a - b) <= 0)
#define TSFTIME_MAX(a, b)               (TSFTIME_IS_GREATER((a), (b))? a : b)

#if UMAC_SUPPORT_TSF_TIMER
/*
 * Functional Prototype for callback function from TSF Timer.
 */

/*
 * To allocate a TSF Timer. Returns the Handle of the timer if successful. Else return NULL if error.
 * Note: for hardware that can support multiple TSF Timer, the TSF value is the one associated to this
 * vap.
 */
tsftimer_handle_t ieee80211_tsftimer_alloc(
    ieee80211_tsf_timer_mgr_t   tsftimermgr,
    tsftimer_clk_id             clk_id,         /* For multiple TSF Timer clock domain - unsupported for now */
    tsftimer_function           fp_timeout,
    int                         arg1,           /* Timer Event ID */
    void                        *arg2,          /* Pointer to argument 2 */
    tsftimer_function           fp_tsf_changed  /* optional and can be null. Callback when TSF is changed and timer has started. */
    );

/*
 * To start TSF Timer. Returns 0 if success and error code if fail. If this is single shot
 * timer event, then period parameter is 0. If period parameter is non zero, then this timer
 * is re-armed itself to fire regularly.
 */
int ieee80211_tsftimer_start(
    tsftimer_handle_t           h_tsftime,
    u_int32_t                   next_tsf,   /* Next TSF value when this timer fires. */
    u_int32_t                   period      /* Time for the next Period. 0 if single shot */
    );

/*
 * To stop TSF Timer. Returns 0 if success and error code if fail.
 */
int ieee80211_tsftimer_stop(
    tsftimer_handle_t           h_tsftime
    );

/*
 * To free a previously allocated TSF Timer. 
 * The can_block parameter is used to indicate that this routine will block and
 * wait till any active timers are completed.
 * Returns 0 if success and 1 if the timer is
 * currently active and can_block==true.
 * Otherwise, return error code if fail.
 */
int ieee80211_tsftimer_free(
    tsftimer_handle_t           h_tsftime,
    bool                        can_block
    );

/*
 * Create Tsf Timer manager.
 */
ieee80211_tsf_timer_mgr_t ieee80211_tsf_timer_attach(struct ieee80211com *ic);

/*
 * Delete Tsf Timer manager.
 */
void ieee80211_tsf_timer_detach(ieee80211_tsf_timer_mgr_t tsftimermgr);

#else

static INLINE tsftimer_handle_t ieee80211_tsftimer_alloc(
    ieee80211_tsf_timer_mgr_t   tsftimermgr,
    tsftimer_clk_id             clk_id,         /* For multiple TSF Timer clock domain - unsupported for now */
    tsftimer_function           fp_timeout,
    int                         arg1,           /* Timer Event ID */
    void                        *arg2,          /* Pointer to argument 2 */
    tsftimer_function           fp_tsf_changed  /* optional and can be null. Callback when TSF is changed and timer has started. */
    ) 
{
   return NULL;

}
#define ieee80211_tsftimer_start(h,n,p)       EINVAL 
#define ieee80211_tsftimer_stop(h)            EINVAL
#define ieee80211_tsftimer_free(h,c)          EINVAL
#define ieee80211_tsf_timer_attach(ic)        NULL
#define ieee80211_tsf_timer_detach(handle)   /**/

#endif  //if UMAC_SUPPORT_TSF_TIMER

#endif  //ifdef _IEEE80211_TSFTIMER_H_

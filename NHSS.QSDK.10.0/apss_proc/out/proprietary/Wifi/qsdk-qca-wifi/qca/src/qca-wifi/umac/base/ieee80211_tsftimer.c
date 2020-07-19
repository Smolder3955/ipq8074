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

#include <ieee80211_var.h>
#include <ieee80211_tsftimer.h>

#if UMAC_SUPPORT_TSF_TIMER

/* We ran out of bits for the ic->ic_debug field. For now, use the "dump 802.1x keys" module. */
#define IEEE80211_MSG_TSFTIMER  IEEE80211_MSG_RADKEYS

/*
 * This TSF Timer module allows the VAP to schedule timer events based on the current TSF
 * clock. The callback functions are done at DPC level. The timer events can be single-shot or
 * periodic.
 * Note that the TSF clock can be changed due to the Infra BSS AP beacons or internal resets.
 * When the TSF has changed, the callback function, fp_tsf_changed, will be called and the
 * vaps should cancel all timers and reschedule them based on the new TSF clock.
 */

#define TSF_ID_0            0       /* For Osprey, we support 2 TSF clocks. For now, only 1 */

/*
 * Note: we will lose the timer if we set a time that is in the past. 
 * This is the default period that is programmed in the hardware. 
 * If the trigger time plus this period has already passed, then the timer hw
 * will trigger the "overflow" interrupt at the next period.
 */
#define TSF_DEFAULT_PERIOD          200

/* Private structure to store the TSF Timer Manager info. Pointer resides in struct ieee80211com */
struct ieee80211_tsf_timer_mgr {
    struct ieee80211com         *ic;
    struct ieee80211_tsf_timer  *ath_timer_handle;
    bool                        ath_timer_started;
    int                         num_alloc_timers;
    int                         num_active_timers;
    u_int32_t                   active_callbks;
    ieee80211_tsf_timer_lock_t  lock;

    /* Queue for all allocated timer */
    ATH_LIST_HEAD(, tsftimer)   alloc_q;

    /* Queue for active timers with scheduled timeouts */
    ATH_LIST_HEAD(, tsftimer)   active_q;

};

#define CALLBK_TIMEOUT      (1<<0)
#define CALLBK_TSF_CHG      (1<<1)

/* Private structure to store the TSF Timer info */
struct tsftimer {
    ieee80211_tsf_timer_mgr_t   h_tsftimermgr;  /* Timer Manager */
    bool                        active;
    u_int32_t                   triggering_bm;  /* Bitmap for Callback in progress */
    bool                        dead;           /* Timer is freed and can be deleted. */
    u_int32_t                   timeout;        /* Scheduled Timeout */
    
    tsftimer_function           fp_timeout;
    int                         arg1;           /* Timer Event ID */
    void                        *arg2;          /* Pointer to argument 2 */
    tsftimer_function           fp_tsf_changed; /* optional and can be null. Callback when TSF is changed and timer has started. */
    
    LIST_ENTRY(tsftimer)        alloc_list;     /* link to Alloc Timers List */
    LIST_ENTRY(tsftimer)        active_list;    /* link to Active Timers List */
};

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
    tsftimer_function           fp_tsf_changed  /* optional and can be null. Callback when TSF is changed. Timer need not have started. */
    )
{
    tsftimer_handle_t           tsftimer;
    struct ieee80211com         *ic;

    if (tsftimermgr == NULL) {
        return NULL;
    }
    ic = tsftimermgr->ic;
    ASSERT(ic);

    /* Allocate ResMgr data structures */
    tsftimer = (tsftimer_handle_t) OS_MALLOC(ic->ic_osdev, 
                                            sizeof(struct tsftimer),
                                            0);
    if (tsftimer == NULL) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_TSFTIMER,
                             "%s : Vap Tsf Timer memory alloction failed\n", __func__);
        return NULL;
    }

    OS_MEMZERO(tsftimer, sizeof(struct tsftimer));

    tsftimer->h_tsftimermgr = tsftimermgr;
    tsftimer->active = FALSE;
    tsftimer->triggering_bm = 0;
    tsftimer->dead = FALSE;

    ASSERT(fp_timeout);
    tsftimer->fp_timeout = fp_timeout;
    tsftimer->arg1 = arg1;
    tsftimer->arg2 = arg2;
    tsftimer->fp_tsf_changed = fp_tsf_changed;

    IEEE80211_TSF_TIMER_LOCK(tsftimermgr);

    /* Add this new timer to the allocated list */
    LIST_INSERT_HEAD(&(tsftimermgr->alloc_q), tsftimer, alloc_list);

    tsftimermgr->num_alloc_timers++;

    IEEE80211_TSF_TIMER_UNLOCK(tsftimermgr);

    return tsftimer;
}

/*
 * This routine will add this timer to Active List in increasing time order.
 * We assumed that the tsftimermgr->lock is held before calling this function.
 */
static int
add_timer_ordered(
    ieee80211_tsf_timer_mgr_t   tsftimermgr, 
    tsftimer_handle_t           new_tsftime, 
    u_int32_t                   next_tsf)
{
    new_tsftime->active = TRUE;
    new_tsftime->timeout = next_tsf;
    tsftimermgr->num_active_timers++;

    if (LIST_EMPTY(&(tsftimermgr->active_q))) {
        // Empty list and we will insert at the head of list.
        LIST_INSERT_HEAD(&(tsftimermgr->active_q), new_tsftime, active_list);
    }
    else {
        tsftimer_handle_t           tsftime = NULL;
        tsftimer_handle_t           prev_tsftime = NULL;

        LIST_FOREACH(tsftime, &(tsftimermgr->active_q), active_list) {
            u_int32_t           diff;
    
            ASSERT(tsftime->active == TRUE);

            // Keep a copy of the previous timer.
            prev_tsftime = tsftime;
    
            diff = next_tsf - tsftime->timeout;
    
            if (((int32_t)diff) <= 0) {
                /* Insert our timer here */
    
                LIST_INSERT_BEFORE(tsftime, new_tsftime, active_list);
    
                return 1;
            }
        }

        // Insert at the end of list.
        ASSERT(prev_tsftime != NULL);
        LIST_INSERT_AFTER(prev_tsftime, new_tsftime, active_list);
    }

    return 0;
}

/*
 * To start TSF Timer. Returns 0 if success and error code if fail. If this is single shot
 * timer event, then period parameter is 0. If period parameter is non zero, then this timer
 * is re-armed itself to fire regularly.
 */
int ieee80211_tsftimer_start(
    tsftimer_handle_t           h_tsftime,
    u_int32_t                   next_tsf,   /* Next TSF value when this timer fires. */
    u_int32_t                   period      /* Time for the next Period. 0 if single shot */
    )
{
    struct ieee80211com         *ic;
    ieee80211_tsf_timer_mgr_t   tsftimermgr;

    if (h_tsftime == NULL) {
        printk("%s: Error: NULL handle.\n", __func__);
        return EINVAL;
    }

    ASSERT(h_tsftime->h_tsftimermgr != NULL);
    tsftimermgr = h_tsftime->h_tsftimermgr;
    ic = tsftimermgr->ic;

    ASSERT(ic->ic_tsf_timer == tsftimermgr);

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_DETAILED, IEEE80211_MSG_TSFTIMER,
                         "%s: handle=0x%x, next_tsf=0x%x, period=0x%x\n",
                         __func__, h_tsftime, next_tsf, period);

    IEEE80211_TSF_TIMER_LOCK(tsftimermgr);

    if (h_tsftime->dead) {
        /* Timer is already slated for deletion. */
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_TSFTIMER,
                             "%s: Error: tsftimer 0x%x is slated for deletion.\n",
                             __func__, h_tsftime);
        IEEE80211_TSF_TIMER_UNLOCK(tsftimermgr);
        return -1;
    }

    if (h_tsftime->active) {
        /* Timer is already active, removed from active list. */
        LIST_REMOVE(h_tsftime, active_list); 
        h_tsftime->active = FALSE;
        tsftimermgr->num_active_timers--;
    }
    /* Added to active list but with ordered in increasing time. */
    add_timer_ordered(tsftimermgr, h_tsftime, next_tsf);

    if ((LIST_FIRST(&(tsftimermgr->active_q)) == h_tsftime) &&
        ((tsftimermgr->active_callbks & CALLBK_TIMEOUT) == 0))
    {
        /* 
         * Our timer is the head of the list. We will need to restart the
         * hw timer.
         */
         
        /* For USB split driver, to reduce time critical issue, remove unnecessary code. */
#ifndef ATH_USB
        u_int32_t curr_tsf;

        ic->ic_tsf_timer_stop(ic, tsftimermgr->ath_timer_handle);

        curr_tsf = ic->ic_get_TSF32(ic);
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_TSFTIMER,
                             "%s: rearm hw timer to 0x%x, curr=0x%x\n",
                             __func__, h_tsftime->timeout, curr_tsf);
#else
        ic->ic_tsf_timer_stop(ic, tsftimermgr->ath_timer_handle);
#endif
        ic->ic_tsf_timer_start(ic, tsftimermgr->ath_timer_handle, next_tsf, TSF_DEFAULT_PERIOD);
    }
    
    IEEE80211_TSF_TIMER_UNLOCK(tsftimermgr);

    return 0;
}

/*
 * To stop TSF Timer. Returns 0 if success and error code if fail.
 */
int ieee80211_tsftimer_stop(
    tsftimer_handle_t           h_tsftime
    )
{
    struct ieee80211com         *ic;
    ieee80211_tsf_timer_mgr_t   tsftimermgr;

    if (h_tsftime == NULL) {
        printk("%s: Error: NULL handle.\n", __func__);
        return EINVAL;
    }

    ASSERT(h_tsftime->h_tsftimermgr != NULL);
    tsftimermgr = h_tsftime->h_tsftimermgr;
    ic = tsftimermgr->ic;

    IEEE80211_TSF_TIMER_LOCK(tsftimermgr);

    if (h_tsftime->active) {
        bool        restart_hw_timer = false;

        /* If we are the head of active list, then restart hw timer */
        if (LIST_FIRST(&(tsftimermgr->active_q)) == h_tsftime) {
            /* Stop the hw timer first */
            /* Note: if we are the last (and only and first) one, then we also stop the timer here */
            ic->ic_tsf_timer_stop(ic, tsftimermgr->ath_timer_handle);
            restart_hw_timer = true;
        }

        LIST_REMOVE(h_tsftime, active_list); 
        h_tsftime->active = FALSE;
        tsftimermgr->num_active_timers--;

        /* 
         * We do not want to re-start the timer during the middle of the Timeout 
         * callback (flag CALLBK_TIMEOUT) because it will done at the end of callback.
         */
        if ((tsftimermgr->num_active_timers > 0) && restart_hw_timer &&
            ((tsftimermgr->active_callbks & CALLBK_TIMEOUT) == 0))
        {
            tsftimer_handle_t       list_head = LIST_FIRST(&(tsftimermgr->active_q));
            
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_TSFTIMER,
                                 "%s: rearm hw timer to 0x%x, curr=0x%x\n",
                                 __func__, list_head->timeout, ic->ic_get_TSF32(ic));
            ic->ic_tsf_timer_start(ic, tsftimermgr->ath_timer_handle, list_head->timeout, TSF_DEFAULT_PERIOD);
        }
    }

    IEEE80211_TSF_TIMER_UNLOCK(tsftimermgr);

    return 0;
}

/*
 * Remove the TSF timer from active list. Assumed that the lock is held.
 */
static void
remove_timer_from_active_list(
    tsftimer_handle_t           h_tsftime
    )
{
    ieee80211_tsf_timer_mgr_t   tsftimermgr = h_tsftime->h_tsftimermgr;

    /* Remove this timer from the active list */
    if (h_tsftime->active) {
        LIST_REMOVE(h_tsftime, active_list); 
        h_tsftime->active = FALSE;

        ASSERT(tsftimermgr->num_active_timers > 0);
        tsftimermgr->num_active_timers--;
    }
}

/*
 * Remove the TSF timer from all list. Assumed that the lock is held.
 */
static void
remove_timer_from_all_lists(
    tsftimer_handle_t           h_tsftime
    )
{
    ieee80211_tsf_timer_mgr_t   tsftimermgr = h_tsftime->h_tsftimermgr;

    /* Remove this timer from the active list */
    remove_timer_from_active_list(h_tsftime);

    /* Remove this timer from the allocated list */
    LIST_REMOVE(h_tsftime, alloc_list); 
    ASSERT(tsftimermgr->num_alloc_timers > 0);
    tsftimermgr->num_alloc_timers--;
}

/*
 * To free a previously allocated TSF Timer. 
 * The can_block parameter is used to indicate that this routine will block and
 * wait till any active timers are completed.
 * Returns 0 if success and 1 if the timer is
 * currently active and can_block==false.
 * Otherwise, return error code if fail.
 */
int ieee80211_tsftimer_free(
    tsftimer_handle_t           h_tsftime,
    bool                        can_block
    )
{
    struct ieee80211com         *ic;
    ieee80211_tsf_timer_mgr_t   tsftimermgr;

    if (h_tsftime == NULL) {
        printk("%s: Error: NULL handle.\n", __func__);
        return EINVAL;
    }

    ASSERT(h_tsftime->h_tsftimermgr != NULL);
    tsftimermgr = h_tsftime->h_tsftimermgr;
    IEEE80211_TSF_TIMER_LOCK(tsftimermgr);
    
    ASSERT(h_tsftime->dead == false);
    ic = tsftimermgr->ic;

    if ((h_tsftime->triggering_bm != 0) && (!can_block)) {
        /* Currently triggered but caller cannot wait. */
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_TSFTIMER,
                             "%s: note: Timer is triggering, cannot free.\n", __func__);
        IEEE80211_TSF_TIMER_UNLOCK(tsftimermgr);
        /* We will just return without freeing this timer */
        return 1;
    }
    
    if ((h_tsftime->triggering_bm != 0) && (can_block)) {
        /* This timer is currently triggered. Wait for the timer to complete. */
        int     sanity_count = 1000000; // max. is 1 sec.

        h_tsftime->dead = true;
        
        /* Remove this timer from all lists */
        remove_timer_from_all_lists(h_tsftime);
        
        IEEE80211_TSF_TIMER_UNLOCK(tsftimermgr);
        // Watch out, spinlock is unlocked!

        while (h_tsftime->triggering_bm != 0) {
            OS_DELAY(10);   // wait for 10 microseconds
            sanity_count--;
            if (sanity_count <= 0) {
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_TSFTIMER,
                                     "%s: Error: waiting too long for timer to complete.\n", __func__);
                break;
            }
        }

        IEEE80211_TSF_TIMER_LOCK(tsftimermgr);
        // Assumed that timer is no longer running.
        ASSERT(h_tsftime->triggering_bm == 0);
    }

    /* Remove this timer from all lists */
    remove_timer_from_all_lists(h_tsftime);
    
    OS_FREE(h_tsftime);

    IEEE80211_TSF_TIMER_UNLOCK(tsftimermgr);

    return 0;
}

static void
trigger_action(void *arg)
{
    ieee80211_tsf_timer_mgr_t   tsftimermgr = (ieee80211_tsf_timer_mgr_t)arg;
    struct ieee80211com         *ic = tsftimermgr->ic;

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_TSFTIMER,
                         "%s : called. arg=0x%x\n", __func__, arg);

    IEEE80211_TSF_TIMER_LOCK(tsftimermgr);

    /* Stop the timer so that the periodic overflow interrupt will not occur */
    ic->ic_tsf_timer_stop(ic, tsftimermgr->ath_timer_handle);

    if ((tsftimermgr->active_callbks & CALLBK_TIMEOUT) != 0) {
		// Re-entrant in this type of callback. Let the existing one handle this.
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_TSFTIMER,
                             "%s : Re-entrant Timeout callback, skip. arg=0x%x\n", __func__, arg);
        IEEE80211_TSF_TIMER_UNLOCK(tsftimermgr);
		return;
    }
    tsftimermgr->active_callbks |= CALLBK_TIMEOUT;

    /* Get out all the timers that have passed the current TSF time and should be triggered */
    while (!LIST_EMPTY(&(tsftimermgr->active_q))) {
        tsftimer_handle_t       list_head;
        u_int32_t               curr_tsf;
        u_int32_t               diff;

        ASSERT(tsftimermgr->num_active_timers > 0);

        list_head = LIST_FIRST(&(tsftimermgr->active_q));
        curr_tsf = ic->ic_get_TSF32(ic);
        diff = list_head->timeout - curr_tsf;

        if (((int32_t)diff) <= 0) {
            /* This timer has timeout-ed and should trigger */

            /* Remove this timer from the active list */
            remove_timer_from_active_list(list_head);

            ASSERT(list_head->dead == false);

            /* Set this flag to make sure that we do not delete this timer during callback */
            ASSERT((list_head->triggering_bm & CALLBK_TIMEOUT) == 0);
            list_head->triggering_bm |= CALLBK_TIMEOUT;

            IEEE80211_TSF_TIMER_UNLOCK(tsftimermgr);

            /* NOTE: Call the callback but without spinlock */
            ASSERT(list_head->fp_timeout);
            
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_DETAILED, IEEE80211_MSG_TSFTIMER,
                                 "%s : called. Callback TsfTimer arg1=%d, arg2=0x%x, curr_tsf=0x%x, scd=0x%x\n",
                                 __func__, list_head->arg1, list_head->arg2, curr_tsf, list_head->timeout);

            (*list_head->fp_timeout)(list_head, list_head->arg1, list_head->arg2, curr_tsf);

            IEEE80211_TSF_TIMER_LOCK(tsftimermgr);
            
            ASSERT((list_head->triggering_bm & CALLBK_TIMEOUT) != 0);
        
            if (list_head->dead) {
                /* Free this timer */
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_TSFTIMER,
                                     "%s: dead tsftimer 0x%x after callback.\n", __func__, list_head);

                /* 
                 * Note: there is another thread waiting to free this timer.
                 * Make sure that we do not touch this structure after setting flag.
                 */
                list_head->triggering_bm &= ~CALLBK_TIMEOUT;

                continue;
            }
            list_head->triggering_bm &= ~CALLBK_TIMEOUT;
        }
        else {
            break;  /* Break this loop since all timers left are unexpired. */
        }
    }

    if (!LIST_EMPTY(&(tsftimermgr->active_q))) {
        /* Active Q is not empty. Rearm the hw timer */
        tsftimer_handle_t       list_head;

        list_head = LIST_FIRST(&(tsftimermgr->active_q));
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_DETAILED, IEEE80211_MSG_TSFTIMER,
                             "%s: rearm hw timer to 0x%x, curr=0x%x\n",
                             __func__, list_head->timeout, ic->ic_get_TSF32(ic));
        ic->ic_tsf_timer_start(ic, tsftimermgr->ath_timer_handle, list_head->timeout, TSF_DEFAULT_PERIOD);
    }

    tsftimermgr->active_callbks &= ~CALLBK_TIMEOUT;
    IEEE80211_TSF_TIMER_UNLOCK(tsftimermgr);
}

static void
overflow_action(void *arg)
{
    ieee80211_tsf_timer_mgr_t   tsftimermgr = (ieee80211_tsf_timer_mgr_t)arg;
    struct ieee80211com         *ic = tsftimermgr->ic;

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_TSFTIMER,
                         "%s : called. Called trigger instead. arg=0x%x\n", __func__, arg);

    trigger_action(arg);
}

static void
outofrange_action(void *arg)
{
    ieee80211_tsf_timer_mgr_t   tsftimermgr = (ieee80211_tsf_timer_mgr_t)arg;
    struct ieee80211com         *ic = tsftimermgr->ic;
    tsftimer_handle_t           curr_timer;

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_TSFTIMER,
                         "%s : Timer Out of Range called. arg=0x%x\n", __func__, arg);

    IEEE80211_TSF_TIMER_LOCK(tsftimermgr);

    if ((tsftimermgr->active_callbks & CALLBK_TSF_CHG) != 0) {
		// Re-entrant in this type of callback. Let the existing one handle this.
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_TSFTIMER,
                             "%s : Re-entrant Timeout callback, skip. arg=0x%x\n", __func__, arg);
        IEEE80211_TSF_TIMER_UNLOCK(tsftimermgr);
		return;
    }
    tsftimermgr->active_callbks |= CALLBK_TSF_CHG;

    /* Go through all the allocated timers. */
    curr_timer = LIST_FIRST(&(tsftimermgr->alloc_q));
    while (curr_timer != NULL) {
        tsftimer_handle_t           next_timer;

        ASSERT(tsftimermgr->num_alloc_timers > 0);

        /* Store the next timer in case the current one is freed. */
        next_timer = LIST_NEXT(curr_timer, alloc_list);
        
        if (curr_timer->fp_tsf_changed != NULL) {
            u_int32_t               curr_tsf;

            /* Set this flag to make sure that we do not delete this timer during callback */
            ASSERT((curr_timer->triggering_bm & CALLBK_TSF_CHG) == 0);
            curr_timer->triggering_bm |= CALLBK_TSF_CHG;

            IEEE80211_TSF_TIMER_UNLOCK(tsftimermgr);

            curr_tsf = ic->ic_get_TSF32(ic);

            /* NOTE: Call the callback but without spinlock */
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_DETAILED, IEEE80211_MSG_TSFTIMER,
                                 "%s : called. Callback TsfChanged arg1=%d, arg2=0x%x, curr_tsf=0x%x, scd=0x%x\n",
                                 __func__, curr_timer->arg1, curr_timer->arg2, 
                                 curr_tsf, curr_timer->timeout);

            (*curr_timer->fp_tsf_changed)(curr_timer, curr_timer->arg1, curr_timer->arg2, curr_tsf);

            IEEE80211_TSF_TIMER_LOCK(tsftimermgr);
            
            ASSERT((curr_timer->triggering_bm & CALLBK_TSF_CHG) != 0);
        
            if (curr_timer->dead) {
                /* Free this timer */
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_TSFTIMER,
                                     "%s: dead tsftimer 0x%x after callback.\n", __func__, curr_timer);

                /* 
                 * Note: there is another thread waiting to free this timer.
                 * Make sure that we do not touch this structure after setting flag.
                 */
                curr_timer->triggering_bm &= ~CALLBK_TSF_CHG;
            }
            else {
                curr_timer->triggering_bm &= ~CALLBK_TSF_CHG;
            }
        }
        
        curr_timer = next_timer;
    }

    tsftimermgr->active_callbks &= ~CALLBK_TSF_CHG;
    IEEE80211_TSF_TIMER_UNLOCK(tsftimermgr);
}

/*
 * Create Tsf Timer manager.
 */
ieee80211_tsf_timer_mgr_t ieee80211_tsf_timer_attach(struct ieee80211com *ic)
{
    ieee80211_tsf_timer_mgr_t   tsftimermgr;
    struct ieee80211_tsf_timer  *tsf_timer_handle = NULL;

    if (ic->ic_tsf_timer) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_TSFTIMER,
                             "%s : TsfTimerMgr already exists \n", __func__); 
        return NULL;
    }

    /* Allocate ResMgr data structures */
    tsftimermgr = (ieee80211_tsf_timer_mgr_t) OS_MALLOC(ic->ic_osdev, 
                                            sizeof(struct ieee80211_tsf_timer_mgr),
                                            0);
    if (tsftimermgr == NULL) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_TSFTIMER,
                             "%s : TsfTimer memory alloction failed\n", __func__); 
        return NULL;
    }

    OS_MEMZERO(tsftimermgr, sizeof(struct ieee80211_tsf_timer_mgr));
    tsftimermgr->ic = ic;

    IEEE80211_TSF_TIMER_LOCK_INIT(tsftimermgr);

    tsf_timer_handle = ic->ic_tsf_timer_alloc(ic, TSF_ID_0, trigger_action, 
                                              overflow_action, outofrange_action,
                                              (void *)tsftimermgr);
    if (tsf_timer_handle == NULL) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_TSFTIMER,
                             "%s : Unable to allocate Ath TSF Timer. "
                             "Maybe, the hw does not support it.\n", __func__); 
        IEEE80211_TSF_TIMER_LOCK_DESTROY(tsftimermgr);
        OS_FREE(tsftimermgr);
        return NULL;
    }
    tsftimermgr->ath_timer_handle = tsf_timer_handle;
    tsftimermgr->num_alloc_timers = 0;
    tsftimermgr->active_callbks = 0;

    /* Init. Queue for all allocated timer */
    LIST_INIT(&tsftimermgr->alloc_q);

    /* Init. Queue for active timers with scheduled timeouts */
    LIST_INIT(&tsftimermgr->active_q);

    return tsftimermgr;
}

/*
 * Delete Tsf Timer manager.
 */
void ieee80211_tsf_timer_detach(ieee80211_tsf_timer_mgr_t tsftimermgr)
{
    struct ieee80211com *ic;

    if (!tsftimermgr) {
        /* Not allocated. Return */
        return;
    }

    ic = tsftimermgr->ic;
    ASSERT(tsftimermgr->num_alloc_timers == 0);

    /* No vaps should exist at this time */
    ASSERT(TAILQ_FIRST(&ic->ic_vaps) == NULL);

    if (tsftimermgr->ath_timer_started) {
        /* Stop the hardware timer */
        ic->ic_tsf_timer_stop(ic, tsftimermgr->ath_timer_handle);
        tsftimermgr->ath_timer_started = false;
    }

    ASSERT(tsftimermgr->active_callbks == 0);

    IEEE80211_TSF_TIMER_LOCK_DESTROY(tsftimermgr);

    ASSERT(tsftimermgr->ath_timer_handle != NULL);
    ic->ic_tsf_timer_free(ic, tsftimermgr->ath_timer_handle);

    /* Free ResMgr data structures */
    OS_FREE(tsftimermgr);
    ic->ic_tsf_timer = NULL;
}

#endif  //if UMAC_SUPPORT_TSF_TIMER


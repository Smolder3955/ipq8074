/*
 *  Copyright (c) 2009 Atheros Communications Inc.  All rights reserved.
 *  Routines for P2P GO power save.
 */
/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */


#include "ieee80211_var.h"
#include "ieee80211_power.h"
#include "ieee80211_sm.h"
#include "ieee80211_tsftimer.h"
#include "ieee80211_p2p_go_power.h"
#include "ieee80211_p2p_go_schedule.h"
#include "p2p_go_power_priv.h"
#include <ieee80211_notify_tx_bcn.h>
#include "ieee80211_p2p_ie.h"
#include "ieee80211_p2p_go.h"

#if UMAC_SUPPORT_P2P
#if UMAC_SUPPORT_P2P_GO_POWERSAVE

/* We ran out of bits for the ic->ic_debug field. For now, use the "dump 802.1x" module. */
#define IEEE80211_MSG_P2P_GO_POWER      IEEE80211_MSG_RADDUMP

#define     TIME_UNIT_TO_MICROSEC   1024    /* 1 TU equals 1024 microsecs */

/* 
 * The next time to update the continuous NOA schedules. We need to do it
 * before the 32-bits NOA start-time wraps around. I added an extra of 61 seconds
 * (0x03FFFFFF) buffer before the wraparound.
 */
#define     P2P_GO_NEXT_NOA_UPDATE_TIME     (0x7FFFFFFF - 0x03FFFFFF)

/* Timeout duration in microsecs for the Transmission of Beacon */
#define DUR_TIMEOUT_BCN_TX              8000

#define MAX_QUEUED_EVENTS  16

struct p2p_go_ps_sm_event {
    ieee80211_vap_t          vap;
    /* Add more fields */
};
typedef struct p2p_go_ps_sm_event *p2p_go_ps_sm_event_t;

/* List of timer types used by this module */
/* Note: Remember to update default_timer_event_map[] when changing this enum */
typedef enum {
    P2P_GO_PS_TIMER_CTWIN = 0,
    P2P_GO_PS_TIMER_BCN_TX,
    P2P_GO_PS_TIMER_TBTT,
    MAX_P2P_GO_PS_TIMER,        /* This should be the last one since it indicates max index */
} p2p_go_timer_type; 

struct  timer_type_to_event_mapping {
    p2p_go_timer_type           type;
    ieee80211_connection_event  timeout_event;
};

/* Note: Remember to update enum p2p_go_timer_type when changing this struc */
const struct timer_type_to_event_mapping   default_timer_event_map[MAX_P2P_GO_PS_TIMER] =
{
    {P2P_GO_PS_TIMER_CTWIN,     P2P_GO_PS_EVENT_CTWIN_EXPIRE},
    {P2P_GO_PS_TIMER_BCN_TX,    P2P_GO_PS_EVENT_TIMEOUT_TX_BCN},
    {P2P_GO_PS_TIMER_TBTT,      P2P_GO_PS_EVENT_TBTT},
};

typedef struct  _p2p_go_timer_info {
    tsftimer_handle_t           h_tsftimer;
    bool                        started;
    ieee80211_connection_event  timeout_event;
} p2p_go_timer_info;

struct ieee80211_p2p_go_ps {
    wlan_if_t           vap;
    wlan_dev_t          ic;
    osdev_t             os_handle;
    wlan_p2p_go_t       p2p_go_handle;          /* P2P G0 Handle */
    ieee80211_hsm_t     hsm_handle;             /* HSM Handle */

    atomic_t            running;                /* This module is running */

    bool                up;                     /* VAP is active and up */

    ieee80211_p2p_go_schedule_t go_scheduler;
    bool                go_sch_paused;          /* GO Scheduler Paused? */
    bool                init_curr_tbtt_tsf;     /* Is the field curr_tbtt_tsf initialized? */
    u_int32_t           curr_tbtt_tsf;          /* TSF of last TBTT */
    u_int32_t           tbtt_intval;            /* TBTT interval */

    bool                opp_ps_en;              /* Opp Power Save enabled? */
    bool                oneshot_noa;            /* Currently in the middle of one-shot NOA sleep */

    atomic_t            opp_ps_chk_sta;         /* Start checking for all stations asleep */

    bool                have_ctwin;             /* Supports CTWin */
    u_int32_t           ctwin_duration;         /* Duration of CTWin (in milliseconds) */
    atomic_t            noa_asleep;             /* In the middle of periodic NOA sleep */

    p2p_go_timer_info   timer[MAX_P2P_GO_PS_TIMER];

    ieee80211_tx_bcn_notify_t
                        h_tx_bcn_notify;

    /* Handle to the ATH INFO notify */
    ieee80211_vap_ath_info_notify_t
                        h_ath_info_notify;      /* Handle of module that notify when ATH info changes. */
    u_int32_t           tsf_offset;             /* TSF Offset for this VAP */

    /* Copy of the current NOA IE */
    spinlock_t          noa_ie_lock;            /* lock to access NOA IE structures */
    bool                noa_schedule_updated;   /* flag indicate that NOE schedule is updated */
    u_int32_t           noa_change_time;        /* Start time of new NOA schedules */
    struct ieee80211_p2p_sub_element_noa        /* Current NOA IE contents */
                        noa_ie;
    bool                pending_ps_change;      /* Pending change to Opp PS or CTWin */

    int                 curr_num_descriptors;   /* Number of active NOA descriptors */
    ieee80211_p2p_go_schedule_req               /* Current active NOA schedules */
                        curr_NOA_schedule[IEEE80211_MAX_NOA_DESCRIPTORS];

    bool                have_continuous_noa_schedule;
    u_int32_t           continuous_noa_change_time;
    u_int32_t           pwr_arbiter_id;         /* Power arbiter requestor ID */
};

static const char* p2p_go_ps_sm_event_name[] = {
    "NULL",                 /* P2P_GO_PS_EVENT_NULL = 0 */
    "STARTED",              /* P2P_GO_PS_EVENT_STARTED */
    "STOPPED",              /* P2P_GO_PS_EVENT_STOPPED */
    "TBTT",                 /* P2P_GO_PS_EVENT_TBTT */
    "TX_BCN",               /* P2P_GO_PS_EVENT_TX_BCN */
    "TSF_CHANGED",          /* P2P_GO_PS_EVENT_TSF_CHANGED */
    "OPP_PS_OK",            /* P2P_GO_PS_EVENT_OPP_PS_OK */
    "TIMEOUT_TX_BCN",       /* P2P_GO_PS_EVENT_TIMEOUT_TX_BCN */
    "CTWIN_EXPIRE",         /* P2P_GO_PS_EVENT_CTWIN_EXPIRE */
    "ONESHOT_NOA_SLEEP",    /* P2P_GO_PS_EVENT_ONESHOT_NOA_SLEEP */
    "PERIOD_NOA_SLEEP",     /* P2P_GO_PS_EVENT_PERIOD_NOA_SLEEP */
    "NOA_WAKE",             /* P2P_GO_PS_EVENT_NOA_WAKE */
};

/* Returns the string name of the given event */
static const char *
event_name(ieee80211_connection_event    event)
{
#define CASE_EVENT(_ev)     case _ev: return(#_ev);

    switch(event) {
    CASE_EVENT(P2P_GO_PS_EVENT_NULL)
    CASE_EVENT(P2P_GO_PS_EVENT_STARTED)
    CASE_EVENT(P2P_GO_PS_EVENT_STOPPED)
    CASE_EVENT(P2P_GO_PS_EVENT_TBTT)
    CASE_EVENT(P2P_GO_PS_EVENT_TX_BCN)
    CASE_EVENT(P2P_GO_PS_EVENT_TSF_CHANGED)
    CASE_EVENT(P2P_GO_PS_EVENT_OPP_PS_OK)
    CASE_EVENT(P2P_GO_PS_EVENT_TIMEOUT_TX_BCN)
    CASE_EVENT(P2P_GO_PS_EVENT_CTWIN_EXPIRE)
    CASE_EVENT(P2P_GO_PS_EVENT_ONESHOT_NOA_SLEEP)
    CASE_EVENT(P2P_GO_PS_EVENT_PERIOD_NOA_SLEEP)
    CASE_EVENT(P2P_GO_PS_EVENT_NOA_WAKE)

    default:
        printk("Note: need to add this new event=%d into function %s()\n", event, __func__);
        return "unknown_event";
    }

#undef CASE_EVENT
}

/*
 * *** Timer Support Functions ***
 */

/* Stop all timers or activity */
static void
stop_timer(
    ieee80211_p2p_go_ps_t   p2p_go_ps,
    p2p_go_timer_type       timer_type      /* Timer type */
    )
{
    int         retval = 0;

    ASSERT(timer_type < MAX_P2P_GO_PS_TIMER);
    ASSERT(p2p_go_ps->timer[timer_type].h_tsftimer != NULL);

    if (p2p_go_ps->timer[timer_type].started) {
        /* Timer is started, stop it */
        retval = ieee80211_tsftimer_stop(p2p_go_ps->timer[timer_type].h_tsftimer);
        if (retval != 0) {
            IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
                "%s: ERROR: ieee80211_tsftimer_stop returns error=%d\n", __func__, retval);
        }
        p2p_go_ps->timer[timer_type].started = false;
    }

    return;
}

/* Stop all timers or activity */
static void
stop_all_timers(
    ieee80211_p2p_go_ps_t   p2p_go_ps)
{
    int         i;

    for (i = 0; i < MAX_P2P_GO_PS_TIMER; i++) {
        stop_timer(p2p_go_ps, i);
    }
}

static int
start_timer(
    ieee80211_p2p_go_ps_t   p2p_go_ps,
    p2p_go_timer_type       timer_type,     /* Timer type */
    u_int32_t               expiry_time     /* Absolute TSF Time to expire timer */
    )
{
    int     retval = 0;

    ASSERT(timer_type < MAX_P2P_GO_PS_TIMER);
    ASSERT(p2p_go_ps->timer[timer_type].h_tsftimer != NULL);

    /* Check that if this timer has already started */
    if (p2p_go_ps->timer[timer_type].started) {
        /* Timer is started, invalid */
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_GO_POWER,
            "%s: NOTE: Timer[%d] has already started. Will restart.\n", __func__, timer_type);
    }

    /* Start this timer */
    retval = ieee80211_tsftimer_start(p2p_go_ps->timer[timer_type].h_tsftimer, expiry_time, 0);
    if (retval != 0) {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
            "%s: ERROR: ieee80211_tsftimer_start returns error=%d\n", __func__, retval);
        return retval;
    }

    p2p_go_ps->timer[timer_type].started = true;

    return retval;
}

/*
 * Routine to get the remainder in microseconds when TSF (64-bits) is divided by
 * beacon_interval (32-bits).
 */
static inline u_int32_t 
calc_mod_tsf64_bint(u_int64_t tsf64, u_int32_t beacon_interval)
{
    u_int64_t           result;

    result  = OS_MOD64_TBTT_OFFSET(tsf64, beacon_interval);
    ASSERT((result >> 32) == 0);
    return((u_int32_t)result);
}

/*
 * Routine to calculate the current (previous) TBTT TSF Time.
 */
static void
calc_curr_tbtt_tsf(ieee80211_p2p_go_ps_t p2p_go_ps, u_int64_t curr_tsf64)
{
    u_int64_t           result;
    u_int32_t           remainder;

    /* Round down to the last interval */
    ASSERT(p2p_go_ps->tbtt_intval != 0);
    /* Note: We must get the modulus of the full 64-bits TSF */
    remainder = calc_mod_tsf64_bint(curr_tsf64, p2p_go_ps->tbtt_intval);

    /* TODO: we could use 32-bits minus here and make result in 32-bits */
    result = curr_tsf64 - (u_int64_t)remainder;

    p2p_go_ps->curr_tbtt_tsf = (u_int32_t)result;
    if (p2p_go_ps->tsf_offset != 0) {
        /* The current TBTT is for the first slot VAP and our TBTT has an offset. */
        ASSERT(p2p_go_ps->tbtt_intval > p2p_go_ps->tsf_offset);
        /* Note that tsf_offset is the offset adjustment when sending out the beacon timestamp. 
           i.e. our extenal TSF in our Tx frames has an offset given by tsf_offset. */
        p2p_go_ps->curr_tbtt_tsf += (p2p_go_ps->tbtt_intval - p2p_go_ps->tsf_offset);
        if (TSFTIME_IS_GREATER(p2p_go_ps->curr_tbtt_tsf, (u_int32_t)curr_tsf64)) {
            p2p_go_ps->curr_tbtt_tsf -= p2p_go_ps->tbtt_intval;
        }
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "p2p_go_ps->tsf_offset=0x%x, p2p_go_ps->curr_tbtt_tsf=0x%x\n", 
                p2p_go_ps->tsf_offset, p2p_go_ps->curr_tbtt_tsf);
    }

    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
        "CurrTSF=0x%x %x, 64-bits result=0x%x %x, tbtt interval=0x%x, remainder=0x%x\n", 
            (u_int32_t)(curr_tsf64>>32), (u_int32_t)curr_tsf64,
            (u_int32_t)(result>>32), (u_int32_t)result, p2p_go_ps->tbtt_intval, remainder);

    p2p_go_ps->init_curr_tbtt_tsf = true;
}

/* Rearm the next Beacon TBTT timer */
static void
rearm_tbtt_timer(
    ieee80211_p2p_go_ps_t   p2p_go_ps
    )
{
    int                 retval;
    u_int32_t           next_tbtt, old_tbtt;
    struct ieee80211com *ic;
    u_int64_t           curr_tsf64;

    old_tbtt = p2p_go_ps->curr_tbtt_tsf;

    ic = p2p_go_ps->ic;
    curr_tsf64 = ic->ic_get_TSF64(ic);
    calc_curr_tbtt_tsf(p2p_go_ps, curr_tsf64);

#if DBG
    if (old_tbtt != (p2p_go_ps->curr_tbtt_tsf - p2p_go_ps->tbtt_intval)) {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_GO_POWER,
            "%s: Warning: There is a jump in TBTT. old=0x%x, expected=0x%x\n", __func__, 
               old_tbtt,  (p2p_go_ps->curr_tbtt_tsf - p2p_go_ps->tbtt_intval));
    }
#endif

    next_tbtt = p2p_go_ps->curr_tbtt_tsf + p2p_go_ps->tbtt_intval;
    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
        "%s: curr_tbtt_tsf=0x%x, set next tbtt timeout=0x%x\n", __func__, 
           p2p_go_ps->curr_tbtt_tsf, next_tbtt);

    retval = start_timer(p2p_go_ps, P2P_GO_PS_TIMER_TBTT, next_tbtt);
    if (retval != 0) {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
            "%s: ERROR: start_timer() returns error=%d\n", __func__, retval);
    }
}

/*
 * Callback from TSF Timer when time is re-sync.
 */
static void
go_ps_tsftimer_resync(tsftimer_handle_t h_tsftime, int arg1, void *arg2, u_int32_t curr_tsf)
{
    ieee80211_p2p_go_ps_t       p2p_go_ps;
    p2p_go_timer_type           timer_type;     /* Timer type */
    ieee80211_connection_event  event_type;

    timer_type = arg1;  /* get the timer type */
    p2p_go_ps = (ieee80211_p2p_go_ps_t)arg2;

    ASSERT(timer_type < MAX_P2P_GO_PS_TIMER);
    ASSERT(p2p_go_ps != NULL);
    ASSERT(p2p_go_ps->timer[timer_type].h_tsftimer != NULL);
    
    //TODO: does the sm handle the sync with the posting of this event.

    /* Post this Re-sync event */
    event_type = P2P_GO_PS_EVENT_TSF_CHANGED;

    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_GO_POWER,
        "%s: NOTE: Timer resync. Send event_type=%s\n", __func__, event_name(event_type));
    ieee80211_sm_dispatch(p2p_go_ps->hsm_handle, event_type, 0, NULL);
}

/*
 * Callback from TSF Timer for timer expiry.
 */
static void
go_ps_tsftimer_timeout(tsftimer_handle_t h_tsftime, int arg1, void *arg2, u_int32_t curr_tsf)
{
    ieee80211_p2p_go_ps_t       p2p_go_ps;
    p2p_go_timer_type           timer_type;     /* Timer type */
    ieee80211_connection_event  event_type;

    timer_type = arg1;  /* get the timer type */
    p2p_go_ps = (ieee80211_p2p_go_ps_t)arg2;

    ASSERT(timer_type < MAX_P2P_GO_PS_TIMER);
    ASSERT(p2p_go_ps != NULL);
    ASSERT(p2p_go_ps->timer[timer_type].h_tsftimer != NULL);
    
    //TODO: does the sm handle the sync with the posting of this event.

    /* Post this timeout event */
    event_type = p2p_go_ps->timer[timer_type].timeout_event;
    ASSERT(event_type != P2P_GO_PS_EVENT_NULL);

    p2p_go_ps->timer[timer_type].started = false;

    if (timer_type == P2P_GO_PS_TIMER_TBTT) {
        /* Calculate the next TBTT. Rearm the timer for the next tbtt */
        // TODO: should we move this to the State machine event handler
        rearm_tbtt_timer(p2p_go_ps);
    }

    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_GO_POWER,
        "%s: Send Timeout event=%s\n", __func__, event_name(event_type));
    ieee80211_sm_dispatch(p2p_go_ps->hsm_handle, event_type, 0, NULL);
}

/*
 * Allocate the TSF Timers and initialize the timer related variables.
 * Return 0 if success.
 */
static int go_init_timers(ieee80211_p2p_go_ps_t   p2p_go_ps)
{
    int                 i;
    int                 retval = 0;
    struct ieee80211com *ic;

    ic = p2p_go_ps->ic;

    do {
        
        for (i = 0; i < MAX_P2P_GO_PS_TIMER; i++) {
            int         j;

            ASSERT(p2p_go_ps->timer[i].h_tsftimer == NULL);

            if (i == 0) {
                /* For the first timer, we add the resync callback */
                p2p_go_ps->timer[i].h_tsftimer = 
                    ieee80211_tsftimer_alloc(ic->ic_tsf_timer, 0, go_ps_tsftimer_timeout,
                                              i, p2p_go_ps, go_ps_tsftimer_resync);;
            }
            else {
                /* No need to have resync callback */
                p2p_go_ps->timer[i].h_tsftimer = 
                    ieee80211_tsftimer_alloc(ic->ic_tsf_timer, 0, go_ps_tsftimer_timeout,
                                              i, p2p_go_ps, NULL);;
            }
            if (p2p_go_ps->timer[i].h_tsftimer == NULL) {
                /* Error: Unable to allocate Timer. */
                IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
                    "%s: Error: unable to alloc timer=%d\n", __func__, i);
                retval = -EPERM;
                break;
            }

            p2p_go_ps->timer[i].started = false;

            /* Find out the EVENT to send for this timer */
            for (j = 0; j < MAX_P2P_GO_PS_TIMER; j++) {
                if (i == default_timer_event_map[j].type) {
                    p2p_go_ps->timer[i].timeout_event = default_timer_event_map[j].timeout_event;
                    break;
                }
            }
            ASSERT(j < MAX_P2P_GO_PS_TIMER);
        }

        if (retval != 0) {
            /* Some error in allocating timer, get out of here */
            break;
        }

    } while ( false );

    if (retval != 0) {
        /* Free all the allocated timers */
        for (i = 0; i < MAX_P2P_GO_PS_TIMER; i++) {
            if (p2p_go_ps->timer[i].h_tsftimer) {
                if (ieee80211_tsftimer_free(p2p_go_ps->timer[i].h_tsftimer, true)) {
                    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
                     "%s: ERROR: ieee80211_tsftimer_free returns error\n", __func__);
                }
                p2p_go_ps->timer[i].h_tsftimer = NULL;
            }
            else break;
        }
    }

    return retval;
}

/*
 * Free the TSF Timers.
 */
static void free_timers(ieee80211_p2p_go_ps_t   p2p_go_ps)
{
    int                 i;
    struct ieee80211com *ic;

    ic = p2p_go_ps->ic;

    /* Free all the allocated timers */
    for (i = 0; i < MAX_P2P_GO_PS_TIMER; i++) {
        if (p2p_go_ps->timer[i].h_tsftimer) {
            stop_timer(p2p_go_ps, i);
            if (ieee80211_tsftimer_free(p2p_go_ps->timer[i].h_tsftimer, true)) {
                    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
                     "%s: ERROR: ieee80211_tsftimer_free returns error\n", __func__);
            }
            p2p_go_ps->timer[i].h_tsftimer = NULL;
            p2p_go_ps->timer[i].h_tsftimer = NULL;
        }
    }
}

/* Stub Entry function for State */
#define STATE_ENTRY(_state)                                                         \
static void p2p_go_ps_state_##_state##_entry(void *ctx)                             \
{                                                                                   \
    ieee80211_p2p_go_ps_t   p2p_go_ps = (ieee80211_p2p_go_ps_t) ctx;                \
    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,   \
        "%s: called. ctx=0x%x.\n", __func__, ctx);                                  \
}                                                                                   \

/* Stub Exit function for State */
#define STATE_EXIT(_state)                                                          \
static void p2p_go_ps_state_##_state##_exit(void *ctx)                              \
{                                                                                   \
    ieee80211_p2p_go_ps_t   p2p_go_ps = (ieee80211_p2p_go_ps_t) ctx;                \
    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,   \
        "%s: called. ctx=0x%x.\n", __func__, ctx);                                  \
}                                                                                   \

/* 
 * ****** Different state machine related functions ********
 */

/* 
 * This function is called when the Power Save Parameters are changed.
 * Check if we need to kickstart or stop the PS state machine.
 */
static void
p2p_go_ps_param_changed(ieee80211_p2p_go_ps_t p2p_go_ps)
{
    if (!p2p_go_ps->up) {
        /* VAP is down. Ignore this call */
        return;
    }

    if (atomic_read(&p2p_go_ps->running)) {
        /* Power Save state machine is running */
        if ((!p2p_go_ps->noa_ie.oppPS) && (p2p_go_ps->noa_ie.num_descriptors == 0)) {
            /* Shut down Power Save */
            IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_GO_POWER,
                "%s: disable ps since no one is using it.", __func__);
            ieee80211_sm_dispatch(p2p_go_ps->hsm_handle, P2P_GO_PS_EVENT_STOPPED, 0, NULL);
        }
    }
    else {
        /* Power Save state machine is stopped */
        if ((p2p_go_ps->noa_ie.oppPS) || (p2p_go_ps->noa_ie.num_descriptors > 0)) {
            /* Enable Power Save */
            IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_GO_POWER,
                "%s: enable ps.", __func__);
            ieee80211_sm_dispatch(p2p_go_ps->hsm_handle, P2P_GO_PS_EVENT_STARTED, 0, NULL);
        }
    }
}

static void
p2p_go_ps_tx_bcn_notify(
    ieee80211_tx_bcn_notify_t h_notify,
    int beacon_id, 
    u_int32_t tx_status,
    void *arg)
{
    ieee80211_p2p_go_ps_t   p2p_go_ps = (ieee80211_p2p_go_ps_t)arg;

    ASSERT(p2p_go_ps);

    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
        "%s: called.", __func__);

    /* TODO: we need to handle the case where Tx status is not OK */
    ASSERT(tx_status == 0);

    ieee80211_sm_dispatch(p2p_go_ps->hsm_handle, P2P_GO_PS_EVENT_TX_BCN, 0, NULL);
}

/*
 * ************************** State IDLE ************************************
 * Description:  Init state.
 */
static void p2p_go_ps_state_idle_entry(void *ctx) 
{
    ieee80211_p2p_go_ps_t   p2p_go_ps = (ieee80211_p2p_go_ps_t) ctx;
    struct ieee80211com     *ic = p2p_go_ps->ic;

    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
        "%s: called. ctx=0x%x.\n", __func__, ctx);

    /* Paused all callback from the GO scheduler */
    p2p_go_ps->go_sch_paused = true;
    ieee80211_p2p_go_schedule_pause(p2p_go_ps->go_scheduler);

    /* Inform the P2P GO that there is a no NOA IE */
    wlan_p2p_go_update_noa_ie(p2p_go_ps->p2p_go_handle, NULL, p2p_go_ps->tsf_offset);

    stop_all_timers(p2p_go_ps);

    /* Disable the Tx Beacon completion callbacks */
    IEEE80211_COMM_LOCK(ic);
    if (p2p_go_ps->h_tx_bcn_notify != NULL) {
        ieee80211_dereg_notify_tx_bcn(p2p_go_ps->h_tx_bcn_notify);
        p2p_go_ps->h_tx_bcn_notify = NULL;
    }
    IEEE80211_COMM_UNLOCK(ic);
    
    atomic_set(&p2p_go_ps->running, 0);
}

STATE_EXIT(idle);

static bool p2p_go_ps_state_idle_event(void *ctx, u_int16_t event_type, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_p2p_go_ps_t   p2p_go_ps = (ieee80211_p2p_go_ps_t) ctx;
    bool                    retVal = true;
    struct ieee80211com     *ic = p2p_go_ps->ic;

    switch (event_type) {
    case P2P_GO_PS_EVENT_STARTED:
    {
        int                     beacon_id = 0;  /* TODO: need to figure out our beacon ID */
        u_int64_t               curr_tsf64;

        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. event_type=P2P_GO_PS_EVENT_STARTED. ctx=0x%x.\n", __func__, ctx);

        /* Calculate the current TBTT */
        curr_tsf64 = ic->ic_get_TSF64(ic);
        calc_curr_tbtt_tsf(p2p_go_ps, curr_tsf64);

        /* Enable the Tx Beacon completion callbacks */
        IEEE80211_COMM_LOCK(ic);
        p2p_go_ps->h_tx_bcn_notify = 
            ieee80211_reg_notify_tx_bcn(ic->ic_notify_tx_bcn_mgr, p2p_go_ps_tx_bcn_notify, beacon_id, p2p_go_ps);
        IEEE80211_COMM_UNLOCK(ic);
        if (p2p_go_ps->h_tx_bcn_notify == NULL) {
            /* Error */
            IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
                "%s: ERROR: unable to register Tx Beacon notify callbacks.\n", __func__);
            break;  /* No change in state */
        }

        /* Paused all callback from the GO scheduler */
        p2p_go_ps->go_sch_paused = false;
        ieee80211_p2p_go_schedule_unpause(p2p_go_ps->go_scheduler);

        atomic_set(&p2p_go_ps->running, 1);

        /* Sync to the next beacon */   
        ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_TSF_RESYNC);
        break;
    }

    default:
        retVal = false;
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. rejected event_type=%s. ctx=0x%x.\n", __func__, event_name(event_type), ctx);
        break;
    }

    return retVal;
}

/*
 * ****************************** State TSF_RESYNC **************************
 * Description:  Waiting for the next beacon to resync TSF.
 */

static void p2p_go_ps_state_tsf_resync_entry(void *ctx) 
{
    ieee80211_p2p_go_ps_t   p2p_go_ps = (ieee80211_p2p_go_ps_t) ctx;

    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
        "%s: called. ctx=0x%x.\n", __func__, ctx);

    /* Start the TBTT timer. If already started, then restart with new value. */
    rearm_tbtt_timer(p2p_go_ps);
}

STATE_EXIT(tsf_resync);

static bool p2p_go_ps_state_tsf_resync_event(
    void *ctx, u_int16_t event_type, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_p2p_go_ps_t   p2p_go_ps = (ieee80211_p2p_go_ps_t) ctx;
    bool                    retVal = true;

    switch (event_type) {
    case P2P_GO_PS_EVENT_TBTT:
    {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. event_type=P2P_GO_PS_EVENT_TBTT. ctx=0x%x.\n", __func__, ctx);

        /* Sync completed since ready to send beacon */
        ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_TX_BCN);
        break;
    }

    case P2P_GO_PS_EVENT_STOPPED:
    {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. event_type=P2P_GO_PS_EVENT_STOPPED. ctx=0x%x.\n", __func__, ctx);

        /* next state : P2P_GO_PS_STATE_INIT */
        ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_INIT);

        break;
    }

    case P2P_GO_PS_EVENT_TX_BCN:
        /* Did not get the TBTT Timer before Tx beacon. Restart the TBTT Timer */
        rearm_tbtt_timer(p2p_go_ps);
        break;

    case P2P_GO_PS_EVENT_TSF_CHANGED:
        /* TSF change again. Restart the TBTT Timer */
        rearm_tbtt_timer(p2p_go_ps);
        break;

    default:
        retVal = false;
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. rejected event_type=%s. ctx=0x%x.\n", __func__, event_name(event_type), ctx);
        break;
    }

    return retVal;
}

/*
 * ****************************** State TX_BCN **************************
 * Description:  Waiting for the next beacon to resync TSF.
 * Description:  Waiting for the transmission of next beacon.
 *               Note: We assumed that the hardware cannot sleep while waiting
 *                     for this beacon to be tx-ed.
 */

static void p2p_go_ps_state_tx_bcn_entry(void *ctx) 
{
    int                     retval;
    ieee80211_p2p_go_ps_t   p2p_go_ps = (ieee80211_p2p_go_ps_t) ctx;

    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
                          "%s: called. ctx=0x%pK.\n", __func__, ctx);

    /* Note: we assumed that the SWBA code will update beacon IE's and 
       prepare the DMA for next beacon. */

    /* Set Beacon Tx timeout timer */
    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
        "%s: tbtt tsf=0x%x, set bcn-tx timeout=0x%x\n", __func__, 
           p2p_go_ps->curr_tbtt_tsf, p2p_go_ps->curr_tbtt_tsf + DUR_TIMEOUT_BCN_TX);
    retval = start_timer(p2p_go_ps, P2P_GO_PS_TIMER_BCN_TX,
                p2p_go_ps->curr_tbtt_tsf + DUR_TIMEOUT_BCN_TX);
    if (retval != 0) {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
            "%s: ERROR: start_timer() returns error=%d\n", __func__, retval);
    }
}

static void p2p_go_ps_state_tx_bcn_exit(void *ctx) 
{
    ieee80211_p2p_go_ps_t   p2p_go_ps = (ieee80211_p2p_go_ps_t) ctx;

    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
        "%s: called. ctx=0x%x.\n", __func__, ctx);

    /* Stop the Beacon Tx timeout timer */
    stop_timer(p2p_go_ps, P2P_GO_PS_TIMER_BCN_TX);
}


/*
 * Plumb a new NOA schedule. It is stored in curr_num_descriptors and
 * curr_NOA_schedule fields of p2p_go_ps.
 * Important: Assumed that the lock is held.
 */
static void
p2p_go_plumb_new_noa_schedule(
    ieee80211_p2p_go_ps_t                   p2p_go_ps)
{
    /* This structure is about 84 bytes. If it is too large on the stack, then
       should consider allocating it from heap */
    int                                     i;

    p2p_go_ps->noa_ie.index++;      /* Increment the instance count */

    for (i = 0; i < p2p_go_ps->curr_num_descriptors; i++) {
        p2p_go_ps->noa_ie.noa_descriptors[i] = p2p_go_ps->curr_NOA_schedule[i].noa_descriptor;
    }

    p2p_go_ps->noa_ie.num_descriptors = p2p_go_ps->curr_num_descriptors;

    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_DETAILED, IEEE80211_MSG_P2P_GO_POWER,
        "%s: called. Num Desc=%d.\n", __func__, p2p_go_ps->curr_num_descriptors);

    p2p_go_ps->noa_schedule_updated = true;
}

/*
 * Check if we need to update the continuous NOA schedules.
 * Note that the longest any NOA schedule can run without wrapping around is 31 minutes.
 * Assumed that the lock is held.
 */
static void
update_continuous_noa(
    ieee80211_p2p_go_ps_t   p2p_go_ps)
{
    if (p2p_go_ps->noa_schedule_updated) {
        /* If there is already a pending NOA update, then skip this check. */
        return;
    }

    if (p2p_go_ps->have_continuous_noa_schedule && 
        TSFTIME_IS_GREATER(p2p_go_ps->curr_tbtt_tsf, p2p_go_ps->continuous_noa_change_time))
    {
        /* Time to update the continuous NOA schedule */
        int     i;
        bool    have_update = false;

        /* This structure is about 84 bytes. If it is too large on the stack, then
           should consider allocating it from heap */
        struct ieee80211_p2p_sub_element_noa    new_noa_ie;

        ASSERT(p2p_go_ps->curr_num_descriptors <= IEEE80211_MAX_NOA_DESCRIPTORS);
        for (i = 0; i < p2p_go_ps->curr_num_descriptors; i++) {
            ieee80211_p2p_go_schedule_req   *schedule;

            schedule = &p2p_go_ps->curr_NOA_schedule[i];

            if (schedule->noa_descriptor.type_count == 255) {
                /* A continuous NOA. Update it now so that it starts around curr_tbtt_tsf. */
                u_int32_t   tmp32, remainder, orig_start;

                // This assertion could be wrong when the TSF clock is reset to 0.
                //ASSERT(TSFTIME_IS_GREATER(p2p_go_ps->curr_tbtt_tsf, schedule->noa_descriptor.start_time));

                tmp32 = p2p_go_ps->curr_tbtt_tsf - schedule->noa_descriptor.start_time;
                remainder = tmp32 % schedule->noa_descriptor.interval;
                orig_start = schedule->noa_descriptor.start_time;
                schedule->noa_descriptor.start_time = p2p_go_ps->curr_tbtt_tsf - remainder;

                /* Calculate the next time to renew this schedule */
                p2p_go_ps->continuous_noa_change_time = schedule->noa_descriptor.start_time + 
                    P2P_GO_NEXT_NOA_UPDATE_TIME;
                have_update = true;

                IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_GO_POWER,
                    "%s: Update cont. NOA. Orig start=0x%x, new start=0x%x, interval=0x%x, nextchange=0x%x\n",
                    __func__, 
                    orig_start, schedule->noa_descriptor.start_time, 
                    schedule->noa_descriptor.interval, p2p_go_ps->continuous_noa_change_time);
            }
        }

        if (!have_update) {
            /* It is possible that the continuous NOA has being removed. */
            IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
                "%s: Warning: continous NOA unexpectedly missing.\n",
                __func__);
            p2p_go_ps->have_continuous_noa_schedule = false;
            return;
        }

        p2p_go_plumb_new_noa_schedule(p2p_go_ps);
        p2p_go_ps->noa_change_time = p2p_go_ps->curr_tbtt_tsf;  /* Change it now */

        /* Make a copy of the NOE IE to use outside the lock */
        OS_MEMCPY(&new_noa_ie, &p2p_go_ps->noa_ie,
                  sizeof(struct ieee80211_p2p_sub_element_noa));

        spin_unlock(&(p2p_go_ps->noa_ie_lock));

        /* Inform the P2P GO that there is a new NOA IE */
        wlan_p2p_go_update_noa_ie(p2p_go_ps->p2p_go_handle, &new_noa_ie, p2p_go_ps->tsf_offset);

        spin_lock(&(p2p_go_ps->noa_ie_lock));
    }
}

static bool p2p_go_ps_state_tx_bcn_event(
    void *ctx, u_int16_t event_type, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_p2p_go_ps_t           p2p_go_ps = (ieee80211_p2p_go_ps_t) ctx;
    bool                            retVal = true;
    int                             num_NOA_desc;
    bool                            setup_new_NOA_schedule;
    ieee80211_p2p_go_schedule_req   *tmp_schedule = NULL;

    switch (event_type) {
    case P2P_GO_PS_EVENT_TX_BCN:
    {
        /* Beacon is transmitted */
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. event_type=P2P_GO_PS_EVENT_EVENT_TX_BCN. ctx=0x%x.\n", __func__, ctx);

        setup_new_NOA_schedule = false;
        num_NOA_desc = 0;

        spin_lock(&(p2p_go_ps->noa_ie_lock));

        /* Check if we need to update the continuous NOA schedules */
        update_continuous_noa(p2p_go_ps);

        /* Check to see if we need to update the Opp PS or CTWin settings */
        if (p2p_go_ps->pending_ps_change) {
            p2p_go_ps->opp_ps_en = p2p_go_ps->noa_ie.oppPS;
            p2p_go_ps->have_ctwin = (p2p_go_ps->noa_ie.ctwindow != 0) ? 1:0;
            p2p_go_ps->ctwin_duration = p2p_go_ps->noa_ie.ctwindow * TIME_UNIT_TO_MICROSEC;

            p2p_go_ps->pending_ps_change = false;
        }

        /* Check to see if we need to plumb a new NOA schedule */
        if (p2p_go_ps->noa_schedule_updated && 
            TSFTIME_IS_GREATER(p2p_go_ps->curr_tbtt_tsf, p2p_go_ps->noa_change_time)) {
            size_t      alloc_size;

            /* Allocate a temp buffer to store the new schedule */
            num_NOA_desc = p2p_go_ps->noa_ie.num_descriptors;
            alloc_size = sizeof(ieee80211_p2p_go_schedule_req) * (num_NOA_desc);
            
            tmp_schedule = (ieee80211_p2p_go_schedule_req *) OS_MALLOC(p2p_go_ps->os_handle, 
                                                    alloc_size,
                                                    0);
            if (tmp_schedule == NULL) {
                IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
                    "%s : Memory alloction failed. req size=%d\n", 
                    __func__, alloc_size); 
            }
            else {
                setup_new_NOA_schedule = true;

                /* Copy the rest of NOA schedule */
                OS_MEMCPY(&(tmp_schedule[0]), p2p_go_ps->curr_NOA_schedule, 
                          sizeof(ieee80211_p2p_go_schedule_req) * num_NOA_desc);
            }

            p2p_go_ps->noa_schedule_updated = false;
        }

        spin_unlock(&(p2p_go_ps->noa_ie_lock));

        if (setup_new_NOA_schedule) {
            int     retval;


            /* Note: we set the new schedule outside of the lock "noa_ie_lock" */
            retval = ieee80211_p2p_go_schedule_setup(p2p_go_ps->go_scheduler, 
                                                     num_NOA_desc, tmp_schedule);
            if (retval != 0) {
                IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
                    "%s: ieee80211_p2p_go_schedule_setup returns error=0x%x.\n", __func__, retval);
            }
            OS_FREE(tmp_schedule);
            tmp_schedule = NULL;
        }
        ASSERT(tmp_schedule == NULL);

        if (p2p_go_ps->have_ctwin) {
            /* Have CT Win */
            IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
                "%s: event=%s and have CT Win\n", __func__, event_name(event_type));
            ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_CTWIN);
        }
        else {
            /* No CT Win */
            if (atomic_read(&p2p_go_ps->noa_asleep)) {
                IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
                    "%s: event=%s and in NOA Sleep\n", __func__, event_name(event_type));
                ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_DOZE);
            }
            else if (p2p_go_ps->opp_ps_en) {

                if (ieee80211_mlme_check_all_nodes_asleep(p2p_go_ps->vap)) {
                    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
                        "%s: event=%s and during Opp PS, all station asleep.\n", __func__, event_name(event_type));
                    ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_DOZE);
                }
                else {
                    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
                        "%s: event=%s and in Opp PS\n", __func__, event_name(event_type));
                    ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_OPP_PS);
                }
            }
            else {
                IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
                    "%s: event=%s and goto Active\n", __func__, event_name(event_type));
                ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_ACTIVE);
            }
        }
        break;
    }

    case P2P_GO_PS_EVENT_STOPPED:
    {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. event_type=P2P_GO_PS_EVENT_STOPPED. ctx=0x%x.\n", __func__, ctx);

        ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_INIT);
        break;
    }

    case P2P_GO_PS_EVENT_TIMEOUT_TX_BCN:
    case P2P_GO_PS_EVENT_TSF_CHANGED:
    {
        ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_TSF_RESYNC);
        break;
    }

    case P2P_GO_PS_EVENT_ONESHOT_NOA_SLEEP:
    {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. event_type=%s. Going to doze state\n", __func__, event_name(event_type));

        p2p_go_ps->oneshot_noa = true;
        ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_DOZE);
        break;
    }

    default:
        retVal = false;
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. rejected event_type=%s. ctx=0x%x.\n", __func__, event_name(event_type), ctx);
        break;
    }

    return retVal;
}

/*
 * ****************************** State CTWIN ******************************
 * Description: CT Windows phase. GO is active. 
 */

static void p2p_go_ps_state_ctwin_entry(void *ctx) 
{
    int                     retval;
    ieee80211_p2p_go_ps_t   p2p_go_ps = (ieee80211_p2p_go_ps_t) ctx;

    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
        "%s: called. ctx=0x%x.\n", __func__, ctx);

    if (p2p_go_ps->ctwin_duration) {
        /* start CTWin timer */
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: tbtt tsf=0x%x, ctwin-dur=0x%x, set CTWin timeout=0x%x\n", 
               __func__, p2p_go_ps->curr_tbtt_tsf, p2p_go_ps->ctwin_duration,
               p2p_go_ps->curr_tbtt_tsf + p2p_go_ps->ctwin_duration);
        retval = start_timer(p2p_go_ps, P2P_GO_PS_TIMER_CTWIN, 
                    p2p_go_ps->curr_tbtt_tsf + p2p_go_ps->ctwin_duration);
        if (retval != 0) {
            IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
                "%s: ERROR: start_timer() returns error=%d\n", __func__, retval);
        }
    }
    else {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
            "%s: Error: CTWin duration is zero. Should not be in here.\n", __func__);
        ieee80211_sm_dispatch(p2p_go_ps->hsm_handle, P2P_GO_PS_EVENT_CTWIN_EXPIRE, 0, NULL);
    }
}

static void p2p_go_ps_state_ctwin_exit(void *ctx) 
{
    ieee80211_p2p_go_ps_t   p2p_go_ps = (ieee80211_p2p_go_ps_t) ctx;

    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
        "%s: called. ctx=0x%x.\n", __func__, ctx);

    /* Stop the CTWin timeout timer */
    stop_timer(p2p_go_ps, P2P_GO_PS_TIMER_CTWIN);
}

static bool p2p_go_ps_state_ctwin_event(
    void *ctx, u_int16_t event_type, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_p2p_go_ps_t   p2p_go_ps = (ieee80211_p2p_go_ps_t) ctx;
    bool                    retVal = true;

    switch (event_type) {

    case P2P_GO_PS_EVENT_CTWIN_EXPIRE:
    {
        if (atomic_read(&p2p_go_ps->noa_asleep)) {
            IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
                "%s: event=%s and in NOA Sleep\n", __func__, event_name(event_type));
            ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_DOZE);
        }
        else if (p2p_go_ps->opp_ps_en) {
            if (ieee80211_mlme_check_all_nodes_asleep(p2p_go_ps->vap)) {
                IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
                    "%s: event=%s and during Opp PS, all station asleep.\n", __func__, event_name(event_type));
                ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_DOZE);
            }
            else {
                IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
                    "%s: event=%s and in Opp PS\n", __func__, event_name(event_type));
                ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_OPP_PS);
            }
        }
        else {
            IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
                "%s: event=%s and goto Active\n", __func__, event_name(event_type));
            ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_ACTIVE);
        }
        break;
    }

    case P2P_GO_PS_EVENT_STOPPED:
    {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. event_type=P2P_GO_PS_EVENT_STOPPED. ctx=0x%x.\n", __func__, ctx);

        ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_INIT);
        break;
    }

    case P2P_GO_PS_EVENT_TSF_CHANGED:
    {
        ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_TSF_RESYNC);
        break;
    }

    case P2P_GO_PS_EVENT_ONESHOT_NOA_SLEEP:
    {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. event_type=%s. Going to doze state\n", __func__, event_name(event_type));

        p2p_go_ps->oneshot_noa = true;
        ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_DOZE);
        break;
    }

    case P2P_GO_PS_EVENT_PERIOD_NOA_SLEEP:
        /* Do nothing */
        break;

#if 0   /* Ignore this case for now due to another problem where we get 
         * multiple tx beacon completion callbacks. When that problem is solved,
         * then we can re-enable this code again. */
    case P2P_GO_PS_EVENT_TX_BCN:
    {
        /* Error: not supposed to get this event Tx BCN in this state. A timer is probably lost */
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
            "%s: Not supposed to get this event. Do re-sync.\n", __func__, event_name(event_type));
        ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_TSF_RESYNC);
        break;
    }
#endif

    default:
        retVal = false;
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. rejected event_type=%s. ctx=0x%x.\n", __func__, event_name(event_type), ctx);
        break;
    }

    return retVal;
}

/*
 * ****************************** State OPP_PS ******************************
 * Description: GO Opportunistic Power Save. Can sleep when all sta clients
 *              are asleep.
 */

static void p2p_go_ps_state_opp_ps_entry(void *ctx) 
{
    ieee80211_p2p_go_ps_t   p2p_go_ps = (ieee80211_p2p_go_ps_t) ctx;

    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
        "%s: called. ctx=0x%x.\n", __func__, ctx);

    ASSERT(p2p_go_ps->opp_ps_en);

    if (ieee80211_mlme_check_all_nodes_asleep(p2p_go_ps->vap)) {
        ieee80211_sm_dispatch(p2p_go_ps->hsm_handle, P2P_GO_PS_EVENT_OPP_PS_OK, 0, NULL);
    }

    /* Start checking for all station asleep */
    atomic_set(&p2p_go_ps->opp_ps_chk_sta, 1);
}

static void p2p_go_ps_state_opp_ps_exit(void *ctx) 
{
    ieee80211_p2p_go_ps_t   p2p_go_ps = (ieee80211_p2p_go_ps_t) ctx;

    /* Stop checking for all station asleep */
    atomic_set(&p2p_go_ps->opp_ps_chk_sta, 0);
}

static bool p2p_go_ps_state_opp_ps_event(
    void *ctx, u_int16_t event_type, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_p2p_go_ps_t   p2p_go_ps = (ieee80211_p2p_go_ps_t) ctx;
    bool                    retVal = true;

    ASSERT(p2p_go_ps->opp_ps_en);

    switch (event_type) {

    case P2P_GO_PS_EVENT_OPP_PS_OK:
    {
        /* This event is to check if all associated stations are sleeping */
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. event_type=%s. All stations asleep. Opp PS ends\n",
               __func__, event_name(event_type));

        ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_DOZE);
        break;
    }

    case P2P_GO_PS_EVENT_TBTT:
    {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. event_type=%s. No chance to sleep. Time for another beacon.\n",
               __func__, event_name(event_type));

        /* Do not get a chance to sleep. Time for another beacon */
        ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_TX_BCN);
        break;
    }

    case P2P_GO_PS_EVENT_STOPPED:
    {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. event_type=P2P_GO_PS_EVENT_STOPPED. ctx=0x%x.\n", __func__, ctx);

        ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_INIT);
        break;
    }

    case P2P_GO_PS_EVENT_TSF_CHANGED:
    {
        ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_TSF_RESYNC);
        break;
    }

    case P2P_GO_PS_EVENT_ONESHOT_NOA_SLEEP:
    {
        p2p_go_ps->oneshot_noa = true;

        ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_DOZE);
        break;
    }

    case P2P_GO_PS_EVENT_PERIOD_NOA_SLEEP:
    {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. event_type=%s. Going to doze state\n", __func__, event_name(event_type));
        ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_DOZE);
        break;
    }

    default:
        retVal = false;
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. rejected event_type=%s. ctx=0x%x.\n", __func__, event_name(event_type), ctx);
        break;
    }

    return retVal;
}

/*
 * ****************************** State ACTIVE ******************************
 * Description: GO is active and awake.
 */

STATE_ENTRY(active);

STATE_EXIT(active);

static bool p2p_go_ps_state_active_event(void *ctx, u_int16_t event_type, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_p2p_go_ps_t   p2p_go_ps = (ieee80211_p2p_go_ps_t) ctx;
    bool                    retVal = true;

    switch (event_type) {

    case P2P_GO_PS_EVENT_ONESHOT_NOA_SLEEP:
    {
        p2p_go_ps->oneshot_noa = true;
        ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_DOZE);
        break;
    }

    case P2P_GO_PS_EVENT_PERIOD_NOA_SLEEP:
    {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. event_type=%s. Going to doze state\n", __func__, event_name(event_type));

        ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_DOZE);
        break;
    }
    
    case P2P_GO_PS_EVENT_TBTT:
    {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. event_type=%s. No chance to sleep. Time for another beacon.\n",
               __func__, event_name(event_type));

        /* Do not get a chance to sleep. Time for another beacon */
        ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_TX_BCN);
        break;
    }

    case P2P_GO_PS_EVENT_STOPPED:
    {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. event_type=P2P_GO_PS_EVENT_STOPPED. ctx=0x%x.\n", __func__, ctx);

        ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_INIT);
        break;
    }
    
    case P2P_GO_PS_EVENT_TSF_CHANGED:
    {
        ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_TSF_RESYNC);
        break;
    }

    default:
        retVal = false;
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. rejected event_type=%s. ctx=0x%x.\n", __func__, event_name(event_type), ctx);
        break;
    }

    return retVal;
}

/*
 * ****************************** State DOZE ******************************
 * Description: GO is asleep.
 */

/*
 * One-shot NOA started. Suspend Tx beacon and Stopped the TBTT timer.
 */
static void
p2p_go_ps_oneshot_noa_started(ieee80211_p2p_go_ps_t p2p_go_ps)
{
    int                     retval;

    retval = ieee80211_mlme_set_beacon_suspend_state(p2p_go_ps->vap, true);
    if (retval != 0) {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. Error=%d from ieee80211_mlme_ap_set_beacon_suspend_state()\n", __func__, retval);
    }

    stop_timer(p2p_go_ps, P2P_GO_PS_TIMER_TBTT);
}

/*
 * One-shot NOA ended. Reenable the Beacon Tx. Restart the TBTT timer.
 */
static void
p2p_go_ps_oneshot_noa_stopped(ieee80211_p2p_go_ps_t p2p_go_ps)
{
    int                     retval;

    retval = ieee80211_mlme_set_beacon_suspend_state(p2p_go_ps->vap, false);
    if (retval != 0) {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. Error=%d from ieee80211_mlme_ap_set_beacon_suspend_state()\n", __func__, retval);
    }

    rearm_tbtt_timer(p2p_go_ps);
    p2p_go_ps->oneshot_noa = false;
}

static void p2p_go_ps_state_doze_entry(void *ctx) 
{
    ieee80211_p2p_go_ps_t   p2p_go_ps = (ieee80211_p2p_go_ps_t) ctx;

    if (p2p_go_ps->oneshot_noa) {
        /* One-shot NOA started. Suspend Tx beacon and Stopped the TBTT timer */
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. One-shot NOA. Stop TBTT timer\n", __func__);
        p2p_go_ps_oneshot_noa_started(p2p_go_ps);
    }

    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
        "%s: called. Need to tell hw that GO is asleep. ctx=0x%x.\n", __func__, ctx);
    ieee80211_vap_force_pause(p2p_go_ps->vap, 0);
    /* only if the vap can sleep then enter network sleep */
    if (ieee80211_vap_cansleep_is_set(p2p_go_ps->vap)) {
        ieee80211_power_arbiter_enter_nwsleep(p2p_go_ps->vap, p2p_go_ps->pwr_arbiter_id);
    }
}

static void p2p_go_ps_state_doze_exit(void *ctx) 
{
    ieee80211_p2p_go_ps_t   p2p_go_ps = (ieee80211_p2p_go_ps_t) ctx;

    if (p2p_go_ps->oneshot_noa) {
        /* One-shot NOA ended. Reenable the Beacon Tx. Restart the TBTT timer */
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. One-shot NOA. Restart TBTT timer\n", __func__);
        p2p_go_ps_oneshot_noa_stopped(p2p_go_ps);
    }

    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
        "%s: called.  Need to tell hw that GO is awake. ctx=0x%x.\n", __func__, ctx);
    /* only if the vap can sleep then exit it out of network sleep */
    if (ieee80211_vap_cansleep_is_set(p2p_go_ps->vap)) {
        ieee80211_power_arbiter_exit_nwsleep(p2p_go_ps->vap, p2p_go_ps->pwr_arbiter_id);
    }

    ieee80211_vap_force_unpause(p2p_go_ps->vap, 0);

}

static bool p2p_go_ps_state_doze_event(void *ctx, u_int16_t event_type, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_p2p_go_ps_t   p2p_go_ps = (ieee80211_p2p_go_ps_t) ctx;
    bool                    retVal = true;

    switch (event_type) {
    
    case P2P_GO_PS_EVENT_TBTT:
    {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. event_type=%s. Wakeup and time for another beacon.\n",
               __func__, event_name(event_type));

        /* Do not get a chance to sleep. Time for another beacon */
        ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_TX_BCN);
        break;
    }

    case P2P_GO_PS_EVENT_ONESHOT_NOA_SLEEP:
    {
        if (p2p_go_ps->oneshot_noa) {
            IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_GO_POWER,
                "%s: called. event_type=%s. Warning: Already in one-shot NOA but this event.\n",
                   __func__, event_name(event_type));
        }
        else {
            /* One-shot NOA start. */
            IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_GO_POWER,
                "%s: called. One-shot NOA started.\n", __func__);
            p2p_go_ps->oneshot_noa = true;
            p2p_go_ps_oneshot_noa_started(p2p_go_ps);
        }
        break;
    }

    case P2P_GO_PS_EVENT_PERIOD_NOA_SLEEP:
        /* Ignore this event */
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_GO_POWER,
            "%s: event=%s and already asleep. Do nothing.\n", __func__, event_name(event_type));
        break;

    case P2P_GO_PS_EVENT_NOA_WAKE:
    {
        if (p2p_go_ps->oneshot_noa)
        {
            struct ieee80211com *ic;
            u_int64_t           curr_tsf64;
            u_int32_t           offset_from_tbtt;
            /*
             * One-shot NOA. We need to know if we are in :
             * (1) CTWin, (2) Opp PS with all STA asleep, (3) Opp PS with waked STA, or (4) active
             */

            /* Calculate the current TBTT TSF. Result is stored in p2p_go_ps->curr_tbtt_tsf */
            ic = p2p_go_ps->ic;
            curr_tsf64 = ic->ic_get_TSF64(ic);
            calc_curr_tbtt_tsf(p2p_go_ps, curr_tsf64);

            ASSERT(TSFTIME_IS_GREATER_EQ((u_int32_t)curr_tsf64, p2p_go_ps->curr_tbtt_tsf));
            offset_from_tbtt = (u_int32_t)curr_tsf64 - p2p_go_ps->curr_tbtt_tsf;
            ASSERT(offset_from_tbtt <= p2p_go_ps->tbtt_intval);

            if ((p2p_go_ps->have_ctwin) && (p2p_go_ps->ctwin_duration > 0) &&
                (offset_from_tbtt < p2p_go_ps->ctwin_duration))
            {
                /* Have CT Win with non-zero duration and within the CT Window */
                ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_CTWIN);
            }
            else if (p2p_go_ps->opp_ps_en) {
                if (ieee80211_mlme_check_all_nodes_asleep(p2p_go_ps->vap)) {
                    /* 
                     * Opp. PS is enabled and all nodes are asleep. Continue sleeping but enable TBTT.
                     */
                    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_GO_POWER,
                        "%s: called. One-shot NOA stopped. All STA asleep. Restart TBTT timer but \n", __func__);
                    p2p_go_ps_oneshot_noa_stopped(p2p_go_ps);
                }
                else {
                    /* Switch to Opp PS state */
                    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
                        "%s: event=%s and some sta awake. Wakeup.\n", __func__, event_name(event_type));
                    ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_OPP_PS);
                }
            }
            else {
                /* Wake up */
                IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
                    "%s: event=%s from one-shot NOA and goto Active\n", __func__, event_name(event_type));
                ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_ACTIVE);
            }
        }
        else {
            /* Periodic NOA */
            if (p2p_go_ps->opp_ps_en) {
                /* Opp. PS is enabled. Check if we need to wake up. */
                if (ieee80211_mlme_check_all_nodes_asleep(p2p_go_ps->vap)) {
                    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
                        "%s: event=%s and during Opp PS, all station asleep. Stay in Doze\n", __func__, event_name(event_type));
                }
                else {
                    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
                        "%s: event=%s and some sta awake. Wakeup.\n", __func__, event_name(event_type));
                    ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_OPP_PS);
                }
            }
            else {
                IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
                    "%s: event=%s and goto Active\n", __func__, event_name(event_type));
                ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_ACTIVE);
            }
        }
        break;
    }
    
    case P2P_GO_PS_EVENT_STOPPED:
    {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. event_type=P2P_GO_PS_EVENT_STOPPED. ctx=0x%x.\n", __func__, ctx);

        ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_INIT);
        break;
    }
    
    case P2P_GO_PS_EVENT_TX_BCN:
    {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. got unexpected event TX_BCN. Do a re-sync.\n", __func__);
        ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_TSF_RESYNC);
        break;
    }

    case P2P_GO_PS_EVENT_TSF_CHANGED:
    {
        ieee80211_sm_transition_to(p2p_go_ps->hsm_handle, P2P_GO_PS_STATE_TSF_RESYNC);
        break;
    }

    default:
        retVal = false;
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. rejected event_type=%s. ctx=0x%x.\n", __func__, event_name(event_type), ctx);
        break;
    }

    return retVal;
}

/************************ END OF STATE MACHINE DEFINITIONS **************************/

/* State Machine for the GO Power Save */
ieee80211_state_info p2p_go_ps_sm_info[] = {
/*
    Fields of ieee80211_state_info:
    u_int8_t  state;                - the state id
    u_int8_t  parent_state;         - valid for sub states 
    u_int8_t  initial_substate;     - initial substate for the state 
    u_int8_t  has_substates;        - true if this state has sub states 
    const char      *name;          - name of the state 
    void (*ieee80211_hsm_entry);    - entry action routine 
    void (*ieee80211_hsm_exit);     - exit action routine  
    bool (*ieee80211_hsm_event)     - event handler. returns true if event is handled
*/
    { 
        (u_int8_t) P2P_GO_PS_STATE_INIT, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "GO_PS_INIT",
        p2p_go_ps_state_idle_entry,
        p2p_go_ps_state_idle_exit,
        p2p_go_ps_state_idle_event
    },
    { 
        (u_int8_t) P2P_GO_PS_STATE_TSF_RESYNC, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "GO_PS_TSF_RESYNC",
        p2p_go_ps_state_tsf_resync_entry,
        p2p_go_ps_state_tsf_resync_exit,
        p2p_go_ps_state_tsf_resync_event
    },
    { 
        (u_int8_t) P2P_GO_PS_STATE_ACTIVE, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "GO_PS_ACTIVE",
        p2p_go_ps_state_active_entry,
        p2p_go_ps_state_active_exit,
        p2p_go_ps_state_active_event
    },
    { 
        (u_int8_t) P2P_GO_PS_STATE_DOZE, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "GO_PS_DOZE",
        p2p_go_ps_state_doze_entry,
        p2p_go_ps_state_doze_exit,
        p2p_go_ps_state_doze_event
    },
    { 
        (u_int8_t) P2P_GO_PS_STATE_OPP_PS, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "GO_PS_OPP_PS",
        p2p_go_ps_state_opp_ps_entry,
        p2p_go_ps_state_opp_ps_exit,
        p2p_go_ps_state_opp_ps_event
    },
    { 
        (u_int8_t) P2P_GO_PS_STATE_TX_BCN, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "GO_PS_TX_BCN",
        p2p_go_ps_state_tx_bcn_entry,
        p2p_go_ps_state_tx_bcn_exit,
        p2p_go_ps_state_tx_bcn_event
    },
    { 
        (u_int8_t) P2P_GO_PS_STATE_CTWIN, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "GO_PS_CTWIN",
        p2p_go_ps_state_ctwin_entry,
        p2p_go_ps_state_ctwin_exit,
        p2p_go_ps_state_ctwin_event
    },
};

static void p2p_go_ps_sm_debug_print (void *ctx, const char *fmt,...) 
{
#if 0 /* DEBUG : remove unnecesssary time of curr_tsf64. */
    char tmp_buf[256];
    va_list ap;
    u_int64_t           curr_tsf64;
    ieee80211_p2p_go_ps_t       p2p_go_ps = (ieee80211_p2p_go_ps_t)ctx;
    curr_tsf64 = p2p_go_ps->ic->ic_get_TSF64(p2p_go_ps->ic);
     
    va_start(ap, fmt);
    vsnprintf (tmp_buf, 256, fmt, ap);
    va_end(ap);
    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_GO_POWER,
                         "tsf: %lld TU:%lld: %s",curr_tsf64, (curr_tsf64>>10),tmp_buf);
#else
    char tmp_buf[256];
    va_list ap;
    ieee80211_p2p_go_ps_t       p2p_go_ps = (ieee80211_p2p_go_ps_t)ctx;

    /* TBD : ic->ic_get_TSF64 involve 2 WMI commands and cause large timing delay even debug is OFF. 
     *       Need another way to debug for the split driver with TSF timestamp.
     */
    va_start(ap, fmt);
    vsnprintf (tmp_buf, 256, fmt, ap);
    va_end(ap);

    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_GO_POWER, " %s", tmp_buf);
#endif                
}

static void go_scheduler_callback(
    ieee80211_p2p_go_schedule_t go_schedule, 
    ieee80211_p2p_go_schedule_event *event, 
    void *arg,
    bool one_shot_noa)
{
    ieee80211_p2p_go_ps_t   p2p_go_ps;

    p2p_go_ps = (ieee80211_p2p_go_ps_t)arg;

    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
        "%s: Event type=%s\n", __func__,
           (event->type == IEEE80211_P2P_GO_SCHEDULE_EVENT_GO_ABSENT)? "NOA" : "Presence");

    if (!one_shot_noa) {
        if (event->type == IEEE80211_P2P_GO_SCHEDULE_EVENT_GO_PRESENT) {
            atomic_set(&p2p_go_ps->noa_asleep, 0);  /* wakeup */
        }
        else {
            atomic_set(&p2p_go_ps->noa_asleep, 1);  /* sleeping */
        }
    }

    if (event->type == IEEE80211_P2P_GO_SCHEDULE_EVENT_GO_PRESENT) {
        /* GO woke up */
        ieee80211_sm_dispatch(p2p_go_ps->hsm_handle, P2P_GO_PS_EVENT_NOA_WAKE, 0, NULL);
    }
    else if (event->type == IEEE80211_P2P_GO_SCHEDULE_EVENT_GO_ABSENT) {
        if (one_shot_noa) {
            /* GO asleep due to one-shot NOA */
            ieee80211_sm_dispatch(p2p_go_ps->hsm_handle, P2P_GO_PS_EVENT_ONESHOT_NOA_SLEEP, 0, NULL);
        }
        else {
            /* GO is asleep due to periodic NOA */
            ieee80211_sm_dispatch(p2p_go_ps->hsm_handle, P2P_GO_PS_EVENT_PERIOD_NOA_SLEEP, 0, NULL);
        }
    }
}

/*
 * Registered VAP event/notification handler
 */
static void p2p_go_ps_vap_evhandler(ieee80211_vap_t vap, ieee80211_vap_event *event, void *arg)
{
    ieee80211_p2p_go_ps_t   p2p_go_ps = (ieee80211_p2p_go_ps_t) arg;

#define CASE_EVENT(_type)                                                                   \
    case _type:                                                                             \
        if (event->type == _type)                                                           \
            IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,   \
                "%s: called "#_type" event_type=%d.\n", __func__, event->type);

    switch(event->type) {
    case IEEE80211_VAP_UP:
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. IEEE80211_VAP_UP event_type=%d.\n", __func__, event->type);
        p2p_go_ps->up = true;

        /* See if we need to kickstart the Power Save state machine */
        p2p_go_ps_param_changed(p2p_go_ps);
        break;

    case IEEE80211_VAP_ACTIVE:
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. IEEE80211_VAP_ACTIVE event_type=%d.\n", __func__, event->type);
        break;

    case IEEE80211_VAP_PAUSED:
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. IEEE80211_VAP_PAUSED event_type=%d.\n", __func__, event->type);
        break;

    case IEEE80211_VAP_UNPAUSED:
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. IEEE80211_VAP_UNPAUSED event_type=%d.\n", __func__, event->type);
        break;

    case IEEE80211_VAP_CSA_COMPLETE:
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. IEEE80211_VAP_CSA_COMPLETE event_type=%d.\n", __func__, event->type);
        break;

    CASE_EVENT(IEEE80211_VAP_DOWN)
        /* Vap is going down. Hence the PS state machine should also go down */
        p2p_go_ps->up = false;
        ieee80211_sm_dispatch(p2p_go_ps->hsm_handle, P2P_GO_PS_EVENT_STOPPED, 0, NULL);
        break;

    case IEEE80211_VAP_FULL_SLEEP:
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. IEEE80211_VAP_FULL_SLEEP event_type=%d.\n", __func__, event->type);
        break;

    default:
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: called. unknown event_type=%d.\n", __func__, event->type);
        break;
    }

#undef CASE_EVENT
}

/*
 * Routine to set a new NOA schedule into the GO vap.
 * Note: we will advertise this new NOA schedule in the beacon and probe responses
 * immediately but to start only at the next beacon interval.
 */
int wlan_p2p_GO_ps_set_noa_schedule(
    ieee80211_p2p_go_ps_t           p2p_go_ps,
    u_int8_t                        num_schedules,
    ieee80211_p2p_go_schedule_req   *request)
{
    int                                     retval = 0;
    int                                     i;
    u_int32_t                               earliest_start_time;
    bool                                    have_continuous_noa = false;

    /* This structure is about 84 bytes. If it is too large on the stack, then
       should consider allocating it from heap */
    struct ieee80211_p2p_sub_element_noa    new_noa_ie;
    ASSERT(p2p_go_ps);

    ASSERT(p2p_go_ps->go_scheduler);

    if (num_schedules > 0) {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: num=%d, Type=%d, Pri=%d, 1st Req: count=%d, dur=0x%x, int=0x%x, start=0x%x\n",
                __func__, num_schedules, request->type, request->pri,
                request->noa_descriptor.type_count, 
                request->noa_descriptor.duration, 
                request->noa_descriptor.interval, 
                request->noa_descriptor.start_time);
    }
    else {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: num=%d\n",
                __func__, num_schedules);
    }

    if (p2p_go_ps->init_curr_tbtt_tsf == false) {
        /* If the state machine has not started, then curr_tbtt_tsf is uninitialized */
        struct ieee80211com *ic;
        u_int64_t           curr_tsf64;

        ic = p2p_go_ps->ic;
        curr_tsf64 = ic->ic_get_TSF64(ic);
        calc_curr_tbtt_tsf(p2p_go_ps, curr_tsf64);
    }

    if (num_schedules > 0) {
        earliest_start_time = request[0].noa_descriptor.start_time;
    }
    else {
        earliest_start_time = p2p_go_ps->curr_tbtt_tsf + p2p_go_ps->tbtt_intval;
    }

    /* Do some simple checks on the new schedule and find earliest start time */
    for (i = 0; i < num_schedules; i++) {

        if (request[i].type != IEEE80211_P2P_GO_ABSENT_SCHEDULE) {
            /* Error: can only accept absent schedules */
            IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
                "%s: Error: can only accept absent schedules. type=%d\n", __func__, request[i].type);
            retval = EINVAL;
            break;
        }

        /* Find the earliest start time */
        if (TSFTIME_IS_SMALLER(request[i].noa_descriptor.start_time, earliest_start_time)) {
            earliest_start_time = request[i].noa_descriptor.start_time;
        }

        if (request[i].noa_descriptor.type_count != 1) {
            /* Not a one-shot NOA */
            if (request[i].noa_descriptor.duration >= request[i].noa_descriptor.interval) {
                /* Error: we cannot allow duration to be larger than interval */
                IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
                    "%s: Error: one-shot NOA [i] : duration(0x%x) >= interval (0x%x)\n",
                    __func__, i, request[i].noa_descriptor.duration,
                    request[i].noa_descriptor.interval);
                retval = EINVAL;
                break;
            }

        }

        if (request[i].noa_descriptor.type_count != 255) {
            /* Not a continuous NOA */
            if (TSFTIME_IS_GREATER_EQ((request[i].noa_descriptor.interval * request[i].noa_descriptor.type_count), 
                                      (0x7FFFFFFF)))
            {
                /* Error: we cannot allow (iterations * interval) to exceed 2^31 */
                IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
                    "%s: Error: NOA[%d] interval (0x%x) with %d iterations is too large.\n",
                    __func__, i, request[i].noa_descriptor.interval, 
                    request[i].noa_descriptor.type_count);
                retval = EINVAL;
                break;
            }
        }
        else {
            /* Got a continuous NOA */
            have_continuous_noa = true;
        }

#if DBG
        /* For one shot NOA, it must start after the next beacon */
        if (request[i].noa_descriptor.type_count == 1) {
            u_int32_t       next_tbtt;

            /* TODO: This code needs to be synchronized */
            next_tbtt = p2p_go_ps->curr_tbtt_tsf + p2p_go_ps->tbtt_intval;
            if (request[i].noa_descriptor.start_time <= next_tbtt) {
                IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
                    "%s: Warning: one-shot NOA start time (0x%x) before next beacon (0x%x)\n",
                    __func__, request[i].noa_descriptor.start_time, next_tbtt);
            }
        }
#endif
    }

    if (retval != 0) {
        return retval;
    }

    spin_lock(&(p2p_go_ps->noa_ie_lock));

    /*
     * TODO: we are switching to the new NOA IE immediately. Ideally, we should
     * try to switch 1 beacon before.
     */

    p2p_go_ps->noa_change_time = earliest_start_time - p2p_go_ps->tbtt_intval;

    /* Make a copy of the new schedule */
    OS_MEMCPY(p2p_go_ps->curr_NOA_schedule, request,
          sizeof(ieee80211_p2p_go_schedule_req) * num_schedules);
    p2p_go_ps->curr_num_descriptors = num_schedules;

    if (have_continuous_noa) {
        p2p_go_ps->have_continuous_noa_schedule = true;
        p2p_go_ps->continuous_noa_change_time = p2p_go_ps->curr_tbtt_tsf +
                                                P2P_GO_NEXT_NOA_UPDATE_TIME;
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_DETAILED, IEEE80211_MSG_P2P_GO_POWER,
            "%s: Have continuous NOA. next change time=0x%x. Curr TBTT=0x%x\n",
            __func__, p2p_go_ps->continuous_noa_change_time, p2p_go_ps->curr_tbtt_tsf);
    }
    else {
        p2p_go_ps->have_continuous_noa_schedule = false;
    }

    p2p_go_plumb_new_noa_schedule(p2p_go_ps);

    /* Make a copy of the NOA IE to use outside the lock */
    OS_MEMCPY(&new_noa_ie, &p2p_go_ps->noa_ie,
              sizeof(struct ieee80211_p2p_sub_element_noa));

    spin_unlock(&(p2p_go_ps->noa_ie_lock));

    /* Check if we need to kickstart or stop the PS state machine */
    p2p_go_ps_param_changed(p2p_go_ps);

    /* Inform the P2P GO that there is a new NOA IE */
    wlan_p2p_go_update_noa_ie(p2p_go_ps->p2p_go_handle, &new_noa_ie, p2p_go_ps->tsf_offset);

    return retval;
}

static int 
wlan_p2p_GO_ps_set_Opp_Ps(
    ieee80211_p2p_go_ps_t           p2p_go_ps,
    u_int32_t                       param)
{
    /* This structure is about 84 bytes. If it is too large on the stack, then
       should consider allocating it from heap */
    struct ieee80211_p2p_sub_element_noa    new_noa_ie;
    bool    new_opp_ps = (param == 0)? false:true;

    spin_lock(&(p2p_go_ps->noa_ie_lock));

    if (new_opp_ps == p2p_go_ps->noa_ie.oppPS) {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_GO_POWER,
            "%s: no change to Opp Ps. Curr state=%d\n",
            __func__, p2p_go_ps->noa_ie.oppPS);
        spin_unlock(&(p2p_go_ps->noa_ie_lock));
        return 0;   /* No change */
    }
    p2p_go_ps->noa_ie.index++;      /* Increment the instance count */

    p2p_go_ps->noa_ie.oppPS = new_opp_ps;
    p2p_go_ps->pending_ps_change = true;
    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_GO_POWER,
        "%s: change to Opp Ps change. New state=%d\n",
        __func__, p2p_go_ps->noa_ie.oppPS);

    OS_MEMCPY(&new_noa_ie, &p2p_go_ps->noa_ie,
              sizeof(struct ieee80211_p2p_sub_element_noa));

    spin_unlock(&(p2p_go_ps->noa_ie_lock));
    
    /* Check if we need to kickstart or stop the PS state machine */
    p2p_go_ps_param_changed(p2p_go_ps);

    /* Inform the P2P GO that there is a new NOA IE */
    wlan_p2p_go_update_noa_ie(p2p_go_ps->p2p_go_handle, &new_noa_ie, p2p_go_ps->tsf_offset);
    
    return 0;
}

static int 
wlan_p2p_GO_ps_set_CtWin(
    ieee80211_p2p_go_ps_t           p2p_go_ps,
    u_int32_t                       param)
{
    /* This structure is about 84 bytes. If it is too large on the stack, then
       should consider allocating it from heap */
    struct ieee80211_p2p_sub_element_noa    new_noa_ie;

    /* 
     * NOTE: if param==0 then CTWin is disabled. Else param will contain the
     * CTWin duration in Time units (not microseconds).
     */
    if ((param & IEEE80211_P2P_NOE_IE_CTWIN_MASK) != param) {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
            "%s: Invalid Param=%d\n",
            __func__, param);
        return EINVAL;
    }

    spin_lock(&(p2p_go_ps->noa_ie_lock));
    
    if (p2p_go_ps->noa_ie.ctwindow == param) {
        /* No change in CTWin duration */
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_GO_POWER,
            "%s: no change to CT Window. Curr state=%d\n",
            __func__, p2p_go_ps->noa_ie.ctwindow);
        spin_unlock(&(p2p_go_ps->noa_ie_lock));
        return 0;   /* No change */
    }

    if (param >= (p2p_go_ps->tbtt_intval / TIME_UNIT_TO_MICROSEC)) {
        /* CT Win is too big. Truncate it. */
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
            "%s: CT Window bigger than TBTT. Truncate from=%d to=%d\n",
            __func__, param, p2p_go_ps->tbtt_intval / TIME_UNIT_TO_MICROSEC);
        param = p2p_go_ps->tbtt_intval / TIME_UNIT_TO_MICROSEC;
    }
    p2p_go_ps->noa_ie.index++;      /* Increment the instance count */
    p2p_go_ps->noa_ie.ctwindow = param;
    p2p_go_ps->pending_ps_change = true;

    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_GO_POWER,
        "%s: CTWin is changed to %d TU.\n", 
        __func__, p2p_go_ps->noa_ie.ctwindow);

    OS_MEMCPY(&new_noa_ie, &p2p_go_ps->noa_ie,
              sizeof(struct ieee80211_p2p_sub_element_noa));

    spin_unlock(&(p2p_go_ps->noa_ie_lock));

    /* Inform the P2P GO that there is a new NOA IE */
    wlan_p2p_go_update_noa_ie(p2p_go_ps->p2p_go_handle, &new_noa_ie, p2p_go_ps->tsf_offset);

    return 0;
}

/*
 * Set a parameter in the P2P GO Power Saving Module.
 * Return 0 if succeed.
 */
int ieee80211_p2p_go_power_set_param(
    ieee80211_p2p_go_ps_t           p2p_go_ps,
    ieee80211_p2p_go_ps_param_type  type,
    u_int32_t                       param)
{
    int         ret = 0;

    switch(type) {
    case P2P_GO_PS_OPP_PS:
    {
        /* Opportunistic Power Save */
        ret = wlan_p2p_GO_ps_set_Opp_Ps(p2p_go_ps, param);
        break;
    }
    case P2P_GO_PS_CTWIN:
    {
        /* Contention Window setting */
        ret = wlan_p2p_GO_ps_set_CtWin(p2p_go_ps, param);
        break;
    }
    default:
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
             "%s: ERROR: invalid param type=%d\n", __func__, type);
        return EINVAL;
    }

    return ret;
}

static void
go_ps_ap_mlme_event_handler(ieee80211_vap_t vap, ieee80211_mlme_event *event, void *arg)
{
    ieee80211_p2p_go_ps_t       p2p_go_ps = (ieee80211_p2p_go_ps_t) arg;
    bool pwr_save_on;

    ASSERT(arg);

    pwr_save_on = (event->type == IEEE80211_MLME_EVENT_STA_ENTER_PS);        /* sta enters power save */

    if (atomic_read(&p2p_go_ps->opp_ps_chk_sta) == 0) {
        /* We are not checking for Opportunistic power sleep right now */
        return;
    }
    if (pwr_save_on) {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
            "%s: node sleeps. num ps_sta=%d\n",__func__, p2p_go_ps->vap->iv_ps_sta);
    }

    /* We are only interested in station going to sleep */
    if (pwr_save_on && ieee80211_mlme_check_all_nodes_asleep(p2p_go_ps->vap)) {
        /* All station asleep. Can sleep now */
        ieee80211_sm_dispatch(p2p_go_ps->hsm_handle, P2P_GO_PS_EVENT_OPP_PS_OK, 0, NULL);
    }
}

/* Callback Function for the ATH_INFO notify when certain ATH information changes. */
static void
p2p_GO_PS_ath_info_notify(
    void *arg, 
    ath_vap_infotype type,
    u_int32_t param1, 
    u_int32_t param2)
{
    ieee80211_p2p_go_ps_t       p2p_go_ps = NULL;

    ASSERT(arg);
    p2p_go_ps = (ieee80211_p2p_go_ps_t)arg;

    if (type & ATH_VAP_INFOTYPE_SLOT_OFFSET) {
        /* Our VAP assigned Slot has changed. */
        u_int32_t   new_tsf_offset = (u_int32_t)param1;
        if ((p2p_go_ps->tsf_offset != new_tsf_offset) && 
            (p2p_go_ps->noa_ie.num_descriptors > 0))
        {
            /* Update the NOE IE's. since the TSF Offset are different */
            /* This structure is about 84 bytes. If it is too large on the stack, then
               should consider allocating it from heap */
            struct ieee80211_p2p_sub_element_noa    new_noa_ie;

            IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_GO_POWER,
                "%s: Updating NOA IE for new TSF Offset=%d\n", __func__, new_tsf_offset);

            spin_lock(&(p2p_go_ps->noa_ie_lock));

            p2p_go_ps->noa_ie.index++;      /* Increment the instance count */

            OS_MEMCPY(&new_noa_ie, &p2p_go_ps->noa_ie,
                      sizeof(struct ieee80211_p2p_sub_element_noa));

            spin_unlock(&(p2p_go_ps->noa_ie_lock));

            /* Inform the P2P GO that there is a new NOA IE since TSF offset has changed */
            wlan_p2p_go_update_noa_ie(p2p_go_ps->p2p_go_handle, &new_noa_ie, new_tsf_offset);
        }
        p2p_go_ps->tsf_offset = new_tsf_offset;
    }
}

/*
 * Return 0 if succeed.
 */
ieee80211_p2p_go_ps_t ieee80211_p2p_go_power_vattach(osdev_t os_handle, wlan_p2p_go_t p2p_go_handle)
{
    ieee80211_p2p_go_ps_t       p2p_go_ps = NULL;
    int                         retval = 0;
    ieee80211_p2p_go_schedule_t go_scheduler = NULL;
    ieee80211_hsm_t             hsm_handle = NULL;
    bool                        vap_event_handler_registered = false;
    bool                        timer_alloc = false;
    wlan_if_t                   vap;
    wlan_dev_t                  devhandle;
    ieee80211_vap_ath_info_notify_t ath_info_notify = NULL;
    char requestor_name[]="P2P_GO";

    vap = wlan_p2p_GO_get_vap_handle(p2p_go_handle);
    devhandle = wlan_vap_get_devhandle(vap);

    IEEE80211_DPRINTF_IC(devhandle, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
        "%s: called", __func__);
    
    do {
        /* Allocate memory for P2P GO PS data structures */
        p2p_go_ps = (ieee80211_p2p_go_ps_t) OS_MALLOC(os_handle, 
                                                sizeof(struct ieee80211_p2p_go_ps),
                                                0);
        if (p2p_go_ps == NULL) {
            IEEE80211_DPRINTF_IC(devhandle, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
                "%s : P2P GO PS memory alloction failed\n", __func__); 
            retval = -ENOMEM;
            break;
        }

        OS_MEMZERO(p2p_go_ps, sizeof(struct ieee80211_p2p_go_ps));

        p2p_go_ps->ic = devhandle;
        p2p_go_ps->vap = vap;
        p2p_go_ps->os_handle = os_handle;
        p2p_go_ps->p2p_go_handle = p2p_go_handle;
        p2p_go_ps->pwr_arbiter_id = ieee80211_power_arbiter_alloc_id(vap, 
                                                                     requestor_name, 
                                                                     IEEE80211_PWR_ARBITER_TYPE_P2P);
        if (p2p_go_ps->pwr_arbiter_id != 0) {
            (void) ieee80211_power_arbiter_enable(vap, p2p_go_ps->pwr_arbiter_id);
        }

        {
            struct ieee80211_node   *ap_node;

            /* Initialize the TBTT interval and the current TBTT */
            ap_node = ieee80211vap_get_bssnode(vap);
            ASSERT(ap_node != NULL);
            p2p_go_ps->tbtt_intval = ap_node->ni_intval * TIME_UNIT_TO_MICROSEC;     // one TU is 1024 usecs.
        }

        p2p_go_ps->noa_ie.index = 0;
        // No need to init curr_tbtt_tsf here since it is unused until the State Machine starts.
        //calc_curr_tbtt_tsf(p2p_go_ps);

        spin_lock_init(&p2p_go_ps->noa_ie_lock);

        retval = go_init_timers(p2p_go_ps);
        if (retval != 0) {
            break;
        }
        else timer_alloc = true;

        /* Get from the ath layer the TSF offset for this VAP. E.g. the AP VAP will have an offset in
           its TSF value in the Tx beacon. */
        {
            u_int32_t   param1 = 0, param2 = 0;

            /* Get the current TSF Offset */
            retval = ieee80211_vap_ath_info_get(vap, ATH_VAP_INFOTYPE_SLOT_OFFSET, &param1, &param2);
            if (retval != 0) {
                /* Error in getting the current TSF Offset. */
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, 
                    "%s: Error=%d from ieee80211_vap_ath_info_get(SLOT_OFFSET)\n",
                                  __func__, retval);
                retval = -EPERM;
                break;
            }
            p2p_go_ps->tsf_offset = param1;
        }

        /* Create State Machine and start */
        hsm_handle = ieee80211_sm_create(os_handle, 
                         "P2PGOPS",
                         (void *)p2p_go_ps, 
                         P2P_GO_PS_STATE_INIT,
                         p2p_go_ps_sm_info,
                         sizeof(p2p_go_ps_sm_info)/sizeof(ieee80211_state_info),
                         MAX_QUEUED_EVENTS,
                         sizeof(struct p2p_go_ps_sm_event), /* size of event data */
                         MESGQ_PRIORITY_HIGH,
                         /* Note for darwin, we get more accurate timings for the NOA start and stop when synchronous */
                         OS_MESGQ_CAN_SEND_SYNC() ? IEEE80211_HSM_SYNCHRONOUS :
                                                    IEEE80211_HSM_ASYNCHRONOUS,
                         p2p_go_ps_sm_debug_print,
                         p2p_go_ps_sm_event_name,
                         IEEE80211_N(p2p_go_ps_sm_event_name)); 
        if (!hsm_handle) {
            IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
                "%s : ieee80211_sm_create failed \n", __func__);
            retval = -ENOMEM;
            break;
        }

        p2p_go_ps->hsm_handle = hsm_handle;

        /* Register an event handler with the VAP module */
        retval = ieee80211_vap_register_event_handler(vap, p2p_go_ps_vap_evhandler, (void *)p2p_go_ps);
        if (retval != 0) {
            IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
                "%s: Error from ieee80211_vap_register_event_handler=%d\n",
                 __func__, retval);
            break;
        }
        vap_event_handler_registered = true;

        /* Attach to the GO Scheduler module */
        go_scheduler = ieee80211_p2p_go_schedule_create(os_handle, vap);
        if (go_scheduler == NULL) {
            IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
                "%s: ERROR, ieee80211_p2p_go_schedule_create fails.\n", __func__);
            retval = -ENOMEM;
            break;
        }
        p2p_go_ps->go_scheduler = go_scheduler;

        /* Pause the callbacks */
        p2p_go_ps->go_sch_paused = true;
        ieee80211_p2p_go_schedule_pause(go_scheduler);

        retval = ieee80211_p2p_go_schedule_register_event_handler(go_scheduler, go_scheduler_callback, p2p_go_ps);
        if (retval != 0) {
            IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
                "%s: ERROR, ieee80211_p2p_go_schedule_register_event_handler fails=%d.\n",
                   __func__, retval);
            ieee80211_p2p_go_schedule_delete(go_scheduler);
            retval = -EPERM;
            break;
        }

        /* Register the callbacks for the AP Node state changes */
        if ( ieee80211_mlme_register_event_handler( vap,go_ps_ap_mlme_event_handler, p2p_go_ps) != EOK) {
            IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
                "%s: ERROR, ieee80211_mlme_register_event_handler failed \n",
                   __func__);
            retval = -EPERM;
            break;
        }

        /* Register for any change to the TSF Offset for this VAP */
        ath_info_notify = ieee80211_vap_ath_info_reg_notify(vap, ATH_VAP_INFOTYPE_SLOT_OFFSET,
                                                            p2p_GO_PS_ath_info_notify, p2p_go_ps);
        if (ath_info_notify == NULL) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, 
                "%s: Error in init. ATH_INFO_NOTIFY\n", __func__);
            retval = -EPERM;
            break;
        }
        p2p_go_ps->h_ath_info_notify = ath_info_notify;

    } while ( false );

    if (retval != 0) {
        /* Some errors, free all the allocated resources */

        if (ath_info_notify != NULL) {
            ieee80211_vap_ath_info_dereg_notify(ath_info_notify);
            ath_info_notify = NULL;
        }

        ieee80211_mlme_unregister_event_handler( vap,go_ps_ap_mlme_event_handler, p2p_go_ps);

        if (timer_alloc) {
            free_timers(p2p_go_ps);
        }

        if (go_scheduler) {
            ieee80211_p2p_go_schedule_delete(go_scheduler);
        }

        if (vap_event_handler_registered) {
            /* Unregister event handler with VAP module */
            ieee80211_vap_unregister_event_handler(vap, p2p_go_ps_vap_evhandler, (void *)p2p_go_ps);
        }

        if (hsm_handle != NULL) {
            ieee80211_sm_delete(hsm_handle);
        }

        if (p2p_go_ps != NULL) {
            spin_lock_destroy(&p2p_go_ps->noa_ie_lock); // assumes that lock is always init.
            OS_FREE(p2p_go_ps);
        }

        IEEE80211_DPRINTF_IC(devhandle, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_POWER,
            "%s: ERROR: code=%d.\n", __func__, retval);
        return NULL;
    }

    return p2p_go_ps;
}

void ieee80211_p2p_go_power_vdettach(ieee80211_p2p_go_ps_t p2p_go_ps)
{
    wlan_if_t                                   vap;
    int                                         retval = 0;
    struct ieee80211com                         *ic = p2p_go_ps->ic;

    vap = p2p_go_ps->vap;

    IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_POWER,
        "%s: called", __func__);

    /* Inform the P2P GO that there is a no NOA IE */
    wlan_p2p_go_update_noa_ie(p2p_go_ps->p2p_go_handle, NULL, p2p_go_ps->tsf_offset);

    retval = ieee80211_vap_ath_info_dereg_notify(p2p_go_ps->h_ath_info_notify);
    if (retval != 0) {
        IEEE80211_DPRINTF_IC(p2p_go_ps->ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_GO_POWER,
            "%s: ieee80211_vap_ath_info_dereg_notify returns error=%d.\n", __func__, retval);
    }

    /* Disable the Tx Beacon completion callbacks */
    IEEE80211_COMM_LOCK(ic);
    if (p2p_go_ps->h_tx_bcn_notify != NULL) {
        ieee80211_dereg_notify_tx_bcn(p2p_go_ps->h_tx_bcn_notify);
        p2p_go_ps->h_tx_bcn_notify = NULL;
    }
    IEEE80211_COMM_UNLOCK(ic);

    stop_all_timers(p2p_go_ps);
    free_timers(p2p_go_ps);

    if (p2p_go_ps->go_scheduler) {
        ieee80211_p2p_go_schedule_delete(p2p_go_ps->go_scheduler);
    }

    /* Unregister event handler with mlme module */
     ieee80211_mlme_unregister_event_handler( vap,go_ps_ap_mlme_event_handler, p2p_go_ps);

    /* Unregister event handler with VAP module */
    ieee80211_vap_unregister_event_handler(vap, p2p_go_ps_vap_evhandler, (void *)p2p_go_ps);

    /* Release or free the power arbiter requestor ID */
    if (p2p_go_ps->pwr_arbiter_id != 0) {
        (void) ieee80211_power_arbiter_disable(vap, p2p_go_ps->pwr_arbiter_id);
        ieee80211_power_arbiter_free_id(vap, p2p_go_ps->pwr_arbiter_id);
        p2p_go_ps->pwr_arbiter_id = 0;
    }

    /* Delete the State Machine */
    if (p2p_go_ps->hsm_handle) {
        ieee80211_sm_delete(p2p_go_ps->hsm_handle);
    }

    spin_lock_destroy(&p2p_go_ps->noa_ie_lock);

    OS_FREE(p2p_go_ps);
}

/************** Private P2P GO Power Save TEST CODE STARTS ******************/
#if P2P_TEST_CODE

bool    gtest_started = false;

int
p2p_private_noa_go_ps_test_start(
    wlan_p2p_go_t p2p_go_handle, ieee80211_p2p_go_ps_t p2p_go_ps, int testno)
{
    struct ieee80211com     *ic;
    u_int32_t               curr_tsf;
    wlan_if_t               vap;
    int                     more_test = 0, ret = 0;

    vap = wlan_p2p_GO_get_vap_handle(p2p_go_handle);
    ic = vap->iv_ic;

    {
        int                                 num_schedules = 0;
        wlan_p2p_go_noa_req                 req[4];
        u_int32_t                           curr_tsf;
        int                                 idx;

        OS_MEMZERO(req, 4*sizeof(wlan_p2p_go_noa_req));

        /* Clear current PS settings */
        ieee80211_p2p_go_power_set_param(p2p_go_ps, P2P_GO_PS_OPP_PS, 0);
        ieee80211_p2p_go_power_set_param(p2p_go_ps, P2P_GO_PS_CTWIN, 0);
        ret = wlan_p2p_GO_set_noa_schedule(p2p_go_handle, num_schedules, req);
        if (ret != 0) {
            printk("%s: Error=%d from wlan_p2p_GO_set_noa_schedule\n", __func__, ret);
        }

        printk("%s: running test %d *********************\n", __func__, testno);
        switch(testno) {
        case 1:
            /* Single NOA. Continuous */
            num_schedules = 1;
            req[0].num_iterations = 255;
            req[0].offset_next_tbtt = 20;
            req[0].duration = 30;
            ret = wlan_p2p_GO_set_noa_schedule(p2p_go_handle, num_schedules, req);
            if (ret != 0) {
                printk("%s: Error=%d from wlan_p2p_GO_set_noa_schedule\n", __func__, ret);
            }
            else more_test = 0; // No more tests.
            break;
        case 11:
            //TODO: For testing, we hardcoded to have Opp PS.
            printk("%s: hardcoded to enable Opp PS and CT Window.\n", __func__);
            ieee80211_p2p_go_power_set_param(p2p_go_ps, P2P_GO_PS_OPP_PS, 1);
            ieee80211_p2p_go_power_set_param(p2p_go_ps, P2P_GO_PS_CTWIN, 20);
            more_test = 1;  /* goto next test */
            break;
        case 2:
            /* Single NOA. Non-continuous */
            num_schedules = 1;
            req[0].num_iterations = 255;
            req[0].offset_next_tbtt = 20;
            req[0].duration = 30;
            ret = wlan_p2p_GO_set_noa_schedule(p2p_go_handle, num_schedules, req);
            if (ret != 0) {
                printk("%s: Error=%d from wlan_p2p_GO_set_noa_schedule\n", __func__, ret);
            }
            else more_test = 1;
            break;
        case 3:
            ieee80211_p2p_go_power_set_param(p2p_go_ps, P2P_GO_PS_OPP_PS, 1);
            ieee80211_p2p_go_power_set_param(p2p_go_ps, P2P_GO_PS_CTWIN, 20);
            num_schedules = 1;
            req[0].num_iterations = 255;
            req[0].offset_next_tbtt = 40;
            req[0].duration = 30;
            ret = wlan_p2p_GO_set_noa_schedule(p2p_go_handle, num_schedules, req);
            if (ret != 0) {
                printk("%s: Error=%d from wlan_p2p_GO_set_noa_schedule\n", __func__, ret);
            }
            else more_test = 1;
            break;
        case 4:
            ieee80211_p2p_go_power_set_param(p2p_go_ps, P2P_GO_PS_OPP_PS, 1);
            ieee80211_p2p_go_power_set_param(p2p_go_ps, P2P_GO_PS_CTWIN, 30);
            num_schedules = 1;
            req[0].num_iterations = 255;
            req[0].offset_next_tbtt = 20;
            req[0].duration = 30;
            ret = wlan_p2p_GO_set_noa_schedule(p2p_go_handle, num_schedules, req);
            if (ret != 0) {
                printk("%s: Error=%d from wlan_p2p_GO_set_noa_schedule\n", __func__, ret);
            }
            else more_test = 1;
            break;
        case 5:
            ieee80211_p2p_go_power_set_param(p2p_go_ps, P2P_GO_PS_OPP_PS, 1);
            ieee80211_p2p_go_power_set_param(p2p_go_ps, P2P_GO_PS_CTWIN, 30);
            num_schedules = 1;
            req[0].num_iterations = 255;
            req[0].offset_next_tbtt = 30;
            req[0].duration = 60;
            ret = wlan_p2p_GO_set_noa_schedule(p2p_go_handle, num_schedules, req);
            if (ret != 0) {
                printk("%s: Error=%d from wlan_p2p_GO_set_noa_schedule\n", __func__, ret);
            }
            else more_test = 1;
            break;
        case 6:
            ieee80211_p2p_go_power_set_param(p2p_go_ps, P2P_GO_PS_OPP_PS, 1);
            ieee80211_p2p_go_power_set_param(p2p_go_ps, P2P_GO_PS_CTWIN, 30);
            num_schedules = 1;
            req[0].num_iterations = 1;
            req[0].offset_next_tbtt = 50;
            req[0].duration = 3000;
            ret = wlan_p2p_GO_set_noa_schedule(p2p_go_handle, num_schedules, req);
            if (ret != 0) {
                printk("%s: Error=%d from wlan_p2p_GO_set_noa_schedule\n", __func__, ret);
            }
            else more_test = 1;
            break;
        case 7:
            ieee80211_p2p_go_power_set_param(p2p_go_ps, P2P_GO_PS_OPP_PS, 1);
            ieee80211_p2p_go_power_set_param(p2p_go_ps, P2P_GO_PS_CTWIN, 30);
            num_schedules = 2;
            req[0].num_iterations = 1;
            req[0].offset_next_tbtt = 50;
            req[0].duration = 3000;
            req[1].num_iterations = 255;
            req[1].offset_next_tbtt = 30;
            req[1].duration = 60;
            ret = wlan_p2p_GO_set_noa_schedule(p2p_go_handle, num_schedules, req);
            if (ret != 0) {
                printk("%s: Error=%d from wlan_p2p_GO_set_noa_schedule\n", __func__, ret);
            }
            else more_test = 1;
            break;
        default:
            more_test = 0;
            printk("%s: unknown test number=%d\n", __func__, testno);
            ret = 0;
        }
    }
#if 0   //older NOA input
    {
        int                                 testno = 1;
        int                                 num_schedules = 0;
        ieee80211_p2p_go_schedule_req       req[4];
        u_int32_t                           curr_tsf;
        int                                 idx;

        OS_MEMZERO(req, 4*sizeof(ieee80211_p2p_go_schedule_req));

        printk("%s: running test %d\n", __func__, testno);
        switch(testno) {
        case 1:
            /* Single NOA. Non-continuous */
            num_schedules = 1;
            req[0].type = IEEE80211_P2P_GO_SCHEDULE_EVENT_GO_ABSENT;
            req[0].pri = 0;
            req[0].noa_descriptor.type_count = 10;
            req[0].noa_descriptor.duration = 0x8000;     // 32.768 millisec
            req[0].noa_descriptor.interval = 0x10000;    // 65.536 millisec
            curr_tsf = ic->ic_get_TSF32(ic);
            req[0].noa_descriptor.start_time = curr_tsf - 0x1000 - (3*req[0].noa_descriptor.interval);
            ret = wlan_p2p_GO_set_noa_schedule(p2p_go_handle, num_schedules, req);
            if (ret != 0) {
            }
            break;
        case 2:
            /* Single NOA. Continuous. */
            num_schedules = 1;
            req[0].type = IEEE80211_P2P_GO_SCHEDULE_EVENT_GO_ABSENT;
            req[0].pri = 0;
            req[0].noa_descriptor.type_count = 255;
            req[0].noa_descriptor.duration = 0x8000;     // 32.768 millisec
            req[0].noa_descriptor.interval = 0x10000;    // 65.536 millisec
            curr_tsf = ic->ic_get_TSF32(ic);
            req[0].noa_descriptor.start_time = curr_tsf - 0x1000 - (3*req[0].noa_descriptor.interval);
            ret = wlan_p2p_GO_set_noa_schedule(p2p_go_handle, num_schedules, req);
            if (ret != 0) {
            }
            break;
        case 3:
            /* Single Presence. Continuous. */
            num_schedules = 1;
            req[0].type = IEEE80211_P2P_GO_SCHEDULE_EVENT_GO_PRESENT;
            req[0].pri = 0;
            req[0].noa_descriptor.type_count = 255;
            req[0].noa_descriptor.duration = 0x8000;     // 32.768 millisec
            req[0].noa_descriptor.interval = 0x10000;    // 65.536 millisec
            curr_tsf = ic->ic_get_TSF32(ic);
            req[0].noa_descriptor.start_time = curr_tsf - 0x1000 - (3*req[0].noa_descriptor.interval);
            ret = wlan_p2p_GO_set_noa_schedule(p2p_go_handle, num_schedules, req);
            if (ret != 0) {
            }
            break;
        case 4:
            /* 2 NOA. Continuous. */
            curr_tsf = ic->ic_get_TSF32(ic) & 0xFFFFF000;   /* remove last 3 digits for easier readout */
            num_schedules = 2;
            // First NOA
            idx = 0;
            req[idx].type = IEEE80211_P2P_GO_SCHEDULE_EVENT_GO_ABSENT;
            req[idx].pri = 0;   //highest
            req[idx].noa_descriptor.type_count = 255;
            req[idx].noa_descriptor.duration = 0x8000;     // 32.768 millisec
            req[idx].noa_descriptor.interval = 0x10000;    // 65.536 millisec
            req[idx].noa_descriptor.start_time = curr_tsf - 0x1000 - (3*req[idx].noa_descriptor.interval);
            // Second NOA
            idx = 1;
            req[idx].type = IEEE80211_P2P_GO_SCHEDULE_EVENT_GO_ABSENT;
            req[idx].pri = 1;
            req[idx].noa_descriptor.type_count = 255;
            req[idx].noa_descriptor.duration = 0x4000;     // 32.768 millisec
            req[idx].noa_descriptor.interval = 0x12000;    // 65.536 millisec
            req[idx].noa_descriptor.start_time = curr_tsf - 0x1000 - (3*req[idx].noa_descriptor.interval);
            ret = wlan_p2p_GO_set_noa_schedule(p2p_go_handle, num_schedules, req);
            if (ret != 0) {
            }
            break;
        default:
            printk("%s: unknown test number=%d\n", __func__, testno);
            ret = -1;
        }
    }
#endif

    if (ret == 0) {
        gtest_started = true;
    }
    return more_test;
}

void
p2p_private_noa_go_schedule_test_stop(wlan_p2p_go_t   p2p_go_handle)
{
    if (gtest_started) {
        wlan_p2p_GO_set_noa_schedule(p2p_go_handle, 0, NULL);
    }
    gtest_started = false;
}

#endif  //P2P_TEST_CODE
/************** Private P2P GO Power Save TEST CODE ENDS ******************/


#endif  //UMAC_SUPPORT_P2P_GO_POWERSAVE
#endif  //UMAC_SUPPORT_P2P


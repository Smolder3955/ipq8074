/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#include "ieee80211_var.h"
#include "ieee80211_p2p_go_schedule.h"
#include "ieee80211_p2p_go_power.h"
#include "ieee80211_tsftimer.h"

/* We ran out of bits for the ic->ic_debug field. For now, use the STATE module. */
#define IEEE80211_MSG_P2P_GO_SCH    IEEE80211_MSG_STATE

#define NO_ACTIVE_SCHEDULE      -1

#define P2P_GO_SCH_MAX_EVENT_QUEUE_DEPTH    2

struct request_state {
    bool        is_active;
    u_int32_t   next_event_time;
    bool        expired;
    bool        valid;
};

struct ieee80211_p2p_go_schedule {

    osdev_t                     os_handle;
    struct ieee80211com         *ic;
    wlan_if_t                   vap;
    tsftimer_handle_t           h_tsftimer;
    bool                        tsftimer_started;
    u_int32_t                   tsftimer_timeout;
    ieee80211_p2p_gosche_lock_t lock;
    bool                        go_present;
    bool                        callback_active;
    bool                        paused;

    /* A queue for the external commands to this module */
    os_mesg_queue_t             cmd_mesg_q;

    /* Array of Event callbacks and its arguments */
    ieee80211_go_schedule_event_handler 
                                evhandler;
    void                        *event_arg;
    int                         event_callbacks;

    int                         num_schedules;
    int                         sorted_sch_idx[P2P_GO_PS_MAX_NUM_SCHEDULE_REQ];
    ieee80211_p2p_go_schedule_req   request[P2P_GO_PS_MAX_NUM_SCHEDULE_REQ];
    struct request_state        req_state[P2P_GO_PS_MAX_NUM_SCHEDULE_REQ];
};

#define     P2P_GO_SCH_CB_ID        0xA1B2C3BF  /* unique callback ID */

enum {
    IEEE80211_P2P_GO_SCH_UNPAUSE = 1,
    IEEE80211_P2P_GO_SCH_SETUP
};

static void  ieee80211_p2p_go_schedule_mesgq_event_handler(void *ctx, u_int16_t event,
                                           u_int16_t event_data_len, void *event_data);

/* Stop the TSF timer if it is running. */
static int
stop_tsftimer(ieee80211_p2p_go_schedule_t go_schedule)
{
    int     ret = 0;

    if (go_schedule->tsftimer_started) {
        /* Stop this timer. */
        IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_DETAILED, IEEE80211_MSG_P2P_GO_SCH,
                             "%s: stop timer.\n", __func__);
        ret = ieee80211_tsftimer_stop(go_schedule->h_tsftimer);
        if (ret != 0) {
            IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_SCH,
                                 "%s: ERROR: ieee80211_tsftimer_stop returns error=%d\n", __func__, ret);
        }

        go_schedule->tsftimer_started = false;
    }
    return ret;
}

/*
 * Do a callback when the GO state has changed since last notification.
 * Assumed the lock is held.
 */
static void
notify_GO_state_change(
    ieee80211_p2p_go_schedule_t go_schedule, 
    bool                        GO_awake,
    bool                        one_shot_noa,    /* Is this a one-shot NOA? */
    bool                        forced_callback  /* Do a callback even if state is not changed */
    )
{
    IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_SCH,
        "%s: GO_awake=%d\n", __func__, GO_awake);

    if ((go_schedule->evhandler && (go_schedule->go_present != GO_awake)) ||
        forced_callback)
    {

        /* New schedule state is different from current state. Do a callback. */
        ieee80211_p2p_go_schedule_event     new_event;

        if (GO_awake) {
            new_event.type = IEEE80211_P2P_GO_SCHEDULE_EVENT_GO_PRESENT;
        }
        else {
            new_event.type = IEEE80211_P2P_GO_SCHEDULE_EVENT_GO_ABSENT;
        }

        // TODO: need to double-check that we are handling the lock and re-entrancy issues

        IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_SCH,
                            "%s: Diff. state. Old=%d, New=%d. One-shot=%d, Do the event callback.\n",
                             __func__, go_schedule->go_present, GO_awake, one_shot_noa);

        if (go_schedule->callback_active) {
            /* Already in the middle of a callback. */
            // TODO: need to handle this error case.
            IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_SCH,
                                 "%s: ERROR: Callback already active.\n", __func__);
            return;
        }

        go_schedule->go_present = GO_awake;
        go_schedule->callback_active = true;

        IEEE80211_P2P_GOSCHE_UNLOCK(go_schedule);

        /* NOTE: Call the callback but without spinlock */
        (*go_schedule->evhandler)(go_schedule, &new_event, go_schedule->event_arg, one_shot_noa);

        IEEE80211_P2P_GOSCHE_LOCK(go_schedule);

        ASSERT(go_schedule->callback_active);
        go_schedule->callback_active = false;
    }
    else {
        go_schedule->go_present = GO_awake;
    }
}

static void 
no_more_events(
    ieee80211_p2p_go_schedule_t     go_schedule
    )
{
    bool                GO_awake;

    // cancel timers (if any)
    stop_tsftimer(go_schedule);

    go_schedule->tsftimer_started = false;
    go_schedule->num_schedules = 0;

    /* Assumed that GO is awake when all requests have expired */
    GO_awake = true;
    notify_GO_state_change(go_schedule, GO_awake, false, false);
}

/*
 * Return false if successful. true if the schedule request is already over (expired).
 * The is_active parameter will return if this current request is currently active.
 * The next_event_time parameter will return the next event time. The said event can be 
 * when the NOA/Presence is going to start or stop.
 * Assumed that the lock is held.
 */
static bool
find_next_event(
    ieee80211_p2p_go_schedule_t         go_schedule,
    u_int32_t                           curr_tsf,           /* current TSF Time */
    ieee80211_p2p_go_schedule_req       *schedule,          /* current NOA or Presence Request */
    bool                                *is_active,         /* Return to indicate if NOA/Presence is active right now */
    u_int32_t                           *next_event_time)   /* Return the next event time; can be the time to stop or start NOA */
{
    u_int32_t                           no_intervals, interval_start;
    struct ieee80211_p2p_noa_descriptor *noa;   /* Note: NOA can also be a Request for Presence. */

    noa = &(schedule->noa_descriptor);

    if ((TSFTIME_IS_SMALLER(curr_tsf, noa->start_time))) {
        /* This request has not started yet */
        *is_active = false;
        *next_event_time = noa->start_time;
        IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_DETAILED, IEEE80211_MSG_P2P_GO_SCH,
                "%s: req has not started yet. curr_tsf=0x%x, noa->start_time=%d, is_active=%d\n", 
                __func__, curr_tsf, noa->start_time, *is_active);
        return false;
    }

    ASSERT((noa->type_count <= 1) || TSFTIME_IS_SMALLER(noa->duration , noa->interval));

    if (noa->type_count != 255) {
        /* Non-continuous schedule. Check if this request is already over. */
        u_int32_t end_time;

        if (noa->type_count == 1) {
            /* For one-shot NOA, the interval may not be valid. */
            noa->interval = noa->duration;
        }

        ASSERT(TSFTIME_IS_SMALLER((noa->interval * noa->type_count), (0x7FFFFFFF)));

        end_time = noa->start_time + (noa->type_count * noa->interval);

        if (TSFTIME_IS_GREATER_EQ(curr_tsf, end_time)) {
            /* This request has already completed. */
            *is_active = false;
            *next_event_time = curr_tsf;
            IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_DETAILED, IEEE80211_MSG_P2P_GO_SCH,
                    "%s: req has ended. curr_tsf=0x%x, pri=%d, is_active=%d, next_event_time=0x%x\n", 
                    __func__, curr_tsf, schedule->pri, *is_active, *next_event_time);
            return true;
        }
    }

    no_intervals = (curr_tsf - noa->start_time) / noa->interval;
    ASSERT((noa->type_count == 255) || (no_intervals < noa->type_count));
    interval_start = noa->start_time + (no_intervals * noa->interval);

    ASSERT(TSFTIME_IS_GREATER_EQ(curr_tsf, interval_start));

    if (TSFTIME_IS_SMALLER(curr_tsf, (interval_start + noa->duration))) {
        /* Currently in the middle of an NOA */
        *is_active = true;
        *next_event_time = (interval_start + noa->duration);
    }
    else {
        /* The NOA has completed */
        *is_active = false;
        *next_event_time = (interval_start + noa->interval);
    }

    IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_DETAILED, IEEE80211_MSG_P2P_GO_SCH,
            "%s: curr_tsf=0x%x, pri=%d, no_intervals=0x%x. interval_start=0x%x, is_active=%d, next_event_time=0x%x\n", 
            __func__, curr_tsf, schedule->pri, no_intervals, interval_start, *is_active, *next_event_time);

    return false;
}

/*
 * Find the next events for requests starting from the highest priority one
 * to the first active one. Subsequent lower priority requests are not processed.
 * Note: earliest_next_event returns the request that has the earliest
 * next event but only for requests equal or greater than highest_pri_active.
 */
static void
find_all_next_events(
    ieee80211_p2p_go_schedule_t go_schedule,
    u_int32_t                   event_time,
    int                         *highest_pri_active,
    int                         *earliest_next_event)
{
    int                     i, earliest_event;
    u_int32_t               earliest_next_time = 0;

    earliest_event = -1;
    OS_MEMZERO(&go_schedule->req_state[0], P2P_GO_PS_MAX_NUM_SCHEDULE_REQ * sizeof(struct request_state));
    for (i=(go_schedule->num_schedules-1); i>=0; i--) {
        int                     sort_idx;
        struct request_state    *req = &go_schedule->req_state[i];

        sort_idx = go_schedule->sorted_sch_idx[i];
        req->valid = true;
        req->expired = find_next_event(go_schedule, event_time, &(go_schedule->request[sort_idx]),
                              &req->is_active, &req->next_event_time);

        if (req->expired) {
            continue;
        }

        if (earliest_event == -1) {
            /* Init. the earliest time for next event */
            earliest_next_time = req->next_event_time;
            earliest_event = i;
        }
        else {
            if (TSFTIME_IS_GREATER(earliest_next_time, req->next_event_time)) {
                /* Found an earlier next event */
                earliest_event = i;
                earliest_next_time = req->next_event_time;
            }
        }

        if (req->is_active) {
            /* Find the highest priority request that is active */
            *highest_pri_active = i;
            *earliest_next_event = earliest_event;

            IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_DETAILED, IEEE80211_MSG_P2P_GO_SCH,
                    "%s: highest_pri_active=%d, earliest_next_event=%d\n",
                    __func__, *highest_pri_active, *earliest_next_event);

            return;
        }
    }

    *highest_pri_active = -1;
    *earliest_next_event = earliest_event;

    IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_SCH,
            "%s: highest_pri_active=%d, earliest_next_event=%d\n",
            __func__, *highest_pri_active, *earliest_next_event);

    return;
}

/*
 * Find earliest event starting at request given by start_idx. 
 * If multiple events with equal time, then choose highest priority one.
 */
static int find_earliest_next_event(
    ieee80211_p2p_go_schedule_t go_schedule,
    int                         start_idx)
{
    int                     index, i;
    u_int32_t               earliest_time = 0;
    
    index = -1;
    start_idx = (start_idx > 0)? start_idx: 0;
    for (i=start_idx; i<go_schedule->num_schedules; i++) {
        struct request_state    *req = &go_schedule->req_state[i];
        
        ASSERT(req->valid);
        if (req->expired) {
            /* Skip this request since it has expired. */
            continue;
        }
        if (index == -1) {
            /* Find the first unexpired one */
            index = i;
            earliest_time = req->next_event_time;
        }
        else {
            if (TSFTIME_IS_SMALLER_EQ(req->next_event_time, earliest_time)) {
                /* This request has the earliest next time */
                index = i;
                earliest_time = req->next_event_time;
            }
        }
    }
    return index;
}

/*
 * Get the GO awake status. Assumed that the current request is active.
 */    
static bool
get_GO_wake_state(
    ieee80211_p2p_go_schedule_t     go_schedule,
    int                             priority)
{
    int     sort_idx;

    ASSERT(priority >= 0);
    ASSERT(go_schedule->req_state[priority].is_active);

    sort_idx = go_schedule->sorted_sch_idx[priority];
    if (go_schedule->request[sort_idx].type == IEEE80211_P2P_GO_SCHEDULE_EVENT_GO_ABSENT) {
        return false;
    }
    else {
        ASSERT(go_schedule->request[sort_idx].type == IEEE80211_P2P_GO_SCHEDULE_EVENT_GO_PRESENT);
        return true;
    }
}
    
/*
 * To calculate the new schedule. Update the current "GO Present" state.
 * Assumed that lock is held.
 * Returns 1 if the next TSF Timer needs to be set. The TSF Time for the 
 * next timer expiry is returned in the next_timeout parameter.
 * highest_active_index is the index in the requested schedules where this event is based on.
 * Returns 0 if there is no more timer to set.
 */
static int
calc_schedules(
    ieee80211_p2p_go_schedule_t go_schedule,
    u_int32_t                   curr_tsf,
    u_int32_t                   *next_timeout,
    bool                        *GO_awake,
    int                         *highest_active_index)
{
    u_int32_t               event_time;
    int                     highest_active, earliest_event;
    int                     earliest_next_idx;
    int                     sanity_count = 64;
    struct request_state    *req = &go_schedule->req_state[0];

    *highest_active_index = NO_ACTIVE_SCHEDULE;

    if (go_schedule->num_schedules == 0) {
        /* Nothing to do next. Schedule is empty. */
        IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_SCH,
            "%s: num_schedules==0. Nothing to do.\n", __func__);
        *GO_awake = true;   /* Assume that GO is awake when no schedule */
        return 0;
    }
    IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_SCH,
        "%s: curr_tsf=0x%x, num_schedules=%d\n", __func__, curr_tsf, go_schedule->num_schedules);
    
    /* 
     * The array of schedule is already sorted by priority (lowest first)
     * using go_schedule->sorted_sch_idx[i].
     */
    
    event_time = curr_tsf;
    
    /* Find the earliest and active event at time==event_time */
    find_all_next_events(go_schedule, event_time, &highest_active, &earliest_event);
    if (highest_active == -1) {
        /* No requests are currently active */
        IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_SCH,
            "%s: Init: No requests are currently active, assumed GO is awake, earliest_event=%d\n",
               __func__, earliest_event);
        *GO_awake = true;       /* Assumed is awake when no outstanding request */

        if (earliest_event == -1) {
            IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_SCH,
                "%s: Init: No next event. Nothing to do.\n", __func__);
            return 0;   // nothing to do.
        }
    }
    else {
        /* Init. the GO state to this active request */
        ASSERT(req[highest_active].is_active);
        *GO_awake = get_GO_wake_state(go_schedule, highest_active);

        IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_SCH,
            "%s: Init: highest active event=%d, GO_awake=0x%x.\n",
               __func__, highest_active, *GO_awake);

        /* Return the index of the highest priority and currently active schedule request */
        *highest_active_index = go_schedule->sorted_sch_idx[highest_active];
    }

    /* Find the next event time */
    while (sanity_count--) {
    
        ASSERT(((highest_active != -1) && req[highest_active].is_active && (!req[highest_active].expired)) ||
               ((highest_active == -1) && (earliest_event != -1)));

        earliest_next_idx = find_earliest_next_event(go_schedule, highest_active);
        ASSERT(earliest_next_idx != -1);
        if (earliest_next_idx == highest_active) {
            /* No higher priority request with earlier next event */
            event_time = req[earliest_next_idx].next_event_time;
        }
        else {
            /* Move time forward to the earliest event */
            event_time = req[earliest_next_idx].next_event_time;
        }

        IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_SCH,
            "%s: While Loop: highest_active=%d, earliest_next_idx=%d, event_time=0x%x\n",
               __func__, highest_active, earliest_next_idx, event_time);

        find_all_next_events(go_schedule, event_time, &highest_active, &earliest_event);
        if (highest_active == -1) {
            /* No active request schedules or all have expired */
            if (*GO_awake) {
                /* Note: we consider GO to be awake when no active requests */
                if (earliest_event != -1) {
                    IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_SCH,
                        "%s: GO awake and earliest_event=%d\n", __func__, earliest_event);
                }
                else {
                    /* No more events */
                    IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_SCH,
                        "%s: No more events and GO awake\n", __func__);
                    return 0;
                }
            }
            else {
                *next_timeout = event_time;
                IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_SCH,
                    "%s: No active requests, earliest_event=%d, GO is asleep and return next_timeout=0x%x\n",
                       __func__, earliest_event, *next_timeout);
                return 1;
            }
        }
        else {
            bool    new_GO_awake;

            ASSERT(req[highest_active].is_active);
            new_GO_awake = get_GO_wake_state(go_schedule, highest_active);
            
            IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_SCH,
                "%s: Have active requests, GO new=%d, current=%d\n",
                   __func__, new_GO_awake, *GO_awake);

            if (new_GO_awake != *GO_awake) {
                *next_timeout = event_time;
                ASSERT(curr_tsf != event_time);
                return 1;
            }
            else {
                ASSERT(!req[highest_active].expired);
            }
        }
    }
    
    /* 
     * We have processed at least "sanity_count" number of next events without
     * GO state changing. We presumed that it is not going to change anymore.
     * E.g. if you have one single Presence request, then we could get here.
     */

    ASSERT(sanity_count==-1);
    ASSERT(*GO_awake);
    
    IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_SCH,
        "%s: the GO state=%d did not change after %d iterations.\n", 
           __func__, *GO_awake, sanity_count);

    /* No more events */
    return 0;    
}

/*
 * Process the given schedule and reschedule the timer for the next callback event.
 * Assumed that the lock is held.
 */
static void
process_schedule(
    ieee80211_p2p_go_schedule_t     go_schedule,
    u_int32_t                       curr_tsf,
    bool                            *one_shot_noa    /* Is this a one-shot NOA? */
    )
{
    u_int32_t           next_timeout = 0;
    bool                GO_awake=false;
    int                 ret;
    int                 highest_active_index;

    IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_SCH,
        "%s: curr_tsf=0x%x, num_schedules=%d\n", __func__, curr_tsf, go_schedule->num_schedules);
    
    *one_shot_noa = false;

    /* Calculate the time for next event and current GO state. */
    ret = calc_schedules(go_schedule, curr_tsf, &next_timeout, &GO_awake, &highest_active_index);

    IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_SCH,
        "%s: calc_schedules ret/More Req.=%d, curr_tsf=0x%x, next_timeout=0x%x, GO_awake=%d\n",
           __func__, ret, curr_tsf, next_timeout, GO_awake);

    if (ret == 0) {
        /* No more active requests. Stop the timer (if started). */
        IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_DETAILED, IEEE80211_MSG_P2P_GO_SCH,
            "%s: No more active requests. Stop the timer. GO_awake=%d\n", __func__, GO_awake);
        ASSERT(GO_awake);
        no_more_events(go_schedule);
        return;
    }
    else if (ret == 1) {
        /* More pending request. Check if we need to adjust the next TSF Timeout. */
        bool restart_timer = false;

        if (!go_schedule->tsftimer_started) {
            /* Start a new timer */
            restart_timer = true;
        }
        else {
            /* Timer is already started. Check if the timeout is still the same. Else Rescheduled. */
            if (go_schedule->tsftimer_timeout != next_timeout) {
                IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_SCH,
                    "%s: stop old timer. Will restart again with new timeout=0x%x\n", __func__, next_timeout);
                stop_tsftimer(go_schedule);
                go_schedule->tsftimer_started = false;

                restart_timer = true;
            }
            else {
                /* No change in current Timeout. Just notify if GO state changes */
            }
        }

        if (!GO_awake) {
            /* Find out if this is a one-shot NOA */
            if (highest_active_index != NO_ACTIVE_SCHEDULE) {

                ieee80211_p2p_go_schedule_req   *req;

                req = &(go_schedule->request[highest_active_index]);

                if ((req->type == IEEE80211_P2P_GO_ABSENT_SCHEDULE) &&
                    (req->noa_descriptor.type_count == 1)) 
                {
                    *one_shot_noa = true;
                }
            }
        }

        notify_GO_state_change(go_schedule, GO_awake, *one_shot_noa, false);

        if ((!go_schedule->paused) && restart_timer) {
            IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_SCH,
                "%s: start timer, timeout=0x%x\n", __func__, next_timeout);
            ret = ieee80211_tsftimer_start(go_schedule->h_tsftimer, next_timeout, 0);
            if (ret != 0) {
                IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_SCH,
                    "%s: ERROR: ieee80211_tsftimer_start returns error=%d\n", __func__, ret);
                go_schedule->tsftimer_started = false;
                return;
            }
            go_schedule->tsftimer_started = true;
            go_schedule->tsftimer_timeout = next_timeout;
        }
        else if (restart_timer) {
            IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_GO_SCH,
                "%s: Paused: do not restart timer\n", __func__);
        }
    }
    else {
        /* TODO: handle this error condition */
        IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_SCH,
            "%s: ERROR: calc_schedules return %d\n", __func__, ret);
    }

}

/*
 * Callback from TSF Timer for timer expiry.
 */
static void
my_tsftimer_timeout(tsftimer_handle_t h_tsftime, int arg1, void *arg2, u_int32_t curr_tsf)
{
    ieee80211_p2p_go_schedule_t h_schedule;

    ASSERT(arg1 == P2P_GO_SCH_CB_ID);
    
    h_schedule = (ieee80211_p2p_go_schedule_t)arg2;

    IEEE80211_P2P_GOSCHE_LOCK(h_schedule);

    ASSERT(h_schedule->h_tsftimer == h_tsftime);
    
    ASSERT(h_schedule->evhandler != NULL);

    // TODO: need to do the callback. Need to unlock before calling too.

    IEEE80211_DPRINTF_IC(h_schedule->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_SCH,
        "%s: called. GO is currently %s. curr_tsf=0x%x\n", __func__, 
           (h_schedule->go_present? "awaked" : "asleep"),
           curr_tsf);

    if (h_schedule->tsftimer_started) {

        h_schedule->tsftimer_started = false;
    
        if (!h_schedule->paused) {
            /* Process the schedule based on new TSF. Any callback will be done in this function. */
            bool        one_shot_noa;   /* Is this a one-shot NOA? */
    
            process_schedule(h_schedule, curr_tsf, &one_shot_noa);
        }
        else {
            IEEE80211_DPRINTF_IC(h_schedule->ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_GO_SCH,
                "%s: TSF Timer triggered but paused. Do nothing.\n", __func__);
        }
    }
    else {
        IEEE80211_DPRINTF_IC(h_schedule->ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_GO_SCH,
            "%s: TSF Timer triggered but timer stopped. Do nothing.\n", __func__);
    }

    IEEE80211_P2P_GOSCHE_UNLOCK(h_schedule);
}


/*
 * Callback from TSF Timer when time is re-sync.
 */
static void
my_tsftimer_resync(tsftimer_handle_t h_tsftime, int arg1, void *arg2, u_int32_t curr_tsf)
{
    ieee80211_p2p_go_schedule_t h_schedule;
    struct ieee80211com         *ic;

    ASSERT(arg1 == P2P_GO_SCH_CB_ID);
    
    h_schedule = (ieee80211_p2p_go_schedule_t)arg2;

    IEEE80211_P2P_GOSCHE_LOCK(h_schedule);

    ic = h_schedule->ic;

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_SCH,
        "%s: TSF Resync. Re-process the current schedule.\n", __func__);

    if (!h_schedule->paused) {
        bool        one_shot_noa;   /* Is this a one-shot NOA? */

        process_schedule(h_schedule, curr_tsf, &one_shot_noa);
    }
    else {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_GO_SCH,
            "%s: TSF Resync but paused. Do nothing.\n", __func__);
    }

    IEEE80211_P2P_GOSCHE_UNLOCK(h_schedule);
}

/*
 * crete an instance of the p2p schedule module.
 */
ieee80211_p2p_go_schedule_t
ieee80211_p2p_go_schedule_create(
    osdev_t os_handle, wlan_if_t vap)
{
    struct ieee80211com         *ic;
    ieee80211_p2p_go_schedule_t h_schedule = NULL;
    bool                        bad_return = false;
    tsftimer_handle_t           h_tsftimer = NULL;
    bool                        msg_queue_init = false;

    ASSERT(vap);
    ic = vap->iv_ic;

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_SCH,
        "%s: called.\n", __func__);

    do {
        // alloc the mem for structure of ieee80211_p2p_go_schedule
        h_schedule = (ieee80211_p2p_go_schedule_t)
                        OS_MALLOC(os_handle, sizeof(struct ieee80211_p2p_go_schedule), 0);
        if (h_schedule == NULL) {
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_SCH,
                "%s: Failed to alloc memory (size=%d).\n", __func__,
                   sizeof(struct ieee80211_p2p_go_schedule));
            bad_return = true;
            break;
        }

        // zero out and then init. the contents
        OS_MEMZERO(h_schedule, sizeof(struct ieee80211_p2p_go_schedule));
        h_schedule->vap = vap;
        h_schedule->ic = ic;
        h_schedule->os_handle = os_handle;
        IEEE80211_P2P_GOSCHE_LOCK_INIT(h_schedule);

        h_schedule->go_present = true;   /* GO start off from being awake */

        if (OS_MESGQ_INIT(os_handle, &h_schedule->cmd_mesg_q,
                          sizeof(ieee80211_p2p_go_schedule_req) * P2P_GO_PS_MAX_NUM_SCHEDULE_REQ,
                          P2P_GO_SCH_MAX_EVENT_QUEUE_DEPTH, 
                          ieee80211_p2p_go_schedule_mesgq_event_handler,
                          h_schedule, 
                          MESGQ_PRIORITY_NORMAL, 
                          MESGQ_ASYNCHRONOUS_EVENT_DELIVERY) != 0)
        {
            IEEE80211_DPRINTF_IC(h_schedule->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_SCH,
                "%s : OS_MESGQ_INIT  failed.\n", __func__);
            bad_return = true;
            break;
        }
        msg_queue_init = true;

        /* alloc a tsf timer */
        h_tsftimer = ieee80211_tsftimer_alloc(ic->ic_tsf_timer, 0, my_tsftimer_timeout,
                                              P2P_GO_SCH_CB_ID, h_schedule, my_tsftimer_resync);
        if (h_tsftimer == NULL) {
            IEEE80211_DPRINTF_IC(h_schedule->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_SCH,
                "%s: 0 ieee80211_tsftimer_alloc returns error.\n", __func__);
            bad_return = true;
            break;
        }
        h_schedule->h_tsftimer = h_tsftimer;
        h_schedule->tsftimer_started = false;
        h_schedule->paused = false;

    } while ( FALSE );

    if (!bad_return) {
        return h_schedule;
    }
    else {
    
        /* Free the TSF Timer */
        if (h_tsftimer) {
            if (ieee80211_tsftimer_free(h_tsftimer, true)) {
               IEEE80211_DPRINTF_IC(h_schedule->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_SCH,
                "%s: 0 ieee80211_tsftimer_free returns error.\n", __func__);
            }
            h_tsftimer = NULL;
        }

        if (msg_queue_init) {
            OS_MESGQ_DESTROY(&h_schedule->cmd_mesg_q);
            msg_queue_init = false;
        }

        /* Free structure of ieee80211_p2p_go_schedule */
        if (h_schedule) {
            IEEE80211_P2P_GOSCHE_LOCK_DESTROY(h_schedule);
            OS_FREE(h_schedule);
            h_schedule = NULL;
        }
    
        return NULL;
    }
}

/*
 * delete the p2p schedule module.
 */
int
ieee80211_p2p_go_schedule_delete(
    ieee80211_p2p_go_schedule_t go_scheduler)
{
    IEEE80211_DPRINTF_IC(go_scheduler->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_SCH,
        "%s: called.\n", __func__);

    // free the tsf timer. Blocked to wait for any existing callbacks to end.
    if (ieee80211_tsftimer_free(go_scheduler->h_tsftimer, true)) {
       IEEE80211_DPRINTF_IC(go_scheduler->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_SCH,
             "%s: 0 ieee80211_tsftimer_free returns error.\n", __func__);
    }

    ASSERT(!go_scheduler->callback_active);

    OS_MESGQ_DESTROY(&go_scheduler->cmd_mesg_q);

    IEEE80211_P2P_GOSCHE_LOCK_DESTROY(go_scheduler);

    // free the structure ieee80211_p2p_go_schedule_t
    OS_FREE(go_scheduler);

    return 0;
}

/*
 * register event handler with go scheduler.
 */
int
ieee80211_p2p_go_schedule_register_event_handler(
    ieee80211_p2p_go_schedule_t go_schedule, 
    ieee80211_go_schedule_event_handler evhandler,
    void *Arg)
{
    ASSERT(go_schedule);
    ASSERT(evhandler);

    IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_SCH,
        "%s: called.\n", __func__);

    IEEE80211_P2P_GOSCHE_LOCK(go_schedule);

    if (go_schedule->evhandler != NULL) {
        IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_GO_SCH,
            "%s: Warning, there is already a event handler registered.\n", __func__);
    }
    ASSERT(!go_schedule->callback_active);

    // add this event callback to the struc.
    go_schedule->evhandler = evhandler;
    go_schedule->event_arg = Arg;

    IEEE80211_P2P_GOSCHE_UNLOCK(go_schedule);

    return 0;
}

/*
 * unregister event handler with go scheduler.
 */
int
ieee80211_p2p_go_schedule_unregister_event_handler(
    ieee80211_p2p_go_schedule_t go_schedule, 
    ieee80211_go_schedule_event_handler evhandler,
    void *Arg)
{
    IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_SCH,
        "%s: called.\n", __func__);
    
    ASSERT(go_schedule);

    IEEE80211_P2P_GOSCHE_LOCK(go_schedule);

    if (go_schedule->evhandler != NULL) {
        IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_SCH,
            "%s: Warning, no event handler registered.\n", __func__);
    }
    if (go_schedule->evhandler != evhandler) {
        IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_SCH,
            "%s: Warning, event handler mismatched 0x%x != 0x%x.\n", 
               __func__, go_schedule->evhandler, evhandler);
    }
    ASSERT(!go_schedule->callback_active);

    go_schedule->evhandler = NULL;
    go_schedule->event_arg = NULL;

    IEEE80211_P2P_GOSCHE_UNLOCK(go_schedule);

    return 0;
}

/*
 * This function will sort the array of schedule requests from lowest to highest.
 * A 0 value in pri field is highest priority and 1 is lower priority than 0 and so on.
 * Note: we do not move the array elements but just update the index, sorted_sch.
 */
static void
sort_schedule_req(
    ieee80211_p2p_go_schedule_t     go_schedule)
{
    int     *sort_idx;
    int     i;
    int     num_schedules = go_schedule->num_schedules;

    sort_idx = go_schedule->sorted_sch_idx;

    for (i=0; i<num_schedules; i++) {
        sort_idx[i] = i;
    }
    for (i=0; i<num_schedules; i++) {
        int         tmp, lowest_idx, j;
        u_int8_t    lowest_pri;

        /* Find the lowest priority to put infront */
        lowest_pri = go_schedule->request[sort_idx[i]].pri;
        lowest_idx = i;
        for (j=i+1; j<num_schedules; j++) {
            /* Note: the lower the priority, the bigger the pri field value. */
            if (lowest_pri < go_schedule->request[sort_idx[j]].pri) {
                lowest_pri = go_schedule->request[sort_idx[j]].pri;
                lowest_idx = j;
            }
        }
        /* swap the lowest to index i */
        if (lowest_idx != i) {
            tmp = sort_idx[i];
            sort_idx[i] = sort_idx[lowest_idx];
            sort_idx[lowest_idx] = tmp;
        }
    }

#ifdef DBG
    /* For Debugging purposes, dump out the list. */
    for (i=0; i<num_schedules; i++) {
        int         sort_idx;

        sort_idx = go_schedule->sorted_sch_idx[i];

        IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_SCH,
            "%s: [%d->%d]: pri=%d, type=%s, cnt=%d, dur=0x%x,int=0x%x,start=0x%x\n",
               __func__, i, sort_idx, 
               go_schedule->request[sort_idx].pri, 
               go_schedule->request[sort_idx].type==IEEE80211_P2P_GO_SCHEDULE_EVENT_GO_ABSENT?"NOA ":"Pres",
               go_schedule->request[sort_idx].noa_descriptor.type_count,
               go_schedule->request[sort_idx].noa_descriptor.duration,
               go_schedule->request[sort_idx].noa_descriptor.interval,
               go_schedule->request[sort_idx].noa_descriptor.start_time
            );
    }
#endif

}

/*
 * Set up p2p go schedules. will start generating the events.  
 * if there was an old schedule in place, it will restart with the new schedule.
 * if num_schdules is 0 then will stop.   
 */
int
ieee80211_p2p_go_schedule_setup(
    ieee80211_p2p_go_schedule_t         go_schedule,
    u_int8_t                            num_schedules,
    ieee80211_p2p_go_schedule_req       *sch_req)
{
    ASSERT(go_schedule);

    if (num_schedules > P2P_GO_PS_MAX_NUM_SCHEDULE_REQ) {
        IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_SCH,
            "%s: Too many schedules=%d, max=%d\n", __func__, 
               num_schedules, P2P_GO_PS_MAX_NUM_SCHEDULE_REQ);
        return EINVAL;
    }

    if (OS_MESGQ_SEND(&go_schedule->cmd_mesg_q, IEEE80211_P2P_GO_SCH_SETUP,
                      (num_schedules * sizeof(ieee80211_p2p_go_schedule_req)),
                      (void *)sch_req) != 0)
    {
        IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_SCH,
            "%s: message queue send failed \n", __func__);
    }
    return 0;
}

static int
deferred_schedule_setup(
    ieee80211_p2p_go_schedule_t         go_schedule,
    u_int8_t                            num_schedules,
    ieee80211_p2p_go_schedule_req       *sch_req)
{

    u_int32_t           curr_tsf;
    struct ieee80211com *ic;
    bool                one_shot_noa;   /* Is this a one-shot NOA? */

    IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_SCH,
        "%s: called.\n", __func__);

    IEEE80211_P2P_GOSCHE_LOCK(go_schedule);

    /* Keep a copy of the new schedule */
    go_schedule->num_schedules = num_schedules;
    if (num_schedules) {
        /* Copy over the schedule */
        OS_MEMCPY(go_schedule->request, sch_req,
                  sizeof(ieee80211_p2p_go_schedule_req) * num_schedules);

        /* Sort the list of schedules by priority (lowest to highest) */
        sort_schedule_req(go_schedule);
    }

    if (go_schedule->paused) {
        /* Already paused. Do nothing. */
        IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_GO_SCH,
            "%s: Already paused; do nothing but keep copy of new schedule.\n", __func__);
        IEEE80211_P2P_GOSCHE_UNLOCK(go_schedule);
        return 0;
    }

    if (num_schedules == 0) {
        /* Empty schedule. We are done. */
        no_more_events(go_schedule);

        IEEE80211_P2P_GOSCHE_UNLOCK(go_schedule);

        return 0;
    }

    ASSERT(sch_req);

    IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_P2P_GO_SCH,
        "%s: num_schedules=%d.\n", __func__, num_schedules);

    ic = go_schedule->vap->iv_ic;
    curr_tsf = ic->ic_get_TSF32(ic);

    process_schedule(go_schedule, curr_tsf, &one_shot_noa);

    IEEE80211_P2P_GOSCHE_UNLOCK(go_schedule);

    return 0;
}

/*
 * pause P2p GO scheduler. the scheduler should cancel all timers and pause.
 */
void ieee80211_p2p_go_schedule_pause(ieee80211_p2p_go_schedule_t go_schedule)
{
    IEEE80211_P2P_GOSCHE_LOCK(go_schedule);

    if (go_schedule->paused) {
        /* Already paused. Do nothing. */
        IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_GO_SCH,
            "%s: Already paused; do nothing.\n", __func__);
        IEEE80211_P2P_GOSCHE_UNLOCK(go_schedule);
        return;
    }

    go_schedule->paused = true;

    // cancel timers (if any)
    stop_tsftimer(go_schedule);
    go_schedule->tsftimer_started = false;

    IEEE80211_P2P_GOSCHE_UNLOCK(go_schedule);
}

/*
 * unpause P2p GO scheduler. the scheduler should restart all the timers.
 * it should also generate a go event idicating the current state of the GO.
 */
void ieee80211_p2p_go_schedule_unpause(ieee80211_p2p_go_schedule_t go_schedule)
{
    if (OS_MESGQ_SEND(&go_schedule->cmd_mesg_q, IEEE80211_P2P_GO_SCH_UNPAUSE, 0, NULL) != 0) {
        IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_SCH,
            "%s: message queue send failed \n", __func__);
    }
}

static void deferred_unpause(ieee80211_p2p_go_schedule_t go_schedule)
{
    bool                prev_GO_state;
    u_int32_t           curr_tsf;
    struct ieee80211com *ic;
    bool                one_shot_noa;

    IEEE80211_P2P_GOSCHE_LOCK(go_schedule);

    if (!go_schedule->paused) {
        /* Already paused. Do nothing. */
        IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_GO_SCH,
            "%s: Not paused; do nothing.\n", __func__);
        IEEE80211_P2P_GOSCHE_UNLOCK(go_schedule);
        return;
    }

    go_schedule->paused = false;

    /* Process the schedule to kick start it. */
    prev_GO_state = go_schedule->go_present;    /* Remember this state before calling process_schedule() */
    ic = go_schedule->vap->iv_ic;
    curr_tsf = ic->ic_get_TSF32(ic);
    process_schedule(go_schedule, curr_tsf, &one_shot_noa);

    if (go_schedule->go_present == prev_GO_state) {
        /* There is no change in GO state. This means that we did not do a callback. Force a callback. */
        IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_DETAILED, IEEE80211_MSG_P2P_GO_SCH,
            "%s: Do a forced callback.\n", __func__);
        notify_GO_state_change(go_schedule, go_schedule->go_present, one_shot_noa, true);
    }

    IEEE80211_P2P_GOSCHE_UNLOCK(go_schedule);
}

static void  ieee80211_p2p_go_schedule_mesgq_event_handler(void *ctx, u_int16_t event,
                                           u_int16_t event_data_len, void *event_data)
{
    ieee80211_p2p_go_schedule_t go_schedule = (ieee80211_p2p_go_schedule_t)ctx;

    if (event == IEEE80211_P2P_GO_SCH_UNPAUSE) {
        deferred_unpause(go_schedule);
    }
    else if (event == IEEE80211_P2P_GO_SCH_SETUP) {
        int         ret;
        u_int8_t    num_schedules;

        num_schedules = (u_int8_t)(event_data_len / sizeof(ieee80211_p2p_go_schedule_req));
        ASSERT((event_data_len % sizeof(ieee80211_p2p_go_schedule_req)) == 0);
        ret = deferred_schedule_setup(go_schedule, num_schedules, 
                                      (ieee80211_p2p_go_schedule_req *)event_data);
        ASSERT(ret == 0);   /* Should not fail */
    }
    else {
        IEEE80211_DPRINTF_IC(go_schedule->ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_GO_SCH,
            "%s: Error: unknown event=%d\n", __func__, event);
        ASSERT(false);
    }
}


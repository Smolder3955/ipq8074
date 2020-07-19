/*
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

#include "ieee80211_var.h"
#include <ieee80211_channel.h>
#include "ieee80211_sme_api.h"
#include "ieee80211_sm.h"
#include <ieee80211_resmgr_priv.h>

#if 0
#if UMAC_SUPPORT_RESMGR_OC_SCHED

static const char* resmgr_event_name[] = {
    /* IEEE80211_RESMGR_EVENT_NONE                */                 "NONE",
    /* IEEE80211_RESMGR_EVENT_VAP_START_REQUEST   */    "VAP_START_REQUEST",
    /* IEEE80211_RESMGR_EVENT_VAP_UP              */               "VAP_UP",
    /* IEEE80211_RESMGR_EVENT_VAP_STOPPED         */          "VAP_STOPPED",
    /* IEEE80211_RESMGR_EVENT_VAP_PAUSE_COMPLETE  */   "VAP_PAUSE_COMPLETE",
    /* IEEE80211_RESMGR_EVENT_VAP_PAUSE_FAIL      */       "VAP_PAUSE_FAIL",
    /* IEEE80211_RESMGR_EVENT_VAP_RESUME_COMPLETE */  "VAP_RESUME_COMPLETE",
    /* IEEE80211_RESMGR_EVENT_OFFCHAN_REQUEST     */      "OFFCHAN_REQUEST",
    /* IEEE80211_RESMGR_EVENT_BSSCHAN_REQUEST     */      "BSSCHAN_REQUEST",
    /* IEEE80211_RESMGR_EVENT_VAP_CANPAUSE_TIMER  */   "VAP_CANPAUSE_TIMER",
    /* IEEE80211_RESMGR_EVENT_CHAN_SWITCH_REQUEST */  "CHAN_SWITCH_REQUEST",
    /* IEEE80211_RESMGR_EVENT_CSA_COMPLETE        */         "CSA_COMPLETE",
    /* IEEE80211_RESMGR_EVENT_SCHED_BSS_REQUEST   */    "SCHED_BSS_REQUEST",
    /* IEEE80211_RESMGR_EVENT_TSF_TIMER_EXPIRED   */    "TSF_TIMER_EXPIRED",
    /* IEEE80211_RESMGR_EVENT_TSF_TIMER_CHANGED   */    "TSF_TIMER_CHANGED",
    /* IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_START_REQUEST */   "OC_SCHED_START_REQUEST",
    /* IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_STOP_REQUEST */    "OC_SCHED_STOP_REQUEST",
};

#define MAX_QUEUED_EVENTS  16

#define IS_CHAN_EQUAL(_c1, _c2)  (((_c1)->ic_freq == (_c2)->ic_freq) && \
                                  (((_c1)->ic_flags & IEEE80211_CHAN_ALLTURBO) == ((_c2)->ic_flags & IEEE80211_CHAN_ALLTURBO)))

static void ieee80211_resmgr_multichan_sm_debug_print (void *ctx, const char *fmt,...) 
{
    char tmp_buf[256];
    va_list ap;
     
    va_start(ap, fmt);
    vsnprintf (tmp_buf,256, fmt, ap);
    va_end(ap);
    printk("%s", tmp_buf);
}


/* VAP Iteration Routines */

/* Get VAP deferred events (if any) */
static void ieee80211_vap_iter_get_def_event(void *arg, struct ieee80211vap *vap)
{
    ieee80211_vap_t *event_vap = (ieee80211_vap_t *)arg;
    ieee80211_resmgr_vap_priv_t resmgr_vap = NULL;

    if ((*event_vap) == NULL) {
        resmgr_vap = ieee80211vap_get_resmgr(vap);
        if (resmgr_vap->def_event_type) {
            *event_vap = vap;
         }
    }
}

struct vap_iter_unpause_params {
    struct ieee80211_ath_channel *chan;
    bool vaps_unpausing;
};

static void ieee80211_vap_iter_resume_request(void *arg, struct ieee80211vap *vap)
{
    ieee80211_resmgr_vap_priv_t resmgr_vap = ieee80211vap_get_resmgr(vap);
    struct vap_iter_unpause_params *params = (struct vap_iter_unpause_params *)arg;

    if (resmgr_vap->state == VAP_PAUSED) {
        if (!params || !params->chan ||
            params->chan->ic_freq == resmgr_vap->chan->ic_freq) {
            ieee80211_vap_resume_bss(vap);
            if (params) {
                params->vaps_unpausing = true;
            }
        }
    } else if (resmgr_vap->resume_notify) {
        ieee80211_vap_resume_bss(vap);
        resmgr_vap->resume_notify = 0;
    }
}

/* Send Unpause Request to all paused VAPs, on matching channel if required */
static void ieee80211_vap_iter_unpause_request(void *arg, struct ieee80211vap *vap)
{
    ieee80211_resmgr_vap_priv_t resmgr_vap = ieee80211vap_get_resmgr(vap);
    struct vap_iter_unpause_params *params = (struct vap_iter_unpause_params *)arg;

    if (resmgr_vap->state == VAP_PAUSED) {
        if (!params || !params->chan ||
            params->chan->ic_freq == resmgr_vap->chan->ic_freq) {
            ieee80211_vap_unpause(vap, IEEE80211_RESMGR_REQID);
            if (params) {
                params->vaps_unpausing = true;
            }
        }
    }
}

#if WORK_IN_PROCESS

/* Send Pause Request to all active VAPs */
static void ieee80211_vap_iter_pause_request(void *arg, struct ieee80211vap *vap)
{
    ieee80211_resmgr_vap_priv_t resmgr_vap = ieee80211vap_get_resmgr(vap);

    if (resmgr_vap->state == VAP_ACTIVE) {
        ieee80211_vap_pause(vap, IEEE80211_RESMGR_REQID);
    }
}

static void ieee80211_vap_iter_standby_request(void *arg, struct ieee80211vap *vap)
{
    ieee80211_resmgr_vap_priv_t resmgr_vap = ieee80211vap_get_resmgr(vap);

    if (resmgr_vap->state == VAP_ACTIVE) {
        ieee80211_vap_standby_bss(vap);

        if (vap->iv_opmode == IEEE80211_M_BTAMP) {
            /* AMP Port goes down during standby operation. However, it still needs a resume notification.
             * Set a special flag for this case.
             */
            resmgr_vap->resume_notify = 1;
        }
    }
}

struct vap_iter_unpause_params {
    struct ieee80211_ath_channel *chan;
    bool vaps_unpausing;
};

/* Send Unpause Request to all paused VAPs, on matching channel if required */
static void ieee80211_vap_iter_unpause_request(void *arg, struct ieee80211vap *vap)
{
    ieee80211_resmgr_vap_priv_t resmgr_vap = ieee80211vap_get_resmgr(vap);
    struct vap_iter_unpause_params *params = (struct vap_iter_unpause_params *)arg;

    if (resmgr_vap->state == VAP_PAUSED) {
        if (!params || !params->chan ||
            params->chan->ic_freq == resmgr_vap->chan->ic_freq) {
            ieee80211_vap_unpause(vap, IEEE80211_RESMGR_REQID);
            if (params) {
                params->vaps_unpausing = true;
            }
        }
    }
}

static void ieee80211_vap_iter_resume_request(void *arg, struct ieee80211vap *vap)
{
    ieee80211_resmgr_vap_priv_t resmgr_vap = ieee80211vap_get_resmgr(vap);
    struct vap_iter_unpause_params *params = (struct vap_iter_unpause_params *)arg;

    if (resmgr_vap->state == VAP_PAUSED) {
        if (!params || !params->chan ||
            params->chan->ic_freq == resmgr_vap->chan->ic_freq) {
            ieee80211_vap_resume_bss(vap);
            if (params) {
                params->vaps_unpausing = true;
            }
        }
    } else if (resmgr_vap->resume_notify) {
        ieee80211_vap_resume_bss(vap);
        resmgr_vap->resume_notify = 0;
    }
}

/* Send Channel Switch Request to all active VAPs on different channel */
static void ieee80211_vap_iter_chanswitch_request(void *arg, struct ieee80211vap *vap)
{
    ieee80211_resmgr_vap_priv_t resmgr_vap = ieee80211vap_get_resmgr(vap);
    struct ieee80211_ath_channel *chan = (struct ieee80211_ath_channel *)arg;

    if (resmgr_vap->state == VAP_ACTIVE &&
        (chan->ic_freq != resmgr_vap->chan->ic_freq)) {
        //ieee80211_vap_switch_chan(vap, chan, IEEE80211_RESMGR_REQID);
        //resmgr_vap->state = VAP_SWITCHING;
        resmgr_vap->chan = chan;
    }
}

/* Pick BSS channel */
static void ieee80211_vap_iter_bss_channel(void *arg, struct ieee80211vap *vap)
{
    ieee80211_resmgr_vap_priv_t resmgr_vap = ieee80211vap_get_resmgr(vap);
    struct ieee80211_ath_channel **chan = (struct ieee80211_ath_channel **)arg;

    if (!(*chan) && resmgr_vap->state == VAP_PAUSED) {
        *chan = resmgr_vap->chan;
    }
}

/* Check if all VAPs can pause */
static void ieee80211_vap_iter_canpause(void *arg, struct ieee80211vap *vap)
{
    bool *can_pause = (bool *)arg;

    if ((*can_pause) && (ieee80211_vap_can_pause(vap) == false)) {
        *can_pause = false;
    }
}

/* Find the starting vap */
static void ieee80211_vap_iter_get_vap_starting(void *arg, struct ieee80211vap *vap)
{
    ieee80211_vap_t *starting_vap = (ieee80211_vap_t *)arg;
    ieee80211_resmgr_vap_priv_t resmgr_vap  = NULL;

    if (!(*starting_vap)) {
        resmgr_vap = ieee80211vap_get_resmgr(vap);
        if (resmgr_vap->state == VAP_STARTING) {
            *starting_vap = vap;
         }
    }
}

/* Event Deferring and Processing */
static void ieee80211_resmgr_defer_event(ieee80211_resmgr_t resmgr,
                                  ieee80211_resmgr_vap_priv_t resmgr_vap,
                                  ieee80211_resmgr_sm_event_t event,
                                  u_int16_t event_type)
{
    if (resmgr_vap) {
        /* VAP related event */
        if (!resmgr_vap->def_event_type) {
            OS_MEMCPY(&resmgr_vap->def_event, event, sizeof(struct ieee80211_resmgr_sm_event));
            resmgr_vap->def_event_type = event_type;
        }
    } else {
        /* SCAN related event */
        if ((resmgr->scandata.def_event_type == IEEE80211_RESMGR_EVENT_OFFCHAN_REQUEST) &&
            (event_type == IEEE80211_RESMGR_EVENT_BSSCHAN_REQUEST)) {
            OS_MEMZERO(&resmgr->scandata.def_event, sizeof(struct ieee80211_resmgr_sm_event));
            resmgr->scandata.def_event_type = 0;

        } else if (!resmgr->scandata.def_event_type) {
            OS_MEMCPY(&resmgr->scandata.def_event, event, sizeof(struct ieee80211_resmgr_sm_event));
            resmgr->scandata.def_event_type = event_type;
        }
    }
}

#endif /* WORK_IN_PROCESS  */

static void ieee80211_resmgr_clear_event(ieee80211_resmgr_t resmgr,
                                          ieee80211_resmgr_vap_priv_t resmgr_vap)
{
    if (resmgr_vap) {
        /* VAP related event */
        OS_MEMZERO(&resmgr_vap->def_event, sizeof(struct ieee80211_resmgr_sm_event));
        resmgr_vap->def_event_type = 0;
    } else {
        OS_MEMZERO(&resmgr->scandata.def_event, sizeof(struct ieee80211_resmgr_sm_event));
        resmgr->scandata.def_event_type = 0;
    }
}

static void
ieee80211_resmgr_process_deferred_events(ieee80211_resmgr_t resmgr)
{
    ieee80211_vap_t vap = NULL;
    ieee80211_resmgr_sm_event_t event = NULL;
    u_int16_t event_type = 0;
    ieee80211_resmgr_vap_priv_t resmgr_vap = NULL;
    struct ieee80211com *ic = resmgr->ic;
    bool dispatch_event = false;

    /*
     * This is a stable state, process deferred events if any ... 
     * Preference goes to deferred events from VAP first and then to SCAn
     */
    wlan_iterate_vap_list(ic, ieee80211_vap_iter_get_def_event, &vap);
    if (vap) {
        resmgr_vap = ieee80211vap_get_resmgr(vap);
        event = &resmgr_vap->def_event;
        event_type  = resmgr_vap->def_event_type;
    }

    if (event_type) {
        switch(event_type) {
        case IEEE80211_RESMGR_EVENT_VAP_START_REQUEST:
        {
            dispatch_event = true;
#if 0
            struct vap_iter_check_state_params params;
        
            resmgr_vap->state = VAP_STARTING;       
            resmgr_vap->chan = event->chan;

            /* Check for other active VAPs */
            OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
            wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

            /* Clear deferred event before changing state */
            resmgr_vap->def_event = NULL;    
            resmgr_vap->def_event_type = 0;

            if (params.vaps_active) {
                if (resmgr->mode == IEEE80211_RESMGR_MODE_SINGLE_CHANNEL) {
                    /* Single Channel Operation
                     * New STA Vap - Switch other vaps to its channel 
                     * New non-STA Vap - Start on channel being used
                     */
                    if (vap->iv_opmode == IEEE80211_M_STA) {
                        /* Send channel switch request to active VAPs on different channels */
                        wlan_iterate_vap_list(ic, ieee80211_vap_iter_chanswitch_request, resmgr_vap->chan);
                        ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_VAP_PRESTART);
                    } else {
                        resmgr_vap->chan = ic->ic_curchan; 
                        ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_VAP_STARTING);
                    }
                }
            } else {
                ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_VAP_STARTING);
            }
#endif
            break;
        }

        default:
            break;
        }

        if (dispatch_event) {
            ieee80211_resmgr_sm_dispatch(resmgr,
                                         event_type,
                                         sizeof(struct ieee80211_resmgr_sm_event),
                                         (void *)event);
            dispatch_event = false;
        }

        /* Clear deferred event */
        ieee80211_resmgr_clear_event(resmgr, resmgr_vap);
    }

    if (resmgr->scandata.def_event_type) {    
        event = &resmgr->scandata.def_event;
        event_type = resmgr->scandata.def_event_type;

        switch(event_type) {
        case IEEE80211_RESMGR_EVENT_OFFCHAN_REQUEST:
            dispatch_event = true;
#if 0
            /* Setup channel to be set after VAPs are paused */
            resmgr->scandata.chan = event->chan;
            resmgr->scandata.canpause_timeout = event->max_time ? event->max_time: CANPAUSE_INFINITE_WAIT;

            /* Clear deferred event before changing state */
            resmgr->scandata.def_event = NULL;    
            resmgr->scandata.def_event_type = 0;

            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_CANPAUSE);
#endif
            break;

        default:
            break;
        }

        if (dispatch_event) {
            ieee80211_resmgr_sm_dispatch(resmgr,
                                         event_type,
                                         sizeof(struct ieee80211_resmgr_sm_event),
                                         (void *)event);
        }
        ieee80211_resmgr_clear_event(resmgr, NULL);
    }
}

/*
 * State IDLE
 */
static void ieee80211_resmgr_state_multichan_idle_entry(void *ctx) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t) ctx;

    /* This is a stable state, process deferred events if any ...  */
    ieee80211_resmgr_process_deferred_events(resmgr);
}

static bool ieee80211_resmgr_state_multichan_idle_event(void *ctx, u_int16_t event_type, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t)ctx;
    ieee80211_resmgr_sm_event_t event = (ieee80211_resmgr_sm_event_t)event_data;
    ieee80211_vap_t vap = event->vap;
    ieee80211_resmgr_vap_priv_t resmgr_vap = NULL;
    bool retVal = true;
    struct ieee80211com *ic = resmgr->ic;
    ieee80211_resmgr_oc_scheduler_operation_data_t next_scheduled_operation = {OC_SCHED_OP_NONE, NULL};
    ieee80211_vap_t curr_vap, next_vap;
    bool transition_vap = false;
    bool transition_scan = false;
    bool check_scheduled_operation = false;
    
    if (vap) {
        resmgr_vap = ieee80211vap_get_resmgr(vap);
    }

    switch (event_type) {
    case IEEE80211_RESMGR_EVENT_VAP_START_REQUEST:
    {
        if (!resmgr_vap)
            break;

        ieee80211_resmgr_oc_sched_set_state(resmgr, OC_SCHEDULER_ON);

        /* Turn scheduler on before invoking next routine */
        ieee80211_resmgr_oc_sched_vap_transition(resmgr, vap,
            resmgr_vap, OFF_CHAN_SCHED_VAP_START);
        
        resmgr_vap->state = VAP_STARTING;
        resmgr_vap->chan = event->chan;
        
        DBG_PRINTK("VAP_STARTING:  resmgr_vap->chan = 0x%08x\n", resmgr_vap->chan);
        check_scheduled_operation = true;
        break;
    }

    case IEEE80211_RESMGR_EVENT_VAP_STOPPED:
    {
        struct vap_iter_check_state_params params;

        if (!resmgr_vap)
            break;

        /* Stop associated VAP */
        resmgr_vap->state = VAP_STOPPED;

        ieee80211_resmgr_oc_sched_vap_transition(resmgr, vap, 
            resmgr_vap, OFF_CHAN_SCHED_VAP_STOPPED);
        
        /* Check if any other vaps exist */
        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

        if (!params.vaps_running && !params.vap_starting) {
            ieee80211_resmgr_oc_sched_set_state(resmgr, OC_SCHEDULER_OFF); 
        }
        else {          
            check_scheduled_operation = true;
        }
        break;
    }

    case IEEE80211_RESMGR_EVENT_VAP_UP:
    {
        struct vap_iter_unpause_params params = {0, 0};
    
        resmgr_vap->state = VAP_ACTIVE;
    
        ieee80211_resmgr_oc_sched_vap_transition(resmgr, vap, 
            resmgr_vap, OFF_CHAN_SCHED_VAP_UP);
        
        /* Unpause VAP's on same channel */
        params.chan = resmgr_vap->chan;
        if (resmgr->mode == IEEE80211_RESMGR_MODE_MULTI_CHANNEL) {
            wlan_iterate_vap_list(ic, ieee80211_vap_iter_unpause_request, &params);
        } else {
            wlan_iterate_vap_list(ic, ieee80211_vap_iter_resume_request, &params);
        }
    
        break;
    }
        
    case IEEE80211_RESMGR_EVENT_VAP_PAUSE_COMPLETE:
    case IEEE80211_RESMGR_EVENT_VAP_PAUSE_FAIL:
    case IEEE80211_RESMGR_EVENT_VAP_RESUME_COMPLETE:
    case IEEE80211_RESMGR_EVENT_CHAN_SWITCH_REQUEST:
    case IEEE80211_RESMGR_EVENT_CSA_COMPLETE:
    case IEEE80211_RESMGR_EVENT_SCHED_BSS_REQUEST:
        DBG_PRINTK("%s : [*** WARNING ***] Encountered unhandled event_type!!\n", __func__);        
        ASSERT(0);
        break;
        
    case IEEE80211_RESMGR_EVENT_BSSCHAN_REQUEST:
    {
        ieee80211_resmgr_notification notif;

        /* Is Off-Channel Scheduler active? */
        if (ieee80211_resmgr_oc_sched_get_state(resmgr) != OC_SCHEDULER_ON) {       
            /* Set home channel */
            ASSERT(resmgr->scandata.chan); /* scanner should go to foreign first? */
            if (resmgr->scandata.chan)
                ic->ic_curchan = resmgr->scandata.chan;
            ic->ic_set_channel(ic);
        }
        
        resmgr->scandata.chan = NULL;
        resmgr->scandata.scan_chan = NULL;
        
        /* Intimate request completion */
        notif.type = IEEE80211_RESMGR_BSSCHAN_SWITCH_COMPLETE;
        notif.req_id = IEEE80211_RESMGR_REQID;
        notif.status = 0;
        notif.vap = NULL;
        IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);
        break;
    }

    case IEEE80211_RESMGR_EVENT_OFFCHAN_REQUEST:
    {
        ieee80211_resmgr_notification notif;

        /* Store current channel as home channel */
        resmgr->scandata.chan = ic->ic_curchan;
        resmgr->scandata.scan_chan = event->chan;

        /* Is Off-Channel Scheduler active? */
        if (ieee80211_resmgr_oc_sched_get_state(resmgr) != OC_SCHEDULER_ON) {       
            /* Set desired channel */
            ic->ic_curchan = event->chan;
            ic->ic_scan_start(ic);
            ic->ic_set_channel(ic);

            /* Intimate request completion */
            notif.type = IEEE80211_RESMGR_OFFCHAN_SWITCH_COMPLETE;
            notif.req_id = IEEE80211_RESMGR_REQID;
            notif.status = EOK;
            notif.vap = NULL;
            IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);
        }
        else {
            DBG_PRINTK("%s : OFFCHAN_REQ - max_dwell_time = %d\n", __func__, event->max_dwell_time);
            ieee80211_resmgr_oc_sched_req_duration_usec_set(resmgr->scandata.oc_sched_req, event->max_dwell_time * 1000);
            ieee80211_resmgr_oc_sched_req_start(resmgr, resmgr->scandata.oc_sched_req);
            
            ieee80211_resmgr_off_chan_scheduler(resmgr, IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_START_REQUEST);           
        }
        
        break;
    }

    case IEEE80211_RESMGR_EVENT_TSF_TIMER_EXPIRED:
    case IEEE80211_RESMGR_EVENT_TSF_TIMER_CHANGED:
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_START_REQUEST:
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_STOP_REQUEST:
        ieee80211_resmgr_off_chan_scheduler(resmgr, event_type);
        check_scheduled_operation = true;
        break;
        
    default:
        retVal = false;
        break;
    }

    if (ieee80211_resmgr_oc_sched_get_state(resmgr) == OC_SCHEDULER_ON) {
        
        if (check_scheduled_operation == true) {
            
            /* Fetch next scheduled operation */
            ieee80211_resmgr_oc_sched_get_scheduled_operation(resmgr, &next_scheduled_operation);

            next_vap = next_scheduled_operation.vap;

            /* Take care of post operation cleanup */
            
            switch (next_scheduled_operation.operation_type) {
                
            case OC_SCHED_OP_VAP:           
                if (resmgr->current_scheduled_operation.operation_type == OC_SCHED_OP_VAP) {
                    curr_vap = resmgr->current_scheduled_operation.vap;
                    resmgr_vap = ieee80211vap_get_resmgr(curr_vap); 
                    
                    if (next_vap != curr_vap) {                     
                        DBG_PRINTK("OP_VAP:  vap_force_pause curr_vap = 0x%08x\n", curr_vap);
                        
                        /* Force pause the currently active VAP */
                        curr_vap->iv_ic->ic_vap_pause_control(curr_vap->iv_ic,curr_vap,true);
                        //ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_PAUSING);
                        
                        resmgr_vap->state = VAP_PAUSED;
                        transition_vap = true;
                    }
                }
                else {
                    transition_vap = true;
                }
                break;
                
            case OC_SCHED_OP_SCAN:
                transition_scan = true;
                /* Falling through INTENTIONALLY */
            case OC_SCHED_OP_SLEEP:
                if (resmgr->current_scheduled_operation.operation_type == OC_SCHED_OP_VAP) {
                    curr_vap = resmgr->current_scheduled_operation.vap;
                    resmgr_vap = ieee80211vap_get_resmgr(curr_vap); 
                    
                    DBG_PRINTK("OP_SCAN:  vap_force_pause curr_vap = 0x%08x\n", curr_vap);

                    /* Force pause the current VAP */
                    curr_vap->iv_ic->ic_vap_pause_control(curr_vap->iv_ic,curr_vap,true);
                    //ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_PAUSING);
                    
                    
                    resmgr_vap->state = VAP_PAUSED;
                }           
                break;
                
            case OC_SCHED_OP_NOP:
            case OC_SCHED_OP_NONE:
            default:
                break;
            }

            /* Initiate the next operation */
            if (transition_scan == true) {
                
                DBG_PRINTK("%s : transition_scan == true\n", __func__);
                ieee80211_resmgr_notification notif;
                    
                /* Set desired channel */
                ic->ic_curchan = resmgr->scandata.scan_chan;
                ic->ic_scan_start(ic);
                ic->ic_set_channel(ic);

                /* Intimate request completion */
                notif.type = IEEE80211_RESMGR_OFFCHAN_SWITCH_COMPLETE;
                notif.req_id = IEEE80211_RESMGR_REQID;
                notif.status = EOK;
                notif.vap = NULL;
                IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);
            }

            if (transition_vap == true) {
                if (next_vap != NULL) {
                    
                    DBG_PRINTK("%s : transition_vap == true\n", __func__);                  
                    resmgr_vap = ieee80211vap_get_resmgr(next_vap);

                    switch (resmgr_vap->state) {
                    case VAP_STARTING:
                        {
                            ieee80211_resmgr_notification notif;
                            struct vap_iter_check_state_params params;
                            
                            /* Check if this is the first VAP starting */
                            OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
                            wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);
                            
                            /* Set the requested channel */
                            if (resmgr_vap && (!params.vaps_active || !IS_CHAN_EQUAL(ic->ic_curchan, resmgr_vap->chan))) {
                                ic->ic_curchan = resmgr_vap->chan;
                                ic->ic_bsschan = ic->ic_curchan;
                                ic->ic_set_channel(ic);
                                ieee80211_reset_erp(ic, ic->ic_curmode, next_vap->iv_opmode);
                                ieee80211_wme_initparams(next_vap);
                            }
                            
                            /* Set passed channel as BSS channel */
                            ic->ic_bsschan = ic->ic_curchan;
                            
                            /* Intimate start completion to VAP module */
                            notif.type = IEEE80211_RESMGR_VAP_START_COMPLETE;
                            notif.req_id = IEEE80211_RESMGR_REQID;
                            notif.status = 0;
                            notif.vap = next_vap;
                            IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);
                        }
                        break;
                        
                    case VAP_PAUSED:
                        DBG_PRINTK("%s : vap_force_unpause; next_vap\n", __func__);
                        
                        next_vap->iv_ic->ic_vap_pause_control(next_vap->iv_ic, next_vap, false);
                        resmgr_vap->state = VAP_ACTIVE;
                        break;
                        
                    default:
                        if (ic->ic_curchan != resmgr_vap->chan) {
                            /* Set home channel */
                            DBG_PRINTK("%s : resmgr_vap->chan = 0x%08x\n", __func__, resmgr_vap->chan);
                            ic->ic_curchan = resmgr_vap->chan;
                            ic->ic_set_channel(ic);
                        }
                        break;
                    }
                }
            }
        }
    }
    
    return retVal;
}

#if WORK_IN_PROCESS

static void ieee80211_resmgr_state_multichan_chansched_entry(void *ctx) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t) ctx;

    /* This is a stable state, process deferred events if any ...  */
    ieee80211_resmgr_process_deferred_events(resmgr);
}

static bool ieee80211_resmgr_state_multichan_chansched_event(void *ctx, u_int16_t event_type, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t)ctx;
    ieee80211_resmgr_sm_event_t event = (ieee80211_resmgr_sm_event_t)event_data;
    ieee80211_vap_t vap = event->vap;
    ieee80211_resmgr_vap_priv_t resmgr_vap = NULL;
    bool retVal = true;
    struct ieee80211com *ic = resmgr->ic;
    ieee80211_resmgr_oc_scheduler_operation_data_t next_scheduled_operation = {OC_SCHED_OP_NONE, NULL};
    ieee80211_vap_t curr_vap, next_vap;
    bool transition_vap = false;
    bool transition_scan = false;
    int retcode = EOK;
    bool check_scheduled_operation = false;
    
    if (vap) {
        resmgr_vap = ieee80211vap_get_resmgr(vap);
    }

}

/*
 * State PAUSING
 */
static void ieee80211_resmgr_state_multichan_pausing_entry(void *ctx) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t) ctx;
    struct ieee80211com *ic = resmgr->ic;

    /* Send Pause request to all active VAPs */
    wlan_iterate_vap_list(ic, ieee80211_vap_iter_pause_request, NULL);
}

static bool ieee80211_resmgr_state_multichan_pausing_event(void *ctx, u_int16_t event_type, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t) ctx;
    ieee80211_resmgr_sm_event_t event =  (ieee80211_resmgr_sm_event_t)event_data;
    ieee80211_vap_t vap = event->vap;
    ieee80211_resmgr_vap_priv_t resmgr_vap = NULL;
    struct ieee80211com *ic = resmgr->ic;
    bool retVal=true;
    ieee80211_resmgr_notification notif;         

    if (vap) {
        resmgr_vap = ieee80211vap_get_resmgr(vap);
    }

    switch (event_type) {
    case IEEE80211_RESMGR_EVENT_VAP_START_REQUEST:
    {
        if (!resmgr_vap)
            break;

        /* Store and change state after all the vaps are paused */
        ieee80211_resmgr_defer_event(resmgr, resmgr_vap, event, event_type);
        break;
    }

    case IEEE80211_RESMGR_EVENT_VAP_STOPPED:
    {
        struct vap_iter_check_state_params params;

        if (!resmgr_vap)
            break;

        /* Stop associated VAP */
        resmgr_vap->state = VAP_STOPPED;

        ieee80211_resmgr_oc_sched_vap_transition(resmgr, vap, 
            resmgr_vap, OFF_CHAN_SCHED_VAP_STOPPED);
        
        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);
        if (!params.vaps_running) {
            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_IDLE);
        }
        break;
    }

    case IEEE80211_RESMGR_EVENT_VAP_PAUSE_COMPLETE:
    {
        struct vap_iter_check_state_params params;

        if (resmgr_vap->state != VAP_STOPPED) {
            /* If VAP is already stopped, don't update state */
            resmgr_vap->state = VAP_PAUSED;
        }

        /* Check if all the VAPs are paused */
        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

        if (!params.vaps_active) {
            if (resmgr->cancel_pause) {
                /* Pending pause cancel */
                resmgr->scandata.chan = ic->ic_curchan;
                resmgr->cancel_pause = false;
                ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_RESUMING);
            } else {
                ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_OFFCHAN);
            }
        } else if (params.vaps_active == 1 && resmgr->pause_failed) {
            if (resmgr->cancel_pause) {
                /* Pending pause cancel */
                resmgr->scandata.chan = ic->ic_curchan;
                resmgr->cancel_pause = false;
                ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_RESUMING);
            } else {
                struct vap_iter_unpause_params unpause_params = {0, 0};
                /* Unpause VAPs on channel */
                unpause_params.chan = ic->ic_curchan;
                wlan_iterate_vap_list(ic, ieee80211_vap_iter_unpause_request, &unpause_params);
                resmgr->pause_failed = false;
            }
        }
        break;
    }

    case IEEE80211_RESMGR_EVENT_VAP_PAUSE_FAIL:
    {
        struct vap_iter_check_state_params params;

        /* Unpause VAPs operating on channel */
        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        params.chan = ic->ic_curchan; 
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

        if (params.vaps_active > 1) {
            /* VAPs still in the process of pausing, need to unpaused them after they are paused */
            resmgr->pause_failed = true;

        } else if (params.vaps_paused) {
            if (resmgr->cancel_pause) {
                /* Pending pause cancel */
                resmgr->scandata.chan = ic->ic_curchan;
                resmgr->cancel_pause = false;
                ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_RESUMING);
            } else {
                struct vap_iter_unpause_params unpause_params = {0, 0};
                /* Unpause VAPs on channel */
                unpause_params.chan = ic->ic_curchan;
                wlan_iterate_vap_list(ic, ieee80211_vap_iter_unpause_request, &unpause_params);
            }
        } else {
            /* Check if there is a cancel pending */
            if (resmgr->cancel_pause) {
                resmgr->cancel_pause = false;
                notif.type = IEEE80211_RESMGR_BSSCHAN_SWITCH_COMPLETE;
                notif.status = EOK;
            } else {
                notif.type = IEEE80211_RESMGR_OFFCHAN_SWITCH_COMPLETE;
                notif.status = EIO;
            }
            notif.req_id = IEEE80211_RESMGR_REQID;
            notif.vap = NULL;
            IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);

            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_BSSCHAN);
        }
        break;
    }

    case IEEE80211_RESMGR_EVENT_BSSCHAN_REQUEST:
    {
        /* Equivalent to cancelling the off channel request */
        struct vap_iter_check_state_params params;

        /* Unpause VAPs operating on channel */
        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        params.chan = ic->ic_curchan; 
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

        if (params.vaps_active) {
            /* VAPs still in the process of pausing, unpause them after pause completion */
            resmgr->cancel_pause = true;

        } else if (params.vaps_paused) {
            resmgr->scandata.chan = ic->ic_curchan;
            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_RESUMING);
        } else {
            resmgr->scandata.chan = NULL;

            /* Intimate request completion */
            notif.type = IEEE80211_RESMGR_BSSCHAN_SWITCH_COMPLETE;
            notif.req_id = IEEE80211_RESMGR_REQID;
            notif.status = 0;
            notif.vap = NULL;
            IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);

            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_BSSCHAN);
        }
        break;
    }

    case IEEE80211_RESMGR_EVENT_VAP_RESUME_COMPLETE:
    {
        struct vap_iter_check_state_params params;
        resmgr_vap->state = VAP_ACTIVE;

        /* Check if all the VAPs on the channel have been unpaused */
        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        params.chan = ic->ic_curchan;
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

        /* Intimate Pause failure to Scan Module */
        if (!params.vaps_paused) {
            if (resmgr->cancel_pause) {
                resmgr->cancel_pause = false;
                notif.type = IEEE80211_RESMGR_BSSCHAN_SWITCH_COMPLETE;
                notif.status = EOK;
            } else {
                notif.type = IEEE80211_RESMGR_OFFCHAN_SWITCH_COMPLETE;
                notif.status = EIO;
            }
            notif.req_id = IEEE80211_RESMGR_REQID;
            notif.vap = NULL;
            IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);

            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_BSSCHAN);
        }
        break;
    }
    
    case IEEE80211_RESMGR_EVENT_TSF_TIMER_EXPIRED:
    case IEEE80211_RESMGR_EVENT_TSF_TIMER_CHANGED:
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_START_REQUEST:
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_STOP_REQUEST:
        ieee80211_resmgr_off_chan_scheduler(resmgr, event_type);
        break;
        
    default:
        retVal = false;
        break;
    }

    return retVal;
}

static void ieee80211_resmgr_state_multichan_pausing_exit(void *ctx) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t) ctx;

    /* Make sure the lingering flags are cleared */
    resmgr->cancel_pause = false;
    resmgr->pause_failed = false;
}

/*
 * State RESUMING
 */
static void ieee80211_resmgr_state_multichan_chanswitch_entry(void *ctx) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t) ctx;
    struct ieee80211com *ic = resmgr->ic;
    struct vap_iter_unpause_params params = {0, 0};

    /* Set required channel */
    ic->ic_curchan = resmgr->scandata.chan;
    ic->ic_scan_end(ic);
    ic->ic_set_channel(ic);

    /* Unpause VAPs operating on selected BSS channel */
    params.chan = ic->ic_curchan;
    wlan_iterate_vap_list(ic, ieee80211_vap_iter_unpause_request, &params);
}

static bool ieee80211_resmgr_state_multichan_chanswitch_event(void *ctx, u_int16_t event_type, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_resmgr_t resmgr = (ieee80211_resmgr_t )ctx;
    struct ieee80211com *ic = resmgr->ic;
    ieee80211_resmgr_vap_priv_t resmgr_vap = NULL;
    ieee80211_resmgr_sm_event_t event =  (ieee80211_resmgr_sm_event_t)event_data;
    ieee80211_vap_t vap = event->vap;
    ieee80211_resmgr_notification notif;
    bool retVal = true;

    if (vap) {
        resmgr_vap = ieee80211vap_get_resmgr(vap);
    }

    switch (event_type) {
    case IEEE80211_RESMGR_EVENT_VAP_START_REQUEST:
    {
        if (!resmgr_vap)
            break;

        ieee80211_resmgr_defer_event(resmgr, resmgr_vap, event, event_type);
        break;
    }

    case IEEE80211_RESMGR_EVENT_VAP_STOPPED:
    {
        struct vap_iter_check_state_params params;

        if (!resmgr_vap)
            break;

        /* Stop associated VAP */
        resmgr_vap->state = VAP_STOPPED;

        ieee80211_resmgr_oc_sched_vap_transition(resmgr, vap, 
            resmgr_vap, OFF_CHAN_SCHED_VAP_STOPPED);
        
        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

        if (!params.vaps_running) {
            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_IDLE);
        }
        break;
    }

    case IEEE80211_RESMGR_EVENT_OFFCHAN_REQUEST:
        /* Store ? */
        break;

    case IEEE80211_RESMGR_EVENT_VAP_RESUME_COMPLETE:
    {
        struct vap_iter_check_state_params params;
        if (resmgr_vap)
            resmgr_vap->state = VAP_ACTIVE;

        /* Check if all the VAPs on the channel have been unpaused */
        OS_MEMZERO(&params, sizeof(struct vap_iter_check_state_params));
        params.chan = ic->ic_curchan;
        wlan_iterate_vap_list(ic, ieee80211_vap_iter_check_state, &params);

        /* Intimate request completion to VAP module */
        if (!params.vaps_paused) {
            notif.type = IEEE80211_RESMGR_BSSCHAN_SWITCH_COMPLETE;
            notif.req_id = IEEE80211_RESMGR_REQID;
            notif.status = 0;
            notif.vap = NULL;
            IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);
            ieee80211_sm_transition_to(resmgr->hsm_handle, IEEE80211_RESMGR_STATE_BSSCHAN);
        }
        break;
    }

    case IEEE80211_RESMGR_EVENT_TSF_TIMER_EXPIRED:
    case IEEE80211_RESMGR_EVENT_TSF_TIMER_CHANGED:
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_START_REQUEST:
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_STOP_REQUEST:
        ieee80211_resmgr_off_chan_scheduler(resmgr, event_type);
        break;
        
    default:
        retVal = false;
        break;
    }

    return retVal;
}

#endif /* WORK_IN_PROCESS */

ieee80211_state_info ieee80211_resmgr_multichan_sm_info[] = {
    { 
        (u_int8_t) IEEE80211_RESMGR_STATE_IDLE, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "IDLE",
        ieee80211_resmgr_state_multichan_idle_entry,
        NULL,
        ieee80211_resmgr_state_multichan_idle_event
    },
    
#if WORK_IN_PROCESS
    { 
        (u_int8_t) IEEE80211_RESMGR_STATE_CHANSCHED, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "CHANSCHED",
        ieee80211_resmgr_state_multichan_chansched_entry,
        NULL,
        ieee80211_resmgr_state_multichan_chansched_event
    },
    { 
        (u_int8_t) IEEE80211_RESMGR_STATE_PAUSING, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "PAUSING",
        ieee80211_resmgr_state_multichan_pausing_entry,
        ieee80211_resmgr_state_multichan_pausing_exit,
        ieee80211_resmgr_state_multichan_pausing_event
    },
    { 
        (u_int8_t) IEEE80211_RESMGR_STATE_CHANSWITCH, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "CHANSWITCH",
        ieee80211_resmgr_state_multichan_chanswitch_entry,
        NULL,
        ieee80211_resmgr_state_multichan_chanswitch_event
    }
#endif /* WORK_IN_PROCESS */

};

int ieee80211_resmgr_multichan_sm_create(ieee80211_resmgr_t resmgr)
{
    struct ieee80211com *ic = resmgr->ic;

    /* Create State Machine and start */
    resmgr->hsm_handle = ieee80211_sm_create(ic->ic_osdev, 
                                             "ResMgr_MultiChan",
                                             (void *)resmgr, 
                                             IEEE80211_RESMGR_STATE_IDLE,
                                             ieee80211_resmgr_multichan_sm_info,
                                             sizeof(ieee80211_resmgr_multichan_sm_info)/sizeof(ieee80211_state_info),
                                             MAX_QUEUED_EVENTS,
                                             sizeof(struct ieee80211_resmgr_sm_event), /* size of event data */
                                             MESGQ_PRIORITY_HIGH,
                                             ic->ic_reg_parm.resmgrSyncSm ? IEEE80211_HSM_SYNCHRONOUS : IEEE80211_HSM_ASYNCHRONOUS, 
                                             ieee80211_resmgr_multichan_sm_debug_print,
                                             resmgr_event_name,
                                             IEEE80211_N(resmgr_event_name)); 
    if (!resmgr->hsm_handle) {
        printk("%s : ieee80211_sm_create failed \n", __func__);
        return ENOMEM;
    }

    return EOK;
}

void ieee80211_resmgr_multichan_sm_delete(ieee80211_resmgr_t resmgr)
{
    ieee80211_sm_delete(resmgr->hsm_handle);
}

#endif
#endif

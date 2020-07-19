/*
 *  Copyright (c) 2009 Atheros Communications Inc.  All rights reserved.
 */
/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */


#include "ieee80211_var.h"
#include <ieee80211_channel.h>
#include <ieee80211_resmgr_priv.h>
#include "ieee80211_resmgr_oc_scheduler_priv.h"

/*
 * Resource Manager Off-Channel Scheduler is not compiled in by default.
 */ 
#ifndef UMAC_SUPPORT_RESMGR_OC_SCHED
#define UMAC_SUPPORT_RESMGR_OC_SCHED 0
#endif

#if UMAC_SUPPORT_RESMGR_OC_SCHED

#define  PERIODIC_NOA_SCHEDULE  0
#define  ONE_SHOT_NOA_SCHEDULE  1

int ieee80211_resmgr_oc_sched_debug_level = OC_SCHED_DEFAULT_DEBUG_LEVEL;
bool ieee80211_resmgr_oc_sched_multi_chan_enable = OC_SCHED_DEFAULT_MULTI_CHAN_ENABLE;
static void   ieee80211_rm_ocs_cleanup_noa_schedules(ieee80211_resmgr_oc_scheduler_t sched);   
static void   ieee80211_rm_ocs_clear_noa_schedules(ieee80211_resmgr_oc_scheduler_t sched);

u_int32_t
ieee80211_resmgr_oc_sched_get_noa_schedule_delay_msec(
    ieee80211_resmgr_oc_scheduler_t sched
    )
{
    if (sched == NULL) {
        return(0);
    }
    
    return((sched->noa_event_scheduling_delay / 1000));
}

int
ieee80211_resmgr_oc_sched_set_noa_schedule_delay_msec(
    ieee80211_resmgr_oc_scheduler_t sched,
    u_int32_t noa_schedule_delay_msec
    )
{
    if (sched == NULL) {
        return(EIO);
    }

    if (noa_schedule_delay_msec < MAX_SCHEDULING_TIME_MSEC) {
        
        IEEE80211_RESMGR_OCSCHE_LOCK(sched);
        
        sched->noa_event_scheduling_delay = (noa_schedule_delay_msec * 1000);

        /* Invoke scheduler in order to begin using the new NOA scheduling delay */
        ieee80211_resmgr_off_chan_scheduler(sched->resmgr, 
                IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_START_REQUEST);
        
        IEEE80211_RESMGR_OCSCHE_UNLOCK(sched);
        
        return(EOK);
    }
   
    return(EINVAL);
}

u_int32_t 
ieee80211_resmgr_oc_sched_get_air_time_limit(
    ieee80211_resmgr_t resmgr,
    ieee80211_resmgr_vap_priv_t resmgr_vap
    )
{
    if (resmgr->oc_scheduler == NULL) {
        return 0;
    }

    return(resmgr_vap->sched_data.scheduled_air_time_limit);
}

int 
ieee80211_resmgr_oc_sched_set_air_time_limit(
    ieee80211_resmgr_t resmgr,
    ieee80211_resmgr_vap_priv_t resmgr_vap,
    u_int32_t scheduled_air_time_limit
    )
{
    int retVal = EOK;
    ieee80211_resmgr_oc_scheduler_t sched = resmgr->oc_scheduler;

    if (sched == NULL) {
        return EOK;
    }

    IEEE80211_RESMGR_OCSCHE_LOCK(sched);
    
    if (scheduled_air_time_limit <= 100) {
        resmgr_vap->sched_data.scheduled_air_time_limit = scheduled_air_time_limit;
    }
    else {
        retVal = EINVAL;
    }
    
    IEEE80211_RESMGR_OCSCHE_UNLOCK(sched);
    
    return(retVal);
}
    

static void
ieee80211_rm_ocs_print_statistics(ieee80211_resmgr_oc_scheduler_t sched)
{
    if (ieee80211_resmgr_oc_sched_debug_level > 0) {
        DBG_PRINTK("%s : **** STATISTICS ****\n", __func__);
        DBG_PRINTK("%s : scheduler_run_count    = %d\n", __func__, sched->stats.scheduler_run_count);
        DBG_PRINTK("%s : tsf_timer_expire_count = %d\n", __func__, sched->stats.tsf_timer_expire_count);
        DBG_PRINTK("%s : tsf_time_changed_count = %d\n", __func__, sched->stats.tsf_time_changed_count);
        DBG_PRINTK("%s : request_start_count    = %d\n", __func__, sched->stats.request_start_count);
        DBG_PRINTK("%s : request_stop_count     = %d\n", __func__, sched->stats.request_stop_count);
        DBG_PRINTK("%s : vap_transition_count   = %d\n", __func__, sched->stats.vap_transition_count);
        DBG_PRINTK("%s : halt_count             = %d\n", __func__, sched->stats.halt_count);
        DBG_PRINTK("%s : mlme_bmiss_count       = %d\n", __func__, sched->stats.mlme_bmiss_count);
        DBG_PRINTK("%s : mlme_bmiss_clear_count = %d\n", __func__, sched->stats.mlme_bmiss_clear_count);
        DBG_PRINTK("%s : one_shot_start_count   = %d\n", __func__, sched->stats.one_shot_start_count);
        DBG_PRINTK("%s : noa_schedule_updates   = %d\n", __func__, sched->stats.noa_schedule_updates);
        DBG_PRINTK("%s : ********************\n", __func__);
    }
}

int 
ieee80211_resmgr_oc_sched_register_noa_event_handler(ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap, ieee80211_resmgr_noa_event_handler handler, void *arg)
{
    int retval = EOK;
    struct ieee80211com         *ic = resmgr->ic;
    ieee80211_resmgr_oc_scheduler_t sched = resmgr->oc_scheduler;
    ieee80211_resmgr_oc_sched_noa_event_vap_t nev = NULL;
    ieee80211_resmgr_oc_sched_noa_event_vap_t cur_nev;
    bool found = false;
    
    if ((sched == NULL) || (vap == NULL)) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                             "%s : Invalid VAP=0x%x or oc_scheduler=0x%x handle\n",
                             __func__, vap, sched);
        return(EINVAL);
    }
    
    if (handler != NULL) {      
        if (sched != NULL) {

            /* Check for a duplicate entry on the noa_event_vap_list */
            if (! TAILQ_EMPTY(&(sched->noa_event_vap_list))) {
                TAILQ_FOREACH(cur_nev, &(sched->noa_event_vap_list), nev_te) {
                    if (cur_nev->vap == vap) {
                        found = true;
                        break;
                    }
                }
            }

            if (found == true) {
                return(retval);
            }
            
            nev = (ieee80211_resmgr_oc_sched_noa_event_vap_t) OS_MALLOC(ic->ic_osdev, 
                sizeof(struct ieee80211_resmgr_oc_sched_noa_event_vap), 0);
            if (nev == NULL) {
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                                     "%s : Memory allocation failure!\n", __func__);
                return(ENOMEM);             
            }

            OS_MEMZERO(nev, sizeof(struct ieee80211_resmgr_oc_sched_noa_event_vap));
            
            nev->vap = vap;
            nev->handler = handler;
            nev->arg = arg;
            
            IEEE80211_RESMGR_OCSCHE_LOCK(sched);

            TAILQ_INSERT_TAIL(&(sched->noa_event_vap_list), nev, nev_te);
            
            IEEE80211_RESMGR_OCSCHE_UNLOCK(sched);
        }       
    }   
    
    return(retval);
}

int 
ieee80211_resmgr_oc_sched_unregister_noa_event_handler(ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap)
{
    int retval = EOK;
    struct ieee80211com         *ic = resmgr->ic;
    ieee80211_resmgr_oc_scheduler_t sched = resmgr->oc_scheduler;
    ieee80211_resmgr_oc_sched_noa_event_vap_t cur_nev, temp_nev;

    if (vap == NULL) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                             "%s : Invalid VAP handle\n", __func__);
        return(EINVAL);
    }
    
    if (sched != NULL) {
        
        IEEE80211_RESMGR_OCSCHE_LOCK(sched);
        
        if (! TAILQ_EMPTY(&(sched->noa_event_vap_list))) {          
            TAILQ_FOREACH_SAFE(cur_nev, &(sched->noa_event_vap_list), nev_te, temp_nev) {
                if (cur_nev->vap == vap) {
                    TAILQ_REMOVE(&(sched->noa_event_vap_list), cur_nev, nev_te);
                    OS_FREE(cur_nev);
                    break;
                }
            }
        }
        
        IEEE80211_RESMGR_OCSCHE_UNLOCK(sched);
    }
    
    return(retval);
}
    
static void
ieee80211_rm_ocs_count_timeslots(
        ieee80211_resmgr_oc_scheduler_t sched, 
        oc_sched_timeslot_head *timeslot_head
        )
{
    ieee80211_rm_oc_sched_timeslot_t cur_ts;
    struct ieee80211com         *ic = sched->resmgr->ic;
    int count_ts = 0;
    
    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);

    if (! TAILQ_EMPTY(timeslot_head)) {
        TAILQ_FOREACH(cur_ts, timeslot_head, ts_te) {
            count_ts++;
        }
    }

    DBG_PRINTK("[TS COUNT] : Timeslots = %d\n", count_ts);
        
    return;
}

ieee80211_resmgr_oc_sched_state_t ieee80211_resmgr_oc_sched_get_state(
    ieee80211_resmgr_t resmgr
    )
{
    ieee80211_resmgr_oc_scheduler_t sched = resmgr->oc_scheduler;

    if (sched != NULL) {
        return(sched->scheduler_state);
    }
        
    return(OC_SCHEDULER_OFF);
}

void ieee80211_resmgr_oc_sched_set_state(
    ieee80211_resmgr_t resmgr,
    ieee80211_resmgr_oc_sched_state_t new_state
    )
{
    ieee80211_resmgr_oc_scheduler_t sched = resmgr->oc_scheduler;
    int retval = EOK;
    
    if (sched != NULL) {
        sched->scheduler_state = new_state;

        switch (new_state) {
            
        case OC_SCHEDULER_OFF:
        default:    
            
            /* Stop any pending TSF timer */            
            retval = ieee80211_tsftimer_stop(sched->tsftimer_hdl);
            if (retval != EOK) {
                DBG_PRINTK("%s : Failed to stop TSF timer!\n", __func__);
            }

            sched->run_state = OC_SCHED_STATE_HALT;

            ieee80211_resmgr_oc_scheduler(resmgr, OFF_CHAN_SCHED_HALT_SCHEDULER);
            
            break;
            
        case OC_SCHEDULER_ON:
            sched->run_state = OC_SCHED_STATE_RESTART;
            break;
        }
    }

    return;
}

void ieee80211_resmgr_oc_sched_vap_transition(
    ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap,
    ieee80211_resmgr_vap_priv_t resmgr_vap,
    ieee80211_resmgr_oc_sched_vap_event_type event_type
    )
{
    ieee80211_resmgr_oc_sched_req_t req;
    int i;
    ieee80211_resmgr_oc_scheduler_t sched;
    bool start_scheduler = false;
    u_int32_t next_tbtt_tsf, bintval;
    u_int32_t   param1 = 0, param2 = 0;
    int retval = EOK;
    struct ieee80211com         *ic = resmgr->ic;
    u_int32_t high_prio_duration;
    u_int32_t bintval_msec;

    sched = resmgr->oc_scheduler;

    if ((sched == NULL) || (sched->scheduler_state != OC_SCHEDULER_ON)) {
        return;
    }
    
    switch (event_type) {
    case OFF_CHAN_SCHED_VAP_START:
        DBG_PRINTK("%s : VAP_START\n", __func__);
        
        /* Register the callback with mlme module to receive mlme events (beacon miss ) */
        if ( ieee80211_mlme_register_event_handler( vap, ieee80211_rm_ocs_mlme_event_handler, resmgr_vap) != EOK) {
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                                 "%s : Unable to register event handler with MLME!\n", __func__);
        }
        
        /* Start the One-Shot scheduling request */
        req = resmgr_vap->sched_data.req_array[OC_SCHED_HIGH_PRIO_1SHOT];

        ieee80211_resmgr_oc_sched_req_stop(resmgr, req);
                        
        ieee80211_resmgr_oc_sched_req_start(resmgr, req); 
        break;
    case OFF_CHAN_SCHED_VAP_UP:
        DBG_PRINTK("%s : VAP_UP\n", __func__);
        
        /* Register the callback with mlme module to receive mlme events (beacon miss ), if already registered mlme layer will ignore it. */
        if ( ieee80211_mlme_register_event_handler( vap, ieee80211_rm_ocs_mlme_event_handler, resmgr_vap) != EOK) {
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                                 "%s : Unable to register event handler with MLME!\n", __func__);
        }
        
        /* Start the One-Shot scheduling request */
        req = resmgr_vap->sched_data.req_array[OC_SCHED_HIGH_PRIO_1SHOT];
        
        ieee80211_resmgr_oc_sched_req_stop(resmgr, req); 

        req = resmgr_vap->sched_data.req_array[OC_SCHED_HIGH_PRIO_PERIODIC];

        retval = ieee80211_vap_ath_info_get(vap, ATH_VAP_INFOTYPE_SLOT_OFFSET, &param1, &param2);
        if (retval != EOK) {
            /* Error in getting the current TSF Offset. */
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                                 "%s : ieee80211_vap_ath_info_get failed\n", __func__);
        }
                
        DBG_PRINTK("%s : param1 = 0x%08x\n", __func__, param1);
        DBG_PRINTK("%s : param2 = 0x%08x\n", __func__, param2);
        
        next_tbtt_tsf = get_next_tbtt_tsf_32(vap) + param1;
        bintval = get_beacon_interval(vap);
        
        /* 
         * The SWBA interrupt must be processed prior to the TBTT in STA mode.
         *  It is necessary to add this additional time requirement on the high prio scheduler
         *  request.
         */
        req->interval_usec = bintval;
        req->previous_pending_timeslot_tsf = next_tbtt_tsf - (bintval + 
            (sched->swba_delay_before_tbtt_usec + sched->device_pause_delay_usec));

        DBG_PRINTK("%s : next_tbtt_tsf_32 = 0x%08x\n", __func__, next_tbtt_tsf);
        DBG_PRINTK("%s : beacon interval = 0x%08x\n", __func__, bintval);
        DBG_PRINTK("%s : previous_pending_timeslot_tsf = 0x%08x\n", __func__, 
            req->previous_pending_timeslot_tsf);
        
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                             "[OC_Sched] next_tbtt_tsf_32 = 0x%08x  beacon_interval = 0x%08x previous_pending_timeslot_tsf = 0x%08x\n", 
                             next_tbtt_tsf, bintval, req->previous_pending_timeslot_tsf);

        /* Adding 500 usec to round up the total time */        
        high_prio_duration = 
            ((req->duration_usec) + 500) / 1000;
        bintval_msec = bintval / 1000;
        resmgr_vap->sched_data.high_prio_air_time_percent = 
            ((((MAX_SCHEDULING_TIME_MSEC / bintval_msec) * high_prio_duration) * 100) / MAX_SCHEDULING_TIME_MSEC);
            
        DBG_PRINTK("%s : high_prio_duration = 0x%08x\n", __func__, high_prio_duration);
        DBG_PRINTK("%s : high_prio_air_time_percent = 0x%08x\n", __func__, resmgr_vap->sched_data.high_prio_air_time_percent);
            
        /* Start the high priority periodic scheduling request */
        ieee80211_resmgr_oc_sched_req_start(resmgr, req);               

        /* Start the low priority periodic scheduling request */
        req = resmgr_vap->sched_data.req_array[OC_SCHED_LOW_PRIO_PERIODIC];
        ieee80211_resmgr_oc_sched_req_start(resmgr, req);

        break;
    case OFF_CHAN_SCHED_VAP_STOPPED:
        DBG_PRINTK("%s : VAP_STOPPED\n", __func__);
        
        ieee80211_mlme_unregister_event_handler( vap, ieee80211_rm_ocs_mlme_event_handler, resmgr_vap);
        
        /* Stop all schedule requests for this VAP */
        for (i=0; i < OC_SCHED_REQ_INDEX_MAX; i++) {
            req = resmgr_vap->sched_data.req_array[i];
            if (req != NULL) {
                ieee80211_resmgr_oc_sched_req_stop(resmgr, req);
            }
        }
        break;
    default:
        break;
    }

    if (sched->current_active_req != NULL) {
        if (vap == sched->current_active_req->vap) {
            start_scheduler = true;
        }
    }
    else {
        start_scheduler = true;
    }

    if (start_scheduler == true) {    
        DBG_PRINTK("%s : Scheduler - VAP TRANSITION\n", __func__);
        ieee80211_resmgr_oc_scheduler(resmgr, OFF_CHAN_SCHED_VAP_TRANSITION);
    }
}

void ieee80211_resmgr_oc_sched_req_duration_usec_set(
    ieee80211_resmgr_oc_sched_req_handle_t req_handle,
    u_int32_t duration_usec
    )
{
    ((ieee80211_resmgr_oc_sched_req_t) req_handle)->duration_usec = duration_usec;
}

void ieee80211_resmgr_oc_sched_req_interval_usec_set(
    ieee80211_resmgr_oc_sched_req_handle_t req_handle,
    u_int32_t interval_usec
    )
{
    ((ieee80211_resmgr_oc_sched_req_t) req_handle)->interval_usec = interval_usec;
}

void 
ieee80211_resmgr_oc_sched_req_priority_set(
    ieee80211_resmgr_oc_sched_req_handle_t req_handle,
    ieee80211_resmgr_oc_sched_prio_t priority
    )
{
    ((ieee80211_resmgr_oc_sched_req_t) req_handle)->priority = priority;
}

void 
ieee80211_resmgr_oc_sched_vattach(
    ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap,
    ieee80211_resmgr_vap_priv_t resmgr_vap
    )
{
    int i;
    ieee80211_resmgr_oc_sched_req_t req = NULL;
    ieee80211_resmgr_oc_scheduler_t sched;

    if (resmgr->oc_scheduler == NULL) {
        return;
    }

#define  HIGH_PRIO_ONE_SHOT_204_MSEC 204
#define  HIGH_PRIO_ONE_SHOT_1_SEC  1000
#define  HIGH_PRIO_PERIODIC_DURATION_8_MSEC  8
#define  HIGH_PRIO_PERIODIC_DURATION_12_MSEC  12
#define  HIGH_PRIO_PERIODIC_DURATION_16_MSEC  16
#define  HIGH_PRIO_PERIODIC_DURATION_20_MSEC  20
#define  HIGH_PRIO_PERIODIC_INTERVAL_102_MSEC  102
#define  NO_AIR_TIME_LIMIT  100

    DBG_PRINTK("%s : VAP Attach\n", __func__);

    if (resmgr_vap != NULL) {

        resmgr_vap->sched_data.scheduled_air_time_limit = NO_AIR_TIME_LIMIT;
        
        sched = resmgr->oc_scheduler;
        
        for (i=0; i < OC_SCHED_REQ_INDEX_MAX; i++) {
            switch (i) {
            case OC_SCHED_HIGH_PRIO_1SHOT:
                req = ieee80211_resmgr_oc_sched_req_alloc(resmgr, vap, 
                    HIGH_PRIO_ONE_SHOT_1_SEC, 
                    0, OFF_CHAN_SCHED_PRIO_HIGH,
                    false, OC_SCHEDULER_VAP_REQTYPE);
                
                req->schedule_asap = true;
                
                break;
            case OC_SCHED_HIGH_PRIO_PERIODIC:
                req = ieee80211_resmgr_oc_sched_req_alloc(resmgr, vap,    
                    HIGH_PRIO_PERIODIC_DURATION_12_MSEC, 
                    HIGH_PRIO_PERIODIC_INTERVAL_102_MSEC, OFF_CHAN_SCHED_PRIO_HIGH,
                    true, OC_SCHEDULER_VAP_REQTYPE);
                if (req) {
                    req->duration_usec += (sched->device_pause_delay_usec + sched->swba_delay_before_tbtt_usec);
                }
                break;
            case OC_SCHED_LOW_PRIO_1SHOT:
                req = ieee80211_resmgr_oc_sched_req_alloc(resmgr, vap, 
                    0, 0, OFF_CHAN_SCHED_PRIO_LOW,
                    false, OC_SCHEDULER_VAP_REQTYPE);
                break;
            case OC_SCHED_LOW_PRIO_PERIODIC:
                req = ieee80211_resmgr_oc_sched_req_alloc(resmgr, vap, 
                    0, 0, OFF_CHAN_SCHED_PRIO_LOW,
                    true, OC_SCHEDULER_VAP_REQTYPE);
                break;
            case OC_SCHED_MLME_1SHOT:
                req = ieee80211_resmgr_oc_sched_req_alloc(resmgr, vap, 
                    HIGH_PRIO_ONE_SHOT_1_SEC, 
                    0, OFF_CHAN_SCHED_PRIO_HIGH,
                    false, OC_SCHEDULER_VAP_REQTYPE);
                
                req->schedule_asap = true;
                
                break;
            default:
                req = NULL;
                break;
            }
            
            resmgr_vap->sched_data.req_array[i] = req;
        }       
    }
    
#undef HIGH_PRIO_ONE_SHOT_204_MSEC
#undef HIGH_PRIO_ONE_SHOT_1_SEC
#undef HIGH_PRIO_PERIODIC_DURATION_8_MSEC
#undef HIGH_PRIO_PERIODIC_DURATION_12_MSEC
#undef HIGH_PRIO_PERIODIC_DURATION_16_MSEC
#undef HIGH_PRIO_PERIODIC_DURATION_20_MSEC
#undef HIGH_PRIO_PERIODIC_INTERVAL_102_MSEC
#undef NO_AIR_TIME_LIMIT

}

void 
ieee80211_resmgr_oc_sched_vdetach(
    ieee80211_resmgr_t resmgr,
    ieee80211_resmgr_vap_priv_t resmgr_vap
    )
{
    int i;

    DBG_PRINTK("%s : VAP Detach\n", __func__);
    
    if (resmgr->oc_scheduler == NULL) {
        return;
    }

    if (resmgr_vap != NULL) {
        for (i=0; i < OC_SCHED_REQ_INDEX_MAX; i++) {
            if (resmgr_vap->sched_data.req_array[i] != NULL) {
                
                ieee80211_resmgr_oc_sched_req_stop(resmgr,
                    resmgr_vap->sched_data.req_array[i]);
                    
                ieee80211_resmgr_oc_sched_req_free(resmgr, 
                    resmgr_vap->sched_data.req_array[i]);
            }
        }
    }
}

#ifdef SCHEDULER_TEST    

void 
ieee80211_rm_ocs_scheduler_test(
    ieee80211_resmgr_t resmgr,
    u_int32_t prev_pending_timeslot_tsf
    )
{
    ieee80211_resmgr_oc_sched_req_handle_t req_handle[20];
    u_int32_t duration;
    u_int32_t interval;
    struct ieee80211vap vap;
    bool periodic;
    int retVal;
    int i;
    ieee80211_resmgr_oc_sched_req_t req = NULL;
    struct ieee80211com         *ic = resmgr->ic;

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : OC_SCHED TEST\n", __func__);
    
    duration = 0;
    interval = 0;
    periodic = false;

    for (i=0; i < 2; i++) {
        if (i == 0) {
            duration = 10;
            interval = 50;
            periodic = true;
        }
        else if (i == 1) {
            duration = 0;
            interval = 0;
            periodic = true;
        }

        if (i < 1) {        
            req_handle[i] = ieee80211_resmgr_oc_sched_req_alloc(resmgr, &vap, duration, interval, 
                OFF_CHAN_SCHED_PRIO_HIGH, periodic, OC_SCHEDULER_VAP_REQTYPE);
            if (req_handle[i] != NULL) {
                req = req_handle[i];
                req->previous_pending_timeslot_tsf = prev_pending_timeslot_tsf;
                req->interval_usec = 0x19000;
            }
        }
        else {
            req_handle[i] = ieee80211_resmgr_oc_sched_req_alloc(resmgr, &vap, duration, interval, 
                OFF_CHAN_SCHED_PRIO_LOW, periodic, OC_SCHEDULER_VAP_REQTYPE);
        }
        
        if (req_handle[i] == NULL) {
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                                 "%s : oc_sched_req_alloc failed\n", __func__);
            return;
        }
    }

    for (i=0; i < 2; i++) {        
        retVal = ieee80211_resmgr_oc_sched_req_start(resmgr, req_handle[i]);
        if (retVal != EOK && retVal != EBUSY) {
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                                 "%s : oc_sched_req_start failed\n", __func__);
            return;
        }
    }
    
    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : OC_SCHED TEST COMPLETED\n", __func__);
    
    return;
}
#endif

/*
 * Invoke the scheduler module with the current native TSF value
 */
int
ieee80211_resmgr_oc_scheduler(
    ieee80211_resmgr_t resmgr,    
    ieee80211_resmgr_oc_sched_tsftimer_event_t event_type
    )
{
    u_int32_t current_tsf;
    ieee80211_resmgr_oc_scheduler_t sched;
    bool done;
    ieee80211_resmgr_oc_sched_req_t next_scheduled_request;
    int retval = EOK;
    struct ieee80211com         *ic = resmgr->ic;
    bool tsf_timer_changed = false;
    bool complete_one_shot = false;
    
    if (ieee80211_resmgr_oc_sched_multi_chan_enable == false) {    
        return(retval);
    }
    
    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);

    if (resmgr->oc_scheduler == NULL) {
        DBG_PRINTK("%s : oc_scheduler is NULL!\n", __func__);
        
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                             "%s : Off-channel scheduler does not exist!\n", __func__);
        ASSERT(0);
        return(EINVAL);
    }

    sched = resmgr->oc_scheduler;
    
    sched->stats.scheduler_run_count += 1;
    switch (event_type) {
    case OFF_CHAN_SCHED_TSF_TIMER_EXPIRE:
        sched->stats.tsf_timer_expire_count += 1;
        break;        
    case OFF_CHAN_SCHED_TSF_TIMER_CHANGE:
        sched->stats.tsf_time_changed_count += 1;
        break;
    case OFF_CHAN_SCHED_START_REQUEST:
        sched->stats.request_start_count += 1;
        break;
    case OFF_CHAN_SCHED_STOP_REQUEST:
        sched->stats.request_stop_count += 1;
        break;
    case OFF_CHAN_SCHED_VAP_TRANSITION:
        sched->stats.vap_transition_count += 1;
        break;
    case OFF_CHAN_SCHED_HALT_SCHEDULER:
        sched->stats.halt_count += 1;
        break;
    default:
        break;
    }
    
    if (sched->run_state == OC_SCHED_STATE_INIT) {
        /* Filter out any TSF Timer Change events before Scheduler is running */
        if (event_type == OFF_CHAN_SCHED_TSF_TIMER_CHANGE) {
            DBG_PRINTK("%s : Filter TSF Timer Change event!\n", __func__);
            return(retval);
        }
    }
    else {
        if (event_type == OFF_CHAN_SCHED_TSF_TIMER_CHANGE) {
            tsf_timer_changed = true;        
            sched->transition_run_q = true;
            sched->noa_update_cache.cache_valid = false;
        }
        else if (event_type == OFF_CHAN_SCHED_TSF_TIMER_EXPIRE) {
            complete_one_shot = true;
        }
    }
    
    /*
     * Get current TSF clock value
     */
    current_tsf = ic->ic_get_TSF32(ic);    
    if (sched->current_tsf == current_tsf) {
        DBG_PRINTK("%s : Current TSF hasn't advanced!\n", __func__);
        return(retval);
    }

    sched->previous_tsf = sched->current_tsf;
    sched->current_tsf = current_tsf;

    DBG_PRINTK("%s : current_tsf = 0x%08x\n", __func__, current_tsf);
    DBG_PRINTK("%s : previous_tsf = 0x%08x\n", __func__, sched->previous_tsf);

    ieee80211_rm_ocs_verify_schedule_window(sched);

    /*
     * If there is a current active request and it is
     * a one-shot or non-periodic type, then the request
     * has been completed.
     */
    if (sched->current_active_req != NULL) {
        if (tsf_timer_changed == false && sched->current_active_req->periodic == false) {
            if (sched->current_active_req->state == ACTIVE ||
                sched->current_active_req->state == RESET) {
                DBG_PRINTK("%s : 1 Shot Completed [Req ID: %d]\n", __func__, sched->current_active_req->req_id);
                sched->current_active_req->state = COMPLETED;
            }
            else {
                sched->current_active_req->schedule_asap = true;
                 DBG_PRINTK("%s : 1 Shot Stopped [Req ID: %d]\n", __func__, sched->current_active_req->req_id);                
            }
        }
        
        sched->current_active_req = NULL;
    }

    /*
     * A VAP has transitioned state and thus the schedule window needs to be 
     * recomputed.
     */
    switch (event_type) {
    case OFF_CHAN_SCHED_VAP_TRANSITION:
    case OFF_CHAN_SCHED_START_REQUEST:
    case OFF_CHAN_SCHED_STOP_REQUEST:
        sched->transition_run_q = true;
        break;
    default:
        break;
    }
    
    next_scheduled_request = NULL;
    done = false;
    while (! done) {
        
        switch (sched->run_state) {
            
        case OC_SCHED_STATE_RESTART:
        case OC_SCHED_STATE_INIT:
            DBG_PRINTK("%s : run_state = OC_SCHED_STATE_INIT\n", __func__);
            
            sched->next_tsf_sched_ev = sched->current_tsf;
            sched->current_active_req = NULL;
            
            retval = ieee80211_rm_ocs_calc_schedule(sched, sched->current_run_q);
            if (retval != EOK) {
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                                     "%s : Off-Channel scheduler failed to compute schedule\n", __func__);
                ASSERT(0);
            }

            retval = ieee80211_rm_ocs_set_next_tsftimer(sched, &next_scheduled_request);
            if (retval == EOK) {
                if (next_scheduled_request != NULL) {
                    DBG_PRINTK("%s : <<< REQUEST %d ACTIVE >>>\n", __func__, next_scheduled_request->req_id); 
                    sched->current_active_req = next_scheduled_request;
                }
                
                sched->run_state = OC_SCHED_STATE_RUN;
            }
            
            done = true;
            break;
            
        case OC_SCHED_STATE_RUN:

            DBG_PRINTK("%s : run_state = OC_SCHED_STATE_RUN\n", __func__);
            if (sched->transition_run_q == true) {
                sched->current_run_q = sched->current_run_q ^ 1;
                sched->next_tsf_sched_ev = sched->current_tsf;
                sched->current_active_req = NULL;
                                            
                DBG_PRINTK("%s : Current Run Queue = %d\n", __func__, sched->current_run_q);
                DBG_PRINTK("%s : NEXT TSF SCHED EVENT = 0x%08x\n", __func__, sched->next_tsf_sched_ev);
                 retval = ieee80211_rm_ocs_calc_schedule(sched, sched->current_run_q);
                if (retval != EOK) {
                    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                                         "%s : Off-Channel scheduler failed to compute schedule\n", __func__);
                    ASSERT(0);
                }
                else {
                    DBG_PRINTK("%s : TRANSITION Run Queue Completed!!\n", __func__);
                    sched->transition_run_q = false;
                }
            }
            
            retval = ieee80211_rm_ocs_set_next_tsftimer(sched, &next_scheduled_request);
            if (retval == EOK) {
                if (next_scheduled_request != NULL) {
                    DBG_PRINTK("%s : <<< REQUEST %d ACTIVE >>>\n", __func__, next_scheduled_request->req_id); 
                    sched->current_active_req = next_scheduled_request;
                }
                
                sched->run_state = OC_SCHED_STATE_RUN;
            }
            
            done = true;
            break;
            
        case OC_SCHED_STATE_HALT:
        {
            ieee80211_rm_oc_sched_run_q_t run_q;
            int i;
            
            DBG_PRINTK("%s : run_state = OC_SCHED_STATE_HALT\n", __func__);

            IEEE80211_RESMGR_OCSCHE_LOCK(sched);

            /* Cleanup internal data structures */
            for (i=0; i < MAX_RUN_QUEUES; i++) {
                run_q = &sched->run_queues[i];
                
                /*
                 * Cleanup previous run queue timeslots
                 */
                ieee80211_rm_ocs_cleanup_timeslot_list(sched, 
                            ((oc_sched_timeslot_head *) &(run_q->run_q_head)));
                
                /*
                 * Cleanup any schedule policy entries
                 */
                ieee80211_rm_ocs_cleanup_sched_policy_list(sched, run_q);
            }

            /* Stop all the active requests */
            ieee80211_rm_ocs_cleanup_request_lists(sched);
            
            ieee80211_rm_ocs_cleanup_noa_schedules(sched);
            ieee80211_rm_ocs_clear_noa_schedules(sched);
            sched->current_active_req = NULL;
            sched->current_run_q = 0;

            IEEE80211_RESMGR_OCSCHE_UNLOCK(sched);
            
            done = true;
            break;
        }    
        default:
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                                 "%s : Off-channel scheduler invalid run_state\n", __func__);
            ASSERT(0);
            done = true;
            break;
        }
    }
    
    return(retval);
}


/*
 * Get the next scheduled timeslot or event
 */

void
ieee80211_resmgr_oc_sched_get_scheduled_operation(
    ieee80211_resmgr_t resmgr,
    ieee80211_resmgr_oc_scheduler_operation_data_t *op_data
    )
{
    ieee80211_resmgr_oc_scheduler_t sched;
    struct ieee80211com         *ic = resmgr->ic;
    ieee80211_resmgr_oc_sched_req_t req = NULL;

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);

    sched = resmgr->oc_scheduler;

    if ((sched == NULL) || (op_data == NULL) || (sched->run_state == OC_SCHED_STATE_HALT)) {
        return;
    }

    op_data->operation_type = OC_SCHED_OP_NONE;
    op_data->vap = NULL;
        
    if (resmgr->oc_scheduler == NULL) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                             "%s : Off-channel scheduler does not exist!\n", __func__);
        return;
    }

    if (sched->current_active_req != NULL) {
        req = sched->current_active_req;

        switch (req->req_type) {
        case OC_SCHEDULER_VAP_REQTYPE:
            op_data->operation_type = OC_SCHED_OP_VAP;
            op_data->vap = req->vap;
            break;
        case OC_SCHEDULER_SCAN_REQTYPE:
            op_data->operation_type = OC_SCHED_OP_SCAN;
            break;
        case OC_SCHEDULER_INVALID_REQTYPE:
        case OC_SCHEDULER_NONE_REQTYPE: 
        default:
            break;
        }
    }
    
    return;
}

/*
 * Create an instance of the Off-Channel Scheduler module
 */
int 
ieee80211_resmgr_oc_scheduler_create(
    ieee80211_resmgr_t resmgr, 
    ieee80211_resmgr_oc_sched_policy_t policy
    )
{
    ieee80211_resmgr_oc_scheduler_t sched;
    int i;
    int total_size;
    struct ieee80211_rm_oc_sched_timeslot *timeslot;
    struct ieee80211_rm_oc_sched_policy_entry *sched_policy_entry;
    int total_entries;
    int retval = EOK;
    struct ieee80211com         *ic = resmgr->ic;

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);
    
    sched = (ieee80211_resmgr_oc_scheduler_t) OS_MALLOC(ic->ic_osdev,
        sizeof(struct ieee80211_resmgr_oc_scheduler),0);
    if (sched == NULL) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                             "%s : [1] ieee80211_resmgr_oc_scheduler_create failed\n", __func__);
        return(ENOMEM);
    }

    DBG_PRINTK("%s : Allocated sched\n", __func__);
    
    /*
     * Initialize the scheduler control block
     */
    OS_MEMZERO(sched, sizeof(struct ieee80211_resmgr_oc_scheduler));
    resmgr->oc_scheduler = sched;
    
    sched->resmgr = resmgr;
    sched->policy = policy;
    
    TAILQ_INIT(&(sched->high_prio_list));
    TAILQ_INIT(&(sched->low_prio_list));
    TAILQ_INIT(&(sched->timeslot_free_q));
    TAILQ_INIT(&(sched->noa_event_vap_list));
    
    sched->current_req_id = INITIAL_REQUEST_ID;
    sched->run_state = OC_SCHED_STATE_INIT;
    sched->scheduler_state = OC_SCHEDULER_OFF;
    sched->device_pause_delay_usec = ic->ic_channelswitchingtimeusec + OC_SCHED_NULL_ACK_DELAY;
    sched->swba_delay_before_tbtt_usec = SWBA_INTR_DELAY_BEFORE_TBTT_USEC;
    sched->noa_event_scheduling_delay = NOA_EVENT_SCHEDULING_DELAY_FOR_NOTIFICATION;
    
    /*
     * Allocate and setup a TSF timer data structure
     */
    sched->tsftimer_hdl = ieee80211_tsftimer_alloc(ic->ic_tsf_timer, 0, 
        ieee80211_rm_ocs_tsf_timeout, 0, (void *) sched, ieee80211_rm_ocs_tsf_changed);
    if (sched->tsftimer_hdl == NULL) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                             "%s : [1] ieee80211_resmgr_oc_scheduler_create failed\n", __func__);
        retval = ENOMEM;
        goto err1;
    }
    
    DBG_PRINTK("%s : tsftimer alloc successful\n", __func__);
    
    IEEE80211_RESMGR_OCSCHE_LOCK_INIT(sched);

    /*
     * Initialize each of the calendar queues
     */
    for (i=0; i < MAX_RUN_QUEUES; i++) {
        sched->run_queues[i].starting_tsf = 0;
        sched->run_queues[i].ending_tsf = 0;
        sched->run_queues[i].curr_ts = NULL;
        TAILQ_INIT(&(sched->run_queues[i].run_q_head));
        TAILQ_INIT(&(sched->run_queues[i].sched_policy_entry_head));       
    }

    /*
     * Allocating 512 timeslot entries per run queue. Hopefully, 
     * this number will be sufficient for most scenarios.
     */
    total_entries = MAX_RUN_QUEUES * MAX_TIMESLOT_ENTRIES_PER_RUNQ;
    total_size = sizeof(struct ieee80211_rm_oc_sched_timeslot) * total_entries;

    sched->timeslot_base = (ieee80211_rm_oc_sched_timeslot_t) OS_MALLOC(ic->ic_osdev, total_size, 0);
    if (sched->timeslot_base == NULL) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                             "%s : [2] ieee80211_resmgr_oc_scheduler_create failed\n", __func__);
        retval = ENOMEM;
        goto err2;
    }

    DBG_PRINTK("%s : allocated timeslot_base\n", __func__);
    
    timeslot = (struct ieee80211_rm_oc_sched_timeslot *) sched->timeslot_base;
    for (i=0; i < total_entries; i++) {
        OS_MEMZERO(timeslot, sizeof(struct ieee80211_rm_oc_sched_timeslot));
        TAILQ_INSERT_HEAD(&(sched->timeslot_free_q), timeslot, ts_te);
        timeslot = (struct ieee80211_rm_oc_sched_timeslot *) (((u_int8_t *) timeslot) + 
            sizeof(struct ieee80211_rm_oc_sched_timeslot));
    }

    
    DBG_PRINTK("%s : TIMESLOT_FREE_Q\n", __func__);
    ieee80211_rm_ocs_count_timeslots(sched, ((oc_sched_timeslot_head *) &(sched->timeslot_free_q)));
    
    /*
     * Allocating 64 schedule policy entries per run queue
     */
    total_entries = MAX_RUN_QUEUES * MAX_SCHED_POLICY_ENTRIES_PER_RUNQ;
    total_size = sizeof(struct ieee80211_rm_oc_sched_policy_entry) * total_entries;
    
    sched->sched_policy_entry_base = (ieee80211_rm_oc_sched_policy_entry_t) OS_MALLOC(ic->ic_osdev, total_size, 0);
    if (sched->sched_policy_entry_base == NULL) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                             "%s : [3] ieee80211_resmgr_oc_scheduler_create failed\n", __func__);
        retval = ENOMEM;
        goto err3;
    }
   
   DBG_PRINTK("%s : allocatead sched_policy_entry_base\n", __func__);
   
    sched_policy_entry = (struct ieee80211_rm_oc_sched_policy_entry *) sched->sched_policy_entry_base;
    for (i=0; i < total_entries; i++) {
        OS_MEMZERO(sched_policy_entry, sizeof(struct ieee80211_rm_oc_sched_policy_entry));
        TAILQ_INIT(&(sched_policy_entry->collision_q_head));
        TAILQ_INSERT_HEAD(&(sched->sched_policy_entry_free_q), sched_policy_entry, pe_te);
        sched_policy_entry = (struct ieee80211_rm_oc_sched_policy_entry *) (((u_int8_t *) sched_policy_entry) + 
            sizeof(struct ieee80211_rm_oc_sched_policy_entry));
    }

    /* Allocate a scheduler request on behalf of the scanner */
    resmgr->scandata.oc_sched_req = ieee80211_resmgr_oc_sched_req_alloc(resmgr, NULL, 10, 0, 
                        OFF_CHAN_SCHED_PRIO_LOW, false, OC_SCHEDULER_SCAN_REQTYPE);
    if (resmgr->scandata.oc_sched_req == NULL) {
        retval = ENOMEM;
        goto err4;
    }
    else{
        resmgr->scandata.oc_sched_req->schedule_asap = true;
    }
    
    return(retval);

err4:
    OS_FREE(sched->sched_policy_entry_base);
    
err3:
    OS_FREE(sched->timeslot_base);

err2:
    IEEE80211_RESMGR_OCSCHE_LOCK_DESTROY(sched);
    retval = ieee80211_tsftimer_free(sched->tsftimer_hdl, true);
    if (retval != EOK) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                             "%s : ieee80211_tsftimer_free failed\n", __func__);
    }
    
err1:    
    resmgr->oc_scheduler = NULL;
    OS_FREE(sched);
    
    return(retval);
}

/*
 * Delete an instance of an Off-Channel Scheduler module
 */
void 
ieee80211_resmgr_oc_scheduler_delete(
    ieee80211_resmgr_t resmgr
    )
{
    ieee80211_resmgr_oc_scheduler_t sched;
    int retval = EOK;
    struct ieee80211com         *ic = resmgr->ic;
    ieee80211_resmgr_oc_sched_noa_event_vap_t cur_nev, temp_nev;

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);
    
    if (resmgr->oc_scheduler == NULL) {
        return;
    }

    sched = resmgr->oc_scheduler;
    ASSERT(TAILQ_EMPTY(&(sched->high_prio_list)));
    ASSERT(TAILQ_EMPTY(&(sched->low_prio_list)));

    if (! TAILQ_EMPTY(&(sched->noa_event_vap_list))) {
        TAILQ_FOREACH_SAFE(cur_nev, &(sched->noa_event_vap_list), nev_te, temp_nev) {
            TAILQ_REMOVE(&(sched->noa_event_vap_list), cur_nev, nev_te);
            OS_FREE(cur_nev);
        }
    }
    
    if (sched->timeslot_base != NULL) {
        OS_FREE(sched->timeslot_base);
    }

    if (sched->sched_policy_entry_base != NULL) {
        OS_FREE(sched->sched_policy_entry_base);
    }
    
    retval = ieee80211_tsftimer_free(sched->tsftimer_hdl, true);
    if (retval != EOK) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                             "%s : ieee80211_tsftimer_free failed\n", __func__);
    }

    IEEE80211_RESMGR_OCSCHE_LOCK_DESTROY(sched);

    OS_FREE(sched);
    resmgr->oc_scheduler = NULL;

    return;
}

/*
 *  Allocate a schedule request for submission to the Resource Manager scheduler. Upon success,
 *  a handle for the request is returned. Otherwise, NULL is returned to indicate a failure
 *  to allocate the request.
 */
ieee80211_resmgr_oc_sched_req_handle_t 
ieee80211_resmgr_oc_sched_req_alloc(
    ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap,
    u_int32_t duration_msec, 
    u_int32_t interval_msec,
    ieee80211_resmgr_oc_sched_prio_t priority, 
    bool periodic,
    ieee80211_resmgr_oc_sched_reqtype_t req_type
    )
{
    ieee80211_resmgr_oc_sched_req_t req;
    struct ieee80211com         *ic = resmgr->ic;
    ieee80211_resmgr_oc_scheduler_t sched = resmgr->oc_scheduler;

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);
    
    if (sched == NULL) {
        return NULL;
    }

    req = (struct ieee80211_resmgr_oc_sched_req *) OS_MALLOC(ic->ic_osdev,
        sizeof(struct ieee80211_resmgr_oc_sched_req),0);
    if (req == NULL) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                             "%s : Failed memory allocation\n", __func__);
        return(req);
    }

    OS_MEMZERO(req, sizeof(struct ieee80211_resmgr_oc_sched_req));

    DBG_PRINTK("%s : Req alloc'd\n", __func__);
    
    IEEE80211_RESMGR_OCSCHE_LOCK(sched);
    
    req->req_id = sched->current_req_id;
    sched->current_req_id += 1;
    
    IEEE80211_RESMGR_OCSCHE_UNLOCK(sched);
    
    req->vap = vap;
    req->duration_usec = duration_msec * 1000;
    req->interval_usec = interval_msec * 1000;
    req->priority = priority;
    req->periodic = periodic;
    req->schedule_asap = false;
    req->state = INACTIVE;
    req->req_type = req_type;

    return(req);
}

/*
 *  Start or initiate the scheduling of air time for this schedule request handle.
 */
int 
ieee80211_resmgr_oc_sched_req_start(
    ieee80211_resmgr_t resmgr,
    ieee80211_resmgr_oc_sched_req_handle_t req_handle
    )
{
    ieee80211_resmgr_oc_sched_req_t req;
    ieee80211_resmgr_oc_scheduler_t sched;
    struct ieee80211com         *ic = resmgr->ic;

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);
    
    sched = resmgr->oc_scheduler;
    if (sched == NULL) {
        return(EINVAL);
    }

    req = (ieee80211_resmgr_oc_sched_req_t) req_handle;
    if (req == NULL) {
        return(EINVAL);
    }

    if (req->periodic == false) {
        sched->stats.one_shot_start_count += 1;
    }

    IEEE80211_RESMGR_OCSCHE_LOCK(sched);
    DBG_PRINTK("%s : Req[%d] = %pK req->vap = %pK started!\n", __func__, req->req_id, req, req->vap);
    

    switch (req->state) {
    case INACTIVE:
    case STOPPED:
    default:    
        break;
        
    case COMPLETED:
        req->state = RESET;
    case ACTIVE:
    case RESET:    
        IEEE80211_RESMGR_OCSCHE_UNLOCK(sched);
        return(EOK);
    }

    if (req->priority == OFF_CHAN_SCHED_PRIO_HIGH) {
        TAILQ_INSERT_TAIL(&(sched->high_prio_list), req, req_te);
    }
    else {
        TAILQ_INSERT_TAIL(&(sched->low_prio_list), req, req_te);
    }

    req->state = RESET;
    
    IEEE80211_RESMGR_OCSCHE_UNLOCK(sched);

    DBG_PRINTK("%s : Req started!\n", __func__);
    
    return(EOK);
}

/*
 *  Stop or cease the scheduling of air time for this schedule request handle.
 */
int 
ieee80211_resmgr_oc_sched_req_stop(
    ieee80211_resmgr_t resmgr,
    ieee80211_resmgr_oc_sched_req_handle_t req_handle
    )
{
    ieee80211_resmgr_oc_sched_req_t req;
    ieee80211_resmgr_oc_scheduler_t sched;
    struct ieee80211com         *ic = resmgr->ic;
    ieee80211_resmgr_oc_sched_req_t cur_req, tmp_req;

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);

    sched = resmgr->oc_scheduler;
    if (sched == NULL) {
        return(EINVAL);
    }

    req = (ieee80211_resmgr_oc_sched_req_t) req_handle;
    if (req == NULL) {
        return(EINVAL);
    }

    IEEE80211_RESMGR_OCSCHE_LOCK(sched);
    DBG_PRINTK("%s : Req[%d] = %pK req->vap = %pK stopped!\n", __func__, req->req_id, req, req->vap);

    switch (req->state) {
    case INACTIVE:
    case STOPPED:
    default:    
        IEEE80211_RESMGR_OCSCHE_UNLOCK(sched);
        return(EOK);
        
    case ACTIVE:
    case COMPLETED:
    case RESET:    
        break;
    }
    
    /* Remove from prio_q tailq */
    if (req->priority == OFF_CHAN_SCHED_PRIO_HIGH) {
        TAILQ_FOREACH_SAFE(cur_req, &(sched->high_prio_list), 
                    req_te, tmp_req) {
            if (cur_req == req) {
                    TAILQ_REMOVE(&(sched->high_prio_list), req, req_te);
                break;
            }
        }
    }
    else { 
        TAILQ_FOREACH_SAFE(cur_req, &(sched->low_prio_list), 
                    req_te, tmp_req) {
            if (cur_req == req) {
                    TAILQ_REMOVE(&(sched->low_prio_list), req, req_te);
                break;
            }
        }
    }
    
    req->state = STOPPED;
    
    IEEE80211_RESMGR_OCSCHE_UNLOCK(sched);

    
    return(EOK);
}

/*
 * Free a previously allocated schedule request handle.
 */
void 
ieee80211_resmgr_oc_sched_req_free(
    ieee80211_resmgr_t resmgr,
    ieee80211_resmgr_oc_sched_req_handle_t req_handle
    )
{
    ieee80211_resmgr_oc_sched_req_t req;
    struct ieee80211com         *ic = resmgr->ic;

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);
    
    req = (ieee80211_resmgr_oc_sched_req_t) req_handle;
    if (req == NULL) {
        return;
    }

    switch (req->state) {
    case INACTIVE:
    case STOPPED:
    case INVALID:   
        break;
    default:
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                             "%s : Off-Channel Scheduler request in wrong state!\n", __func__);
        ASSERT(0);
        break;
    }

    OS_FREE(req);

    DBG_PRINTK("%s : Req freed!\n", __func__);
    
    return;
}

/*
 * callback function to receive mlme events.
 */
void
ieee80211_rm_ocs_mlme_event_handler(ieee80211_vap_t vap, ieee80211_mlme_event *event, void *arg)
{
   ieee80211_resmgr_vap_priv_t resmgr_vap = (ieee80211_resmgr_vap_priv_t) arg;
   ieee80211_resmgr_oc_sched_req_t req;
   ieee80211_resmgr_t resmgr;
   ieee80211_resmgr_oc_scheduler_t sched;

    ASSERT(arg);

    resmgr = resmgr_vap->resmgr;
    sched = resmgr->oc_scheduler;
    
    /* Start the One-Shot scheduling request */
    req = resmgr_vap->sched_data.req_array[OC_SCHED_MLME_1SHOT];

    switch (event->type) {
    case IEEE80211_MLME_EVENT_BEACON_MISS:         
        /* 
         * beacon miss detected, for some reason the STA vap is not receiving beacons 
         * and if continues VAP looses its connection. allocate a big chunk of continuous time.
         */                
        ieee80211_resmgr_off_chan_sched_req_start(resmgr, req);

        sched->stats.mlme_bmiss_count += 1;
        
        DBG_PRINTK("%s : MLME_EVENT_BEACON_MISS Req_ID=%d\n", __func__, req->req_id);
        break;
    case IEEE80211_MLME_EVENT_BEACON_MISS_CLEAR:   
        /* beacon miss was cleared */
        ieee80211_resmgr_off_chan_sched_req_stop(resmgr, req);

        sched->stats.mlme_bmiss_clear_count += 1;
        
        DBG_PRINTK("%s : MLME_EVENT_BEACON_MISS_CLEAR\n", __func__);
        break;
    default:
        break;
    }
}

void
ieee80211_rm_ocs_cleanup_request_lists(
    ieee80211_resmgr_oc_scheduler_t sched
    )
{
    ieee80211_resmgr_oc_sched_req_t req;
    struct ieee80211com         *ic = sched->resmgr->ic;

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);
    
    while ( ! TAILQ_EMPTY(&(sched->high_prio_list))) {
        req = TAILQ_FIRST(&(sched->high_prio_list));
        req->state = STOPPED;
        TAILQ_REMOVE(&(sched->high_prio_list),req,req_te);
    }

    while ( ! TAILQ_EMPTY(&(sched->low_prio_list))) {
        req = TAILQ_FIRST(&(sched->low_prio_list));
        req->state = STOPPED;
        TAILQ_REMOVE(&(sched->low_prio_list),req,req_te);
    }

    sched->resmgr->scandata.oc_sched_req->state = STOPPED;
    
    return;
}

/*
 * Get next tsf timer event
 */
int
ieee80211_rm_ocs_get_next_tsftimer(
    ieee80211_resmgr_oc_scheduler_t sched,
    u_int32_t *next_tsf,    
    struct ieee80211_resmgr_oc_sched_req **sched_req
    )
{
    ieee80211_rm_oc_sched_timeslot_t cur_ts;
    u_int32_t current_tsf;
    ieee80211_rm_oc_sched_run_q_t run_q;
    bool found;
    int retval = EOK;
    struct ieee80211com *ic;

    if (sched == NULL || next_tsf == NULL || sched_req == NULL) {
        printk("%s : Off-channel scheduler does not exist!\n", __func__);
        return(EINVAL);
    }

    ic = sched->resmgr->ic;
    IEEE80211_DPRINTF_IC(
        ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
        "%s : entry\n", __func__);
    
    run_q = &sched->run_queues[sched->current_run_q];
    current_tsf = sched->current_tsf;
    found = false;
    *sched_req = NULL;
    
    TAILQ_FOREACH(cur_ts, &(run_q->run_q_head), ts_te) {
        if (TSFTIME_IS_SMALLER(current_tsf, cur_ts->start_tsf)) {
            *next_tsf = cur_ts->start_tsf;
            DBG_PRINTK("%s : START_TSF [0x%08x]\n", __func__, cur_ts->start_tsf);
            found = true;
            break;
        }
        else if (TSFTIME_IS_GREATER_EQ(current_tsf, cur_ts->start_tsf)) {
            if (TSFTIME_IS_SMALLER(current_tsf, cur_ts->stop_tsf)) {
                *next_tsf = cur_ts->stop_tsf;
                *sched_req = cur_ts->req;
                
                DBG_PRINTK("%s : STOP_TSF [0x%08x]\n", __func__, cur_ts->stop_tsf);
                found = true;
                break;
            }
        }
    }

    if (found == false) {
        if (TSFTIME_IS_SMALLER(current_tsf, run_q->ending_tsf)) {
            *next_tsf = run_q->ending_tsf;
            DBG_PRINTK("%s : using ENDING_TSF [0x%08x]\n", __func__, run_q->ending_tsf);
            
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                                 "[OC_Sched]  using Ending_tsf = 0x%08x\n", run_q->ending_tsf);         
        }

        /*
         * The end of the scheduling window has been reached.
         * It is time to transition to a new scheduling window.
         */
        DBG_PRINTK("%s : TRANSITION Scheduler Run Queue!!\n", __func__);
        sched->transition_run_q = true;
    }
    else {
        
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_STATE,
                             "[OC_Sched] ReqID[%d] - Start_tsf = 0x%08x Stop_tsf = 0x%08x\n", 
                             cur_ts->req->req_id, cur_ts->start_tsf, cur_ts->stop_tsf);
        
        DBG_PRINTK("%s : OCS Servicing REQUEST ID[%d]\n", __func__, cur_ts->req->req_id);
        
        if (TAILQ_NEXT(cur_ts, ts_te) == NULL) {
            DBG_PRINTK("%s : Encountered last timeslot in window!!\n", __func__);
            
            /* 
             * The last timeslot in the scheduling window has been reached.
             * It is time to transition to a new scheduling window.
             */
            DBG_PRINTK("%s : TRANSITION Scheduler Run Queue!!\n", __func__);
            sched->transition_run_q = true;
        }
    }
    
    return(retval);
}

/*
 * Verify that the current TSF clock is within the 
 * current scheduled window of events.
 */
void
ieee80211_rm_ocs_verify_schedule_window(
    ieee80211_resmgr_oc_scheduler_t sched   
    )
{
    ieee80211_rm_oc_sched_run_q_t run_q;
    bool need_transition = true;

    if (sched->run_state != OC_SCHED_STATE_RUN || sched->transition_run_q == true) {
        return;
    }
    
    run_q = &sched->run_queues[sched->current_run_q];

    if (TSFTIME_IS_GREATER_EQ(sched->current_tsf, run_q->starting_tsf)) {
        if (TSFTIME_IS_SMALLER_EQ(sched->current_tsf,run_q->ending_tsf)) {
            need_transition = false;
        }
    }

    if (need_transition == true) {
        DBG_PRINTK("%s : WARNING! Current_tsf outside of scheduled window!\n", __func__);                
        sched->transition_run_q = need_transition;
    }
    
    return;
}

/*
 * Set next tsf timer value
 */
int
ieee80211_rm_ocs_set_next_tsftimer(
    ieee80211_resmgr_oc_scheduler_t sched,    
    struct ieee80211_resmgr_oc_sched_req **sched_req
    )
{
    u_int32_t next_tsf;
    int retval = EOK;
    struct ieee80211com         *ic = sched->resmgr->ic;

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);
    
    if (TIMESLOTS_PENDING(sched, sched->current_run_q)) {
        retval = ieee80211_rm_ocs_get_next_tsftimer(sched, &next_tsf, sched_req);
        if (retval != EOK) {
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                                 "%s : get_next_tsftimer failed\n", __func__);
            ASSERT(0);
            retval = EINVAL;
        }
        else {
            /* Stop any pending TSF timer */            
            retval = ieee80211_tsftimer_stop(sched->tsftimer_hdl);
            if (retval != EOK) {
                DBG_PRINTK("%s : Failed to stop TSF timer!\n", __func__);
            }
            
            /* Start the TSF timer */
            retval = ieee80211_tsftimer_start(sched->tsftimer_hdl, next_tsf, 0);
            if (retval != EOK) {
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                                     "%s : tsftimer_start failed\n", __func__);
                ASSERT(0);
                retval = EINVAL;
            }
        }
    }
    else {
        /* Stop any pending TSF timer */            
        retval = ieee80211_tsftimer_stop(sched->tsftimer_hdl);
        if (retval != EOK) {
            DBG_PRINTK("%s : Failed to stop TSF timer!\n", __func__);
        }
    }

    return(retval);
}

/*
 * TSF timeout callback from the TSF Timer module
 */
void
ieee80211_rm_ocs_tsf_timeout(
    tsftimer_handle_t h_tsftime, 
    int arg1, 
    void *arg2, 
    u_int32_t curr_tsf
    )
{    
    struct ieee80211_resmgr_sm_event event;
    ieee80211_resmgr_oc_scheduler_t sched = (ieee80211_resmgr_oc_scheduler_t) arg2;

    if (sched == NULL) {
        ASSERT(0);
        return;
    }
    
    OS_MEMZERO(&event, sizeof(struct ieee80211_resmgr_sm_event));
    
    /* Post event to ResMgr SM */
    ieee80211_resmgr_sm_dispatch(sched->resmgr, IEEE80211_RESMGR_EVENT_TSF_TIMER_EXPIRED, &event);

    return;
}

/*
 * TSF changed callback from the TSF Timer module
 */
void
ieee80211_rm_ocs_tsf_changed(
    tsftimer_handle_t h_tsftime, 
    int arg1, 
    void *arg2, 
    u_int32_t curr_tsf
    )
{
    struct ieee80211_resmgr_sm_event event;
    ieee80211_resmgr_oc_scheduler_t sched = (ieee80211_resmgr_oc_scheduler_t) arg2;

    if (sched == NULL) {
        ASSERT(0);
        return;
    }

    OS_MEMZERO(&event, sizeof(struct ieee80211_resmgr_sm_event));
    
    /* Post event to ResMgr SM */
    ieee80211_resmgr_sm_dispatch(sched->resmgr, IEEE80211_RESMGR_EVENT_TSF_TIMER_CHANGED, &event);

    return;
}

/*
 * Allocate a scheduler timeslot element
 */
ieee80211_rm_oc_sched_timeslot_t 
ieee80211_rm_ocs_timeslot_alloc(
    ieee80211_resmgr_oc_scheduler_t sched
    )
{
    ieee80211_rm_oc_sched_timeslot_t ts = NULL;

    if (! TAILQ_EMPTY(&(sched->timeslot_free_q))) {
        ts = TAILQ_FIRST(&(sched->timeslot_free_q));
        TAILQ_REMOVE(&(sched->timeslot_free_q), ts, ts_te);
        
        sched->timeslots_inuse_count += 1;
        sched->max_timeslots_inuse_count = MAX(sched->timeslots_inuse_count,
                    sched->max_timeslots_inuse_count);
    }

    return(ts);
}

/*
 * Release or free a previously allocated scheduler timeslot element
 */
void 
ieee80211_rm_ocs_timeslot_free(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_timeslot_t ts
    )
{
    OS_MEMZERO(ts, sizeof(struct ieee80211_rm_oc_sched_timeslot));
    TAILQ_INSERT_HEAD(&(sched->timeslot_free_q), ts, ts_te);
    
    sched->timeslots_inuse_count -= 1;
    
    return;
}

/*
 * Allocate a scheduler policy entry
 */
ieee80211_rm_oc_sched_policy_entry_t 
ieee80211_rm_ocs_policy_entry_alloc(
    ieee80211_resmgr_oc_scheduler_t sched
    )
{
    ieee80211_rm_oc_sched_policy_entry_t pe = NULL;

    if (! TAILQ_EMPTY(&(sched->sched_policy_entry_free_q))) {
        pe = TAILQ_FIRST(&(sched->sched_policy_entry_free_q));
        TAILQ_REMOVE(&(sched->sched_policy_entry_free_q), pe, pe_te);
    }
    
    return(pe);
}

/*
 * Release or free a previously allocated scheduler policy entry element
 */
void 
ieee80211_rm_ocs_policy_entry_free( 
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_policy_entry_t pe
    )
{
    OS_MEMZERO(pe, sizeof(struct ieee80211_rm_oc_sched_policy_entry));
    TAILQ_INSERT_HEAD(&(sched->sched_policy_entry_free_q), pe, pe_te);

    return;
}

/*
 * Add a schedule policy entry to the run queue
 */
ieee80211_rm_oc_sched_policy_entry_t 
ieee80211_rm_ocs_add_pe(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_run_q_t run_q,
    ieee80211_resmgr_oc_sched_req_t req
    )
{
    ieee80211_rm_oc_sched_policy_entry_t pe;
    struct ieee80211com         *ic = sched->resmgr->ic;

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);
    
    pe = ieee80211_rm_ocs_policy_entry_alloc(sched);
    if (pe != NULL) {
        pe->req = req;    
        TAILQ_INSERT_TAIL(&(run_q->sched_policy_entry_head), pe, pe_te);
    }
    else {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                             "%s : Failed to allocate a schedule policy entry\n", __func__);
        ASSERT(0);
    }

    return(pe);
}

/*
 * Find an existing schedule policy entry in the run queue
 */
ieee80211_rm_oc_sched_policy_entry_t 
ieee80211_rm_ocs_find_policy_entry(
    ieee80211_resmgr_oc_scheduler_t sched,    
    ieee80211_rm_oc_sched_run_q_t run_q,
    ieee80211_resmgr_oc_sched_req_t req
    )
{
    ieee80211_rm_oc_sched_policy_entry_t pe = NULL;
    struct ieee80211com         *ic = sched->resmgr->ic;

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);
    
    TAILQ_FOREACH(pe, &(run_q->sched_policy_entry_head), pe_te) {
        if (pe->req == req) {
            DBG_PRINTK("%s : Found policy entry\n", __func__);
            break;
        }
    }
    
    return(pe);
}

/*
 * Return the next scheduling policy entry
 */
ieee80211_rm_oc_sched_policy_entry_t 
ieee80211_rm_ocs_get_next_pe(
    ieee80211_resmgr_oc_scheduler_t sched,    
    ieee80211_rm_oc_sched_run_q_t run_q, 
    ieee80211_rm_oc_sched_policy_entry_t cur_pe
    )
{
    ieee80211_rm_oc_sched_policy_entry_t next_pe;    
    struct ieee80211com         *ic = sched->resmgr->ic;

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);
    
    next_pe = TAILQ_NEXT(cur_pe, pe_te);
    if (next_pe == NULL) {
        next_pe = TAILQ_FIRST(&(run_q->sched_policy_entry_head));
    }
    
    return(next_pe);
}

/*
 * Save the colliding timeslot on the policy entry collision list
 */
void 
ieee80211_rm_ocs_insert_collision_list(
    ieee80211_resmgr_oc_scheduler_t sched,    
    ieee80211_rm_oc_sched_policy_entry_t pe, 
    ieee80211_rm_oc_sched_timeslot_t new_ts
    )
{
    ieee80211_rm_oc_sched_timeslot_t cur_ts, prev_ts;
    bool inserted, collision;
    struct ieee80211com         *ic = sched->resmgr->ic;

    inserted = false;
    collision = false;
    prev_ts = NULL;    

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);
    
    TAILQ_FOREACH(cur_ts, &(pe->collision_q_head), ts_te) {
        if (TSFTIME_IS_SMALLER(new_ts->start_tsf,cur_ts->start_tsf)) {
            
            /* New timeslot start occurs before the current timeslot start */
            if (prev_ts) {
                
                if (TSFTIME_IS_GREATER_EQ(new_ts->start_tsf,prev_ts->stop_tsf)) {
                    
                    /* New timeslot start occurs after the previous timeslot stop */
                    if (TSFTIME_IS_SMALLER_EQ(new_ts->stop_tsf, cur_ts->start_tsf)) {
                        
                        /* insert before current timeslot */
                        inserted = true;
                        TAILQ_INSERT_BEFORE(cur_ts, new_ts, ts_te);
                    }
                    else {
                        
                        /* collision */
                        /* New timeslot stops after current timeslot starts */
                        collision = true;
                    }
                }
                else {
                    
                    /* collision */
                    /* New timeslot start occurs before previous timeslot has stopped */
                    collision = true;
                }
            }
            else {
                if (TSFTIME_IS_SMALLER_EQ(new_ts->stop_tsf, cur_ts->start_tsf)) {
                    
                    /* insert before current timeslot */
                    inserted = true;
                    TAILQ_INSERT_BEFORE(cur_ts, new_ts, ts_te);
                }
                else {
                    
                    /* collision */
                    /* New timeslot does not stop before the current timeslot needs to start */
                    collision = true;
                }
            }
        }
        else {
           if (TSFTIME_IS_SMALLER(new_ts->start_tsf, cur_ts->stop_tsf)) {
            
               /* Collision */
               /* New timeslot starts after current timeslot but it starts before the current timeslot stops */
               collision = true;
           }
        }

        if (inserted || collision) {
            break;
        }
        
        prev_ts = cur_ts;        
    }

    if (collision) {
        /* 
         * There shouldn't be any collisions within a single request. 
         * If so, then something is really wrong! 
         */
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                             "%s : Collision occurred within single request!\n", __func__);
        ASSERT(0);        
    }
    
    if (! inserted && ! collision) {
        /* Insert at tail */
        TAILQ_INSERT_TAIL(&(pe->collision_q_head), new_ts, ts_te);
    }
    
    return;
}

void 
ieee80211_rm_ocs_dump_timeslot(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_timeslot_t ts
    )
{
    struct ieee80211com         *ic = sched->resmgr->ic;

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);
    
    DBG_PRINTK("@@@@  Timeslot(%pK)\n", ts);
    DBG_PRINTK("@@@@  Start TSF  = 0x%08x\n", ts->start_tsf);
    DBG_PRINTK("@@@@  Stop TSF   = 0x%08x\n", ts->stop_tsf);
    DBG_PRINTK("@@@@  Request ID = %d\n", ts->req->req_id);

    return;    
}

/*
 * Selectively remove the appropriate timeslot from a policy entry collision list
 */
ieee80211_rm_oc_sched_timeslot_t 
ieee80211_rm_ocs_remove_collision_list(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_policy_entry_t pe, 
    u_int32_t prev_stop_tsf, 
    u_int32_t next_start_tsf
    )
{
    ieee80211_rm_oc_sched_timeslot_t cur_ts;
    ieee80211_rm_oc_sched_timeslot_t ts = NULL;
    struct ieee80211com         *ic = sched->resmgr->ic;

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);
    
    TAILQ_FOREACH(cur_ts, &(pe->collision_q_head), ts_te) {
        if (TSFTIME_IS_GREATER_EQ(cur_ts->start_tsf, prev_stop_tsf)) {
           if (TSFTIME_IS_SMALLER(cur_ts->start_tsf, next_start_tsf)) {
                /* Found a timeslot to fit the available time window */
                ts = cur_ts;
                
                DBG_PRINTK("%s : Found suitable timeslot on collision list\n", __func__);
                ieee80211_rm_ocs_dump_timeslot(sched, ts);
                break;
            }
        }
    }

    if (ts) {    
        TAILQ_REMOVE(&(pe->collision_q_head), ts, ts_te);
    }
    
    return(ts);
}

/*
 * Select between conflicting timeslots using a round robin algorithm
 */
ieee80211_rm_oc_sched_timeslot_t 
ieee80211_rm_ocs_round_robin_scheduler(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_run_q_t run_q,
    ieee80211_rm_oc_sched_timeslot_t new_ts,
    ieee80211_rm_oc_sched_timeslot_t conflict_ts
    )
{
    ieee80211_rm_oc_sched_timeslot_t ts;
    ieee80211_resmgr_oc_sched_req_t new_req;
    ieee80211_resmgr_oc_sched_req_t cnflt_req;
    int i;
    ieee80211_rm_oc_sched_policy_entry_t new_pe;    
    ieee80211_rm_oc_sched_policy_entry_t cnflt_pe;
    ieee80211_rm_oc_sched_policy_entry_t sched_pe, starting_sched_pe;
    u_int32_t prev_stop_tsf;
    u_int32_t next_start_tsf;
    bool done;
    ieee80211_rm_oc_sched_timeslot_t prev_ts, next_ts;
    int rc;        
    struct ieee80211com         *ic = sched->resmgr->ic;
                
    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);
    
    ts = conflict_ts;

    new_req = new_ts->req;
    cnflt_req = conflict_ts->req;

    if (! TAILQ_EMPTY(&(run_q->sched_policy_entry_head))) {

        DBG_PRINTK("%s[1] : new_req is ID %d\n", __func__, new_req->req_id);
        DBG_PRINTK("%s[1] : cnflt_req is ID %d\n", __func__, cnflt_req->req_id);
        DBG_PRINTK("%s[1] : next_sched_pe is ID %d\n", __func__, 
            run_q->next_scheduled_policy_entry->req->req_id);
                
        cnflt_pe = ieee80211_rm_ocs_find_policy_entry(sched, run_q, cnflt_req);
        if (cnflt_pe == NULL) {
            cnflt_pe = ieee80211_rm_ocs_add_pe(sched, run_q, cnflt_req);
        }

        new_pe = ieee80211_rm_ocs_find_policy_entry(sched, run_q, new_req);
        if (new_pe == NULL) {
            new_pe = ieee80211_rm_ocs_add_pe(sched, run_q, new_req);
        }

        sched_pe = run_q->next_scheduled_policy_entry;
        
        if (sched_pe == new_pe) {
            /* New PE wins the timeslot */
            
            DBG_PRINTK("%s : next_scheduled_policy_entry == new_pe\n", __func__);
            
            ts = new_ts;

            /* Insert scheduled timeslot into run queue */
            rc = ieee80211_rm_ocs_insert_high_prio_timeslot(sched, run_q, ts, conflict_ts, 
                        IEEE80211_OC_SCHED_KEEP_INSPT);
            if (rc != EOK) {
                ieee80211_rm_ocs_insert_collision_list(sched, sched_pe, ts);
            }

            ieee80211_rm_ocs_cleanup_collisions(sched, run_q, new_ts, conflict_ts);            
        }
        else if (sched_pe == cnflt_pe) {
            /* Conflicting PE wins the timeslot */
            
            DBG_PRINTK("%s : next_scheduled_policy_entry == cnflt_pe\n", __func__);

            /* Move new timeslot to PE collisioni list for future reference */ 
            ieee80211_rm_ocs_insert_collision_list(sched, new_pe, new_ts);
        }
        else {
            /* Next scheduled PE wins the timeslot */
            
            DBG_PRINTK("%s : Neither timeslot matches the next_scheduled_pe\n", __func__);
            
            /* Move new timeslot to PE collision list for future reference */
            ieee80211_rm_ocs_insert_collision_list(sched, new_pe, new_ts);

            /* Save off the preceding and succeeding timeslots of the conflicting timeslot */
            prev_ts = TAILQ_PREV(conflict_ts, timeslot_head_s, ts_te);
            next_ts = TAILQ_NEXT(conflict_ts, ts_te);

            if (prev_ts) {
                prev_stop_tsf = prev_ts->stop_tsf;
            }
            else {
                DBG_PRINTK("%s : No prev_ts; using starting_tsf\n", __func__);
                prev_stop_tsf = run_q->starting_tsf;
            }

            if (next_ts) {
                next_start_tsf = next_ts->start_tsf;
            }
            else {
                DBG_PRINTK("%s : No next_ts; using ending_tsf\n", __func__);
                next_start_tsf = run_q->ending_tsf;
            }
            
            /* Search for a timeslot that fits the open window using round robin function */        
            starting_sched_pe = sched_pe;            
            done = false;
            while (! done) {
                /* Remove scheduled timeslot from PE collision list */
                ts = ieee80211_rm_ocs_remove_collision_list(sched, sched_pe, prev_stop_tsf, next_start_tsf);
                if (ts != NULL) {
                    DBG_PRINTK("%s : Found timeslot on collision list[%d]\n", __func__, sched_pe->req->req_id);
                    done = true;
                    break;
                }
                
                sched_pe = ieee80211_rm_ocs_get_next_pe(sched, run_q, sched_pe);
                if (sched_pe == starting_sched_pe) {
                    break;
                }
            }

            if (done) {            
                /* Insert scheduled timeslot into run queue */
                rc = ieee80211_rm_ocs_insert_high_prio_timeslot(sched, run_q, ts, conflict_ts, 
                            IEEE80211_OC_SCHED_KEEP_INSPT);
                if (rc != EOK) {
                    ieee80211_rm_ocs_insert_collision_list(sched, sched_pe, ts);
                }
                else {
                    ieee80211_rm_ocs_cleanup_collisions(sched, run_q, ts, conflict_ts); 
                }
            }
            else {
                ts = conflict_ts;
            }
        }
        
        run_q->next_scheduled_policy_entry = ieee80211_rm_ocs_get_next_pe(sched, run_q, sched_pe);
    }
    else {
        /*
         * This is the first collision to be encountered
         */
        for (i=0; i <= 1; i++) {
            if (i == 0) {
                cnflt_pe = ieee80211_rm_ocs_add_pe(sched, run_q, cnflt_req);
                if (!cnflt_pe) {
                    DBG_PRINTK(
                        "%s : rm_ocs_add_pe call failed "
                        "in empty sched_policy queue case\n", __func__);
                }
            }
            else {
                new_pe = ieee80211_rm_ocs_add_pe(sched, run_q, new_req);
                run_q->next_scheduled_policy_entry = new_pe;
                ieee80211_rm_ocs_insert_collision_list(sched, new_pe, new_ts);
            }
        }
        
        DBG_PRINTK("%s[2] : new_req is ID %d\n", __func__, new_req->req_id);
        DBG_PRINTK("%s[2] : cnflt_req is ID %d\n", __func__, cnflt_req->req_id);
        DBG_PRINTK("%s[2] : next_sched_pe is ID %d\n", __func__, 
                             run_q->next_scheduled_policy_entry->req->req_id);
    }

    if (ieee80211_resmgr_oc_sched_debug_level > 0) {
        if (ts == conflict_ts) {
            DBG_PRINTK("Timeslot winner is ID: %d\n", cnflt_req->req_id);
            ieee80211_rm_ocs_dump_timeslot(sched, ts);
        }
        else if (ts == new_ts) {
            DBG_PRINTK("Timeslot winner is ID: %d\n", new_req->req_id);
            ieee80211_rm_ocs_dump_timeslot(sched, ts);
        }
    }

    if (ts == NULL) {
        DBG_PRINTK("%s : ERROR - timeslot ptr is NULL!\n", __func__);
        ASSERT(0);
    }
    return(ts);
}
    
/*
 * Select between conflicting timeslots using a Most Recently Used Throughput algorithm
 */
ieee80211_rm_oc_sched_timeslot_t 
ieee80211_rm_ocs_mru_thruput_scheduler(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_run_q_t run_q,
    ieee80211_rm_oc_sched_timeslot_t new_ts,
    ieee80211_rm_oc_sched_timeslot_t conflict_ts
    )
{
    ieee80211_rm_oc_sched_timeslot_t ts;
    ieee80211_resmgr_oc_sched_req_t new_req;
    ieee80211_resmgr_oc_sched_req_t cnflt_req;
    struct ieee80211com         *ic = sched->resmgr->ic;
    
    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);
        
    ts = conflict_ts;
    
    new_req = new_ts->req;
    cnflt_req = conflict_ts->req;

    return(ts);
}
        
/*
 * Resolve a timeslot collision between two scheduling requests
 */
int 
ieee80211_rm_ocs_resolve_collision(
        ieee80211_resmgr_oc_scheduler_t sched, 
        ieee80211_rm_oc_sched_run_q_t run_q,
        ieee80211_rm_oc_sched_timeslot_t new_ts,
        ieee80211_rm_oc_sched_timeslot_t conflict_ts,
        u_int32_t *new_tsf
        )
{
    ieee80211_resmgr_oc_sched_req_t new_req;
    ieee80211_resmgr_oc_sched_req_t conflict_req;
    int rc;
    ieee80211_rm_oc_sched_timeslot_t resolved_ts;
    struct ieee80211com         *ic = sched->resmgr->ic;
    bool collision_resolved = false;

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);
            
    resolved_ts = conflict_ts;
    
    new_req = new_ts->req;
    conflict_req = conflict_ts->req;

    if (new_req->periodic == false || conflict_req->periodic == false) {
        if (new_req->periodic == false) {
            if (conflict_req->periodic == true) {
                /* New timeslot has higher priority */
                resolved_ts = new_ts;
                
                rc = ieee80211_rm_ocs_insert_high_prio_timeslot(sched, run_q, new_ts, conflict_ts, 
                            IEEE80211_OC_SCHED_REMOVE_INSPT);
                if (rc == EOK) {
                    collision_resolved = true;
                }
            }
            else if (new_req->req_type == OC_SCHEDULER_VAP_REQTYPE && 
                conflict_req->req_type == OC_SCHEDULER_SCAN_REQTYPE){
                /* New timeslot has higher priority */
                resolved_ts = new_ts;
                
                rc = ieee80211_rm_ocs_insert_high_prio_timeslot(sched, run_q, new_ts, conflict_ts, 
                            IEEE80211_OC_SCHED_REMOVE_INSPT);
                if (rc == EOK) {
                    collision_resolved = true;
                }                
            }
        }
    }
    else {    
        if (new_req->priority > conflict_req->priority) {            
            /* New timeslot has higher priority */
            resolved_ts = new_ts;
            
            rc = ieee80211_rm_ocs_insert_high_prio_timeslot(sched, run_q, new_ts, conflict_ts, 
                        IEEE80211_OC_SCHED_REMOVE_INSPT);
            if (rc == EOK) {
                collision_resolved = true;
            }
        }
        else if (new_req->priority == conflict_req->priority) {
            collision_resolved = true;

            /* Both timeslots have equivalent priorities */
            switch (sched->policy) {
            case OFF_CHAN_SCHED_POLICY_ROUND_ROBIN:
                resolved_ts = ieee80211_rm_ocs_round_robin_scheduler(sched, run_q, new_ts, conflict_ts);
                break;
            case OFF_CHAN_SCHED_POLICY_MRU_THRUPUT:
                resolved_ts = ieee80211_rm_ocs_mru_thruput_scheduler(sched, run_q, new_ts, conflict_ts);
                break;
            default:
                collision_resolved = false;
                /* 
                 * The schedule policy is set at scheduler creation time. 
                 * Therefore if no available policy is selected, then log error
                 */
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                                     "%s : Schedule Policy is out of range\n", __func__);
                ASSERT(0);
                break;
            }
        }
    }
    
    if ( ! collision_resolved ) {
        /* Release the new timeslot given insertion failure */
        ieee80211_rm_ocs_timeslot_free(sched, new_ts);
    }                

    if (resolved_ts != NULL) {
        *new_tsf = resolved_ts->stop_tsf;
    }
    
    return(EOK);
}

/*
 * Cleanup all of the colliding timeslots
 */
void 
ieee80211_rm_ocs_cleanup_collisions(
    ieee80211_resmgr_oc_scheduler_t sched,
    ieee80211_rm_oc_sched_run_q_t run_q,
    ieee80211_rm_oc_sched_timeslot_t new_ts,
    ieee80211_rm_oc_sched_timeslot_t conflict_ts
    )
{
    ieee80211_rm_oc_sched_timeslot_t collide_ts, temp_ts;
    ieee80211_rm_oc_sched_policy_entry_t temp_pe;
    oc_sched_timeslot_head collide_head;
    struct ieee80211com         *ic = sched->resmgr->ic;
                
    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);
        
    TAILQ_INIT(&collide_head);
        
    ieee80211_rm_ocs_find_all_collisions(sched, run_q, new_ts, conflict_ts, &collide_head);
    
    TAILQ_FOREACH_SAFE(collide_ts, &collide_head, ts_te, temp_ts) {
        TAILQ_REMOVE(&collide_head, collide_ts, ts_te);
        
        temp_pe = ieee80211_rm_ocs_find_policy_entry(sched, run_q, collide_ts->req);
    
        if (temp_pe != NULL) {
            /* Move conflict timeslot to PE collision list for future reference */
            ieee80211_rm_ocs_insert_collision_list(sched, temp_pe, collide_ts);
        }
        else {
            /* Release the timeslot at insertion point; it failed collision arbitration */
            ieee80211_rm_ocs_timeslot_free(sched, collide_ts);                    
        }
    }
    
    return;
}

/*
 * Collect all colliding timeslots onto a list
 */
void 
ieee80211_rm_ocs_find_all_collisions(
    ieee80211_resmgr_oc_scheduler_t sched,
    ieee80211_rm_oc_sched_run_q_t run_q,
    ieee80211_rm_oc_sched_timeslot_t new_ts,
    ieee80211_rm_oc_sched_timeslot_t inspt_ts,
    oc_sched_timeslot_head *collide_head_ptr
    )
{
    ieee80211_rm_oc_sched_timeslot_t cur_ts, temp_ts;
    bool done;
    struct ieee80211com         *ic = sched->resmgr->ic;

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);
            
    done = false;
    cur_ts = inspt_ts;
    
    while (! done && cur_ts != NULL) {
        if (TSFTIME_IS_SMALLER_EQ(new_ts->stop_tsf, cur_ts->start_tsf)) {
            done = true;
            break;
        }

        temp_ts = TAILQ_NEXT(cur_ts, ts_te);

        TAILQ_REMOVE(&(run_q->run_q_head), cur_ts, ts_te);
        
        TAILQ_INSERT_TAIL(collide_head_ptr, cur_ts, ts_te);

        cur_ts = temp_ts;
    }
    
    return;
}

/*
 * Insert a timeslot entry into the scheduler run queue
 */
int 
ieee80211_rm_ocs_insert_high_prio_timeslot(
    ieee80211_resmgr_oc_scheduler_t sched,
    ieee80211_rm_oc_sched_run_q_t run_q,
    ieee80211_rm_oc_sched_timeslot_t new_ts,
    ieee80211_rm_oc_sched_timeslot_t inspt_ts,
    ieee80211_rm_oc_sched_insert_pt_policy inspt_policy
    )
{
    struct ieee80211com         *ic = sched->resmgr->ic;
    oc_sched_timeslot_head collide_head;
    ieee80211_rm_oc_sched_timeslot_t cur_ts, temp_ts;

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);
            
    if (inspt_ts != NULL) {        
        TAILQ_INSERT_BEFORE(inspt_ts, new_ts, ts_te);
        
        if (inspt_policy == IEEE80211_OC_SCHED_REMOVE_INSPT) {
            TAILQ_INIT(&collide_head);

            ieee80211_rm_ocs_find_all_collisions(sched, run_q, new_ts, inspt_ts, &collide_head);

            if (! TAILQ_EMPTY(&collide_head)) {
                TAILQ_FOREACH_SAFE(cur_ts, &collide_head, ts_te, temp_ts) {
                    TAILQ_REMOVE(&collide_head, cur_ts, ts_te);
                    
                    /* Release the timeslot at insertion point; it failed collision arbitration */
                    ieee80211_rm_ocs_timeslot_free(sched, cur_ts);
                }
            }
        }
    }
    else {
        TAILQ_INSERT_TAIL(&(run_q->run_q_head), new_ts, ts_te);
    }
    
    new_ts->req->previous_pending_timeslot_tsf = new_ts->start_tsf;
    
    return(EOK);
}

/*
 * Find the insertion point for a new timeslot object
 */
int 
ieee80211_rm_ocs_find_timeslot_insertion_pt(
        ieee80211_resmgr_oc_scheduler_t sched, 
        ieee80211_rm_oc_sched_run_q_t run_q, 
        ieee80211_rm_oc_sched_timeslot_t new_ts,
        u_int32_t *new_tsf
        )
{   
    ieee80211_rm_oc_sched_timeslot_t prev_ts;
    ieee80211_rm_oc_sched_timeslot_t cur_ts;
    bool inserted;
    bool collision;
    int retval = EOK;
    struct ieee80211com         *ic = sched->resmgr->ic;
    
    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);
            
    prev_ts = NULL;
    inserted = false;
    collision = false;

    TAILQ_FOREACH(cur_ts, &(run_q->run_q_head), ts_te) {
        if (TSFTIME_IS_SMALLER(new_ts->start_tsf, cur_ts->start_tsf)) {
            
            /* New timeslot start occurs before the current timeslot start */
            if (prev_ts) {                
                if (TSFTIME_IS_GREATER_EQ(new_ts->start_tsf, prev_ts->stop_tsf)) {
                    
                    /* New timeslot start occurs after the previous timeslot stop */
                    if (TSFTIME_IS_SMALLER_EQ(new_ts->stop_tsf, cur_ts->start_tsf)) {
                        
                        /* insert before current timeslot */
                        inserted = true;
                        *new_tsf = new_ts->stop_tsf;
                        retval = ieee80211_rm_ocs_insert_high_prio_timeslot(sched, run_q, new_ts, cur_ts, 
                                        IEEE80211_OC_SCHED_KEEP_INSPT);
                    }
                    else {
                        
                        /* collision */
                        /* New timeslot stops after current timeslot starts */
                        collision = true;
                        retval = ieee80211_rm_ocs_resolve_collision(sched, run_q, new_ts, cur_ts, new_tsf);
                    }
                }
                else {
                    
                    /* collision */
                    /* New timeslot start occurs before previous timeslot has stopped */
                    collision = true;
                    retval = ieee80211_rm_ocs_resolve_collision(sched, run_q, new_ts, prev_ts, new_tsf);
                }
            }
            else {
                if (TSFTIME_IS_SMALLER_EQ(new_ts->stop_tsf, cur_ts->start_tsf)) {
                    
                    /* insert before current timeslot */
                    inserted = true;
                    *new_tsf = new_ts->stop_tsf;
                    retval = ieee80211_rm_ocs_insert_high_prio_timeslot(sched, run_q, new_ts, cur_ts, 
                                    IEEE80211_OC_SCHED_KEEP_INSPT);
                }
                else {
                    
                    /* collision */
                    /* New timeslot does not stop before the current timeslot needs to start */
                    collision = true;
                    retval = ieee80211_rm_ocs_resolve_collision(sched, run_q, new_ts, cur_ts, new_tsf);
                }
            }
        }
        else {
           if (TSFTIME_IS_SMALLER(new_ts->start_tsf, cur_ts->stop_tsf)) {
            
               /* Collision */
               /* New timeslot starts after current timeslot but it starts before the current timeslot stops */
               collision = true;
               retval = ieee80211_rm_ocs_resolve_collision(sched, run_q, new_ts, cur_ts, new_tsf);
           }
        }

        if (inserted || collision) {
            break;
        }
        
        prev_ts = cur_ts;
    }

    if (! inserted && ! collision) {
        /* Insert at head */
        TAILQ_INSERT_TAIL(&(run_q->run_q_head), new_ts, ts_te);
        new_ts->req->previous_pending_timeslot_tsf = new_ts->start_tsf;
        *new_tsf = new_ts->stop_tsf;
    }
    
    return(retval);
}

/*
 * Can a request be scheduled for air-time
 */
bool
ieee80211_rm_ocs_can_schedule_request(
    ieee80211_resmgr_oc_sched_req_t req
    )
{
    switch (req->state) {
    case INACTIVE:
    case RESET:    
        req->state = ACTIVE;
        /* NOTICE: Intentionally Falling through */
    case ACTIVE:
        break;
    case STOPPED:
    case COMPLETED:
    default:
        return(false);
    }
    
    return(true);
}

/*
** Verify that next TBTT is within the schedule window
*/
int
ieee80211_rm_ocs_validate_next_tbtt_tsf(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_resmgr_oc_sched_req_t req,
    u_int32_t bintval,
    u_int32_t base_tsf,
    u_int32_t end_tsf
    )
{
    int retval = EOK;
    u_int32_t next_req_start_tsf;
    struct ieee80211com *ic;

    if ( ! sched || ! req ) {
        return(EIO);
    }
    
    ic = sched->resmgr->ic;
    
    next_req_start_tsf = req->previous_pending_timeslot_tsf + bintval;

    if (TSFTIME_IS_SMALLER(next_req_start_tsf, base_tsf)) {
        DBG_PRINTK("%s : Request invalid! : next_req_start_tsf = 0x%08x base_tsf = 0x%08x end_tsf = 0x%08x\n", 
            __func__, next_req_start_tsf, base_tsf, end_tsf);
        retval = EINVAL;
    }
    else {
        if (TSFTIME_IS_GREATER(next_req_start_tsf, end_tsf)) {
        	DBG_PRINTK("%s : Request invalid! : next_req_start_tsf = 0x%08x base_tsf = 0x%08x end_tsf = 0x%08x\n", 
                __func__, next_req_start_tsf, base_tsf, end_tsf);
            retval = EINVAL;
        }
    }

    return(retval);
}

/*
 * Populate or fill-out a new scheduler run queue
 */
int 
ieee80211_rm_ocs_populate_high_prio_timeslots(
        ieee80211_resmgr_oc_scheduler_t sched, 
        ieee80211_rm_oc_sched_run_q_t run_q,        
        ieee80211_resmgr_oc_sched_req_t req
        )
{
    u_int32_t base_tsf;
    u_int32_t end_tsf;
    u_int32_t new_tsf;
    u_int32_t interval;
    u_int32_t duration;   
    ieee80211_rm_oc_sched_timeslot_t ts;
    int retval = EOK;
    bool done;
    int periodic_cnt;
    u_int32_t cache_prev_sched_tsf_ts;
    struct ieee80211com         *ic = sched->resmgr->ic;
    u_int32_t next_tbtt_tsf, bintval;
    u_int32_t   param1 = 0, param2 = 0;
    u_int32_t cache_previous_pending_timeslot_tsf;
    
    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);
            
    base_tsf = sched->next_tsf_sched_ev;    
    new_tsf = base_tsf;
    end_tsf = base_tsf + MS_TO_US(MAX_SCHEDULING_TIME_MSEC);

    if (ieee80211_rm_ocs_can_schedule_request(req) == false) {    
        return(EOK);
    }

    if (req->periodic == false) {
        req->previous_pending_timeslot_tsf = base_tsf;
    }
    else {
        retval = ieee80211_vap_ath_info_get(req->vap, ATH_VAP_INFOTYPE_SLOT_OFFSET, &param1, &param2);
        if (retval != EOK) {
            /* Error in getting the current TSF Offset. */
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                                 "%s : ieee80211_vap_ath_info_get failed\n", __func__);
        }

        if (ieee80211_resmgr_oc_sched_debug_level > 10) {                
            DBG_PRINTK("%s : param1 = 0x%08x\n", __func__, param1);
            DBG_PRINTK("%s : param2 = 0x%08x\n", __func__, param2);
        }
        
        next_tbtt_tsf = get_next_tbtt_tsf_32(req->vap) + param1;
        bintval = get_beacon_interval(req->vap);
        
        /* 
         * The SWBA interrupt must be processed prior to the TBTT in STA mode.
         *  It is necessary to add this additional time requirement on the high prio scheduler
         *  request.
         */
        cache_previous_pending_timeslot_tsf = req->previous_pending_timeslot_tsf;
        req->previous_pending_timeslot_tsf = next_tbtt_tsf - (bintval + 
            (sched->swba_delay_before_tbtt_usec + sched->device_pause_delay_usec));

        /*
                * TSF clock may have changed while the scheduler was computing the next
                * scheduling window. Thus, we need to validate the next tbtt_tsf. If it is out-of-range,
                * then skip this scheduling request. A TSF changed event will follow and cause a new
                * scheduling window to be computed that includes this request.
                */
        retval = ieee80211_rm_ocs_validate_next_tbtt_tsf(sched, req, bintval, base_tsf, end_tsf);
        if ( retval != EOK ) {
            DBG_PRINTK("%s : next_tbtt_tsf is outside of scheduling window! Request ignored!\n", __func__);
            req->previous_pending_timeslot_tsf = cache_previous_pending_timeslot_tsf;
            return(EOK);
        }
    }

    duration = req->duration_usec;
    interval = req->interval_usec;            

    /*
         * Adding duration to ending TSF in case the TBTT falls at the end of the scheduling window.
         * In essence, we need to expand the scheduling window to include the full requested time.
         */
    end_tsf += duration;
    
    run_q->starting_tsf = base_tsf;
    run_q->ending_tsf = end_tsf;
    
    /* 
     * Are there any Notice-Of-Absence event VAPs? If not, then schedule
     * at earliest convenience
     */
    
    if (TAILQ_EMPTY(&(sched->noa_event_vap_list)) || req->schedule_asap == true) {
        run_q->current_1shot_start_tsf = run_q->starting_tsf;
    }
    else {
        /* 
         * Add an additional NOA notification delay to the scheduling of a
         * One-Shot scheduler request.
         */
        run_q->current_1shot_start_tsf = 
                ieee80211_rm_ocs_compute_noa_scheduling_delay(sched, run_q, OC_SCHED_HIGH_PRIO_1SHOT);
    }
    
    periodic_cnt = 0;
    cache_prev_sched_tsf_ts = req->previous_pending_timeslot_tsf;
    done = false;    
    
    if (ieee80211_resmgr_oc_sched_debug_level > 10) {                
        DBG_PRINTK("%s[0] : base_tsf = 0x%08x end_tsf = 0x%08x\n", __func__, base_tsf, end_tsf);
        DBG_PRINTK("%s[0] : new_tsf = 0x%08x\n", __func__, new_tsf);
        DBG_PRINTK("%s[0] : req = %pK req->vap = %pK\n", __func__, req, req->vap);
        if (req && req->vap) {
        	DBG_PRINTK("%s[0] : req->vap->iv_unit = %d\n", __func__, req->vap->iv_unit);            
        }
        DBG_PRINTK("%s[0] : req_id = %d req_type = %d periodic = %d\n", __func__, req->req_id, req->req_type, req->periodic);
        DBG_PRINTK("%s[0] : duration_usec = 0x%08x interval_usec = 0x%08x\n", __func__, req->duration_usec, req->interval_usec);
        DBG_PRINTK("%s[0] : previous_pending_timeslot_tsf = 0x%08x\n", __func__, req->previous_pending_timeslot_tsf);
    }
    
    while (! done && TSFTIME_IS_SMALLER(new_tsf, end_tsf)) {
        ts =  ieee80211_rm_ocs_timeslot_alloc(sched);
        if (ts == NULL) {
            
            if (ieee80211_resmgr_oc_sched_debug_level > 10) {                
                DBG_PRINTK("%s : base_tsf = 0x%08x end_tsf = 0x%08x\n", __func__, base_tsf, end_tsf);
                DBG_PRINTK("%s : new_tsf = 0x%08x\n", __func__, new_tsf);
                DBG_PRINTK("%s : req_id = %d req_type = %d periodic = %d\n", __func__, req->req_id, req->req_type, req->periodic);
                DBG_PRINTK("%s : duration_usec = 0x%08x interval_usec = 0x%08x\n", __func__, req->duration_usec, req->interval_usec);
                DBG_PRINTK("%s : previous_pending_timeslot_tsf = 0x%08x\n", __func__, req->previous_pending_timeslot_tsf);
            }
            
            ieee80211_resmgr_oc_sched_debug_level = 1;
            
            ieee80211_rm_ocs_print_schedule(sched, 0);
            ieee80211_rm_ocs_print_schedule(sched, 1);
            
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                                 "%s : ieee80211_rm_ocs_timeslot_alloc failed\n", __func__);
            ASSERT(0);
            
            return(ENOMEM);
        }
        
        ts->req = req;

        /*
         * Check for a reoccurring request or one-shot
         */
        if (req->periodic == true) {
            if (cache_prev_sched_tsf_ts == req->previous_pending_timeslot_tsf) {
                /* Timeslot collision encountered and new timeslot failed arbitration */
                periodic_cnt += 1;                
            }
            else {
                /* New timeslot inserted successfully */
                periodic_cnt = 1;
            }
            
            if (ieee80211_resmgr_oc_sched_debug_level > 10) {                
                DBG_PRINTK("%s[1] : periodic_cnt = %d interval = %d\n", __func__, periodic_cnt, interval);
                DBG_PRINTK("%s[1] : previous_pending_timeslot_tsf = 0x%08x cache_prev_sched_tsf_ts = 0x%08x\n", 
                    __func__, req->previous_pending_timeslot_tsf, cache_prev_sched_tsf_ts);
            }
            
            ts->start_tsf = req->previous_pending_timeslot_tsf + (interval * periodic_cnt);
            cache_prev_sched_tsf_ts = req->previous_pending_timeslot_tsf;
        }
        else {

            if (ieee80211_resmgr_oc_sched_debug_level > 10) {                
                DBG_PRINTK("%s[1] : current_1shot_start_tsf = 0x%08x\n", __func__, run_q->current_1shot_start_tsf);
            }
            
            ts->start_tsf = run_q->current_1shot_start_tsf;
            run_q->current_1shot_start_tsf += duration;
            
            if (ieee80211_resmgr_oc_sched_debug_level > 10) {                
                DBG_PRINTK("%s[1] : NEW current_1shot_start_tsf = 0x%08x\n", __func__, run_q->current_1shot_start_tsf);
            }
            
            done = true;
        }
        
        ts->stop_tsf = ts->start_tsf + duration;
        
        /*
         * Find insertion point for new timeslot into run queue
         */
        retval = ieee80211_rm_ocs_find_timeslot_insertion_pt(sched, run_q, ts, &new_tsf);
        if (retval != EOK) {
           ieee80211_rm_ocs_timeslot_free(sched, ts);
           done = true;
        }
        
        if (ieee80211_resmgr_oc_sched_debug_level > 10) {                
            DBG_PRINTK("%s[1] : new_tsf = 0x%08x\n", __func__, new_tsf);
        }
    }

    return(retval);
}

/*
 * Calculate the scheduling delay needed in order
 * give sufficient time to successfully notify any
 * clients via the beacon IEs of a new NOA
 */
u_int32_t
ieee80211_rm_ocs_compute_noa_scheduling_delay(
    ieee80211_resmgr_oc_scheduler_t sched,
    ieee80211_rm_oc_sched_run_q_t run_q,
    ieee80211_resmgr_oc_sched_req_index req_type
)
{
    u_int32_t start_tsf = run_q->starting_tsf;
    
    if (req_type == OC_SCHED_HIGH_PRIO_1SHOT) {
        start_tsf += sched->noa_event_scheduling_delay;
    }
    else if (req_type == OC_SCHED_LOW_PRIO_1SHOT) {
        start_tsf += sched->noa_event_scheduling_delay;
    }
    
    return(start_tsf);
}


/*
 * compare the cached schedules with the new ones.
 * if there is a difference then update the cache and return true.
 * if there is no change then just resutn false.
 */
static bool
ieee80211_rm_ocs_update_noa_schedules(
    ieee80211_resmgr_oc_scheduler_t sched,   
    struct ieee80211_resmgr_noa_schedule *noa_sched,
    u_int8_t  *valid_schedules
    )
{
  bool update = false;
  ieee80211_resmgr_noa_schedule_t noa_sched_ptr = NULL;
  ieee80211_rm_oc_sched_noa_update_cache_t *cache = NULL;
  u_int32_t delta_periodic_noa_update_tsf = 0;

#define  PERIODIC_NOA_UPDATE_THRESHOLD_USEC  (28 * 60 * 1000 * 1000)

  cache = &sched->noa_update_cache;
  
  if (cache->cache_valid == false) {
    update = true;
  }
  else {

      /* check and update periodic NOA schedule cache */
      if ( valid_schedules[PERIODIC_NOA_SCHEDULE] ) { 
          noa_sched_ptr = &noa_sched[PERIODIC_NOA_SCHEDULE];
          if (noa_sched_ptr->duration != cache->duration_continuous) {
              update = true;
          }
    
          if (noa_sched_ptr->interval != cache->interval_continuous) {
              update = true;
          }

          delta_periodic_noa_update_tsf = noa_sched_ptr->start_tsf - cache->last_periodic_noa_update_tsf;
          if (TSFTIME_IS_GREATER_EQ(delta_periodic_noa_update_tsf, PERIODIC_NOA_UPDATE_THRESHOLD_USEC)) {
              update = true;
          }
          /* updat the periodic NOA in the cache */
          cache->last_periodic_noa_update_tsf = noa_sched_ptr->start_tsf;
          cache->duration_continuous = noa_sched_ptr->duration;
          cache->interval_continuous = noa_sched_ptr->interval;
      } else if ( cache->valid_schedules[PERIODIC_NOA_SCHEDULE] ) { 
          /* if the periodica NOA is disabled in the new schedule , then update the schedule */
          update = true;
      }
    
      /* check and update oneshot NOA schedule cache */
      if (valid_schedules[ONE_SHOT_NOA_SCHEDULE]) { 
          noa_sched_ptr = &noa_sched[ONE_SHOT_NOA_SCHEDULE];
        
          if (noa_sched_ptr->start_tsf != cache->start_tsf_one_shot) {
              update = true;
          }

          if (noa_sched_ptr->duration != cache->duration_one_shot) {
              update = true;
          }
          cache->start_tsf_one_shot = noa_sched_ptr->start_tsf;
          cache->duration_one_shot = noa_sched_ptr->duration;
      }  else if ( cache->valid_schedules[ONE_SHOT_NOA_SCHEDULE] ) { 
          /* if the one NOA is disabled in the new schedule , then update the schedule */
          update = true;
      }

    
      if (update == true) {    
          cache->valid_schedules[ONE_SHOT_NOA_SCHEDULE]  = valid_schedules[ONE_SHOT_NOA_SCHEDULE]; 
          cache->valid_schedules[PERIODIC_NOA_SCHEDULE]  = valid_schedules[PERIODIC_NOA_SCHEDULE]; 
          cache->cache_valid = true;
          DBG_PRINTK("%s : delta_periodic_noa_update_tsf = 0x%08x\n", __func__, delta_periodic_noa_update_tsf);
          if (ieee80211_resmgr_oc_sched_debug_level > 2) {
              int i;
                    
              for (i=0; i < IEEE80211_RESMGR_MAX_NOA_SCHEDULES; i++) {
                  if (valid_schedules[i]) { 
                      noa_sched_ptr = &noa_sched[i];
                      DBG_PRINTK("%s : **** NOA SCHEDULE[%d] ****\n", __func__, i);
                      DBG_PRINTK("%s : type_count = %d\n", __func__, noa_sched_ptr->type_count);
                      DBG_PRINTK("%s : priority   = %d\n", __func__, noa_sched_ptr->priority);
                      DBG_PRINTK("%s : start_tsf  = 0x%08x\n", __func__, noa_sched_ptr->start_tsf);
                      DBG_PRINTK("%s : duration   = 0x%08x\n", __func__, noa_sched_ptr->duration);
                      DBG_PRINTK("%s : interval   = 0x%08x\n", __func__, noa_sched_ptr->interval);
                  }
              }
          }
      }   
  }

  return(update);
}
    
/*
* invalidate  the cache 
*/
static void   ieee80211_rm_ocs_cleanup_noa_schedules(ieee80211_resmgr_oc_scheduler_t sched)   
{
  sched->noa_update_cache.cache_valid = false;
}

/*
* clear all existing NOAs with all the NOA vaps.
*/
static void   ieee80211_rm_ocs_clear_noa_schedules(ieee80211_resmgr_oc_scheduler_t sched)
{
    ieee80211_resmgr_oc_sched_noa_event_vap_t cur_nev = NULL;
    ieee80211_vap_t cur_vap = NULL;

    TAILQ_FOREACH(cur_nev, &(sched->noa_event_vap_list), nev_te) {
        cur_vap = cur_nev->vap;
        cur_nev->handler(cur_nev->arg, 0, 0);
    }

}


typedef enum _noa_state_t {
        INIT = 0,
        SYNC,
        START,
        STOP
} noa_state_t;

/*
 * Compute the Notice-Of-Absence for each registered VAP
 */
void
ieee80211_rm_ocs_compute_notice_of_absence(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_run_q_t run_q
    )
{
    struct ieee80211com         *ic = sched->resmgr->ic;
    ieee80211_resmgr_oc_sched_noa_event_vap_t cur_nev = NULL;
    ieee80211_rm_oc_sched_timeslot_t ts, prev_ts = NULL;
    u_int32_t start_tsf, stop_tsf;
    ieee80211_vap_t cur_vap = NULL;
    noa_state_t noa_state = INIT;
    bool computed, found_one_shot = false;
    u_int32_t duration, interval;
    struct ieee80211_resmgr_noa_schedule noa_sched[IEEE80211_RESMGR_MAX_NOA_SCHEDULES];
    u_int8_t  valid_schedules[IEEE80211_RESMGR_MAX_NOA_SCHEDULES];
    ieee80211_resmgr_noa_schedule_t noa_sched_ptr;
    int noa_sched_count = 0;
    u_int32_t interval_start_tsf, interval_stop_tsf = 0;

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);
            
    if (TAILQ_EMPTY(&(sched->noa_event_vap_list))) {
        return;
    }

    TAILQ_FOREACH(cur_nev, &(sched->noa_event_vap_list), nev_te) {
        
        noa_state = INIT;
        cur_vap = cur_nev->vap;
        prev_ts = NULL;
        computed = found_one_shot = false;
        start_tsf = stop_tsf = duration = 0;        
        noa_sched_count = 0;
        interval_start_tsf = interval_stop_tsf = 0;

        OS_MEMZERO(&noa_sched[0], sizeof(noa_sched));
        OS_MEMZERO(valid_schedules, sizeof(valid_schedules));
        
        TAILQ_FOREACH(ts, &(run_q->run_q_head), ts_te) {

            if (found_one_shot == false) {
                if (ts->req->periodic == false) {
                    if (ts->req->vap != cur_vap) {
                        noa_sched_ptr = &noa_sched[ONE_SHOT_NOA_SCHEDULE];
                        
                        noa_sched_ptr->type_count   = 1;
                        noa_sched_ptr->priority     = IEEE80211_RESMGR_NOA_HIGH_PRIO;
                        noa_sched_ptr->start_tsf    = ts->start_tsf;
                        noa_sched_ptr->duration     = ts->req->duration_usec;
                        noa_sched_ptr->interval     = 0;
                        
                        DBG_PRINTK("%s : One-Shot NOA : start_tsf = 0x%08x duration = 0x%08x\n", __func__, noa_sched_ptr->start_tsf, noa_sched_ptr->duration);
                        
                        noa_sched_count += 1;
                        
                        found_one_shot = true;
                        valid_schedules[ONE_SHOT_NOA_SCHEDULE] = 1;

                        if (noa_state != STOP) {
                            computed = false;
                            noa_state = INIT;
                        }
                    }
                }
            }
            
            switch (noa_state) {
            case INIT:
                if (ts->req->priority == OFF_CHAN_SCHED_PRIO_HIGH) {
                    if (ts->req->periodic == true) {
                        if (ts->req->vap == cur_vap) {
                            interval_start_tsf = ts->start_tsf;
                            DBG_PRINTK("%s : INIT -> SYNC\n", __func__);
                            ieee80211_rm_ocs_dump_timeslot(sched, ts);
                            noa_state = SYNC;
                        }
                    }
                }
                break;
                
            case SYNC:
                if (ts->req->periodic == false) {
                    computed = false;
                    noa_state = INIT;
                }
                else {              
                    if (ts->req->vap != cur_vap) {
                        start_tsf = ts->start_tsf;
                        DBG_PRINTK("%s : SYNC -> START\n", __func__);
                        ieee80211_rm_ocs_dump_timeslot(sched, ts);
                        noa_state = START;                  
                    }
                }
                break;
                
            case START: 
                if (ts->req->periodic == false) {
                    computed = false;
                    noa_state = INIT;
                }
                else {
                    if (ts->req->vap == cur_vap) {
                        if (computed == false) {
                            if (prev_ts != NULL) {
                                stop_tsf = prev_ts->stop_tsf;
                                DBG_PRINTK("%s : START [stop_tsf]\n", __func__);
                                ieee80211_rm_ocs_dump_timeslot(sched, prev_ts);
                                computed = true;
                            }
                            else {
                                computed = false;
                                noa_state = INIT;
                            }
                        }
                        
                        if (computed == true && ts->req->priority == OFF_CHAN_SCHED_PRIO_HIGH) {
                            interval_stop_tsf = ts->start_tsf;
                            DBG_PRINTK("%s : START -> STOP\n", __func__);
                            ieee80211_rm_ocs_dump_timeslot(sched, ts);
                            noa_state = STOP;
                        }
                    }
                }
                break;
                
            case STOP:
            default:
                break;
            }

            prev_ts = ts;
        }

        if (computed == true) {
            duration = abs((int32_t) (stop_tsf - start_tsf));
            interval = abs((int32_t) (interval_stop_tsf - interval_start_tsf));
            
            noa_sched_ptr = &noa_sched[PERIODIC_NOA_SCHEDULE];
            
            noa_sched_ptr->type_count   = 255;
            noa_sched_ptr->priority     = IEEE80211_RESMGR_NOA_LOW_PRIO;
            noa_sched_ptr->start_tsf    = start_tsf + sched->device_pause_delay_usec;
            noa_sched_ptr->duration     = duration;
            noa_sched_ptr->interval     = interval;
            
            valid_schedules[PERIODIC_NOA_SCHEDULE] = 1;
            DBG_PRINTK("%s : Periodic NOA : start_tsf = 0x%08x duration = 0x%08x interval = 0x%08x\n", 
                __func__, noa_sched_ptr->start_tsf, duration, interval);
            
            noa_sched_count += 1;

            if (ieee80211_rm_ocs_update_noa_schedules(sched, &noa_sched[0], valid_schedules) == true) {
               /* Notify the NOA handler of the latest schedule information */
                cur_nev->handler(cur_nev->arg, &noa_sched[0], noa_sched_count);

                sched->stats.noa_schedule_updates += 1;
                
            }
        } else if (found_one_shot == true) {
             if (ieee80211_rm_ocs_update_noa_schedules(sched, &noa_sched[0], valid_schedules) == true) {
                /* Notify the NOA handler of the latest schedule information */
                 if (valid_schedules[0]) {
                     /* either periodic NOA is valid (or) both periodic and one shot are valid */
                     cur_nev->handler(cur_nev->arg, &noa_sched[0], noa_sched_count);
                 } else {
                     /* only one shot is valid , no periodic*/
                     cur_nev->handler(cur_nev->arg, &noa_sched[ONE_SHOT_NOA_SCHEDULE], noa_sched_count);
                 }

                 sched->stats.noa_schedule_updates += 1;

             }
         }


    }

    return;
}

/*
 * Calculate a new schedule based on the latest off-channel air-time requests
 */
int 
ieee80211_rm_ocs_calc_schedule(
        ieee80211_resmgr_oc_scheduler_t sched,
        u_int8_t new_runq_ndx
        )
{
    ieee80211_rm_oc_sched_run_q_t run_q;
    ieee80211_resmgr_oc_sched_req_t req;
    int retval = EOK;
    struct ieee80211com         *ic = sched->resmgr->ic;
    
    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);
            
    if (TAILQ_EMPTY(&(sched->high_prio_list)) && TAILQ_EMPTY(&(sched->low_prio_list))) {
        return(retval);
    }
    
    IEEE80211_RESMGR_OCSCHE_LOCK(sched);

    run_q = &sched->run_queues[new_runq_ndx];

    /*
     * Cleanup previous run queue timeslots
     */
    ieee80211_rm_ocs_cleanup_timeslot_list(sched, 
                ((oc_sched_timeslot_head *) &(run_q->run_q_head)));
    
    /*
     * Cleanup any schedule policy entries
     */
    ieee80211_rm_ocs_cleanup_sched_policy_list(sched, run_q);

    DBG_PRINTK("%s : TIMESLOT_FREE_Q\n", __func__);
    ieee80211_rm_ocs_count_timeslots(sched, ((oc_sched_timeslot_head *) &(sched->timeslot_free_q)));
    
    /*
     * Populate timeslots in the schedule for high priority requests
     */    
    TAILQ_FOREACH(req, &(sched->high_prio_list), req_te) {
        retval = ieee80211_rm_ocs_populate_high_prio_timeslots(sched, run_q, req);
        if (retval != EOK) {
            IEEE80211_RESMGR_OCSCHE_UNLOCK(sched);
            return(retval);
        }
    }

    /*
     * Cleanup any schedule policy entries
     */
    ieee80211_rm_ocs_cleanup_sched_policy_list(sched, run_q);
    
    /*
     * Populate timeslots in the schedule for low priority requests
     */
    if (! TAILQ_EMPTY(&(sched->noa_event_vap_list))) {
        if (! TAILQ_EMPTY(&(sched->low_prio_list))) {
            retval = ieee80211_rm_ocs_populate_low_prio_timeslots(sched, run_q);
            if (retval != EOK) {
                IEEE80211_RESMGR_OCSCHE_UNLOCK(sched);
                return(retval);
            }
        }
    }
    
    if (! TAILQ_EMPTY(&(sched->noa_event_vap_list))) {
        ieee80211_rm_ocs_compute_notice_of_absence(sched, run_q);   
    }
    
    ieee80211_rm_ocs_print_schedule(sched, new_runq_ndx);
    ieee80211_rm_ocs_print_statistics(sched);
    
    IEEE80211_RESMGR_OCSCHE_UNLOCK(sched);
 
    return(EOK);
}

/*
 * Print or dump the computed schedule
 */
void 
ieee80211_rm_ocs_print_schedule(
    ieee80211_resmgr_oc_scheduler_t sched, 
    int8_t run_q_ndx
    )
{
    ieee80211_rm_oc_sched_run_q_t run_q;
    ieee80211_rm_oc_sched_timeslot_t cur_ts;
    int cnt;
    struct ieee80211com         *ic = sched->resmgr->ic;
    
    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);
            
    if (ieee80211_resmgr_oc_sched_debug_level < 9) {
        return;
    }
    
    if (run_q_ndx < 0 || run_q_ndx > (MAX_RUN_QUEUES - 1)) {
        return;
    }

    run_q = &(sched->run_queues[run_q_ndx]);

    DBG_PRINTK("%s\n", "==================== Print Off-Channel Schedule ====================");
    DBG_PRINTK("Run queue index = %d\n", run_q_ndx);
    DBG_PRINTK("Timeslots Inuse Count = %d\n", sched->timeslots_inuse_count);
    DBG_PRINTK("Maximum Timeslots Inuse Count = %d\n", sched->max_timeslots_inuse_count);    
    DBG_PRINTK("Starting_TSF = 0x%08x   Ending_TSF = 0x%08x\n", run_q->starting_tsf, run_q->ending_tsf);
    DBG_PRINTK("%s\n", "====================================================================");

    cnt = 0;    
    TAILQ_FOREACH(cur_ts, &(run_q->run_q_head), ts_te) {
        DBG_PRINTK("%s", "\n");
        DBG_PRINTK("Timeslot[%d] start_tsf = 0x%08x\n", cnt, cur_ts->start_tsf);
        DBG_PRINTK("Timeslot[%d] stop_tsf = 0x%08x\n", cnt, cur_ts->stop_tsf);
        DBG_PRINTK("Timeslot[%d] request id = %d\n", cnt, cur_ts->req->req_id);
        
        if (cur_ts->req->priority == OFF_CHAN_SCHED_PRIO_HIGH) {
            DBG_PRINTK("Timeslot[%d] priority = HIGH\n", cnt);
       }
        else {
            DBG_PRINTK("Timeslot[%d] priority = LOW\n", cnt);
        }
        DBG_PRINTK("%s", "\n");

        cnt += 1;
    }
    
    DBG_PRINTK("%s\n", "==================== Completed Print Off-Channel Schedule ====================");

    return;
}

/*
 * Populate one-shot and periodic low priority timeslots into new schedule
 */
int 
ieee80211_rm_ocs_populate_low_prio_timeslots(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_run_q_t run_q
    )
{
    ieee80211_resmgr_oc_sched_req_t req;
    bool do_periodic_req = false;
    int retval = EOK;
    struct ieee80211com         *ic = sched->resmgr->ic;
    
    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);

    /*
     * Non-periodic or one-shot requests have priority over the
     * periodic ones
     */

    TAILQ_FOREACH(req, &(sched->low_prio_list), req_te) {
    
        if (ieee80211_rm_ocs_can_schedule_request(req) == true) {
            
            if (req->periodic == false) {
                
                retval = ieee80211_rm_ocs_insert_low_prio_one_shot_request(sched, 
                                run_q, req);
                if (retval != EOK) {
                    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                                         "%s : No available timeslots\n", __func__);
                    ASSERT(0);
                    return(retval);
                }                
            }
            else {                
                do_periodic_req = true;
            }
        }
    }

    if (do_periodic_req == true) {
        /*
         * Fill-in any remain timeslots with any remaining low priority 
         * periodic requests
         */
        retval = ieee80211_rm_ocs_fill_in_avail_timeslots(sched, run_q);
        if (retval != EOK) {
            IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                                 "%s : No available timeslots\n", __func__);
            ASSERT(0);
        }                
    }
    
    return(retval);
}

/*
 * Insert a low priority non-periodic or one-shot request into schedule
 */
int
ieee80211_rm_ocs_insert_low_prio_one_shot_request(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_run_q_t run_q,
    ieee80211_resmgr_oc_sched_req_t req
    )
{
    int32_t gap_duration, req_duration;
    struct ieee80211com         *ic = sched->resmgr->ic;
    ieee80211_rm_oc_sched_timeslot_t cur_ts, new_ts;
    u_int32_t stop_tsf, noa_start_tsf;
    int retval = EOK;
    bool inserted_req = false;
    
    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);

    req_duration = req->duration_usec;
    stop_tsf = run_q->starting_tsf;
    
    if (TAILQ_EMPTY(&(sched->noa_event_vap_list)) || req->schedule_asap == true) {
        noa_start_tsf = run_q->starting_tsf;
    }
    else {
        noa_start_tsf = ieee80211_rm_ocs_compute_noa_scheduling_delay(sched, run_q, OC_SCHED_LOW_PRIO_1SHOT);
    }
    
    TAILQ_FOREACH(cur_ts, &(run_q->run_q_head), ts_te) {

        if (TSFTIME_IS_SMALLER_EQ(noa_start_tsf,cur_ts->start_tsf)) {
            
            gap_duration = cur_ts->start_tsf - stop_tsf;
            
            DBG_PRINTK("%s : gap_duration = 0x%08x\n", 
                __func__, gap_duration);
                    
            if (TSFTIME_IS_GREATER_EQ(gap_duration, req_duration)) { 
                
                new_ts = ieee80211_rm_ocs_timeslot_alloc(sched);
                if (new_ts != NULL) {
                    new_ts->start_tsf = stop_tsf;
                    new_ts->stop_tsf = new_ts->start_tsf + req_duration;
                    new_ts->req = req;
                    
                    TAILQ_INSERT_BEFORE(cur_ts, new_ts, ts_te);

                    DBG_PRINTK("%s[0] : low prio one-shot inserted (Req ID: %d)\n", 
                        __func__, req->req_id);
                    
                    inserted_req = true;
                    break;
                }
                else {
                    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                                         "%s : No available timeslots\n", __func__);
                    ASSERT(0);
                    retval = ENOMEM;
                    break;
                }
            }
        } 
        
        stop_tsf = cur_ts->stop_tsf;
    }

    if (retval == EOK && inserted_req == false) {
        
        /* Check the end of the run queue window */
        gap_duration = run_q->ending_tsf - stop_tsf;
        
        if (TSFTIME_IS_GREATER_EQ(gap_duration, req_duration)) { 
            
            new_ts = ieee80211_rm_ocs_timeslot_alloc(sched);
            if (new_ts != NULL) {
                new_ts->start_tsf = stop_tsf;
                new_ts->stop_tsf = new_ts->start_tsf + req_duration;
                new_ts->req = req;
                
                DBG_PRINTK("%s[1] : low prio one-shot inserted at tail (Req ID: %d)\n", 
                    __func__, req->req_id);
                
                TAILQ_INSERT_TAIL(&(run_q->run_q_head), new_ts, ts_te);
            }
            else {
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                                     "%s : No available timeslots\n", __func__);
                ASSERT(0);
                retval = ENOMEM;
            }
        }        
    } 
    
    return(retval);
}

/*
 * Walk a timeslot list and free each timeslot element
 */
void
ieee80211_rm_ocs_cleanup_timeslot_list(
    ieee80211_resmgr_oc_scheduler_t sched, 
    oc_sched_timeslot_head *timeslot_head
    )
{
    ieee80211_rm_oc_sched_timeslot_t cur_ts, temp_ts;
    struct ieee80211com         *ic = sched->resmgr->ic;
    
    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);

    if (! TAILQ_EMPTY(timeslot_head)) {
        TAILQ_FOREACH_SAFE(cur_ts, timeslot_head, ts_te, temp_ts) {
            TAILQ_REMOVE(timeslot_head, cur_ts, ts_te);
            
            /* Release the timeslot at insertion point; it failed collision arbitration */
            ieee80211_rm_ocs_timeslot_free(sched, cur_ts);
        }
    }
    
    return;
}

void
ieee80211_rm_ocs_walk_sched_policy_list(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_run_q_t run_q
    )
{
    ieee80211_rm_oc_sched_policy_entry_t pe, temp_pe;
    
    if (TAILQ_EMPTY(&(run_q->sched_policy_entry_head))) {        
        DBG_PRINTK("%s : sched_policy_entry_head[%pK] is EMPTY!\n", 
            __func__, &(run_q->sched_policy_entry_head));
        return;
    }
    
    TAILQ_FOREACH_SAFE(pe, &(run_q->sched_policy_entry_head), pe_te, temp_pe) {    
        DBG_PRINTK("%s : pe = %pK reqID = %d\n", 
            __func__, pe, pe->req->req_id);
    }
    
    return;
}

/*
 * Walk a schedule policy list and free each schedule policy element
 */
void
ieee80211_rm_ocs_cleanup_sched_policy_list(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_run_q_t run_q
    )
{
    ieee80211_rm_oc_sched_policy_entry_t pe, temp_pe;    
    struct ieee80211com         *ic = sched->resmgr->ic;
    
    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);

    if (TAILQ_EMPTY(&(run_q->sched_policy_entry_head))) {
        return;
    }
    
    TAILQ_FOREACH_SAFE(pe, &(run_q->sched_policy_entry_head), pe_te, temp_pe) {    
        ieee80211_rm_ocs_cleanup_timeslot_list(sched, 
                    ((oc_sched_timeslot_head *) &(pe->collision_q_head)));
        
        TAILQ_REMOVE(&(run_q->sched_policy_entry_head), pe, pe_te);
        pe->req = NULL;
        
        TAILQ_INSERT_HEAD(&(sched->sched_policy_entry_free_q), pe, pe_te);
    }
    
    return;
}

/*
 * Get next low priority periodic request
 */
ieee80211_resmgr_oc_sched_req_t
ieee80211_rm_ocs_get_next_lowprio_periodic_request(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_run_q_t run_q,
    ieee80211_resmgr_oc_sched_req_t cur_req
    )
{
    ieee80211_resmgr_oc_sched_req_t req, next_req;

    req = cur_req;
    next_req = NULL;
    
    while (true) {
        /* Advance to next request */
        if (req == NULL) {
            req = TAILQ_FIRST(&(sched->low_prio_list));        
        }
        else {
            req = TAILQ_NEXT(req, req_te);
        }

        /* If periodic then stop */
        if (req && req->periodic == true) {
            next_req = req;
            break;
        }

        /* Circled completely around */
        if (req == cur_req) {
            break;
        }

    }
    
    return(next_req);
}

/*
 * Fill in periodic low prority requests into appropriately sized timeslots
 */
int
ieee80211_rm_ocs_fill_in_avail_timeslots(
    ieee80211_resmgr_oc_scheduler_t sched, 
    ieee80211_rm_oc_sched_run_q_t run_q
    )
{
    int32_t gap_duration, duration_limit_offset;
    ieee80211_rm_oc_sched_timeslot_t cur_ts, new_ts;
    u_int32_t stop_tsf;
    ieee80211_resmgr_oc_sched_req_t cur_req;
    struct ieee80211com         *ic = sched->resmgr->ic;
    int retval = EOK;
    ieee80211_resmgr_vap_priv_t resmgr_vap;
    u_int32_t air_time_limit;
    
#define  AIR_TIME_100_PERCENT  100
    
    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_FUNCTION, IEEE80211_MSG_RESMGR_OC_SCHED,
                         "%s : entry\n", __func__);

    cur_req = ieee80211_rm_ocs_get_next_lowprio_periodic_request(sched, run_q, NULL);
    if (cur_req == NULL) {
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                             "%s : No low priority periodic requests\n", __func__);
        ASSERT(0);
        return(EINVAL);
    }

    stop_tsf = run_q->starting_tsf;
    
    TAILQ_FOREACH(cur_ts, &(run_q->run_q_head), ts_te) {

        gap_duration = cur_ts->start_tsf - stop_tsf;
        
        if (ieee80211_resmgr_oc_sched_debug_level > 10) {                
            DBG_PRINTK("%s : gap_duration = 0x%08x\n", 
                __func__, gap_duration);
        }
                
        if (TSFTIME_IS_GREATER_EQ(gap_duration, MINIMUM_GAP_DURATION_USEC)) { 
            
            new_ts = ieee80211_rm_ocs_timeslot_alloc(sched);
            if (new_ts != NULL) {
                
                resmgr_vap = ieee80211vap_get_resmgr(cur_req->vap);
                air_time_limit = resmgr_vap->sched_data.scheduled_air_time_limit;

                if (air_time_limit > resmgr_vap->sched_data.high_prio_air_time_percent) {
                    
                    duration_limit_offset = 0;


                    if (air_time_limit < AIR_TIME_100_PERCENT) {
                        /* air_time_limit is a percentage of time. Thus, use fixed-point math to compute */
                        duration_limit_offset = gap_duration - (((gap_duration * air_time_limit) + 50) / 100);
                        DBG_PRINTK("%s : duration_limit_offset = 0x%08x\n", __func__, duration_limit_offset);
                    }
                    
                    new_ts->start_tsf = stop_tsf + duration_limit_offset;                   
                    new_ts->stop_tsf = cur_ts->start_tsf;
                    new_ts->req = cur_req;
                    
                    TAILQ_INSERT_BEFORE(cur_ts, new_ts, ts_te);
                }
                else {
                    
                    ieee80211_rm_ocs_timeslot_free(sched, new_ts);
                }
                
                if (ieee80211_resmgr_oc_sched_debug_level > 10) {
                    DBG_PRINTK("%s[0] : low prio periodic inserted (Req ID: %d)\n", 
                        __func__, cur_req->req_id);
                }
                                
                cur_req = ieee80211_rm_ocs_get_next_lowprio_periodic_request(sched, run_q, cur_req);
                if (cur_req == NULL) {
                    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                                         "%s : No low priority periodic requests\n", __func__);
                    ASSERT(0);
                    retval = EINVAL;
                    break;
               }
            }
            else {
                DBG_PRINTK("%s : stop_tsf = 0x%08x\n", __func__, stop_tsf);
                DBG_PRINTK("%s : req_id = %d req_type = %d periodic = %d\n", __func__, cur_req->req_id, cur_req->req_type, cur_req->periodic);
                DBG_PRINTK("%s : duration_usec = 0x%08x interval_usec = 0x%08x\n", __func__, cur_req->duration_usec, cur_req->interval_usec);
                DBG_PRINTK("%s : previous_pending_timeslot_tsf = 0x%08x\n", __func__, cur_req->previous_pending_timeslot_tsf);
                
                ieee80211_resmgr_oc_sched_debug_level = 1;
                
                ieee80211_rm_ocs_print_schedule(sched, 0);
                ieee80211_rm_ocs_print_schedule(sched, 1);
                
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                                     "%s : No available timeslots\n", __func__);
                ASSERT(0);
                retval = ENOMEM;
                break;
            }
        }
        
        stop_tsf = cur_ts->stop_tsf;
    }

    if (retval == EOK && cur_req) {
        /* Check the end of the run queue window */
        gap_duration = abs((int32_t) (stop_tsf - run_q->ending_tsf));

        if (gap_duration >= (int32_t) MINIMUM_GAP_DURATION_USEC) { 
            
            new_ts = ieee80211_rm_ocs_timeslot_alloc(sched);
            if (new_ts != NULL) {
                
                resmgr_vap = ieee80211vap_get_resmgr(cur_req->vap);
                air_time_limit = resmgr_vap->sched_data.scheduled_air_time_limit;

                if (air_time_limit > resmgr_vap->sched_data.high_prio_air_time_percent) {
                    
                    duration_limit_offset = 0;


                    if (air_time_limit < AIR_TIME_100_PERCENT) {
                        /* air_time_limit is a percentage of time. Thus, use fixed-point math to compute */
                        duration_limit_offset = gap_duration - (((gap_duration * air_time_limit) + 50) / 100);
                        DBG_PRINTK("%s : duration_limit_offset = 0x%08x\n", __func__, duration_limit_offset);
                    }
                    
                    new_ts->start_tsf = stop_tsf + duration_limit_offset;                   
                    new_ts->stop_tsf = run_q->ending_tsf;
                    new_ts->req = cur_req;
                    
                    TAILQ_INSERT_TAIL(&(run_q->run_q_head), new_ts, ts_te);
                }
                else {
                    DBG_PRINTK("%s : air_time_limit <= high_prio_percent( free TS )\n", __func__);
                    
                    ieee80211_rm_ocs_timeslot_free(sched, new_ts);
                }
                
                if (ieee80211_resmgr_oc_sched_debug_level > 10) {                
                    DBG_PRINTK("%s[1] : low prio periodic inserted at tail (Req ID: %d)\n", 
                        __func__, cur_req->req_id);
                }
            }
            else {
                IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_RESMGR_OC_SCHED,
                                     "%s : No available timeslots\n", __func__);
                ASSERT(0);
                retval = ENOMEM;
            }
        }        
    } 
    
#undef  AIR_TIME_100_PERCENT
    
    return(retval);
}


static const char *oc_sched_op_names[OC_SCHED_OP_MAX+1] = 
{
    "NONE",
    "VAP",
    "SCAN",
    "SLEEP",
    "NOP",
    "UNKNOWN"
};

/*
 * return human readable name for the scheduler operation.
 */
const char *ieee80211_resmgr_oc_sched_get_op_name( ieee80211_resmgr_oc_sched_operation_t operation_type)
{
    if (operation_type >= OC_SCHED_OP_MAX) {
        operation_type = (OC_SCHED_OP_MAX -1 );
    }

    return oc_sched_op_names[operation_type];
}

#endif /* UMAC_SUPPORT_RESMGR_OC_SCHED */

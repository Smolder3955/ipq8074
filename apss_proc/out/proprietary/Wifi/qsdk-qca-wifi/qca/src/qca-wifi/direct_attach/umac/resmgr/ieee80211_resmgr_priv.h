/*
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*
 *  Copyright (c) 2008 Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

/*
 * Private file for Resource Manager module. Not to be included
 * outside.
 */

#ifndef _IEEE80211_RESMGR_PRIV_H_
#define _IEEE80211_RESMGR_PRIV_H_

#include <_ieee80211.h>
#include <ieee80211.h>
#include <ieee80211_api.h>
#include "ieee80211_vap.h"
#include "ieee80211_sm.h"
#include "ieee80211_resmgr_oc_scheduler_priv.h"
#include "ieee80211_resmgr_oc_scheduler_api.h"

/*
 * resource manager SM is not compiled in by default.
 */ 
#ifndef UMAC_SUPPORT_RESMGR_SM
#define UMAC_SUPPORT_RESMGR_SM 0
#endif

/*
 * State machine states
 */
typedef enum {
    IEEE80211_RESMGR_STATE_IDLE = 0,
    IEEE80211_RESMGR_STATE_VAP_PRESTART,
    IEEE80211_RESMGR_STATE_VAP_STARTING,
    IEEE80211_RESMGR_STATE_BSSCHAN,
    IEEE80211_RESMGR_STATE_CANPAUSE,
    IEEE80211_RESMGR_STATE_PAUSING,
    IEEE80211_RESMGR_STATE_OFFCHAN,
    IEEE80211_RESMGR_STATE_RESUMING,
    IEEE80211_RESMGR_STATE_CHANSWITCH,
    IEEE80211_RESMGR_STATE_OC_CHANSWITCH,
    IEEE80211_RESMGR_STATE_CHANSCHED,
} ieee80211_resmgr_state;

/*
 * State machine events.
 * Posted as part of API calls to Resource Manager
 */
typedef enum {
    IEEE80211_RESMGR_EVENT_NONE,
    IEEE80211_RESMGR_EVENT_VAP_START_REQUEST,
    IEEE80211_RESMGR_EVENT_VAP_UP,
    IEEE80211_RESMGR_EVENT_VAP_STOPPED,
    IEEE80211_RESMGR_EVENT_VAP_PAUSE_COMPLETE,
    IEEE80211_RESMGR_EVENT_VAP_PAUSE_FAIL,
    IEEE80211_RESMGR_EVENT_VAP_RESUME_COMPLETE,
    IEEE80211_RESMGR_EVENT_OFFCHAN_REQUEST,
    IEEE80211_RESMGR_EVENT_BSSCHAN_REQUEST,
    IEEE80211_RESMGR_EVENT_VAP_CANPAUSE_TIMER,
    IEEE80211_RESMGR_EVENT_CHAN_SWITCH_REQUEST,
    IEEE80211_RESMGR_EVENT_CSA_COMPLETE,
    IEEE80211_RESMGR_EVENT_SCHED_BSS_REQUEST,
    IEEE80211_RESMGR_EVENT_TSF_TIMER_EXPIRED,
    IEEE80211_RESMGR_EVENT_TSF_TIMER_CHANGED,
    IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_START_REQUEST,
    IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_STOP_REQUEST,
    IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_SWITCH_CHAN,
    IEEE80211_RESMGR_EVENT_VAP_AUTH_COMPLETE,
} ieee80211_resmgr_event;

/* 
 * Resource manager API's would post events to the state machine and NOT
 * complete processing in the same thread.
 * Events include complete notifications from other modules. 
 * Resource manager can only be running ONE thread.
 * Notifications are sent to the registered modules as part of event processing.
 */

typedef enum {
    VAP_STOPPED = 1,     /* Vap is still down */ /* XXX  do not change the order */
    VAP_SLEEP,           /* Vap is in network sleep (only for STA vap)*/
    VAP_STARTING,        /* Vap has just started */
    VAP_ACTIVE,          /* Vap is up and running */
    VAP_PAUSE_REQUESTED, /* Vap pause is requested */
    VAP_PAUSED,          /* Vap is paused */
    VAP_SWITCHING,       /* Vap is switching channel */
} ieee80211_resmgr_vapstate_t;

typedef enum {
    VAP_PM_FULL_SLEEP = 1, /* Vap is doing Full Sleep*/ 
    VAP_PM_NW_SLEEP,       /* Vap is in network sleep (only for STA vap)*/
    VAP_PM_ACTIVE,         /* Vap is active */
} ieee80211_resmgr_vap_pmstate_t;

struct ieee80211_resmgr_sm_event {
    ieee80211_vap_t          vap;
    struct ieee80211_node    *ni_bss_node;
    struct ieee80211_ath_channel *chan;
    u_int16_t                max_time;
    u_int32_t                max_dwell_time;
};
typedef struct ieee80211_resmgr_sm_event *ieee80211_resmgr_sm_event_t;

typedef struct ieee80211_resmgr_vap_priv {
    ieee80211_resmgr_t                resmgr;
    ieee80211_resmgr_vapstate_t       state;
    ieee80211_resmgr_vap_pmstate_t    pm_state;
    struct ieee80211_ath_channel          *chan;
    struct ieee80211_resmgr_sm_event  def_event;    
    u_int16_t                         def_event_type;    
    u_int16_t                         resume_notify:1;
    u_int32_t                         chan_freq;
    u_int64_t                         chan_flags;
    bool			      lmac_vap_paused;
    struct ieee80211_resmgr_oc_sched_vap_data   sched_data;
} *ieee80211_vapentry_t;

typedef struct ieee80211_resmgr_scan {
    struct ieee80211_ath_channel          *chan;
    struct ieee80211_ath_channel          *scan_chan;
    struct ieee80211_resmgr_sm_event  def_event;
    u_int16_t                         def_event_type;    
    os_timer_t                        canpause_timer;
    ieee80211_resmgr_oc_sched_req_t   oc_sched_req;
    u_int32_t                         canpause_timeout;
#define CANPAUSE_CHECK_INTERVAL   20              /* 20 ms */ 
#define CANPAUSE_INFINITE_WAIT     1
} ieee80211_resmgr_scan_t;

typedef spinlock_t ieee80211_resmgr_lock_t;

/* Resource Manager structure */
struct ieee80211_resmgr {
#if UMAC_SUPPORT_RESMGR
    /* the function indirection SHALL be the first fileds */
    struct ieee80211_resmgr_func_table              resmgr_func_table; 
#endif
    struct ieee80211com                             *ic;                           /* Back pointer to ic */
    ieee80211_resmgr_mode                           mode;
    ieee80211_resmgr_lock_t                         rm_lock;                       /* Lock ensures that only one thread runs res mgr at a time */
    /* Add one lock for notif_handler register/deregister operation, notif_handler will be modified by 
       wlan_scan_run() call and cause schedule-while-atomic in split driver. */
    spinlock_t                                      rm_handler_lock;
    ieee80211_hsm_t                                 hsm_handle;                    /* HSM Handle */
    ieee80211_resmgr_notification_handler           notif_handler[IEEE80211_MAX_RESMGR_EVENT_HANDLERS];
    void                                            *notif_handler_arg[IEEE80211_MAX_RESMGR_EVENT_HANDLERS];
    ieee80211_resmgr_scan_t                         scandata;
    int                                             pm_state;                      /* power state of the radio */ 
    u_int16_t                                       is_scan_in_progress:1,
                                                    cancel_pause:1,
                                                    pause_failed:1,
						    						oc_scan_req_pending:1, /* a scan request is pending with scheduler */
                                                    oc_scheduler_active:1;           /* if oc scheuduler running */
    ieee80211_resmgr_oc_scheduler_t                 oc_scheduler;                  /* Off-channel scheduler state */
    ieee80211_resmgr_oc_scheduler_operation_data_t  current_scheduled_operation;
    ieee80211_resmgr_oc_scheduler_operation_data_t  previous_scheduled_operation;
};

#define IEEE80211_RESMGR_LOCK_INIT(_rm)             spin_lock_init(&(_rm)->rm_lock)
#define IEEE80211_RESMGR_LOCK_DESTROY(_rm)
#define IEEE80211_RESMGR_LOCK(_rm)                  spin_lock(&(_rm)->rm_lock)
#define IEEE80211_RESMGR_UNLOCK(_rm)                spin_unlock(&(_rm)->rm_lock)

/* Only two VAPs supported for now */
#define IEEE80211_RESMGR_REQID     0x5555

#define IEEE80211_RESMGR_NOTIFICATION(_resmgr,_notif)  do {                 \
        int i;                                                              \
        for(i = 0;i < IEEE80211_MAX_RESMGR_EVENT_HANDLERS; ++i) {                  \
            if (_resmgr->notif_handler[i]) {                                \
                (* _resmgr->notif_handler[i])                               \
                    (_resmgr, _notif, _resmgr->notif_handler_arg[i]);       \
             }                                                              \
        }                                                                   \
    } while(0)

struct vap_iter_check_state_params {
    struct ieee80211_ath_channel *chan;
    u_int8_t vaps_paused;
    u_int8_t vaps_pause_requested;
    u_int8_t vaps_active;
    u_int8_t vap_starting;
    u_int8_t vaps_switching;
    u_int8_t vaps_running;
    u_int8_t num_sta_vaps_active;
    u_int8_t num_ap_vaps_active;
    u_int8_t num_ibss_vaps_active;
};

struct vap_iter_check_channel_params {
    struct ieee80211_ath_channel *chan;
    bool oc_multichannel_enabled;  /* if multi channel is enabled */
    bool vap_needs_scheduler; /* one of the vap needs scheduler to scan */  
    bool oc_scheduler_state;  /* the final state of the scheduler */
};

#if UMAC_SUPPORT_RESMGR_SM

/* Get current state of all VAPs */
static INLINE void ieee80211_vap_iter_check_state(void *arg, struct ieee80211vap *vap)
{
    struct vap_iter_check_state_params *params = (struct vap_iter_check_state_params *)arg;
    ieee80211_resmgr_vap_priv_t resmgr_vap = ieee80211vap_get_resmgr(vap);

    if (!params || (resmgr_vap->state == VAP_STOPPED))
        return;

    if (vap->iv_opmode == IEEE80211_M_STA) {
         params->num_sta_vaps_active++;
    } else if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
         params->num_ap_vaps_active++;
    } else if(vap->iv_opmode == IEEE80211_M_IBSS) {
         params->num_ibss_vaps_active++;
    }

    if (!params->chan || params->chan->ic_freq == resmgr_vap->chan->ic_freq) {
        if (resmgr_vap->state == VAP_PAUSED){
            params->vaps_paused++;
            params->vaps_running++;
	}else if (resmgr_vap->state == VAP_PAUSE_REQUESTED) {
	    	params->vaps_pause_requested++;
            params->vaps_running++;
        } else if (resmgr_vap->state == VAP_ACTIVE) {
            params->vaps_active++;
            params->vaps_running++;
        } else if (resmgr_vap->state == VAP_STARTING) {
            params->vap_starting++;
        } else if (resmgr_vap->state == VAP_SWITCHING) {
            params->vaps_switching++;
            params->vaps_running++;
        }
    }
}

static INLINE int ieee80211_resmgr_sm_dispatch(ieee80211_resmgr_t resmgr,u_int16_t event, 
                                                                   struct ieee80211_resmgr_sm_event *event_data) 
{
    if (event_data->vap && event_data->vap->iv_ic->ic_reg_parm.resmgrSyncSm == 0)  { 
        /* NOTE: there is potential race condition where the VAP could be deleted. */
        if ((event_data->vap->iv_node_count == 0) || (ieee80211_vap_deleted_is_set(event_data->vap))) {
            /* 
             * VAP will be freed right after we return  from here. So do not send the event to  the
             * resource manage if the resmgr is running asynchronously. drop it here.   
             * EV 71462 has been filed to find a proper fix .
             * this condition hits on win7 for monitor mode.
             */
             printk("%s: ****  node count is 0 , drop the event %d **** \n",__func__,event );
             return EOK;
        }
        event_data->ni_bss_node = ieee80211_try_ref_bss_node(event_data->vap);
    } else {
        /* if  SM is running synchronosly , do not need vap/bss node ref */
        event_data->ni_bss_node=NULL;
    }
    ieee80211_sm_dispatch(resmgr->hsm_handle,event,sizeof(struct ieee80211_resmgr_sm_event),event_data);
    return EOK;
}

static INLINE void ieee80211_resmgr_off_chan_scheduler(ieee80211_resmgr_t resmgr, 
    u_int16_t event_type)
{
#if UMAC_SUPPORT_RESMGR_OC_SCHED
    switch (event_type) {
    case IEEE80211_RESMGR_EVENT_TSF_TIMER_EXPIRED:
        ieee80211_resmgr_oc_scheduler(resmgr, OFF_CHAN_SCHED_TSF_TIMER_EXPIRE);
        break;
    case IEEE80211_RESMGR_EVENT_TSF_TIMER_CHANGED:
        ieee80211_resmgr_oc_scheduler(resmgr, OFF_CHAN_SCHED_TSF_TIMER_CHANGE);
        break;
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_START_REQUEST:
        ieee80211_resmgr_oc_scheduler(resmgr, OFF_CHAN_SCHED_START_REQUEST);
        break;
    case IEEE80211_RESMGR_EVENT_OFF_CHAN_SCHED_STOP_REQUEST:
        ieee80211_resmgr_oc_scheduler(resmgr, OFF_CHAN_SCHED_STOP_REQUEST);
        break;
    default:
        break;
    }   
#endif
}


/*
 *  @Allocate a schedule request for submission to the Resource Manager off-channel scheduler. Upon success,
 *  a handle for the request is returned. Otherwise, NULL is returned to indicate a failure
 *  to allocate the request.
 *  ARGS:
 *      resmgr          :   handle to resource manager.
 *      vap             :   handle to the vap.
 *      start_tsf       :   Value of starting TSF for scheduling window.
 *      duration_msec   :   Air-time duration in milliseconds (msec).
 *      interval_msec   :   Air-time period or interval in milliseconds (msec).
 *      priority        :   Off-Channel scheduler priority, i.e. High or Low.
 *      chan            :   Channel information
 *      periodic        :   Is request a one-shot or reoccurring type request
 */
ieee80211_resmgr_oc_sched_req_handle_t ieee80211_resmgr_off_chan_sched_req_alloc(ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap, u_int32_t duration_msec, u_int32_t interval_msec,
    ieee80211_resmgr_oc_sched_prio_t priority, bool periodic);

/*
 *  @Start or initiate the scheduling of air time for this schedule request handle.
 *  ARGS:
 *      resmgr          :   handle to resource manager
 *      req_handle      :   handle to off-channel scheduler request
 */
int ieee80211_resmgr_off_chan_sched_req_start(ieee80211_resmgr_t resmgr, 
        ieee80211_resmgr_oc_sched_req_handle_t req_handle);

/*
 *  @Stop or cease the scheduling of air time for this schedule request handle.
 *  ARGS:
 *      resmgr          :   handle to resource manager.
 *      req_handle      :   handle to off-channel scheduler request.
 */

int ieee80211_resmgr_off_chan_sched_req_stop(ieee80211_resmgr_t resmgr,
        ieee80211_resmgr_oc_sched_req_handle_t req_handle);

/*
 *  @Free a previously allocated schedule request handle.
 *  ARGS:
 *      resmgr          :   handle to resource manager.
 *      req_handle      :   handle to off-channel scheduler request.
 */
void ieee80211_resmgr_off_chan_sched_req_free(ieee80211_resmgr_t resmgr,
    ieee80211_resmgr_oc_sched_req_handle_t req_handle);



int ieee80211_resmgr_singlechan_sm_create(ieee80211_resmgr_t resmgr);

static INLINE int ieee80211_resmgr_sm_create(ieee80211_resmgr_t resmgr)
{
    return(ieee80211_resmgr_singlechan_sm_create(resmgr));
}

void ieee80211_resmgr_singlechan_sm_delete(ieee80211_resmgr_t resmgr);

static INLINE void ieee80211_resmgr_sm_delete(ieee80211_resmgr_t resmgr)
{
    ieee80211_resmgr_singlechan_sm_delete(resmgr);
}

#else

#define ieee80211_resmgr_sm_create(resmgr)   (EOK)
#define ieee80211_resmgr_sm_delete(resmgr)    /**/
#define ieee80211_resmgr_sm_dispatch(resmgr,event,event_data)  (EINVAL) 
static INLINE void ieee80211_vap_iter_check_state(void *arg, struct ieee80211vap *vap){}

#endif

#endif

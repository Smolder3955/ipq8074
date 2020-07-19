/*
* Copyright (c) 2011, 2018-2019 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
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

#include "ieee80211_var.h"
#include "ieee80211_sme_api.h"
#include "ieee80211_sm.h"
#if UMAC_SUPPORT_IBSS
typedef enum {
    IEEE80211_IBSS_STATE_INIT, 
    IEEE80211_IBSS_STATE_SCAN, 
    IEEE80211_IBSS_STATE_SCAN_CANCELLING, 
    IEEE80211_IBSS_STATE_JOIN, 
    IEEE80211_IBSS_STATE_CREATE, 
    IEEE80211_IBSS_STATE_RUN, 
} ieee80211_connection_state; 

typedef enum {
    IEEE80211_IBSS_EVENT_NONE,
    IEEE80211_IBSS_EVENT_CONNECT_REQUEST, 
    IEEE80211_IBSS_EVENT_CREATE_REQUEST, 
    IEEE80211_IBSS_EVENT_DISCONNECT_REQUEST, 
    IEEE80211_IBSS_EVENT_JOIN_SUCCESS, 
    IEEE80211_IBSS_EVENT_JOIN_FAIL, 
    IEEE80211_IBSS_EVENT_SCAN_START,
    IEEE80211_IBSS_EVENT_SCAN_CANCELLED,
    IEEE80211_IBSS_EVENT_SCAN_END,
    IEEE80211_IBSS_EVENT_SCAN_RESTART
} ieee80211_connection_event; 

#define MAX_TSFSYNC_TIME        6000     /* msec */
#define MAX_QUEUED_EVENTS         16
#define MAX_SCAN_ATTEMPTS          3 
#define SCAN_CACHE_VALID_TIME  60000 /* 60 seconds */ 
#define SCAN_RESTART_TIME       3000

static OS_TIMER_FUNC(ibss_sm_timer_handler);

struct _wlan_ibss_sm {
    osdev_t                     os_handle;
    wlan_if_t                   vap_handle;
    ieee80211_hsm_t             hsm_handle; 
    wlan_scan_entry_t           scan_entry;
    wlan_ibss_sm_event_handler  sm_evhandler;
    wlan_ibss_sm_event_disconnect_reason   last_failure;
    os_if_t                     osif;
    os_timer_t                  sm_timer; /* generic timer */
    u_int16_t                   max_tsfsync_time;   /* max time to wait for a beacon in msec */
    IEEE80211_SCAN_REQUESTOR    scan_requestor; /* requestor id assigned by scan module */
    IEEE80211_SCAN_ID           scan_id;        /* scan id assigned by the scan scheduler */
    u_int8_t                    timeout_event;      /* event to dispacth when timer expires */
    u_int8_t                    scan_attempt;       /* current scan attempt */
    u_int32_t                   is_stop_requested:1,
                                sync_stop_requested:1,    /* SM is being stopped synchronously */
                                is_running:1,
                                is_join:1;
};

static const char*    ibss_event_names[] = {
    /* IEEE80211_IBSS_EVENT_NONE               */ "NONE",
    /* IEEE80211_IBSS_EVENT_CONNECT_REQUEST    */ "CONNECT_REQUEST",
    /* IEEE80211_IBSS_EVENT_CREATE_REQUEST     */ "CREATE_REQUEST",
    /* IEEE80211_IBSS_EVENT_DISCONNECT_REQUEST */ "DISCONNECT_REQUEST",
    /* IEEE80211_IBSS_EVENT_JOIN_SUCCESS       */ "JOIN_SUCCESS",
    /* IEEE80211_IBSS_EVENT_JOIN_FAIL          */ "JOIN_FAIL",
    /* IEEE80211_IBSS_EVENT_SCAN_START         */ "SCAN_START",
    /* IEEE80211_IBSS_EVENT_SCAN_CANCELLED     */ "SCAN_CANCELLED",
    /* IEEE80211_IBSS_EVENT_SCAN_END           */ "SCAN_END",
    /* IEEE80211_IBSS_EVENT_SCAN_RESTART       */ "SCAN_RESTART"
    };
    
static void ieee80211_ibss_sm_debug_print (void *ctx,const char *fmt,...) 
{
#if ATH_DEBUG
    wlan_ibss_sm_t sm = (wlan_ibss_sm_t) ctx;
#endif
    char tmp_buf[256];
    va_list ap;
     
    va_start(ap, fmt);
    vsnprintf (tmp_buf,256, fmt, ap);
    va_end(ap);
    IEEE80211_DPRINTF(sm->vap_handle, IEEE80211_MSG_STATE, "%s", tmp_buf);
}
/*
 * 802.11 station ibssiation state machine implementation.
 */
static void ieee80211_send_event(wlan_ibss_sm_t sm, wlan_ibss_sm_event_type type,wlan_ibss_sm_event_disconnect_reason reason)
{
    wlan_ibss_sm_event sm_event;
    sm_event.event = type;
    sm_event.reason = reason ;
    (* sm->sm_evhandler) (sm, sm->osif, &sm_event);
}

/*
 * 802.11 station connection state machine implementation.
 */

static void ieee80211_ibss_sm_scan_evhandler(wlan_if_t vaphandle, ieee80211_scan_event *event, void *arg)  
{
    wlan_ibss_sm_t sm = (wlan_ibss_sm_t) arg; 

    IEEE80211_DPRINTF(vaphandle, IEEE80211_MSG_SCAN, "%s evt_scan_id %d sm scan_id %d event %d reason %d \n", 
                      __func__, event->scan_id, sm->scan_id, event->type, event->reason);

#if ATH_SUPPORT_MULTIPLE_SCANS
    /*
     * Ignore notifications received due to scans requested by other modules
     * and handle new event IEEE80211_SCAN_DEQUEUED.
     */
    ASSERT(0);

    if (sm->scan_id != event->scan_id)
        return;
#endif    /* ATH_SUPPORT_MULTIPLE_SCANS */

    if ((event->type == IEEE80211_SCAN_COMPLETED) || (event->type == IEEE80211_SCAN_DEQUEUED)) {
        if (event->reason == IEEE80211_REASON_COMPLETED) {
            ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_IBSS_EVENT_SCAN_END, 0, NULL); 
        } else {
            ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_IBSS_EVENT_SCAN_CANCELLED, 0, NULL); 
        }
    } else if (event->type == IEEE80211_SCAN_STARTED) {
       ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_IBSS_EVENT_SCAN_START, 0, NULL); 
    }
}

/* 
 * different state related functions.
 */


/*
 * INIT
 */
static void ieee80211_ibss_state_init_entry(void *ctx) 
{
    wlan_ibss_sm_t sm = (wlan_ibss_sm_t) ctx;
    u_int32_t _is_stop_requested = 0;

    if (sm->is_join) {
       sm->is_join=0;
       wlan_mlme_connection_reset(sm->vap_handle, 0);
    }
    if (sm->scan_entry) {
        wlan_scan_entry_remove_reference(sm->scan_entry);
        sm->scan_entry=NULL;
    }
    /*                 
     * force the vap to stop, this will flush the HW and
     * close the ath device if this is the last vap.
     */
    wlan_mlme_stop_bss(sm->vap_handle, WLAN_MLME_STOP_BSS_F_FORCE_STOP_RESET);
  
    _is_stop_requested = sm->is_stop_requested;
    sm->is_stop_requested = 0;
    sm->is_running = 0;
    sm->sync_stop_requested = 0;
    sm->scan_attempt = 0;

    if (_is_stop_requested ) {
	    ieee80211_send_event(sm, WLAN_IBSS_SM_EVENT_DISCONNECT, 0);
    } else {
	   ieee80211_send_event(sm, WLAN_IBSS_SM_EVENT_FAILED, sm->last_failure);
    }
}

static void ieee80211_ibss_state_init_exit(void *ctx) 
{
    /* NONE */
}

static bool ieee80211_ibss_state_init_event(void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data) 
{
    wlan_ibss_sm_t sm = (wlan_ibss_sm_t) ctx;
    systime_t last_scan_time; 
    u_int32_t elapsed;
    ieee80211_scan_info last_scan_info;
    int retval=0;

    switch(event) {
    case IEEE80211_IBSS_EVENT_CONNECT_REQUEST:
        last_scan_time = wlan_get_last_scan_time(sm->vap_handle);
        wlan_get_last_scan_info(sm->vap_handle, &last_scan_info);
        elapsed = CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP() - last_scan_time);
        /* 
         * if the scan cache is hot then build the candidate ap list using the scan cache
         * and move to the connecting state. 
         * if the scan cache is stale (or) last scan was canelled then move to the scan state.
         */
        if (elapsed > SCAN_CACHE_VALID_TIME || last_scan_info.cancelled) {
            ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_IBSS_STATE_SCAN); 
        } else {
            u_int32_t    maximum_scan_entry_age;

            /*
             * build the candidate ap list.
             */
            maximum_scan_entry_age = 
                ieee80211_mlme_maximum_scan_entry_age(sm->vap_handle,
                                                      wlan_get_last_full_scan_time(sm->vap_handle));
            wlan_candidate_list_build(sm->vap_handle,TRUE, NULL, maximum_scan_entry_age);
            if (wlan_candidate_list_count(sm->vap_handle)) {
    	        sm->scan_entry = wlan_candidate_list_get(sm->vap_handle,0); /* only try the very first entry */
                wlan_scan_entry_add_reference(sm->scan_entry);
                ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_IBSS_STATE_JOIN); 
            } else {
                ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_IBSS_STATE_SCAN); 
            }
            wlan_candidate_list_free(sm->vap_handle);
        }
        return true;
        break;
    case IEEE80211_IBSS_EVENT_CREATE_REQUEST:
        retval = wlan_mlme_start_bss(sm->vap_handle,0);
       /*
        * if resource manager is present, it will return EBUSY.
        * treat EBUSY as a success.
        */

        if (retval !=0 && retval != EBUSY) {
            IEEE80211_DPRINTF(sm->vap_handle, IEEE80211_MSG_STATE, "%s : ibss create failed \n", __func__);
            sm->is_running = 0;
         } else {
           /* create succefull */
            if (retval == EBUSY) {
                 /* create will complete asychronously , will deliver  success/failure of creation via sm_join_complete callback*/
                ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_IBSS_STATE_CREATE); 
            } else {
                 /* create completed synchronously */
                ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_IBSS_STATE_RUN); 
            }
         }
        return true;
        break;
    case IEEE80211_IBSS_EVENT_DISCONNECT_REQUEST:
        return true;

    default:
        return false;
    }
    return false;
}

/*
 * SCAN
 */
static void ieee80211_ibss_state_scan_entry(void *ctx) 
{
    wlan_ibss_sm_t sm = (wlan_ibss_sm_t) ctx;
    ieee80211_scan_params scan_params; 
    ieee80211_ssid    *ssid_list;
    int               n_ssid;

    ssid_list = OS_MALLOC(sm->vap_handle->iv_ic->ic_osdev, sizeof(ieee80211_ssid) * IEEE80211_SCAN_MAX_SSID,
                          GFP_KERNEL);
    if (ssid_list == NULL) {
        return;
    }

    /*
     * if there is a bg scan in progress.
     * convert it into an fg scan (TO-DO).
     */
    OS_MEMZERO(&scan_params,sizeof(ieee80211_scan_params));
    n_ssid = ieee80211_get_desired_ssidlist(sm->vap_handle, ssid_list, IEEE80211_SCAN_MAX_SSID);
    wlan_set_default_scan_parameters(sm->vap_handle, &scan_params,IEEE80211_M_STA,true,true,false,true,n_ssid,ssid_list,0);
    scan_params.min_dwell_time_active = scan_params.max_dwell_time_active = 100;
    scan_params.type = IEEE80211_SCAN_FOREGROUND;
    scan_params.flags = IEEE80211_SCAN_ALLBANDS ;
    switch (sm->vap_handle->iv_des_mode)
    {
        case IEEE80211_MODE_11A:
        case IEEE80211_MODE_TURBO_A:
        case IEEE80211_MODE_11NA_HT20:
        case IEEE80211_MODE_11NA_HT40PLUS:
        case IEEE80211_MODE_11NA_HT40MINUS:
        case IEEE80211_MODE_11NA_HT40:
        case IEEE80211_MODE_11AC_VHT20: 
        case IEEE80211_MODE_11AC_VHT40PLUS: 
        case IEEE80211_MODE_11AC_VHT40MINUS:
        case IEEE80211_MODE_11AC_VHT40:
        case IEEE80211_MODE_11AC_VHT80:
            /* Initiating passive scan will wait for beacon on 
              particlar channel before broadcasting probe,
              as 5G also involves DFS channel and sending probe 
              wihtout sensing activity leads to test case failure*/
            scan_params.flags |= IEEE80211_SCAN_PASSIVE;
            scan_params.min_dwell_time_passive = 200;
            scan_params.max_dwell_time_passive = 300;
            break;
        case IEEE80211_MODE_11B:
        case IEEE80211_MODE_11G:
        case IEEE80211_MODE_TURBO_G:
        case IEEE80211_MODE_11NG_HT20:
        case IEEE80211_MODE_11NG_HT40PLUS:
        case IEEE80211_MODE_11NG_HT40MINUS:
        case IEEE80211_MODE_11NG_HT40:
        default:
            scan_params.flags |= IEEE80211_SCAN_ACTIVE;
            break;
    }

    scan_params.flags |= IEEE80211_SCAN_ADD_BCAST_PROBE;
    if (wlan_scan_start(sm->vap_handle, 
                        &scan_params,
                        sm->scan_requestor,
                        IEEE80211_SCAN_PRIORITY_MEDIUM,
                        &(sm->scan_id)) != EOK) {
        IEEE80211_DPRINTF(sm->vap_handle, IEEE80211_MSG_STATE, "%s scan_start failed\n",__func__);
    }

    OS_FREE(ssid_list);
}

static bool ieee80211_ibss_state_scan_event(void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data) 
{
    wlan_ibss_sm_t    sm = (wlan_ibss_sm_t) ctx;
    u_int32_t         maximum_scan_entry_age;

    switch(event) {
    case IEEE80211_IBSS_EVENT_SCAN_END:
        /*
         * build the candidate ap list.
         */
        maximum_scan_entry_age = 
            ieee80211_mlme_maximum_scan_entry_age(sm->vap_handle,
                                                  wlan_get_last_full_scan_time(sm->vap_handle));
        wlan_candidate_list_build(sm->vap_handle,TRUE,NULL, maximum_scan_entry_age);
        if (wlan_candidate_list_count(sm->vap_handle)) {
            sm->scan_entry = wlan_candidate_list_get(sm->vap_handle,0); /* only try the very first entry */
            wlan_scan_entry_add_reference(sm->scan_entry);
            ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_IBSS_STATE_JOIN); 
        } else {
            if (++sm->scan_attempt  == MAX_SCAN_ATTEMPTS) {
                /*
                 * No candidate APs found.
                 */
                IEEE80211_DPRINTF(sm->vap_handle, IEEE80211_MSG_STATE, "%s no candidate APs found\n",__func__);
                ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_IBSS_STATE_INIT); 
            } else {
                ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_IBSS_STATE_SCAN); 
            }
        }
        wlan_candidate_list_free(sm->vap_handle);
        break;


    case IEEE80211_IBSS_EVENT_DISCONNECT_REQUEST: 
        if (wlan_scan_cancel(sm->vap_handle,
                             sm->scan_requestor,
                             IEEE80211_VAP_SCAN,
                             sm->sync_stop_requested ? IEEE80211_SCAN_CANCEL_SYNC : IEEE80211_SCAN_CANCEL_ASYNC) == EOK) { 
	   /* cancel is succenfully initiated */
           ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_IBSS_STATE_SCAN_CANCELLING); 
        } else {
	   /* already cancelled  */
           ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_IBSS_STATE_INIT); 
        }
        return true;
        break;

    case IEEE80211_IBSS_EVENT_SCAN_CANCELLED:
        /* 
         * somebody  else has cancelled our scan.
         * strt a timer if nobody restarts the scan , 
         * we will restart the scan when the timer fires.
         */
         sm->timeout_event = IEEE80211_IBSS_EVENT_SCAN_RESTART;
         OS_SET_TIMER(&sm->sm_timer,SCAN_RESTART_TIME);
        return true;
        break;

    case IEEE80211_IBSS_EVENT_SCAN_START:
        /*
         * clear the restart timer. the timer may have been armed if
         * we received CANCELLED event before.
         */
         sm->timeout_event = IEEE80211_IBSS_EVENT_NONE;
         OS_CANCEL_TIMER(&sm->sm_timer);
        return true;
        break;

    case IEEE80211_IBSS_EVENT_SCAN_RESTART:
        /*
         * nobody has restarted the scan after cancelling our scan.
         * lests restart.
         */
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_IBSS_STATE_SCAN); 
        return true;
        break;

    default:
        return false;
        break;
    }
    return false;
}

static bool ieee80211_ibss_state_scan_cancelling_event(void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data) 
{
    wlan_ibss_sm_t sm = (wlan_ibss_sm_t) ctx;
    switch(event) {

    case IEEE80211_IBSS_EVENT_SCAN_END: 
    case IEEE80211_IBSS_EVENT_SCAN_CANCELLED:
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_IBSS_STATE_INIT); 
        return true;
        break;
    case IEEE80211_IBSS_EVENT_DISCONNECT_REQUEST:
        if (sm->sync_stop_requested) {
            /*
             * synchronous stop requested. A scan cancel request is in progress and scanner is trying to
             * cancel scan asynchronously.
             * do not wait for scan async thread to complete scan,issue
             * a sync cancel request.
             */
    
           wlan_scan_cancel(sm->vap_handle,
                            sm->scan_requestor,
                            IEEE80211_VAP_SCAN,
                            IEEE80211_SCAN_CANCEL_SYNC);
        }
        return true;
        break;
    default:
        return false;
        break;
    }
    return false;
}

/*
 *JOIN 
 */
static void ieee80211_ibss_state_join_entry(void *ctx) 
{
    wlan_ibss_sm_t sm = (wlan_ibss_sm_t) ctx;
    sm->is_join=1;
    if (wlan_mlme_join_adhoc(sm->vap_handle, sm->scan_entry, sm->max_tsfsync_time) != 0 ) {
        IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s: join_infra failed \n",__func__);
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_IBSS_EVENT_JOIN_FAIL,0,NULL);
        return;
    }
}

static void ieee80211_ibss_state_join_exit(void *ctx) 
{
    wlan_ibss_sm_t sm = (wlan_ibss_sm_t) ctx;
    wlan_mlme_cancel(sm->vap_handle);/* cancel any pending mlme join req */
}

static bool ieee80211_ibss_state_join_event(void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data) 
{
    wlan_ibss_sm_t sm = (wlan_ibss_sm_t) ctx;
    switch(event) {
    case IEEE80211_IBSS_EVENT_JOIN_SUCCESS:
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_IBSS_STATE_RUN); 
        return true;
        break;

    case IEEE80211_IBSS_EVENT_JOIN_FAIL:
        sm->last_failure = WLAN_IBSS_SM_REASON_JOIN_TIMEOUT;
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_IBSS_STATE_INIT); 
        return true;
        break;

    case IEEE80211_IBSS_EVENT_DISCONNECT_REQUEST:
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_IBSS_STATE_INIT); 
        return true;
        break;

    default:
        return false;
    }
    return false;
}

/*
 * CREATE 
 */
static bool ieee80211_ibss_state_create_event(void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data) 
{
    wlan_ibss_sm_t sm = (wlan_ibss_sm_t) ctx;

    switch(event) {
    case IEEE80211_IBSS_EVENT_JOIN_SUCCESS:
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_IBSS_STATE_RUN); 
        return true;
    case IEEE80211_IBSS_EVENT_JOIN_FAIL:
    case IEEE80211_IBSS_EVENT_DISCONNECT_REQUEST:
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_IBSS_STATE_INIT); 
        return true;
    default:
        return false;
    }
    return false;

}

/*
 *RUN 
 */
static void ieee80211_ibss_state_run_entry(void *ctx) 
{
    wlan_ibss_sm_t sm = (wlan_ibss_sm_t) ctx;
    wlan_ibss_sm_event sm_event;

    sm->scan_attempt = 0;
    wlan_mlme_connection_up(sm->vap_handle);
    sm_event.event = WLAN_IBSS_SM_EVENT_SUCCESS;
    sm_event.reason = WLAN_IBSS_SM_REASON_NONE;
    (* sm->sm_evhandler) (sm, sm->osif, &sm_event);
}

static void ieee80211_ibss_state_run_exit(void *ctx) 
{
    u_int8_t  bssid[IEEE80211_ADDR_LEN];
    wlan_ibss_sm_t sm = (wlan_ibss_sm_t) ctx;
    wlan_mlme_connection_down(sm->vap_handle);
   /* delete any unicast key installed */
   wlan_vap_get_bssid(sm->vap_handle,bssid);
   wlan_del_key(sm->vap_handle,IEEE80211_KEYIX_NONE,bssid); 
   wlan_mlme_stop_bss(sm->vap_handle,0);
}



static bool ieee80211_ibss_state_run_event(void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data) 
{
    wlan_ibss_sm_t sm = (wlan_ibss_sm_t) ctx;

    switch(event) {

    case IEEE80211_IBSS_EVENT_DISCONNECT_REQUEST:
        /* no need to send disibss usully caaled while romaing */
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_IBSS_STATE_INIT); 
        return true;
        break;
    default:
        return false;
    }
    return false;
}

ieee80211_state_info ieee80211_ibss_sm_info[] = {
    { 
        (u_int8_t) IEEE80211_IBSS_STATE_INIT, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "INIT",
        ieee80211_ibss_state_init_entry,
        ieee80211_ibss_state_init_exit,
        ieee80211_ibss_state_init_event
    },
    { 
        (u_int8_t) IEEE80211_IBSS_STATE_SCAN, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "SCAN",
        ieee80211_ibss_state_scan_entry,
        NULL,
        ieee80211_ibss_state_scan_event
    },
    { 
        (u_int8_t) IEEE80211_IBSS_STATE_SCAN_CANCELLING, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "SCAN_CANCELLING",
        NULL,
        NULL,
        ieee80211_ibss_state_scan_cancelling_event
    },
    { 
        (u_int8_t) IEEE80211_IBSS_STATE_JOIN, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "JOIN",
        ieee80211_ibss_state_join_entry,
        ieee80211_ibss_state_join_exit,
        ieee80211_ibss_state_join_event
    },
    { 
        (u_int8_t) IEEE80211_IBSS_STATE_CREATE, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "CREATE",
        NULL,
        NULL,
        ieee80211_ibss_state_create_event
    },
    { 
        (u_int8_t) IEEE80211_IBSS_STATE_RUN, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "RUN",
        ieee80211_ibss_state_run_entry,
        ieee80211_ibss_state_run_exit,
        ieee80211_ibss_state_run_event
    },
};

static OS_TIMER_FUNC(ibss_sm_timer_handler)
{
    wlan_ibss_sm_t sm;

    OS_GET_TIMER_ARG(sm, wlan_ibss_sm_t);
    IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s: timed out cur state %s \n",
                      __func__, ieee80211_ibss_sm_info[ieee80211_sm_get_curstate(sm->hsm_handle)].name);
    ieee80211_sm_dispatch(sm->hsm_handle, sm->timeout_event,0,NULL); 

}

/*
 * mlme event handlers.
 */

static void sm_join_complete(os_handle_t osif, IEEE80211_STATUS status)
{
    wlan_ibss_sm_t sm = (wlan_ibss_sm_t) osif;
    if (status == IEEE80211_STATUS_SUCCESS) {
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_IBSS_EVENT_JOIN_SUCCESS,0,NULL);
    } else {
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_IBSS_EVENT_JOIN_FAIL,0,NULL);
    }
}

wlan_mlme_event_handler_table ibss_mlme_evt_handler = {
   	NULL,
   	sm_join_complete,
   	NULL,
   	NULL,
   	NULL,
   	NULL,
   	NULL,
 	NULL,
   	NULL,
   	NULL,
 	NULL,
   	NULL,
   	NULL,
};

wlan_ibss_sm_t wlan_ibss_sm_create(osdev_t oshandle, wlan_if_t vaphandle)
{
    wlan_ibss_sm_t    sm;
    int               rc;
    u_int8_t module_name[]="connection_sm";
    
    sm = (wlan_ibss_sm_t) OS_MALLOC(oshandle,sizeof(struct _wlan_ibss_sm),0);
    if (!sm) {
        return NULL;
    }
    OS_MEMZERO(sm, sizeof(struct _wlan_ibss_sm));
    sm->os_handle = oshandle;
    sm->vap_handle = vaphandle;
    sm->hsm_handle = ieee80211_sm_create(oshandle, 
                                         "ibss",
                                         (void *) sm, 
                                         IEEE80211_IBSS_STATE_INIT,
                                         ieee80211_ibss_sm_info, 
                                         sizeof(ieee80211_ibss_sm_info)/sizeof(ieee80211_state_info),
                                         MAX_QUEUED_EVENTS, 
                                         0 /* no event data */, 
                                         MESGQ_PRIORITY_HIGH,
                                         IEEE80211_HSM_ASYNCHRONOUS, /* run the SM asynchronously */
                                         ieee80211_ibss_sm_debug_print,
                                         ibss_event_names,
                                         IEEE80211_N(ibss_event_names));
    if (!sm->hsm_handle) {
        IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s : ieee80211_sm_create failed \n", __func__); 
        OS_FREE(sm);
        return NULL;
    }

    wlan_scan_get_requestor_id(vaphandle, module_name, &sm->scan_requestor); 
    sm->max_tsfsync_time =  MAX_TSFSYNC_TIME;
    OS_INIT_TIMER(oshandle, &(sm->sm_timer), ibss_sm_timer_handler, (void *)sm, QDF_TIMER_TYPE_WAKE_APPS);
       
    sm->last_failure = WLAN_IBSS_SM_REASON_NONE;
    wlan_vap_register_mlme_event_handlers(vaphandle,(os_if_t) sm, &ibss_mlme_evt_handler);

    rc = wlan_scan_register_event_handler(sm->vap_handle, ieee80211_ibss_sm_scan_evhandler, sm);
    if (rc != EOK) {
        IEEE80211_DPRINTF(sm->vap_handle, IEEE80211_MSG_ANY,
                          "%s: wlan_scan_register_event_handler() failed handler=%08p,%08p rc=%08X\n",
                          __func__, ieee80211_ibss_sm_scan_evhandler, sm, rc);
    }

    wlan_aplist_set_desired_bsstype(vaphandle,IEEE80211_M_IBSS);

    return sm;                                   
}



void  wlan_ibss_sm_delete(wlan_ibss_sm_t smhandle)
{
    int rc;

    if (smhandle->is_running) {
        IEEE80211_DPRINTF(smhandle->vap_handle,IEEE80211_MSG_STATE,"%s : can not delete while still runing \n", __func__); 
    }
    OS_CANCEL_TIMER(&(smhandle->sm_timer));
    OS_FREE_TIMER(&(smhandle->sm_timer));
    wlan_vap_register_mlme_event_handlers(smhandle->vap_handle,(os_if_t) NULL, NULL);
    rc = wlan_scan_unregister_event_handler(smhandle->vap_handle, ieee80211_ibss_sm_scan_evhandler, smhandle);
    if (rc != EOK) {
        IEEE80211_DPRINTF(smhandle->vap_handle, IEEE80211_MSG_STATE,
                          "%s: wlan_scan_unregister_event_handler() "
                          "failed handler=%08p,%08p rc=%08X\n",
                          __func__, ieee80211_ibss_sm_scan_evhandler,
                          smhandle, rc);
    }
    ieee80211_sm_delete(smhandle->hsm_handle);
    OS_FREE(smhandle);
}

void wlan_ibss_sm_register_event_handlers(wlan_ibss_sm_t smhandle, os_if_t osif,
                                            wlan_ibss_sm_event_handler sm_evhandler)
{
    smhandle->osif = osif;
    smhandle->sm_evhandler = sm_evhandler;
    
}

/*
 * start the state machine and handling the events.
 */
int wlan_ibss_sm_start(wlan_ibss_sm_t smhandle, u_int32_t flags)
{
    if ( smhandle->is_running ) {
        IEEE80211_DPRINTF(smhandle->vap_handle,IEEE80211_MSG_STATE,"%s: ibssiation SM is already running!!  \n", __func__);
        return -EINPROGRESS;
    }
  
    /* Reset HSM to INIT state. */
    ieee80211_sm_reset(smhandle->hsm_handle, IEEE80211_IBSS_STATE_INIT, NULL);
    /* mark it as running */
    smhandle->is_running = 1;
    if (flags & IEEE80211_IBSS_CREATE_NETWORK) {
       ieee80211_sm_dispatch(smhandle->hsm_handle,
                              IEEE80211_IBSS_EVENT_CREATE_REQUEST,0,NULL); 
    } else {
       ieee80211_sm_dispatch(smhandle->hsm_handle,
                              IEEE80211_IBSS_EVENT_CONNECT_REQUEST,0,NULL); 
    }
    return EOK;
}

/*
 * stop handling the events.
 */
int wlan_ibss_sm_stop(wlan_ibss_sm_t smhandle, u_int32_t flags)
{
    /*
     * return an error if it is already stopped (or)
     * there is a stop request is pending.
     */
    if (!smhandle->is_running ) {
        IEEE80211_DPRINTF(smhandle->vap_handle,IEEE80211_MSG_STATE,"%s: ibssiation SM is already stopped  !!  \n",__func__);
        return -EALREADY;
    }
    if (smhandle->is_stop_requested) {
        IEEE80211_DPRINTF(smhandle->vap_handle,IEEE80211_MSG_STATE,"%s: ibssiation SM is already being stopped !!  \n",__func__);
        return -EALREADY;
    }
    smhandle->is_stop_requested = 1;
    if (flags & IEEE80211_IBSS_SM_STOP_SYNC) {
        smhandle->sync_stop_requested=1;
        /*  
         * flush any pending events and dispatch the mesg synchronously.
         */  
        ieee80211_sm_dispatch_sync(smhandle->hsm_handle,
                                   IEEE80211_IBSS_EVENT_DISCONNECT_REQUEST, 0, NULL, true);
    }  else {
        ieee80211_sm_dispatch(smhandle->hsm_handle,
                              IEEE80211_IBSS_EVENT_DISCONNECT_REQUEST,0,NULL); 
    }
    return EOK;
}

/*
 * set a ibss ame param.
 */
int wlan_ibss_sm_set_param(wlan_ibss_sm_t smhandle, wlan_ibss_sm_param param,u_int32_t val)
{

    switch(param) {
    case PARAM_IBSS_MAX_TSFSYNC_TIME:
        smhandle->max_tsfsync_time = (u_int16_t)val;
        if (val == 0) return -EINVAL;
        break;
    case PARAM_IBSS_CURRENT_STATE:
        break;
    }

    return EOK;
}

/*
 * get an ibss ame param.
 */
u_int32_t wlan_ibss_sm_get_param(wlan_ibss_sm_t smhandle, wlan_ibss_sm_param param)
{
    u_int32_t val=0;

    switch(param) {
    case PARAM_IBSS_MAX_TSFSYNC_TIME:
        val = smhandle->max_tsfsync_time;
        break;
    case PARAM_IBSS_CURRENT_STATE:
        switch((ieee80211_connection_state)ieee80211_sm_get_curstate(smhandle->hsm_handle)) {
        case IEEE80211_IBSS_STATE_INIT:
        case IEEE80211_IBSS_STATE_JOIN:
        case IEEE80211_IBSS_STATE_CREATE:
        case IEEE80211_IBSS_STATE_SCAN_CANCELLING:
           val=WLAN_IBSS_STATE_INIT;
           break;
        case IEEE80211_IBSS_STATE_SCAN:
           val=WLAN_IBSS_STATE_SCAN;
           break;
        case IEEE80211_IBSS_STATE_RUN:
           val=WLAN_IBSS_STATE_RUN;
           break;
        }
        break;

    }

    return val;
}

#if ATH_SUPPORT_IBSS_NETLINK_NOTIFICATION
/*
 * sm_join_complete notify from OAL
 */
void wlan_ibss_sm_join_complete(wlan_ibss_sm_t smhandle, IEEE80211_STATUS status)
{
    if (status == IEEE80211_STATUS_SUCCESS) {
        ieee80211_sm_dispatch(smhandle->hsm_handle, IEEE80211_IBSS_EVENT_JOIN_SUCCESS, 0, NULL);
    } else {
        ieee80211_sm_dispatch(smhandle->hsm_handle, IEEE80211_IBSS_EVENT_JOIN_FAIL, 0, NULL);
    }
}
#endif

#endif /* UMAC_SUPPORT_IBSS */

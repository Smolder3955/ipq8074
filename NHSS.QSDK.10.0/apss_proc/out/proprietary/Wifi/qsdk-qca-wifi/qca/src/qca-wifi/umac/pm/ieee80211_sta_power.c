/*
* Copyright (c) 2011-2016,2017-2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2010, Atheros Communications Inc.
 * All Rights Reserved.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include "ieee80211_power_priv.h"
#include "ieee80211_sm.h"
#include "ieee80211_power.h"
#include "ieee80211_vap_tsf_offset.h"
#include "ieee80211_wnm.h"
#include <wlan_mlme_dp_dispatcher.h>
#include <wlan_utility.h>

#if UMAC_SUPPORT_STA_POWERSAVE

#define IEEE80211_WNMSLEEP_TIMEOUT                200 /* 200 msec */
#define IEEE80211_PWRSAVE_TIMER_INTERVAL          200 /* 200 msec */
#define IEEE80211_PWRSAVE_PENDING_TX_CHECK_INTERVAL          5 /* 5 msec */
#define IEEE80211_PS_INACTIVITYTIME               400 /* pwrsave inactivity time in sta mode (in msec) */
#define IEEE80211_MAX_POWER_STA_EVENT_HANDLERS    10 

#define IEEE80211_MAX_SLEEP_TIME                  10  /* # of beacon intervals to sleep in MAX PWRSAVE*/
#define IEEE80211_NORMAL_SLEEP_TIME               1   /* # of beacon intervals to sleep in NORMAL PWRSAVE*/
#define IEEE80211_MAX_SLEEP_ATTEMPTS              3   /* how many times to try sending null  frames before giving up*/
#define IEEE80211_BEACON_TIMEOUT                  10  /* beacon timeout in msec, used for SW assisted wake up to receive beacon */ 

typedef enum {
    IEEE80211_POWER_STA_STATE_INIT,           /* init state no powersave is enabled */ 
    IEEE80211_POWER_STA_STATE_ACTIVE,         /*  vap is active (with data) */ 
    IEEE80211_POWER_STA_STATE_PENDING_TX,     /* waiting for tx frames on HW queue for the vap to complete*/ 
    IEEE80211_POWER_STA_STATE_NULL_SENT,      /* waiting for null data completion */ 
    IEEE80211_POWER_STA_STATE_SLEEP,          /* in SLEEP state */ 
    IEEE80211_POWER_STA_STATE_PSPOLL,         /* send PS_POLL waiting for response */ 
    IEEE80211_POWER_STA_STATE_PAUSE,          /* paused state */ 
    IEEE80211_POWER_STA_STATE_PAUSE_INIT,	  /* pause init state. ##substate of  paused state */ 
    IEEE80211_POWER_STA_STATE_PAUSE_NOTIF_DELAY,  /* paused event delay state. ## substate of paused state */ 
    IEEE80211_POWER_STA_STATE_FORCE_SLEEP,    /* in Force SLEEP state */
    IEEE80211_POWER_STA_STATE_WNMREQ_SENT,    /* waiting for WNM-Sleep response */  
} ieee80211_sta_sleep_state; 

typedef enum {
    IEEE80211_POWER_STA_EVENT_START,               /* start the sleep SM*/
    IEEE80211_POWER_STA_EVENT_STOP,                /* stop the SM */
    IEEE80211_POWER_STA_EVENT_FORCE_SLEEP,         /* start sleep (do not wait for activity on vap) */
    IEEE80211_POWER_STA_EVENT_FORCE_AWAKE,         /* exit force sleep */
    IEEE80211_POWER_STA_EVENT_NO_ACTIVITY,         /* no tx/rx activity for vap */
    IEEE80211_POWER_STA_EVENT_NO_PENDING_TX,       /* no pending tx for vap */
    IEEE80211_POWER_STA_EVENT_SEND_NULL_SUCCESS,   /* send null is succesful */
    IEEE80211_POWER_STA_EVENT_SEND_NULL_FAILED,    /* send null failed */
    IEEE80211_POWER_STA_EVENT_RECV_UCAST,          /* received unicast frame (used when in PSPOLL state) */
    IEEE80211_POWER_STA_EVENT_TX,                  /* tx path received  a frame for transmit */
    IEEE80211_POWER_STA_EVENT_LAST_MCAST,          /* received last multicast */
    IEEE80211_POWER_STA_EVENT_TIM,                 /* received beacon with TIM */
    IEEE80211_POWER_STA_EVENT_DTIM,                /* received beacon frame with DTIM */
    IEEE80211_POWER_STA_EVENT_PAUSE,               /* pause request */
    IEEE80211_POWER_STA_EVENT_UNPAUSE,             /* unpause request */
    IEEE80211_POWER_STA_EVENT_SEND_KEEP_ALIVE,     /* send keep alive */
    IEEE80211_POWER_STA_EVENT_PAUSE_TIMEOUT,       /* pause timeout */
    IEEE80211_POWER_STA_EVENT_PAUSE_NOTIF_TIMEOUT, /* pause notification timeout */
         /* the following 4 event are used for the SW assisted wake up to receive beacons for the second STA in STA + STA vap case */
    IEEE80211_POWER_STA_EVENT_TBTT,                /* tbtt timer, used for SW assisted wake up to receive beacon */
    IEEE80211_POWER_STA_EVENT_TSF_CHANGED,         /* hw tsf has changed, used for SW assisted wake up to receive beacon */
    IEEE80211_POWER_STA_EVENT_BCN_TIMEOUT,         /* beacon timeout timer, used for SW assisted wake up to receive beacon */
    IEEE80211_POWER_STA_EVENT_BCN_RECVD,           /* beacon received */
    IEEE80211_POWER_STA_EVENT_WNMSLEEP_RESP_RECVD, /* WNM-sleep response received */
    IEEE80211_POWER_STA_EVENT_WNMSLEEP_SEND_TIMEOUT,
    IEEE80211_POWER_STA_EVENT_WNMSLEEP_CONFIRM_ENTER,
    IEEE80211_POWER_STA_EVENT_WNMSLEEP_CONFIRM_EXIT,
} ieee80211_sta_sleep_event; 


#define MAX_QUEUED_EVENTS  10

#ifdef ATH_USB
#define IPS_LOCK_INIT(handle) OS_USB_LOCK_INIT(&handle->ips_lock);
#define IPS_LOCK(handle)      OS_USB_LOCK(&handle->ips_lock)
#define IPS_UNLOCK(handle)    OS_USB_UNLOCK(&handle->ips_lock)
#else
#define IPS_LOCK_INIT(handle) spin_lock_init(&handle->ips_lock);
#define IPS_LOCK(handle)    spin_lock(&handle->ips_lock)
#define IPS_UNLOCK(handle)  spin_unlock(&handle->ips_lock)
#endif

#if DA_SUPPORT
static void ieee80211_sta_power_txrx_event_handler (struct wlan_objmgr_vdev *vdev, ieee80211_vap_txrx_event *event, void *arg);
#endif
static void ieee80211_sta_power_bmiss_handler(void *arg);
static void ieee80211_sta_power_update_sm(ieee80211_sta_power_t powersta);
static OS_TIMER_FUNC(ieee80211_sta_power_activity_timer);
static OS_TIMER_FUNC(ieee80211_sta_power_pause_timer);
static OS_TIMER_FUNC(ieee80211_sta_power_pause_notif_timer);
static void ieee80211_sta_power_tbtt_handler(tsftimer_handle_t h_tsftime, int arg1, void *arg2, u_int32_t curr_tsf);
#if UMAC_SUPPORT_WNM
static void ieee80211_sta_power_wnmsleep_handler(tsftimer_handle_t h_tsftime, int arg1, void *arg2, u_int32_t curr_tsf);
#endif
static void ieee80211_sta_power_tsf_changed_handler(tsftimer_handle_t h_tsftime, int arg1, void *arg2, u_int32_t curr_tsf);
static OS_TIMER_FUNC(ieee80211_sta_power_bcn_timeout_event_timer);
#if UMAC_SUPPORT_WNM
static OS_TIMER_FUNC(ieee80211_sta_power_wnmsleep_timeout_event_timer);
#endif

static wlan_misc_event_handler_table misc_evtab = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    ieee80211_sta_power_bmiss_handler,
    NULL,                               /* wlan_beacon_rssi_indication */
    ieee80211_sta_power_bmiss_handler,
    NULL,                               /* wlan_sta_clonemac_indication */
    NULL,                               /* wlan_sta_scan_entry_update */
    NULL,                               /* wlan_ap_stopped */
#if ATH_SUPPORT_WAPI
    NULL,                               /* wlan_sta_rekey_indication */
#endif
#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
    NULL,                 /* wlan_sta_cac_started */
#endif
};

#ifndef ATH_USB
typedef spinlock_t ieee80211_power_lock_t;
#else
typedef usblock_t  ieee80211_power_lock_t;
#endif

struct ieee80211_sta_power {
    struct ieee80211vap     *ips_vap;
    ieee80211_hsm_t         ips_hsm_handle; 
    ieee80211_pwrsave_mode  ips_sta_psmode;
    os_timer_t              ips_timer;                   /* to monitor vap activity */ 
    os_timer_t              ips_pause_timer;             /* pause timeout*/ 
    os_timer_t              ips_pause_notif_timer;       /* pause notification timer*/ 
    os_timer_t              ips_async_null_event_timer;  /* timer used ro delivering async null complete failed event*/ 
    ieee80211_power_lock_t  ips_lock;                    /* lock to protect access to the data */
    u_int32_t               ips_connected:1, 
        ips_fullsleep_enable:1,
        ips_use_pspoll:1,            /* Use PS-POLL to retrieve first data frame after TIM */
        ips_pause_request:1,         /* pause request received */
        ips_apPmState:1,             /* AP received information that STA is in power save mode (Network Sleep) */
        ips_pspoll_moredata:1,       /* send more pspoll frames if there is more than 1 frame  */
        ips_auto_mcastsleep:1,       /* hw automatically goes to sleep at the end of mcast */
        ips_force_sleep:1,           /* in forced sleep */
        ips_nw_sleep:1,              /* the has issued nwsleep request to the PM module */
        ips_run_async:1,             /* run SM asynchronously */
        ips_data_paused:1;           /* the data has been paused*/
    u_int32_t               ips_inactivitytime;
    u_int32_t               ips_max_sleeptime;         /* station wakes after this many mS in max power save mode */
    u_int32_t               ips_normal_sleeptime;      /* station wakes after this many mS in max performance mode */
    u_int32_t               ips_low_sleeptime;         /* station wakes after this many mS in low power save mode */
    u_int32_t               ips_max_inactivitytime;    /* in max PS mode, how long (in mS) w/o Tx/Rx before going back to sleep */
    u_int32_t               ips_normal_inactivitytime; /* in max perf mode, how long (in mS) w/o Tx/Rx before going back to sleep */
    u_int32_t               ips_low_inactivitytime;    /* in low PS mode, how long (in mS) w/o Tx/Rx before going back to sleep */
    u_int16_t               ips_sleep_attempt;         /* current attempt for sending out null frame */ 
    u_int16_t               ips_pause_timeout;         /* timeout for pause */
    u_int16_t               ips_pause_notif_timeout;  /* timeout for pause notification */
    ieee80211_sta_power_event_handler ips_event_handlers[IEEE80211_MAX_POWER_STA_EVENT_HANDLERS]; 
    void                    *ips_event_handler_arg[IEEE80211_MAX_POWER_STA_EVENT_HANDLERS]; 
    tsftimer_handle_t       ips_tbtt_tsf_timer;    /* tbtt tsf timer to wake up and receive beacon (only used for SW assisted wakeup) */
    os_timer_t              ips_bcn_timeout_timer; /* beacon timeout timer  (only used for SW assisted wakeup) */ 
    atomic_t                ips_tx_in_progress_count; /* tx is in progress */
    u_int32_t               ips_pwr_arbiter_id;
#if UMAC_SUPPORT_WNM
    os_timer_t              ips_wnmsleep_timeout_timer; /* timeout for the response */
    tsftimer_handle_t       ips_wnmsleep_tsf_timer;     /* tsf timer for wnmsleep */
    u_int16_t               ips_wnmsleep_intval;        /* wnmsleep interval, # of DTIM */
    bool                    ips_wnmsleep_entered;       /* request has been sent to enter wnmsleep mode */ 
#endif   
};

static const char*    sta_power_event_name[] = {
    /* IEEE80211_POWER_STA_EVENT_START             */ "START",
    /* IEEE80211_POWER_STA_EVENT_STOP              */ "STOP",
    /* IEEE80211_POWER_STA_EVENT_FORCE_SLEEP       */ "FORCE_SLEEP",
    /* IEEE80211_POWER_STA_EVENT_FORCE_AWAKE       */ "FORCE_AWAKE",
    /* IEEE80211_POWER_STA_EVENT_NO_ACTIVITY       */ "NO_ACTIVITY",
    /* IEEE80211_POWER_STA_EVENT_NO_PENDING_TX     */ "NO_PENDING_TX",
    /* IEEE80211_POWER_STA_EVENT_SEND_NULL_SUCCESS */ "SEND_NULL_SUCCESS",
    /* IEEE80211_POWER_STA_EVENT_SEND_NULL_FAILED  */ "SEND_NULL_FAIL",
    /* IEEE80211_POWER_STA_EVENT_RECV_UCAST        */ "RECV_UCAST",
    /* IEEE80211_POWER_STA_EVENT_TX                */ "TX",
    /* IEEE80211_POWER_STA_EVENT_LAST_MCAST        */ "LAST_MCAST",
    /* IEEE80211_POWER_STA_EVENT_TIM               */ "TIM",
    /* IEEE80211_POWER_STA_EVENT_DTIM              */ "DTIM",
    /* IEEE80211_POWER_STA_EVENT_PAUSE             */ "PAUSE",
    /* IEEE80211_POWER_STA_EVENT_UNPAUSE           */ "UNPAUSE",
    /* IEEE80211_POWER_STA_EVENT_SEND_KEEP_ALIVE   */ "SEND_KEEP_ALIVE",
    /* IEEE80211_POWER_STA_EVENT_PAUSE_TIMEOUT     */ "PAUSE_TIMEOUT",
    /* IEEE80211_POWER_STA_EVENT_PAUSE_NOTIF_TIMEOUT*/ "PAUSE_NOTIF_TIMEOUT",
    /* IEEE80211_POWER_STA_EVENT_TBTT              */ "EVENT_TBTT", 
    /* IEEE80211_POWER_STA_EVENT_TSF_CHANGED       */ "EVENT_TSF_CHANGED",       
    /* IEEE80211_POWER_STA_EVENT_BCN_TIMEOUT       */ "EVENT_BCN_TIMEOUT",     
    /* IEEE80211_POWER_STA_EVENT_BCN_RECVD         */ "EVENT_BCN_RECVD",
    /* IEEE80211_POWER_STA_EVENT_WNMRESP_RECVD     */ "EVENT_WNMRESP_RECVD",
    /* IEEE80211_POWER_STA_EVENT_WNMSLEEP_SEND_TIMEOUT  */ "EVENT_WNMSLEEP_SEND_TIMEOUT",
    /* IEEE80211_POWER_STA_EVENT_WNMSLEEP_CONFIRM_ENTER */ "EVENT_WNMSLEEP_CONFIRM_ENTER",
    /* IEEE80211_POWER_STA_EVENT_WNMSLEEP_CONFIRM_EXIT */  "EVENT_WNMSLEEP_CONFIRM_EXIT"
};
    
static void ieee80211_sta_power_deliver_event(ieee80211_sta_power_t     powersta, 
                                              ieee80211_sta_power_event *event)
{
    int                                  i, nhandlers = 0;
    ieee80211_sta_power_event_handler    handlers[IEEE80211_MAX_POWER_STA_EVENT_HANDLERS];
    void                                 *handler_arg[IEEE80211_MAX_POWER_STA_EVENT_HANDLERS];
    
    IPS_LOCK(powersta);
    for (i = 0; i < IEEE80211_MAX_POWER_STA_EVENT_HANDLERS; ++i) {
        if (powersta->ips_event_handlers[i]) {
            handlers[nhandlers] = powersta->ips_event_handlers[i];
            handler_arg[nhandlers++] = powersta->ips_event_handler_arg[i];
        }
    }
    IPS_UNLOCK(powersta);
    for(i = 0;i < nhandlers; ++i) {
        (*handlers[i])(powersta->ips_vap, event, handler_arg[i]);
    }
}

static void ieee80211_sta_power_set_power(ieee80211_sta_power_t powersta, 
                                          int                   power_mode) 
{
    switch(power_mode) {
    case IEEE80211_PWRSAVE_AWAKE: /* put the chip in awake state */
        if (powersta->ips_nw_sleep) {
            ieee80211_power_arbiter_exit_nwsleep(powersta->ips_vap,
                    powersta->ips_pwr_arbiter_id);
            powersta->ips_nw_sleep=0;
        }
        break;
        
    case IEEE80211_PWRSAVE_NETWORK_SLEEP: /* put the chip in network sleep state */
        if (!powersta->ips_nw_sleep) {
            ieee80211_power_arbiter_enter_nwsleep(powersta->ips_vap,
                    powersta->ips_pwr_arbiter_id);
            powersta->ips_nw_sleep=1;
        }
        break;
    }
}

/*
 * pause data queue.
 * used when running powersave SM asynchronously.
 */
static void ieee80211_sta_power_pause_data(ieee80211_sta_power_t powersta) 
{
    if (!powersta->ips_run_async) return;

    if (!powersta->ips_data_paused) {
        struct ieee80211vap    *vap;
        struct ieee80211_node *ni;


        vap = powersta->ips_vap;

        ni = ieee80211_try_ref_bss_node(vap);
        if (ni) {
            ieee80211node_pause(ni);
            ieee80211_free_node(ni);
        }
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s data queue paused ", __func__ );
        powersta->ips_data_paused = true;
    }
}

/*
 * unpause data queue.
 * used when running powersave SM asynchronously.
 */
static void ieee80211_sta_power_unpause_data(ieee80211_sta_power_t powersta) 
{
    if (!powersta->ips_run_async) return;

    if (powersta->ips_data_paused) {
        struct ieee80211vap    *vap;
        struct ieee80211_node *ni;


        vap = powersta->ips_vap;

        ni = ieee80211_try_ref_bss_node(vap);
        if (ni) {
            ieee80211node_unpause(ni);
            ieee80211_free_node(ni);
        }
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s data queue unpaused\n", __func__ );
        powersta->ips_data_paused = false;
    }
}

int _ieee80211_sta_power_unregister_event_handler(struct ieee80211vap *vap,
                                                 ieee80211_sta_power_event_handler evhandler, 
                                                 void                              *arg)
{
    ieee80211_sta_power_t    powersta = vap->iv_pwrsave_sta;
    int    i;
    
    if (!powersta) {
        return EINVAL; /* can happen if called from an ap vap */
    }
    
    IPS_LOCK(powersta);
    for (i = 0; i < IEEE80211_MAX_POWER_STA_EVENT_HANDLERS; ++i) {
        if ((powersta->ips_event_handlers[i] == evhandler) && (powersta->ips_event_handler_arg[i] == arg)) {
            powersta->ips_event_handlers[i] = NULL; 
            powersta->ips_event_handler_arg[i] = NULL;
            IPS_UNLOCK(powersta);
            return EOK;
        }
    }
    IPS_UNLOCK(powersta);
    return EEXIST;
}

int _ieee80211_sta_power_register_event_handler(struct ieee80211vap *vap,
                                               ieee80211_sta_power_event_handler evhandler, 
                                               void                              *arg)
{
    ieee80211_sta_power_t    powersta = vap->iv_pwrsave_sta;
    int    i;
    
    /* unregister if there exists one already */
    if (!powersta) {
        return EINVAL; /* can happen if called from an ap vap */
    }
    ieee80211_sta_power_unregister_event_handler(vap,evhandler,arg);

    IPS_LOCK(powersta);
    for (i = 0; i < IEEE80211_MAX_POWER_STA_EVENT_HANDLERS; ++i) {
        if (powersta->ips_event_handlers[i] == NULL) {
            powersta->ips_event_handlers[i] = evhandler;
            powersta->ips_event_handler_arg[i] = arg;
            IPS_UNLOCK(powersta);
            return EOK;
        }
    }
    IPS_UNLOCK(powersta);
    return -ENOMEM;
}

/*
 * COMMON event handler function for all the states.
 */
static bool ieee80211_sta_power_common_event_handler(ieee80211_sta_power_t powersta, 
                                                     u_int16_t             event, 
                                                     u_int16_t             event_data_len, 
                                                     void                  *event_data) 
{
    ieee80211_node_saveq_info    qinfo;
    int                          rc;
    
    struct ieee80211vap      *vap = powersta->ips_vap;
    struct ieee80211com      *ic = vap->iv_ic;
    struct ieee80211_node    *ni = vap->iv_bss;
    
    switch(event) {
    case IEEE80211_POWER_STA_EVENT_STOP:                    
        /*
         * if not being paused then move to INIT.
         */
        if (!powersta->ips_pause_request) {
            ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_INIT);
        } else if (!powersta->ips_connected) {
            /* EV 65403: UNPAUSE if VAP is DOWN */
            /* 
             * scanner needs unpause event when the vap goes down other wise
             * the scanner will assert waiting for unpause event. Note that 
             * when resource manager is present then the resource manager will
             * send an event to scanner indicating vap unpause state. The resource
             * manager will ignore the unpause event when present. 
             */  
            ieee80211_sta_power_event ps_event;
            ps_event.type = IEEE80211_POWER_STA_UNPAUSE_COMPLETE;
            ps_event.status = IEEE80211_POWER_STA_STATUS_DISCONNECT;
            ieee80211_sta_power_deliver_event(powersta, &ps_event);
            powersta->ips_pause_request=false;
            ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_INIT);
            IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, "%s[%d] CANCEL powersta->ips_pause_timeout %d", __func__, __LINE__, powersta->ips_pause_timeout);
            OS_CANCEL_TIMER(&powersta->ips_pause_timer);
        } 
        if (powersta->ips_force_sleep) {
           powersta->ips_force_sleep = false;
           ieee80211_vap_forced_sleep_clear(powersta->ips_vap);
        }
        return true;
        
    case IEEE80211_POWER_STA_EVENT_PAUSE:                    
        /* 
         * unregister for  all activity events 
         * ignore any activity events when pause is requested.
         */
        if (!powersta->ips_pause_request) {
            powersta->ips_pause_timeout = *((u_int32_t*) event_data);
            powersta->ips_pause_request=true;
            if (powersta->ips_pause_timeout) {
                OS_SET_TIMER(&powersta->ips_pause_timer,powersta->ips_pause_timeout);
            }
        }
        return true;

    case IEEE80211_POWER_STA_EVENT_UNPAUSE: 
        if (powersta->ips_pause_request) {
            ieee80211_sta_power_event ps_event;
            systime_t                lastData;
            lastData = ieee80211_get_last_data_timestamp(powersta->ips_vap);

            ieee80211_node_saveq_get_info(powersta->ips_vap->iv_bss, &qinfo);
            /* 
             * if no data frames in the powersave queue (pause queue), no data frames in lmac queues and no tx/rx data activy 
             * and powersave is not enabled then send null with ps=0 and enter INIT. 
             * if no data frames in the powersave queue (pause queue), no data frames in lmac queues  and no tx/rx data activy 
             * and powersave is enabled and no need to sync for beacon and no beacon misss then enter SLEEP. 
             * if no data frames in the powersave queue (pause queue), no data frames in lmac queues  and no tx/rx data activy 
             * and powersave is enabled and either need to sync for beacon or beacon misss then enter ACTIVE. 
             * if data frames in the powersave queue (pause queue), or data frames in lmac queues  or tx/rx data activity 
             * and powersave is not enabled then enter INIT. 
             * if data frames in the powersave queue (pause queue) or data frames in lmac queue
             * and powersave is enabled then enter ACTIVE. 
             */
            if ((CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP() - lastData) > powersta->ips_inactivitytime) && 
                 qinfo.data_count == 0 && qinfo.mgt_count == 0  && ((ic->ic_node_queue_depth)(ni) == 0)) {

                if (powersta->ips_sta_psmode == IEEE80211_PWRSAVE_NONE 
                    || ic->ic_need_beacon_sync(ic) || vap->iv_bmiss_count ) {

                    ieee80211_sta_power_set_power(powersta, IEEE80211_PWRSAVE_AWAKE); 
                    rc = ieee80211_send_nulldata(powersta->ips_vap->iv_bss, false);
                    if (rc != EOK) {
                        IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, "%s: ieee80211_send_nulldata failed PM=0 rc=%d (%0x08X)\n",
                                      __func__,
                                      rc, rc);
                    }
                    if (powersta->ips_sta_psmode == IEEE80211_PWRSAVE_NONE) {
                        ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_INIT);
                    } else {
                        ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_ACTIVE);
                    }
                } else {
                    ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_SLEEP);
                }
            } else {
                ieee80211_sta_power_set_power(powersta, IEEE80211_PWRSAVE_AWAKE); 
                rc = ieee80211_send_nulldata(powersta->ips_vap->iv_bss, false);
                
                if (powersta->ips_sta_psmode == IEEE80211_PWRSAVE_NONE) {
                    ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_INIT);
                } else {
                    ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_ACTIVE);
                }
            }
            ps_event.type = IEEE80211_POWER_STA_UNPAUSE_COMPLETE;
            ps_event.status = IEEE80211_POWER_STA_STATUS_SUCCESS;
            ieee80211_sta_power_deliver_event(powersta, &ps_event);
            powersta->ips_pause_request=false;
            OS_CANCEL_TIMER(&powersta->ips_pause_timer);
        }
        return true;
        
    case IEEE80211_POWER_STA_EVENT_FORCE_SLEEP:                       /* force sleep enable*/
        if (!powersta->ips_force_sleep) {
           powersta->ips_force_sleep = true;
           ieee80211_vap_forced_sleep_set(powersta->ips_vap);
        }
        return true;

    case IEEE80211_POWER_STA_EVENT_FORCE_AWAKE:                       /* force sleep disable*/
        if (powersta->ips_force_sleep) {
           powersta->ips_force_sleep = false;
           ieee80211_vap_forced_sleep_clear(powersta->ips_vap);
        }
        return true;

    case IEEE80211_POWER_STA_EVENT_PAUSE_TIMEOUT:
        if (powersta->ips_pause_request) {
            ieee80211_sta_power_event ps_event;
            /* 
             * pause timedout , goback to init/active.
             */
            if (powersta->ips_sta_psmode == IEEE80211_PWRSAVE_NONE) {
                ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_INIT);
            } else {
                ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_ACTIVE);
            }
            ps_event.type = IEEE80211_POWER_STA_PAUSE_COMPLETE;
            ps_event.status = IEEE80211_POWER_STA_STATUS_TIMED_OUT;
            ieee80211_sta_power_deliver_event(powersta, &ps_event);
            powersta->ips_pause_request=false;
        }
        return true;
        
    default:
        return false;
    }
}

/*
 * INIT state.
 */
static void ieee80211_sta_power_state_init_entry(void *ctx) 
{
    ieee80211_sta_power_t    powersta = (ieee80211_sta_power_t)ctx;
    
    powersta->ips_apPmState = false;
    wlan_vdev_txrx_unregister_event_handler(powersta->ips_vap->vdev_obj,ieee80211_sta_power_txrx_event_handler,powersta);
    ieee80211_sta_power_unpause_data(powersta); 
}

static bool ieee80211_sta_power_state_init_event (void      *ctx, 
                                                  u_int16_t event, 
                                                  u_int16_t event_data_len, 
                                                  void      *event_data) 
{
    ieee80211_sta_power_t    powersta = (ieee80211_sta_power_t)ctx;
    struct ieee80211com      *ic = powersta->ips_vap->iv_ic;
    bool                     event_handled = false;
    int                      rc;

    switch(event) {
    case IEEE80211_POWER_STA_EVENT_START:                       /* sleep enable*/
        ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_ACTIVE);
        event_handled = true;
        break;
        
    case IEEE80211_POWER_STA_EVENT_PAUSE:                       /* sleep enable*/
    case IEEE80211_POWER_STA_EVENT_FORCE_SLEEP:                       /* sleep enable*/
        /*
         * should check for actual vaps pending txq.
         */
        if (ic->ic_txq_depth(ic) != 0) {
            ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_PENDING_TX);
        } else {
            ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_NULL_SENT);
        }
        /* return false so that the COMMON state will handle the common part */
        break;
        
    case IEEE80211_POWER_STA_EVENT_SEND_KEEP_ALIVE:                       /* send keep alive*/
        rc = ieee80211_send_nulldata(powersta->ips_vap->iv_bss, false);
        if (rc != EOK) {
            IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, "%s: ieee80211_send_nulldata failed PM=0 rc=%d (%0x08X)\n",
                              __func__,
                              rc, rc);
        }
        event_handled = true;
        break;

    case IEEE80211_POWER_STA_EVENT_TIM:  
        ieee80211_sta_power_set_power(powersta, IEEE80211_PWRSAVE_AWAKE); 
        rc = ieee80211_send_nulldata(powersta->ips_vap->iv_bss, false);
        if (rc != EOK) {
            IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, "%s: ieee80211_send_nulldata failed PM=0 rc=%d (%0x08X)\n",
                              __func__,
                              rc, rc);
        }
        event_handled = true;
        break;

    case IEEE80211_POWER_STA_EVENT_DTIM:  
        ieee80211_sta_power_set_power(powersta, IEEE80211_PWRSAVE_AWAKE); 
        event_handled = true;
        break;
    
    default:
        event_handled = false;
        break;
    }
    
    if (!event_handled) {
        return ieee80211_sta_power_common_event_handler(powersta, event, event_data_len, event_data); 
    }
    
    return event_handled;
}

static void ieee80211_sta_power_state_init_exit(void *ctx) 
{

}

/*
 * READY state.
 */
static void ieee80211_sta_power_state_active_entry(void *ctx) 
{
    ieee80211_sta_power_t    powersta = (ieee80211_sta_power_t)ctx;
    
    powersta->ips_apPmState = false;
    /*
     * start activity timer.
     */ 
    OS_SET_TIMER(&powersta->ips_timer,IEEE80211_PWRSAVE_TIMER_INTERVAL);
    wlan_vdev_txrx_unregister_event_handler(powersta->ips_vap->vdev_obj,ieee80211_sta_power_txrx_event_handler,powersta);
    ieee80211_sta_power_unpause_data(powersta); 
}

static bool ieee80211_sta_power_state_active_event(void      *ctx, 
                                                   u_int16_t event, 
                                                   u_int16_t event_data_len, 
                                                   void      *event_data) 
{
    ieee80211_sta_power_t    powersta = (ieee80211_sta_power_t)ctx;
    struct ieee80211com      *ic = powersta->ips_vap->iv_ic;    
    bool                     event_handled = false;
    int                      rc;
    
    switch(event) {
    case IEEE80211_POWER_STA_EVENT_NO_ACTIVITY:               /* no activity on vap*/
    case IEEE80211_POWER_STA_EVENT_FORCE_SLEEP:                       /*force sleep enable*/
        ieee80211_sta_power_pause_data(powersta); 
        if (atomic_read(&powersta->ips_tx_in_progress_count)) {
            /* if tx is in progress,then unpause and ignore the event */
            ieee80211_sta_power_unpause_data(powersta); 
            break;
        }
        if (ic->ic_txq_depth(ic) != 0) {
            ieee80211_sm_transition_to(powersta->ips_hsm_handle, IEEE80211_POWER_STA_STATE_PENDING_TX);
        } else {
#if UMAC_SUPPORT_WNM
            if (powersta->ips_sta_psmode == IEEE80211_PWRSAVE_WNM) {
                /* WNM-Sleep mode, send WNM-Sleep request first */
                powersta->ips_wnmsleep_entered = false;
                ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_WNMREQ_SENT);
            } else
#endif
                ieee80211_sm_transition_to(powersta->ips_hsm_handle, IEEE80211_POWER_STA_STATE_NULL_SENT);
        }
        if (event == IEEE80211_POWER_STA_EVENT_NO_ACTIVITY) {     
            event_handled = true;
        }
        /*
         * if the event is EVENT_PAUSE then leave event_handled to false
         * so that the common event handler will handle the common part
         */
        break;


    case IEEE80211_POWER_STA_EVENT_PAUSE:                    
        ieee80211_sta_power_pause_data(powersta); 
        if (ic->ic_txq_depth(ic) != 0 || atomic_read(&powersta->ips_tx_in_progress_count)) {
            ieee80211_sm_transition_to(powersta->ips_hsm_handle, IEEE80211_POWER_STA_STATE_PENDING_TX);
        } else {
            ieee80211_sm_transition_to(powersta->ips_hsm_handle, IEEE80211_POWER_STA_STATE_NULL_SENT);
        }
        break;
        
    case IEEE80211_POWER_STA_EVENT_TIM:
        rc = ieee80211_send_nulldata(powersta->ips_vap->iv_bss, false);
        if (rc != EOK) {
            IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, "%s: ieee80211_send_nulldata failed PM=0 rc=%d (%0x08X)\n",
                              __func__,
                              rc, rc);
        }
        break;
        
    default:
        break;
    }
    
    if (!event_handled) {
        return ieee80211_sta_power_common_event_handler(powersta, event, event_data_len, event_data); 
    }
    
    return event_handled;
}

static void ieee80211_sta_power_state_active_exit(void *ctx) 
{
    ieee80211_sta_power_t    powersta = (ieee80211_sta_power_t)ctx;
    /*
     * cancel activity monitor timer.
     */ 
    OS_CANCEL_TIMER(&powersta->ips_timer);
}

/*
 * PENDING_TX state.
 */
static void ieee80211_sta_power_state_pending_tx_entry(void *ctx) 
{
#if DA_SUPPORT
    ieee80211_sta_power_t    powersta = (ieee80211_sta_power_t)ctx;
#endif
    
    wlan_vdev_txrx_register_event_handler(powersta->ips_vap->vdev_obj,ieee80211_sta_power_txrx_event_handler,powersta,
                                              IEEE80211_VAP_INPUT_EVENT_UCAST | IEEE80211_VAP_OUTPUT_EVENT_DATA | IEEE80211_VAP_OUTPUT_EVENT_TXQ_EMPTY);
}

static bool ieee80211_sta_power_state_pending_tx_event (void      *ctx, 
                                                        u_int16_t event, 
                                                        u_int16_t event_data_len, 
                                                        void      *event_data) 
{
    ieee80211_sta_power_t    powersta = (ieee80211_sta_power_t)ctx;
    bool                     event_handled = false;
    
    switch(event) {
    case IEEE80211_POWER_STA_EVENT_NO_PENDING_TX:               /* no activity on vap*/
        if (atomic_read(&powersta->ips_tx_in_progress_count)) {
          if (!powersta->ips_pause_request && !powersta->ips_force_sleep) {
              IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, 
                              "%s: no pending activity ,but tx thread just started sending some data move to ACTIVE \n",__func__);
              ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_ACTIVE);
          } else {
              /* wait for pending tx count to become 0 */

              OS_SET_TIMER(&powersta->ips_timer,IEEE80211_PWRSAVE_PENDING_TX_CHECK_INTERVAL);          
          }

        } else {
            ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_NULL_SENT);
        }
        
        break;
    case IEEE80211_POWER_STA_EVENT_RECV_UCAST:  
    case IEEE80211_POWER_STA_EVENT_TX:  
        if (!powersta->ips_pause_request && !powersta->ips_force_sleep) {
            ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_ACTIVE);
        }
        event_handled = true;
        break;
    default:
        /* return false so that the common event handler will handle the common part */
        break;
    }
    
    if (!event_handled) {
        return ieee80211_sta_power_common_event_handler(powersta, event, event_data_len, event_data); 
    }
    
    return event_handled;
}

static void ieee80211_sta_power_state_pending_tx_exit(void *ctx) 
{
    ieee80211_sta_power_t    powersta = (ieee80211_sta_power_t)ctx;

    OS_CANCEL_TIMER(&powersta->ips_timer);
}

/*
 * NULL_SENT state.
 */
static void ieee80211_sta_power_state_null_sent_entry(void *ctx) 
{
    ieee80211_sta_power_t    powersta = (ieee80211_sta_power_t)ctx;
    int                      rc;
    
    powersta->ips_sleep_attempt = 0;         /* current attempt for sending out null frame */ 
    wlan_vdev_txrx_register_event_handler(powersta->ips_vap->vdev_obj,
                                              ieee80211_sta_power_txrx_event_handler,
                                              powersta,
                                              (IEEE80211_VAP_INPUT_EVENT_UCAST | 
                                               IEEE80211_VAP_OUTPUT_EVENT_DATA |
                                               IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_PS_NULL));
    rc = ieee80211_send_nulldata(powersta->ips_vap->iv_bss, true);
    if (rc != EOK) {
        IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, "%s: ieee80211_send_nulldata failed PM=1 rc=%d (%0x08X)\n",
                          __func__,
                          rc, rc);
    }
}

static bool ieee80211_sta_power_state_null_sent_event(void      *ctx, 
                                                      u_int16_t event, 
                                                      u_int16_t event_data_len, 
                                                      void      *event_data) 
{
    ieee80211_sta_power_t    powersta = (ieee80211_sta_power_t)ctx;
    bool                     event_handled = false;
    int                      rc;
    
    switch(event) {
    case IEEE80211_POWER_STA_EVENT_SEND_NULL_FAILED:   
        ++powersta->ips_sleep_attempt;         /* current attempt for sending out null frame */ 
        event_handled = true;
        if (powersta->ips_sleep_attempt < IEEE80211_MAX_SLEEP_ATTEMPTS) {
            rc = ieee80211_send_nulldata(powersta->ips_vap->iv_bss, true);
            if (rc != EOK) {
                IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, "%s: ieee80211_send_nulldata failed PM=1 rc=%d (%0x08X)\n",
                                  __func__,
                                  rc, rc);
            }
        } else {
            /* max attempts reached */
            if (powersta->ips_sta_psmode == IEEE80211_PWRSAVE_NONE) {
                ieee80211_sm_transition_to(powersta->ips_hsm_handle, IEEE80211_POWER_STA_STATE_INIT);
            } else {
                ieee80211_sm_transition_to(powersta->ips_hsm_handle, IEEE80211_POWER_STA_STATE_ACTIVE);
            }
            if (powersta->ips_pause_request) {
                ieee80211_sta_power_event ps_event;
                /* send pause failed event */
                ps_event.type = IEEE80211_POWER_STA_PAUSE_COMPLETE;
                ps_event.status = IEEE80211_POWER_STA_STATUS_TIMED_OUT;
                ieee80211_sta_power_deliver_event(powersta, &ps_event);
                powersta->ips_pause_request = false;
            } 
        }
        break;
        
    case IEEE80211_POWER_STA_EVENT_SEND_NULL_SUCCESS:   
        if (powersta->ips_pause_request) {
           ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_PAUSE);
        }  else if (powersta->ips_force_sleep) {
           ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_FORCE_SLEEP);
        } else {
           ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_SLEEP);
        }
        event_handled = true;
        break;
        
    case IEEE80211_POWER_STA_EVENT_STOP:               /* stop the SM*/
        if (!powersta->ips_pause_request) {
            rc = ieee80211_send_nulldata(powersta->ips_vap->iv_bss, false);
            if (rc != EOK) {
                IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, "%s: ieee80211_send_nulldata failed PM=0 rc=%d (%0x08X)\n",
                                  __func__,
                                  rc, rc);
            }
        }
        /* return false so that the common event handler will handle the common part */
        break;

    case IEEE80211_POWER_STA_EVENT_TX:  
        if (!powersta->ips_pause_request) {
            ieee80211_sm_transition_to(powersta->ips_hsm_handle, IEEE80211_POWER_STA_STATE_ACTIVE);
        }
        event_handled = true;
        break;
        
    default:
        /* return false so that the common event handler will handle the common part */
        break;
    }
    
    if (!event_handled) {
        return ieee80211_sta_power_common_event_handler(powersta, event, event_data_len, event_data); 
    }
    
   return event_handled;
}

static void ieee80211_sta_power_state_null_sent_exit(void *ctx) 
{

}

/*
 * SLEEP state.
 */

/*
 * handler for tbtt timer.
 */
static void ieee80211_sta_power_tbtt_handler(tsftimer_handle_t h_tsftime, int arg1, void *arg2, u_int32_t curr_tsf)
{
    ieee80211_sta_power_t        powersta = (ieee80211_sta_power_t)arg2;

    /* deliver TBTT event and start the beacon timeout timer */
    ieee80211_sm_dispatch(powersta->ips_hsm_handle,IEEE80211_POWER_STA_EVENT_TBTT, 0, NULL);
    OS_SET_TIMER(&powersta->ips_bcn_timeout_timer, IEEE80211_BEACON_TIMEOUT ); /* arm the beacon timeout timer */

}

/*
* tsf changed event,could be called after reset (or) when the tsf jumps around.
* reprogram the tsf timer.
* also wake us up if we are sleeping.
*/
static void ieee80211_sta_power_tsf_changed_handler(tsftimer_handle_t h_tsftime, int arg1, void *arg2, u_int32_t curr_tsf)
{
    ieee80211_sta_power_t        powersta = (ieee80211_sta_power_t)arg2;

    ieee80211_sm_dispatch(powersta->ips_hsm_handle,IEEE80211_POWER_STA_EVENT_TSF_CHANGED, 0, NULL);

}

static OS_TIMER_FUNC(ieee80211_sta_power_bcn_timeout_event_timer)
{
    ieee80211_sta_power_t    powersta;
    
    OS_GET_TIMER_ARG(powersta, ieee80211_sta_power_t);
    ieee80211_sm_dispatch(powersta->ips_hsm_handle, IEEE80211_POWER_STA_EVENT_BCN_TIMEOUT, 0, NULL);
}


#define MIN_TSF_TIMER_TIME  5000 /* 5 msec */
#define MIN_TSF_TIME_DIFF 500    /* 500 usec */
#define MIN_PRE_TBTT_WAKEUP_TIME 1000  /* 1 msec */

/* Get the beacon interval in microseconds */
static inline u_int32_t get_beacon_interval(wlan_if_t vap)
{
    /* The beacon interval is in terms of Time Units */
    return(IEEE80211_TU_TO_USEC(vap->iv_bss->ni_intval));
}


/*
* get the nextbtt for the given vap.
* for the second STA vap the tsf offset is used for calculating tbtt.
*/
static u_int32_t ieee80211_get_next_tbtt_tsf_32(wlan_if_t vap, ieee80211_pwrsave_mode ips_sta_psmode)
{
   struct ieee80211com *ic = vap->iv_ic;
   u_int64_t           curr_tsf64, nexttbtt_tsf64;
   u_int32_t           bintval; /* beacon interval in micro seconds */
   ieee80211_vap_tsf_offset tsf_offset_info;

   curr_tsf64 = ic->ic_get_TSF64(ic);
   /* calculate the next tbtt */

   /* tsf offset from our HW tsf */
   ieee80211_vap_get_tsf_offset(vap,&tsf_offset_info);

   /* adjust tsf to the clock of the GO */
   if (tsf_offset_info.offset_negative) {
       curr_tsf64 -= tsf_offset_info.offset;
   } else {
       curr_tsf64 += tsf_offset_info.offset;
   }

   bintval = get_beacon_interval(vap);

#if UMAC_SUPPORT_WNM
   if (ips_sta_psmode == IEEE80211_PWRSAVE_WNM)
       nexttbtt_tsf64 =  curr_tsf64 + bintval*vap->iv_wnmsleep_intval;
   else
#endif
   nexttbtt_tsf64 =  curr_tsf64 + bintval;

   nexttbtt_tsf64  = nexttbtt_tsf64 - OS_MOD64_TBTT_OFFSET(nexttbtt_tsf64, bintval);

   if ((nexttbtt_tsf64 - curr_tsf64) < MIN_TSF_TIMER_TIME ) {  /* if the immediate next tbtt is too close go to next one */
       nexttbtt_tsf64 += bintval;
   }

   /* adjust tsf back to the clock of our HW */
   if (tsf_offset_info.offset_negative) {
       nexttbtt_tsf64 += tsf_offset_info.offset;
   } else {
       nexttbtt_tsf64 -= tsf_offset_info.offset;
   }

   return (u_int32_t) nexttbtt_tsf64;
}

#ifdef DEBUG_TSF_OFFSET

void ieee80211_print_tsf_info(wlan_if_t vap)
{

   struct ieee80211com *ic = vap->iv_ic;
   u_int64_t           curr_tsf64;
   u_int32_t           bintval; /* beacon interval in micro seconds */
   ieee80211_vap_tsf_offset tsf_offset_info;

   curr_tsf64 = ic->ic_get_TSF64(ic);
   /* calculate the next tbtt */

   /* tsf offset from our HW tsf */
   ieee80211_vap_get_tsf_offset(vap,&tsf_offset_info);

   bintval = get_beacon_interval(vap);

   IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s: curr_tsf (tsf1) %llu lower 0x%x higher 0x%x bintval %d\n",
                         __func__,curr_tsf64, (u_int32_t)(curr_tsf64 & 0xffffffff),
                         (u_int32_t)((curr_tsf64 >> 32) & 0xffffffff), bintval);
   /* adjust tsf to the clock of the GO */
   if (tsf_offset_info.offset_negative) {
       curr_tsf64 -= tsf_offset_info.offset;
   } else {
       curr_tsf64 += tsf_offset_info.offset;
   }


   IEEE80211_DPRINTF(vap, IEEE80211_MSG_POWER, "%s: negative %d offset %llu curtsf(tsf2) %llu  nextbtt %llu \n ",
                              __func__, tsf_offset_info.offset_negative, tsf_offset_info.offset,curr_tsf64,(curr_tsf64 >> 10));

}
#else

#define ieee80211_print_tsf_info(_vap) /**/

#endif

static void ieee80211_sta_power_start_tbtt_timer( ieee80211_sta_power_t        powersta) 
{
   ieee80211_vap_tsf_offset tsf_offset_info;

   /* tsf offset from our HW tsf */
   ieee80211_vap_get_tsf_offset(powersta->ips_vap,&tsf_offset_info);

   /* if required, setup the tsf timer, this is only required in the STA + STA case */
   if (tsf_offset_info.offset > MIN_TSF_TIME_DIFF ) {
       int                 retval;
       u_int32_t           nexttbtt_tsf32;
     
       nexttbtt_tsf32 = ieee80211_get_next_tbtt_tsf_32(powersta->ips_vap, powersta->ips_sta_psmode);

       /* adjust tbtt timer by a fudge factor so that the chip is awake to receive beacons slightly ahead */
       nexttbtt_tsf32 -= MIN_PRE_TBTT_WAKEUP_TIME; 
       /* Start this timer */
       retval = ieee80211_tsftimer_start(powersta->ips_tbtt_tsf_timer, nexttbtt_tsf32, 0);
   }

}

static void ieee80211_sta_power_state_sleep_entry(void *ctx) 
{
    ieee80211_sta_power_t        powersta = (ieee80211_sta_power_t)ctx;
    ieee80211_sta_power_event    ps_event;


    wlan_vdev_txrx_register_event_handler(powersta->ips_vap->vdev_obj,
                                              ieee80211_sta_power_txrx_event_handler,
                                              powersta,
                                              (IEEE80211_VAP_INPUT_EVENT_UCAST | 
                                               IEEE80211_VAP_INPUT_EVENT_LAST_MCAST | 
                                               IEEE80211_VAP_OUTPUT_EVENT_DATA));


    ieee80211_sta_power_start_tbtt_timer(powersta); 
    ieee80211_print_tsf_info(powersta->ips_vap);
    /*
     * put the vap into network sleep.
     */
    ieee80211_sta_power_set_power(powersta, IEEE80211_PWRSAVE_NETWORK_SLEEP); 
    ps_event.type = IEEE80211_POWER_STA_SLEEP;
    ps_event.status = IEEE80211_POWER_STA_STATUS_SUCCESS;
    ieee80211_sta_power_deliver_event(powersta, &ps_event);
}

static bool ieee80211_sta_power_state_sleep_event (void      *ctx, 
                                                   u_int16_t event, 
                                                   u_int16_t event_data_len, 
                                                   void      *event_data) 
{
    ieee80211_sta_power_t    powersta = (ieee80211_sta_power_t)ctx;
    bool                     event_handled = false;
    int                      rc;
    
    switch(event) {
    case IEEE80211_POWER_STA_EVENT_STOP:               /* stop the SM*/
        ieee80211_sta_power_set_power(powersta, IEEE80211_PWRSAVE_AWAKE); 
        rc = ieee80211_send_nulldata(powersta->ips_vap->iv_bss, false);
        if (rc != EOK) {
            IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, "%s: ieee80211_send_nulldata failed PM=0 rc=%d (%0x08X)\n",
                              __func__,
                              rc, rc);
        }
        /* return false so that the common event handler will handle the common part */
        break;
        
    case IEEE80211_POWER_STA_EVENT_TIM:
#if UMAC_SUPPORT_WNM
        if (powersta->ips_sta_psmode == IEEE80211_PWRSAVE_WNM) {
            ieee80211_sta_power_set_power(powersta, IEEE80211_PWRSAVE_AWAKE);

            /* Send WNM-Sleep Request to exit */
            ASSERT(powersta->ips_wnmsleep_entered);
            ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_WNMREQ_SENT);
        } else if (powersta->ips_use_pspoll) {
#else  
        if (powersta->ips_use_pspoll) {
#endif
            ieee80211_sm_transition_to(powersta->ips_hsm_handle, IEEE80211_POWER_STA_STATE_PSPOLL);
        } else {
            ieee80211_sta_power_set_power(powersta, IEEE80211_PWRSAVE_AWAKE); 
            rc = ieee80211_send_nulldata(powersta->ips_vap->iv_bss, false);
            if (rc != EOK) {
                IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, "%s: ieee80211_send_nulldata failed PM=0 rc=%d (%0x08X)\n",
                                  __func__,
                                  rc, rc);
            }
            ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_ACTIVE);
        }
        event_handled = true;
        break;
        
    case IEEE80211_POWER_STA_EVENT_TX:  
        if (!powersta->ips_pause_request) {
#if UMAC_SUPPORT_WNM
            if (powersta->ips_sta_psmode == IEEE80211_PWRSAVE_WNM) {
                ieee80211_print_tsf_info(powersta->ips_vap);
                /* Send WNM-Sleep Request to exit */
                ASSERT(powersta->ips_wnmsleep_entered);
                ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_WNMREQ_SENT);
            } else
#endif
                ieee80211_sm_transition_to(powersta->ips_hsm_handle, IEEE80211_POWER_STA_STATE_ACTIVE);
        }
        event_handled = true;
        break;

    case IEEE80211_POWER_STA_EVENT_DTIM:  
        ieee80211_sta_power_set_power(powersta, IEEE80211_PWRSAVE_AWAKE); 
        event_handled = true;
        break;
        
    case IEEE80211_POWER_STA_EVENT_LAST_MCAST:  
        ieee80211_sta_power_set_power(powersta, IEEE80211_PWRSAVE_NETWORK_SLEEP); 
        event_handled = true;
        break;
        
    case IEEE80211_POWER_STA_EVENT_PAUSE:                    
        ieee80211_sm_transition_to(powersta->ips_hsm_handle, IEEE80211_POWER_STA_STATE_PAUSE);
        /* return false so that the COMMON state will handle the common part */
        break;
        
    case IEEE80211_POWER_STA_EVENT_SEND_KEEP_ALIVE:                       /* send keep alive*/
#if UMAC_SUPPORT_WNM
        if (powersta->ips_sta_psmode == IEEE80211_PWRSAVE_WNM)
            /* Send WNM-Sleep request to exit */
            ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_WNMREQ_SENT);
        else
#endif
            ieee80211_sm_transition_to(powersta->ips_hsm_handle, IEEE80211_POWER_STA_STATE_NULL_SENT);
        event_handled = true;
        break;
        
    case IEEE80211_POWER_STA_EVENT_TBTT:           
    case IEEE80211_POWER_STA_EVENT_WNMSLEEP_SEND_TIMEOUT: 
        /* wake up the chip */
        ieee80211_sta_power_set_power(powersta, IEEE80211_PWRSAVE_AWAKE); 
        ieee80211_print_tsf_info(powersta->ips_vap);
        event_handled = true;
        break;

    case IEEE80211_POWER_STA_EVENT_TSF_CHANGED:    
        /* wake up the chip until we received our beacon */
        ieee80211_sta_power_set_power(powersta, IEEE80211_PWRSAVE_AWAKE); 
        event_handled = true;
        break;

    case IEEE80211_POWER_STA_EVENT_BCN_TIMEOUT:   
        ieee80211_sta_power_start_tbtt_timer(powersta); 
        ieee80211_sta_power_set_power(powersta, IEEE80211_PWRSAVE_NETWORK_SLEEP); 
        event_handled = true;
        break;

    case IEEE80211_POWER_STA_EVENT_BCN_RECVD:    
        ieee80211_sta_power_start_tbtt_timer(powersta); 
        ieee80211_sta_power_set_power(powersta, IEEE80211_PWRSAVE_NETWORK_SLEEP); 
        OS_CANCEL_TIMER(&powersta->ips_bcn_timeout_timer);
        event_handled = true;
        break;

    default:
        break;
    }
    
    if (!event_handled) {
        return ieee80211_sta_power_common_event_handler(powersta, event, event_data_len, event_data); 
    }
    
    return event_handled;
}

static void ieee80211_sta_power_state_sleep_exit(void *ctx) 
{
    ieee80211_sta_power_t        powersta = (ieee80211_sta_power_t)ctx;
    ieee80211_sta_power_event    ps_event;
    
    OS_CANCEL_TIMER(&powersta->ips_bcn_timeout_timer);
    /* 
     * switch the power state to awake if not moving to PAUSE state.
     */
    if (ieee80211_sm_get_nextstate(powersta->ips_hsm_handle) !=   IEEE80211_POWER_STA_STATE_PAUSE) {
        ieee80211_sta_power_set_power(powersta, IEEE80211_PWRSAVE_AWAKE); 
        ps_event.type = IEEE80211_POWER_STA_AWAKE;
        ps_event.status = IEEE80211_POWER_STA_STATUS_SUCCESS;
        ieee80211_sta_power_deliver_event(powersta, &ps_event);
     }
}

/*
 * PSPOLL state.
 */
static void ieee80211_sta_power_state_pspoll_entry(void *ctx) 
{
    ieee80211_sta_power_t    powersta = (ieee80211_sta_power_t)ctx;
    int                      rc;
    
    wlan_vdev_txrx_register_event_handler(powersta->ips_vap->vdev_obj,
                                              ieee80211_sta_power_txrx_event_handler,
                                              powersta,
                                              (IEEE80211_VAP_INPUT_EVENT_UCAST | 
                                               IEEE80211_VAP_INPUT_EVENT_LAST_MCAST | 
                                               IEEE80211_VAP_OUTPUT_EVENT_DATA));
    ieee80211_sta_power_set_power(powersta, IEEE80211_PWRSAVE_AWAKE); 
    rc = ieee80211_send_pspoll(powersta->ips_vap->iv_bss);
    if (rc != EOK) {
        IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, "%s: ieee80211_send_pspoll failed rc=%d (%0x08X)\n",
                          __func__,
                          rc, rc);
    }
}

static bool ieee80211_sta_power_state_pspoll_event(void      *ctx, 
                                                   u_int16_t event, 
                                                   u_int16_t event_data_len, 
                                                   void      *event_data) 
{
    ieee80211_sta_power_t    powersta = (ieee80211_sta_power_t)ctx;
    bool                     event_handled = false;
    u_int32_t                more_data;
    int                      rc;
    
    switch(event) {
    case IEEE80211_POWER_STA_EVENT_STOP:               /* stop the SM*/
        ieee80211_sta_power_set_power(powersta, IEEE80211_PWRSAVE_AWAKE); 
        rc = ieee80211_send_nulldata(powersta->ips_vap->iv_bss, false);
        if (rc != EOK) {
            IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, "%s: ieee80211_send_nulldata failed PM=0 rc=%d (%0x08X)\n",
                              __func__,
                              rc, rc);
        }
        /* return false so that the COMMON state will handle the common part */
        break;
        
    case IEEE80211_POWER_STA_EVENT_TX:  
        if (!powersta->ips_pause_request) {
            ieee80211_sm_transition_to(powersta->ips_hsm_handle, IEEE80211_POWER_STA_STATE_ACTIVE);
        }
        event_handled = true;
        break;
        
    case IEEE80211_POWER_STA_EVENT_TIM:  
        rc = ieee80211_send_pspoll(powersta->ips_vap->iv_bss);
        if (rc != EOK) {
            IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, "%s: ieee80211_send_pspoll failed rc=%d (%0x08X)\n",
                              __func__,
                              rc, rc);
        }
        /* remain in the PSPOLL state */
        event_handled = true;
        break;
         
    case IEEE80211_POWER_STA_EVENT_RECV_UCAST:
        more_data = *((u_int32_t*) event_data);
        if (more_data) {
            if (powersta->ips_pspoll_moredata) {
                rc = ieee80211_send_pspoll(powersta->ips_vap->iv_bss);
                if (rc != EOK) {
                    IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, "%s: ieee80211_send_pspoll failed rc=%d (%0x08X)\n",
                                      __func__,
                                      rc, rc);
                }
                /* remain in the PSPOLL state */
            } else {
                ieee80211_sta_power_set_power(powersta, IEEE80211_PWRSAVE_AWAKE); 
                rc = ieee80211_send_nulldata(powersta->ips_vap->iv_bss, false);
                if (rc != EOK) {
                    IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, "%s: ieee80211_send_nulldata failed PM=0 rc=%d (%0x08X)\n",
                                      __func__,
                                      rc, rc);
                }
                ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_ACTIVE);
            }

        } else {
            ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_SLEEP);
        }
        event_handled = true;
        break;
        
    case IEEE80211_POWER_STA_EVENT_PAUSE:                    
        ieee80211_sm_transition_to(powersta->ips_hsm_handle, IEEE80211_POWER_STA_STATE_PAUSE);
        /* return false so that the COMMON state will handle the common part */
        break;
        
    default:
        break;
    }
    
    if (!event_handled) {
        return ieee80211_sta_power_common_event_handler(powersta, event, event_data_len, event_data); 
    }
    
    return event_handled;
}

/*
 * PAUSE state.
 */
static void ieee80211_sta_power_state_pause_entry(void *ctx) 
{
    ieee80211_sta_power_t        powersta = (ieee80211_sta_power_t)ctx;
    
    IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, "%s: \n",__func__);
    if (powersta->ips_pause_timeout) {
        OS_CANCEL_TIMER(&powersta->ips_pause_timer);
    }
    wlan_vdev_txrx_unregister_event_handler(powersta->ips_vap->vdev_obj, ieee80211_sta_power_txrx_event_handler, powersta);
}

static bool ieee80211_sta_power_state_pause_event(void      *ctx, 
                                                  u_int16_t event, 
                                                  u_int16_t event_data_len, 
                                                  void      *event_data) 
{
    ieee80211_sta_power_t    powersta = (ieee80211_sta_power_t)ctx;
    
    switch (event) {
    case IEEE80211_POWER_STA_EVENT_UNPAUSE:                    
    case IEEE80211_POWER_STA_EVENT_STOP:                    
        /* break so that the common event handler will handle the common part */
        break;
        
    default:
        return false;
    }
    
    return ieee80211_sta_power_common_event_handler(powersta, event, event_data_len, event_data); 
}

static void ieee80211_sta_power_state_pause_exit(void *ctx) 
{
    ieee80211_sta_power_t    powersta = (ieee80211_sta_power_t)ctx;
    /* wake up the chip if we are not transitioning to SLEEP state */
    if (ieee80211_sm_get_nextstate(powersta->ips_hsm_handle) !=   IEEE80211_POWER_STA_STATE_SLEEP) {
        ieee80211_sta_power_set_power(powersta, IEEE80211_PWRSAVE_AWAKE); 
    }
    IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, "%s: \n",__func__);
}

/*
 * PAUSE sbstate 1: PAUSE_INIT state(initial sub state).
 */
static void ieee80211_sta_power_state_pause_init_entry(void *ctx)
{
    ieee80211_sta_power_t        powersta = (ieee80211_sta_power_t)ctx;
    IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, "%s: \n",__func__);
    OS_SET_TIMER(&powersta->ips_pause_notif_timer,powersta->ips_pause_notif_timeout);
}

static bool ieee80211_sta_power_state_pause_init_event(void *ctx,
                                                  u_int16_t event, 
                                                  u_int16_t event_data_len, 
                                                  void      *event_data) 
{
    ieee80211_sta_power_t        powersta = (ieee80211_sta_power_t)ctx;
    bool                     event_handled = false;

    switch (event) {
       case IEEE80211_POWER_STA_EVENT_PAUSE_NOTIF_TIMEOUT :
         event_handled=true;
         ieee80211_sm_transition_to(powersta->ips_hsm_handle, IEEE80211_POWER_STA_STATE_PAUSE_NOTIF_DELAY);
         break;
       default:
         /* all the events are handled in the super state PAUSE event handler */
         break;
    }
   return event_handled;
}

static void ieee80211_sta_power_state_pause_init_exit(void *ctx)
{
   ieee80211_sta_power_t        powersta = (ieee80211_sta_power_t)ctx;
   OS_CANCEL_TIMER(&powersta->ips_pause_notif_timer);
}

/*
 * PAUSE sbstate 2: PAUSE_NOTIF_DELAY state.
 */

static void ieee80211_sta_power_state_pause_notif_delay_entry(void *ctx)
{
    ieee80211_sta_power_t        powersta = (ieee80211_sta_power_t)ctx;
    ieee80211_sta_power_event    event;
 
    event.type = IEEE80211_POWER_STA_PAUSE_COMPLETE;
    event.status = IEEE80211_POWER_STA_STATUS_SUCCESS;
    ieee80211_sta_power_deliver_event(powersta, &event);
    if (ieee80211_resmgr_exists(powersta->vap->iv_ic)) {
        /*
         * put the vap into network sleep when resource manager exists.
         * resource manager will determine the right chip state.
         */
        ieee80211_sta_power_set_power(powersta, IEEE80211_PWRSAVE_NETWORK_SLEEP);
    }  else {
        /*
         * put the vap into awake state when resource manager does not exists.
         * in this case the pause is used by scanner and putting the chip to netowrk
         * will result in not receive any beacons/probe responses.
         */
        ieee80211_sta_power_set_power(powersta, IEEE80211_PWRSAVE_AWAKE);
    }
    IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, "%s: \n",__func__);
}


static bool ieee80211_sta_power_state_pause_notif_delay_event(void *ctx, 
                                                  u_int16_t event, 
                                                  u_int16_t event_data_len, 
                                                  void      *event_data) 
{
   /* all the events are handled in the super state PAUSE event handler */
   return false;
}

/*
 * FORCE_SLEEP state.
 */
static void ieee80211_sta_power_state_force_sleep_entry(void *ctx) 
{
#if DA_SUPPORT
    ieee80211_sta_power_t    powersta = (ieee80211_sta_power_t)ctx;
#endif
    wlan_vdev_txrx_register_event_handler(powersta->ips_vap->vdev_obj,
                                              ieee80211_sta_power_txrx_event_handler,
                                              powersta,
                                              IEEE80211_VAP_INPUT_EVENT_UCAST);
}

static bool ieee80211_sta_power_state_force_sleep_event(void      *ctx, 
                                                        u_int16_t event, 
                                                        u_int16_t event_data_len, 
                                                        void      *event_data) 
{
    ieee80211_sta_power_t    powersta = (ieee80211_sta_power_t)ctx;
    int                      rc;
    bool                     event_handled = false;
    u_int32_t                more_data;


    switch (event) {
    case IEEE80211_POWER_STA_EVENT_STOP:                    
    case IEEE80211_POWER_STA_EVENT_FORCE_AWAKE:                    
        rc = ieee80211_send_nulldata(powersta->ips_vap->iv_bss, false);
        if (rc != EOK) {
            IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, "%s: ieee80211_send_nulldata failed PM=0 rc=%d (%0x08X)\n",
                              __func__,
                              rc, rc);
        }
        if (powersta->ips_sta_psmode == IEEE80211_PWRSAVE_NONE) {
            ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_INIT);
        } else {
            ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_ACTIVE);
        }
        break;

    case IEEE80211_POWER_STA_EVENT_TIM:
    case IEEE80211_POWER_STA_EVENT_DTIM:
        if (powersta->ips_use_pspoll ) {
            rc = ieee80211_send_pspoll(powersta->ips_vap->iv_bss);
            if (rc != EOK) {
                IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, "%s: ieee80211_send_pspoll failed rc=%d (%0x08X)\n",
                                  __func__,
                                  rc, rc);
            }
        }
		else {
            if (powersta->ips_vap->iv_uapsd != 0) {
                /*
                 * If WMM Power Save is enabled then transmit qosnull frames.
                */

                /* Force a QoS Null for testing. */
                rc = ieee80211_send_qosnulldata(powersta->ips_vap->iv_bss, WME_AC_BK, 0);
                if (rc != EOK) {
                    IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, "%s: ieee80211_send_qosnulldata failed rc=%d (%0x08X)\n",
                                  __func__,
                                  rc, rc);
                }
            }
        }
        /* remain in the PSPOLL state */
        event_handled = true;
        break;

    case IEEE80211_POWER_STA_EVENT_RECV_UCAST:
        more_data = *((u_int32_t*) event_data);
        if (powersta->ips_use_pspoll && more_data) {
            if (powersta->ips_pspoll_moredata) {
                rc = ieee80211_send_pspoll(powersta->ips_vap->iv_bss);
                if (rc != EOK) {
                    IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, "%s: ieee80211_send_pspoll failed rc=%d (%0x08X)\n",
                                      __func__,
                                      rc, rc);
                }
            }
        }
        event_handled = true;
        break;


    default:
        return false;
    }

    if (!event_handled) {
        return ieee80211_sta_power_common_event_handler(powersta, event, event_data_len, event_data); 
    }
    return false;

}

static void ieee80211_sta_power_state_force_sleep_exit(void *ctx) 
{
}

#if UMAC_SUPPORT_WNM
/*
 * WNMREQ_SENT state.
 */
static OS_TIMER_FUNC(ieee80211_sta_power_wnmsleep_timeout_event_timer)
{
    ieee80211_sta_power_t    powersta;
    
    OS_GET_TIMER_ARG(powersta, ieee80211_sta_power_t);
    ieee80211_sm_dispatch(powersta->ips_hsm_handle, IEEE80211_POWER_STA_EVENT_WNMSLEEP_SEND_TIMEOUT, 0, NULL);
}


/* handler for sw-assisted wnmsleep tsf timer */
static void ieee80211_sta_power_wnmsleep_handler(tsftimer_handle_t h_tsftime, int arg1, void *arg2, u_int32_t curr_tsf)
{
    ieee80211_sta_power_t    powersta  = (ieee80211_sta_power_t)arg2;
    
    ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_WNMREQ_SENT);              
    OS_SET_TIMER(&powersta->ips_bcn_timeout_timer, IEEE80211_BEACON_TIMEOUT);
}

static void ieee80211_sta_power_state_wnmreq_sent_entry(void *ctx)
{
    ieee80211_sta_power_t    powersta = (ieee80211_sta_power_t)ctx;

    wlan_vdev_txrx_register_event_handler(powersta->ips_vap->vdev_obj,
                                              ieee80211_sta_power_txrx_event_handler,
                                              powersta,
                                              (IEEE80211_VAP_INPUT_EVENT_UCAST |
                                               IEEE80211_VAP_OUTPUT_EVENT_DATA |
                                               IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_PS_NULL|
                                               IEEE80211_VAP_INPUT_EVENT_WNMSLEEP_RESP));

    /* read the sleep interval from vap */
    powersta->ips_wnmsleep_intval = powersta->ips_vap->iv_wnmsleep_intval;
    ieee80211_sta_power_set_power(powersta, IEEE80211_PWRSAVE_AWAKE);
    if (!powersta->ips_wnmsleep_entered)
        ieee80211_sta_power_pause_data(powersta);
    else
        ieee80211_sta_power_unpause_data(powersta);

    /* Ask app to send sleep req frame */
    if (!powersta->ips_wnmsleep_entered) {
        ieee80211_wnm_sleepreq_to_app(powersta->ips_vap, IEEE80211_WNMSLEEP_ACTION_ENTER, powersta->ips_wnmsleep_intval);
    }
    else {
        ieee80211_wnm_sleepreq_to_app(powersta->ips_vap, IEEE80211_WNMSLEEP_ACTION_EXIT, powersta->ips_wnmsleep_intval);
    }

    /* start the timeout for response frame */
    OS_SET_TIMER(&powersta->ips_wnmsleep_timeout_timer, IEEE80211_WNMSLEEP_TIMEOUT);
}

static bool ieee80211_sta_power_state_wnmreq_sent_event(void      *ctx,
                                                        u_int16_t event,
                                                        u_int16_t event_data_len,
                                                        void      *event_data)
{
    ieee80211_sta_power_t    powersta = (ieee80211_sta_power_t)ctx;
    bool                     event_handled = false;

    switch(event) {
    case IEEE80211_POWER_STA_EVENT_WNMSLEEP_SEND_TIMEOUT:
        ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_ACTIVE);
        OS_CANCEL_TIMER(&powersta->ips_wnmsleep_timeout_timer);
        event_handled = true;
        break;

    case IEEE80211_POWER_STA_EVENT_WNMSLEEP_CONFIRM_ENTER:
        if (powersta->ips_pause_request) {
           ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_PAUSE);
        }  else if (powersta->ips_force_sleep) {
           ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_FORCE_SLEEP);
        } else if (!powersta->ips_wnmsleep_entered) {
            ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_SLEEP);
            powersta->ips_wnmsleep_entered = true;
            OS_CANCEL_TIMER(&powersta->ips_wnmsleep_timeout_timer);
        }
        event_handled = true;
        break;

    case IEEE80211_POWER_STA_EVENT_WNMSLEEP_CONFIRM_EXIT:
        if (powersta->ips_wnmsleep_entered) {
            powersta->ips_wnmsleep_entered = false;
            ieee80211_sm_transition_to(powersta->ips_hsm_handle,IEEE80211_POWER_STA_STATE_ACTIVE);
        }
        event_handled = true;
        break;

    case IEEE80211_POWER_STA_EVENT_STOP:               /* stop the SM*/
        if (!powersta->ips_pause_request) {
            int rc = ieee80211_send_nulldata(powersta->ips_vap->iv_bss, false);
            if (rc != EOK) {
                IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, "%s: ieee80211_send_nulldata failed PM=0 rc=%d (%0x08X)\n",
                                  __func__,
                                  rc, rc);
            }
        }
        powersta->ips_wnmsleep_entered = false;
        /* return false so that the common event handler will handle the common part */
        break;

    case IEEE80211_POWER_STA_EVENT_TIM:
    case IEEE80211_POWER_STA_EVENT_TX:
        /* Do not handle these event when STA is exiting WNM-Sleep */
        if (!powersta->ips_pause_request && !powersta->ips_wnmsleep_entered) {
            ieee80211_sm_transition_to(powersta->ips_hsm_handle, IEEE80211_POWER_STA_STATE_ACTIVE);
        }

        event_handled = true;
        break;

    default:
        /* return false so that the common event handler will handle the common part */
        break;
    }

    if (!event_handled) {
        return ieee80211_sta_power_common_event_handler(powersta, event, event_data_len, event_data);
    }

    return true;
}

static void ieee80211_sta_power_state_wnmreq_sent_exit(void *ctx)
{
    ieee80211_sta_power_t    powersta = (ieee80211_sta_power_t)ctx;

    OS_CANCEL_TIMER(&powersta->ips_wnmsleep_timeout_timer);
}
#endif

ieee80211_state_info ieee80211_sta_power_sm_info[] = {
    { 
        IEEE80211_POWER_STA_STATE_INIT, 
        IEEE80211_HSM_STATE_NONE,
        IEEE80211_HSM_STATE_NONE,
        false,
        "INIT",
        ieee80211_sta_power_state_init_entry,
        ieee80211_sta_power_state_init_exit,
        ieee80211_sta_power_state_init_event
    },
    { 
        IEEE80211_POWER_STA_STATE_ACTIVE, 
        IEEE80211_HSM_STATE_NONE,
        IEEE80211_HSM_STATE_NONE,
        false,
        "ACTIVE",
        ieee80211_sta_power_state_active_entry,
        ieee80211_sta_power_state_active_exit,
        ieee80211_sta_power_state_active_event
    },
    { 
        IEEE80211_POWER_STA_STATE_PENDING_TX, 
        IEEE80211_HSM_STATE_NONE,
        IEEE80211_HSM_STATE_NONE,
        false,
        "PENDING_TX",
        ieee80211_sta_power_state_pending_tx_entry,
        ieee80211_sta_power_state_pending_tx_exit,
        ieee80211_sta_power_state_pending_tx_event
    },
    { 
        IEEE80211_POWER_STA_STATE_NULL_SENT, 
        IEEE80211_HSM_STATE_NONE,
        IEEE80211_HSM_STATE_NONE,
        false,
        "NULL_SENT",
        ieee80211_sta_power_state_null_sent_entry,
        ieee80211_sta_power_state_null_sent_exit,
        ieee80211_sta_power_state_null_sent_event
    },
    { 
        IEEE80211_POWER_STA_STATE_SLEEP, 
        IEEE80211_HSM_STATE_NONE,
        IEEE80211_HSM_STATE_NONE,
        false,
        "SLEEP",
        ieee80211_sta_power_state_sleep_entry,
        ieee80211_sta_power_state_sleep_exit,
        ieee80211_sta_power_state_sleep_event
    },
    { 
        IEEE80211_POWER_STA_STATE_PSPOLL, 
        IEEE80211_HSM_STATE_NONE,
        IEEE80211_HSM_STATE_NONE,
        false,
        "PSPOLL",
        ieee80211_sta_power_state_pspoll_entry,
        NULL,
        ieee80211_sta_power_state_pspoll_event
    },
    { 
        IEEE80211_POWER_STA_STATE_PAUSE, 
        IEEE80211_HSM_STATE_NONE,
        IEEE80211_POWER_STA_STATE_PAUSE_INIT,  /* initial sub state */
        true,    /* has sub states */
        "PAUSE",
        ieee80211_sta_power_state_pause_entry,
        ieee80211_sta_power_state_pause_exit,
        ieee80211_sta_power_state_pause_event
    },
    { 
        IEEE80211_POWER_STA_STATE_PAUSE_INIT, 
        IEEE80211_POWER_STA_STATE_PAUSE,
        IEEE80211_HSM_STATE_NONE,
        false,
        "PAUSE_INIT",
        ieee80211_sta_power_state_pause_init_entry,
        ieee80211_sta_power_state_pause_init_exit,
        ieee80211_sta_power_state_pause_init_event
    },
    { 
        IEEE80211_POWER_STA_STATE_PAUSE_NOTIF_DELAY, 
        IEEE80211_POWER_STA_STATE_PAUSE,
        IEEE80211_HSM_STATE_NONE,
        false,
        "PAUSE_NOTIF_DELAY",
        ieee80211_sta_power_state_pause_notif_delay_entry,
        NULL,
        ieee80211_sta_power_state_pause_notif_delay_event
    },
    { 
        IEEE80211_POWER_STA_STATE_FORCE_SLEEP, 
        IEEE80211_HSM_STATE_NONE,
        IEEE80211_HSM_STATE_NONE,
        false,
        "FORCE_SLEEP",
        ieee80211_sta_power_state_force_sleep_entry,
        ieee80211_sta_power_state_force_sleep_exit,
        ieee80211_sta_power_state_force_sleep_event
    },
#if UMAC_SUPPORT_WNM
    {
        IEEE80211_POWER_STA_STATE_WNMREQ_SENT,
        IEEE80211_HSM_STATE_NONE,
        IEEE80211_HSM_STATE_NONE,
        false,
        "WNMREQ_SENT",
        ieee80211_sta_power_state_wnmreq_sent_entry,
        ieee80211_sta_power_state_wnmreq_sent_exit,
        ieee80211_sta_power_state_wnmreq_sent_event
    },
#endif
};

void ieee80211_sta_power_vap_event_handler( ieee80211_sta_power_t    powersta, 
                                                  ieee80211_vap_event *event) 
{
    struct ieee80211vap    *vap = powersta->ips_vap;
    struct ieee80211_node    *ni = vap->iv_bss;

    switch(event->type) 
    {
    case IEEE80211_VAP_UP:
        ni = powersta->ips_vap->iv_bss;
        powersta->ips_connected     = true;
        ieee80211_sta_power_update_sm(powersta);

        /*
         * allocate a tsf timer to be used for SW assisted powersave.
         * this is required when 2 STA vaps are active and HW has only one tsf clock.
         * in which case the HW will automatically wakeup at tbtt of the primary STA
         * and receive the beacon. For the secondary STA the HW wakeup is done using
         * the tsf timer. 
         */
        if (!powersta->ips_tbtt_tsf_timer ) { 
            powersta->ips_tbtt_tsf_timer = ieee80211_tsftimer_alloc(vap->iv_ic->ic_tsf_timer , 0,
                                                           ieee80211_sta_power_tbtt_handler,
                                                           0,powersta,
                                                           ieee80211_sta_power_tsf_changed_handler);
        }

#if UMAC_SUPPORT_WNM
        if (!powersta->ips_wnmsleep_tsf_timer ) {
            powersta->ips_wnmsleep_tsf_timer = ieee80211_tsftimer_alloc(vap->iv_ic->ic_tsf_timer , 0,
                                                           ieee80211_sta_power_wnmsleep_handler,
                                                           0,powersta,
                                                           ieee80211_sta_power_tsf_changed_handler);
        }
#endif

        break;
        
    case IEEE80211_VAP_FULL_SLEEP:
    case IEEE80211_VAP_DOWN:
        powersta->ips_nw_sleep=false;
        if (powersta->ips_connected) {
            powersta->ips_connected         = false;
            powersta->ips_apPmState         = false;
            ieee80211_sta_power_update_sm(powersta);
        }
        if (powersta->ips_tbtt_tsf_timer) {
           if (ieee80211_tsftimer_stop(powersta->ips_tbtt_tsf_timer)) {
               IEEE80211_DPRINTF(vap,IEEE80211_MSG_POWER,
               "%s: ERROR: ieee80211_tsftimer_stop returns error \n", __func__);
           }
           if (ieee80211_tsftimer_free(powersta->ips_tbtt_tsf_timer, true) ) {
              IEEE80211_DPRINTF(vap,IEEE80211_MSG_POWER,
                  "%s: ERROR: ieee80211_tsftimer_free returns error \n", __func__);
           }
           powersta->ips_tbtt_tsf_timer = NULL;
        }
#if UMAC_SUPPORT_WNM
        if (powersta->ips_wnmsleep_tsf_timer) {
           if (ieee80211_tsftimer_stop(powersta->ips_wnmsleep_tsf_timer)) {
               IEEE80211_DPRINTF(vap,IEEE80211_MSG_POWER,
               "%s: ERROR: ieee80211_tsftimer_stop returns error \n", __func__);
           }
           if (ieee80211_tsftimer_free(powersta->ips_wnmsleep_tsf_timer, true) ) {
              IEEE80211_DPRINTF(vap,IEEE80211_MSG_POWER,
                  "%s: ERROR: ieee80211_tsftimer_free returns error \n", __func__);
           }
           powersta->ips_wnmsleep_tsf_timer = NULL;
        }
#endif

        break;

    default :
        break;

    }
}

static OS_TIMER_FUNC(ieee80211_sta_power_async_null_event_timer)
{
    ieee80211_sta_power_t    powersta;
    
    OS_GET_TIMER_ARG(powersta, ieee80211_sta_power_t);
    ieee80211_sm_dispatch(powersta->ips_hsm_handle, IEEE80211_POWER_STA_EVENT_SEND_NULL_FAILED, 0, NULL);
}

static OS_TIMER_FUNC(ieee80211_sta_power_activity_timer)
{
    systime_t                lastData;
    ieee80211_sta_power_t    powersta;
    struct ieee80211vap      *vap;
    struct ieee80211com      *ic;
    
    OS_GET_TIMER_ARG(powersta, ieee80211_sta_power_t);

    vap = powersta->ips_vap;
    ic = vap->iv_ic;

    lastData = ieee80211_get_last_data_timestamp(vap);

    /*
     * re arm the timer.
     * it may be cancelled part of event handling.
     */
    OS_SET_TIMER(&powersta->ips_timer,IEEE80211_PWRSAVE_TIMER_INTERVAL);

    if ((CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP() - lastData) > powersta->ips_inactivitytime) && 
        !ic->ic_need_beacon_sync(ic) &&
        (vap->iv_bmiss_count == 0) ) 
    {
        ieee80211_sm_dispatch(powersta->ips_hsm_handle,IEEE80211_POWER_STA_EVENT_NO_ACTIVITY , 0, NULL);
    } 

    if (ic->ic_txq_depth(ic) == 0) {
        ieee80211_sm_dispatch(powersta->ips_hsm_handle,IEEE80211_POWER_STA_EVENT_NO_PENDING_TX, 0, NULL);
    }
}

static OS_TIMER_FUNC(ieee80211_sta_power_pause_timer)
{
   ieee80211_sta_power_t    powersta;
   
   OS_GET_TIMER_ARG(powersta, ieee80211_sta_power_t);
   ieee80211_sm_dispatch(powersta->ips_hsm_handle,IEEE80211_POWER_STA_EVENT_PAUSE_TIMEOUT, 0, NULL);
}

static OS_TIMER_FUNC(ieee80211_sta_power_pause_notif_timer)
{
   ieee80211_sta_power_t    powersta;
   
   OS_GET_TIMER_ARG(powersta, ieee80211_sta_power_t);
   ieee80211_sm_dispatch(powersta->ips_hsm_handle,IEEE80211_POWER_STA_EVENT_PAUSE_NOTIF_TIMEOUT, 0, NULL);

}
static void ieee80211_sta_power_sm_debug_print (void *ctx,const char *fmt,...) 
{
#if ATH_DEBUG
    ieee80211_sta_power_t    powersta = (ieee80211_sta_power_t)ctx;
#endif
    char                     tmp_buf[256];
    va_list                  ap;
     
    va_start(ap, fmt);
    vsnprintf (tmp_buf,256, fmt, ap);
    va_end(ap);
    IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, "%s", tmp_buf);
}

ieee80211_sta_power_t ieee80211_sta_power_vattach(struct ieee80211vap *vap,
                                                  int                 fullsleep_enable,
                                                  u_int32_t           max_sleeptime,
                                                  u_int32_t           norm_sleeptime,
                                                  u_int32_t           low_sleeptime,
                                                  u_int32_t           max_inactivitytime,
                                                  u_int32_t           norm_inactivitytime,
                                                  u_int32_t           low_inactivitytime,
                                                  u_int32_t           pspoll_enabled)
{
    ieee80211_sta_power_t    handle;
    int                      i;
    osdev_t                  os_handle = vap->iv_ic->ic_osdev;

    handle = (ieee80211_sta_power_t)OS_MALLOC(os_handle,sizeof(struct ieee80211_sta_power),0);
    if (!handle) {
        return handle;
    }
    OS_MEMZERO(handle, sizeof(struct ieee80211_sta_power));
    handle->ips_hsm_handle = ieee80211_sm_create(os_handle, 
                                                 "pwrsav_sta",
                                                 (void *) handle, 
                                                 IEEE80211_POWER_STA_STATE_INIT,
                                                 ieee80211_sta_power_sm_info, 
                                                 sizeof(ieee80211_sta_power_sm_info)/sizeof(ieee80211_state_info),
                                                 MAX_QUEUED_EVENTS, 
                                                 sizeof(u_int32_t) /* event data of size int */, 
                                                 MESGQ_PRIORITY_HIGH,
                                                 OS_MESGQ_CAN_SEND_SYNC() ? MESGQ_SYNCHRONOUS_EVENT_DELIVERY : MESGQ_ASYNCHRONOUS_EVENT_DELIVERY,
                                                 ieee80211_sta_power_sm_debug_print,
                                                 sta_power_event_name,
                                                 IEEE80211_N(sta_power_event_name));
    if (handle->ips_hsm_handle == NULL) {
        OS_FREE(handle);
        return NULL;
    }

    handle->ips_run_async = OS_MESGQ_CAN_SEND_SYNC() ? false : true;

    IPS_LOCK_INIT(handle);
    for (i = 0;i < IEEE80211_MAX_POWER_STA_EVENT_HANDLERS; ++i) {                         
        handle->ips_event_handlers[i] = NULL;
    }
    /*
     * Initialize pwrsave timer 
     */
    OS_INIT_TIMER(os_handle, &handle->ips_timer,                         
                  ieee80211_sta_power_activity_timer, handle, QDF_TIMER_TYPE_WAKE_APPS);
    OS_INIT_TIMER(os_handle, &handle->ips_pause_timer,                         
                  ieee80211_sta_power_pause_timer, handle, QDF_TIMER_TYPE_WAKE_APPS);
    OS_INIT_TIMER(os_handle, &handle->ips_pause_notif_timer,                         
                  ieee80211_sta_power_pause_notif_timer, handle, QDF_TIMER_TYPE_WAKE_APPS);
    OS_INIT_TIMER(os_handle, &handle->ips_async_null_event_timer,                         
                  ieee80211_sta_power_async_null_event_timer, handle, QDF_TIMER_TYPE_WAKE_APPS);
    OS_INIT_TIMER(os_handle, &handle->ips_bcn_timeout_timer,
                  ieee80211_sta_power_bcn_timeout_event_timer, handle, QDF_TIMER_TYPE_WAKE_APPS);
#if UMAC_SUPPORT_WNM
    OS_INIT_TIMER(os_handle, &handle->ips_wnmsleep_timeout_timer,
                  ieee80211_sta_power_wnmsleep_timeout_event_timer, handle, QDF_TIMER_TYPE_WAKE_APPS);
#endif
    handle->ips_inactivitytime        = IEEE80211_PS_INACTIVITYTIME;
    handle->ips_sta_psmode            = IEEE80211_PWRSAVE_NONE;
    handle->ips_connected             = false;
    handle->ips_fullsleep_enable      = fullsleep_enable;
    handle->ips_apPmState             = false;

    handle->ips_max_sleeptime         = max_sleeptime;
    handle->ips_normal_sleeptime      = norm_sleeptime;
    handle->ips_low_sleeptime         = low_sleeptime;
    handle->ips_max_inactivitytime    = max_inactivitytime;
    handle->ips_normal_inactivitytime = norm_inactivitytime;
    handle->ips_low_inactivitytime    = low_inactivitytime;
    handle->ips_vap = vap;
    handle->ips_use_pspoll            = pspoll_enabled;

    handle->ips_pwr_arbiter_id        = ieee80211_power_arbiter_alloc_id(vap, 
                                                                         "STATION", 
                                                                         IEEE80211_PWR_ARBITER_TYPE_STA);
    if (handle->ips_pwr_arbiter_id != 0) {
        (void) ieee80211_power_arbiter_enable(vap, handle->ips_pwr_arbiter_id);
    }

    ieee80211_vap_register_misc_events(vap, handle, &misc_evtab);

    atomic_set(&handle->ips_tx_in_progress_count,0);

#if UMAC_SUPPORT_WNM
    handle->ips_wnmsleep_intval = 0;
    handle->ips_wnmsleep_entered = false;
#endif

    return handle;
}

void ieee80211_sta_power_vdetach(ieee80211_sta_power_t handle)
{
    ieee80211_vap_unregister_misc_events(handle->ips_vap, handle, &misc_evtab);
    wlan_vdev_txrx_unregister_event_handler(handle->ips_vap->vdev_obj, ieee80211_sta_power_txrx_event_handler, handle);
    
    if (handle->ips_pwr_arbiter_id != 0) {
        (void) ieee80211_power_arbiter_disable(handle->ips_vap, handle->ips_pwr_arbiter_id);
        ieee80211_power_arbiter_free_id(handle->ips_vap, handle->ips_pwr_arbiter_id);
        handle->ips_pwr_arbiter_id = 0;
    }

    OS_CANCEL_TIMER(&handle->ips_timer);
    OS_CANCEL_TIMER(&handle->ips_pause_timer);
    OS_CANCEL_TIMER(&handle->ips_pause_notif_timer);
    OS_CANCEL_TIMER(&handle->ips_async_null_event_timer);
    OS_CANCEL_TIMER(&handle->ips_bcn_timeout_timer);
#if UMAC_SUPPORT_WNM
    OS_CANCEL_TIMER(&handle->ips_wnmsleep_timeout_timer);
#endif

    ieee80211_sm_delete(handle->ips_hsm_handle); 
    
    OS_FREE_TIMER(&handle->ips_timer);
    OS_FREE_TIMER(&handle->ips_pause_timer);
    OS_FREE_TIMER(&handle->ips_pause_notif_timer);
    OS_FREE_TIMER(&handle->ips_async_null_event_timer);
    OS_FREE_TIMER(&handle->ips_bcn_timeout_timer);
#if UMAC_SUPPORT_WNM
    OS_FREE_TIMER(&handle->ips_wnmsleep_timeout_timer);
#endif
    
    OS_FREE(handle);
}

void _ieee80211_sta_power_event_tim(struct ieee80211vap *vap)
{
    ieee80211_sta_power_t handle = vap->iv_pwrsave_sta;

    if (!handle) {
        return;
    }
    ieee80211_sm_dispatch(handle->ips_hsm_handle,IEEE80211_POWER_STA_EVENT_TIM, 0, NULL);
}

void _ieee80211_sta_power_event_dtim(struct ieee80211vap *vap)
{
    ieee80211_sta_power_t handle = vap->iv_pwrsave_sta;

    if (!handle) {
        return;
    }
    ieee80211_sm_dispatch(handle->ips_hsm_handle,IEEE80211_POWER_STA_EVENT_DTIM, 0, NULL);
}

#if NOT_USED
void  ieee80211_sta_power_event_recvd_beacon(ieee80211_sta_power_t handle)
{
    if (!handle) {
        return;
    }
    ieee80211_sm_dispatch(handle->ips_hsm_handle,IEEE80211_POWER_STA_EVENT_BCN_RECVD, 0, NULL);
}
#endif

#if DA_SUPPORT
static void ieee80211_sta_power_txrx_event_handler (struct wlan_objmgr_vdev *vdev, ieee80211_vap_txrx_event *event, void *arg)
{
    ieee80211_sta_power_t    powersta = (ieee80211_sta_power_t)arg;
    u_int32_t                more_data;
#if UMAC_SUPPORT_WNM
    unsigned long            flags;
#endif
    
    switch(event->type) {
    case IEEE80211_VAP_INPUT_EVENT_UCAST:
        more_data = event->u.more_data;
        ieee80211_sm_dispatch(powersta->ips_hsm_handle,IEEE80211_POWER_STA_EVENT_RECV_UCAST, sizeof(u_int32_t),&more_data);
        break;
        
    case IEEE80211_VAP_INPUT_EVENT_LAST_MCAST:
        ieee80211_sm_dispatch(powersta->ips_hsm_handle,IEEE80211_POWER_STA_EVENT_LAST_MCAST, 0, NULL);
        break;
        
#if UMAC_SUPPORT_WNM
    case IEEE80211_VAP_INPUT_EVENT_WNMSLEEP_RESP:
        /* ioctl context and soft irq */
        local_irq_save(flags);
        if (event->u.status) {
            if (!powersta->ips_wnmsleep_entered)
                ieee80211_sm_dispatch(powersta->ips_hsm_handle, IEEE80211_POWER_STA_EVENT_WNMSLEEP_CONFIRM_ENTER, 0, NULL);
            else
                ieee80211_sm_dispatch(powersta->ips_hsm_handle, IEEE80211_POWER_STA_EVENT_WNMSLEEP_CONFIRM_EXIT, 0, NULL);
        } else {
            /* Hack: use the timeout event to exit */
            ieee80211_sm_dispatch(powersta->ips_hsm_handle, IEEE80211_POWER_STA_EVENT_WNMSLEEP_SEND_TIMEOUT, 0, NULL);
        }
        local_irq_restore(flags);
        break;
#endif

    case IEEE80211_VAP_OUTPUT_EVENT_DATA:
        ieee80211_sm_dispatch(powersta->ips_hsm_handle,IEEE80211_POWER_STA_EVENT_TX, 0, NULL);
        break;
        
    case IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_PS_NULL:
        if (event->u.status == 0) {
            ieee80211_sm_dispatch(powersta->ips_hsm_handle,IEEE80211_POWER_STA_EVENT_SEND_NULL_SUCCESS, 0, NULL);
        } else {
            /* 
             * this could have been failed synchronously from the SM with the ieee80211_send_nulldata() function call
             * do not dispatch the event synchronously instead send it asynchronously.
             */
             OS_SET_TIMER(&powersta->ips_async_null_event_timer,0);
        }
        break;
        
    case IEEE80211_VAP_OUTPUT_EVENT_TXQ_EMPTY:
        ieee80211_sm_dispatch(powersta->ips_hsm_handle,IEEE80211_POWER_STA_EVENT_NO_PENDING_TX, 0, NULL);
        break;
        
    default:
        break;
    }
}
#endif

static void ieee80211_sta_power_bmiss_handler(void *arg)
{
    ieee80211_sta_power_t    powersta = (ieee80211_sta_power_t)arg;
    
    ieee80211_sm_dispatch(powersta->ips_hsm_handle,IEEE80211_POWER_STA_EVENT_TX, 0, NULL);
}

int _ieee80211_sta_power_send_keepalive(struct ieee80211vap *vap)
{
    ieee80211_sta_power_t powersta = vap->iv_pwrsave_sta;

    if (powersta && powersta->ips_connected) {
        ieee80211_sm_dispatch(powersta->ips_hsm_handle,IEEE80211_POWER_STA_EVENT_SEND_KEEP_ALIVE, 0, NULL);
        return EOK;
    }
    
    return EINVAL;
}

int _ieee80211_sta_power_pause(struct ieee80211vap  *vap,
                               u_int32_t            pause_timeout)
{
    ieee80211_sta_power_t powersta = vap->iv_pwrsave_sta;
    
    if (!powersta) {
        return EINVAL;
    }    

    if (powersta->ips_connected) {
        struct ieee80211_node *ni;
        ni = ieee80211_try_ref_bss_node(vap);
        if (ni) {
             ieee80211node_pause(ni);
             ieee80211_free_node(ni);
        }
        ieee80211_sm_dispatch(powersta->ips_hsm_handle,IEEE80211_POWER_STA_EVENT_PAUSE, sizeof(u_int32_t), &pause_timeout);
        return EOK;
    }
    
    return EINVAL;
}

int _ieee80211_sta_power_unpause(struct ieee80211vap *vap)
{
    ieee80211_sta_power_t powersta = vap->iv_pwrsave_sta;

    if (!powersta) {
        return EINVAL;
    }

    if (powersta->ips_connected) {
        struct ieee80211_node *ni;
        ieee80211_sm_dispatch(powersta->ips_hsm_handle,IEEE80211_POWER_STA_EVENT_UNPAUSE, 0, NULL);
        ni = ieee80211_try_ref_bss_node(vap);
        if (ni) {
            ieee80211node_unpause(ni);
            ieee80211_free_node(ni);
        }
        return EOK;
    }
    return EINVAL;
}

static void ieee80211_sta_power_update_sm(ieee80211_sta_power_t powersta)
{
    if (powersta->ips_connected &&  powersta->ips_sta_psmode != IEEE80211_PWRSAVE_NONE) {
        ieee80211_sm_dispatch(powersta->ips_hsm_handle, IEEE80211_POWER_STA_EVENT_START, 0, NULL);
    } else {
        ieee80211_sm_dispatch(powersta->ips_hsm_handle, IEEE80211_POWER_STA_EVENT_STOP, 0, NULL);
    }
}

static void ieee80211_sta_power_set_mode(ieee80211_sta_power_t powersta, 
                                         u_int32_t             mode)
{
#if ATH_DEBUG
    u_int32_t    now = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());
#endif
#if UMAC_SUPPORT_WNM
    unsigned long flags = 0;
    u_int32_t prev_mode;
#endif

    IEEE80211_DPRINTF(powersta->ips_vap, IEEE80211_MSG_POWER, "%d.%03d | %s: mode=%02d=>%02d\n",
                      now / 1000, now % 1000, __func__,
                      powersta->ips_sta_psmode, mode);

    if (powersta->ips_vap->iv_opmode != IEEE80211_M_STA) {
        return;
    }

    if (mode > IEEE80211_PWRSAVE_WNM) {
        return;
    }
    /*
     * send mode update event.
     */
    if (mode != powersta->ips_sta_psmode) {
        switch (mode) {
        case IEEE80211_PWRSAVE_NONE:
            break;
            
        case IEEE80211_PWRSAVE_LOW:
            powersta->ips_inactivitytime = powersta->ips_low_inactivitytime;
            powersta->ips_vap->iv_update_ps_mode(powersta->ips_vap);
            break;
            
        case IEEE80211_PWRSAVE_NORMAL:
            powersta->ips_inactivitytime = powersta->ips_normal_inactivitytime;
            powersta->ips_vap->iv_update_ps_mode(powersta->ips_vap);
            break;
            
        case IEEE80211_PWRSAVE_MAXIMUM:
            powersta->ips_inactivitytime = powersta->ips_max_inactivitytime;
            powersta->ips_vap->iv_update_ps_mode(powersta->ips_vap);
            break;

#if UMAC_SUPPORT_WNM
        case IEEE80211_PWRSAVE_WNM:
            powersta->ips_inactivitytime = powersta->ips_low_inactivitytime;
            powersta->ips_vap->iv_update_ps_mode(powersta->ips_vap);
            break;
#endif
 
        default:
            break;
        }
#if UMAC_SUPPORT_WNM
        prev_mode = powersta->ips_sta_psmode;
#endif
        powersta->ips_sta_psmode = mode;
#if UMAC_SUPPORT_WNM
        if (prev_mode == IEEE80211_PWRSAVE_WNM)
            local_irq_save(flags);
#endif
        ieee80211_sta_power_update_sm(powersta);
#if UMAC_SUPPORT_WNM
        if (prev_mode == IEEE80211_PWRSAVE_WNM)
            local_irq_restore(flags);
#endif
    }
}

/*
 * enable/disable pspoll mode of operation.
 */
int ieee80211_sta_power_set_pspoll(struct ieee80211vap *vap, u_int32_t pspoll)
{
    ieee80211_sta_power_t powersta = vap->iv_pwrsave_sta;

    if (!powersta) {
        return EOK;
    }

    if (pspoll) {
        powersta->ips_use_pspoll = true;
    } else {
        powersta->ips_use_pspoll = false;
    }
    return EOK;
}
 
/*
 * enable/disable a mode  when enabled, the SM will send pspolls (in response to TIM)
 * to receive  data frames (one pspoll at a time) until all the data 
 * frames are received (data frame with more data bit cleared)    
 * when disabled, it will only send one pspoll in response to TIM 
 * to receive first data frame and if more frames are  queued on the AP
 * (more dat bit set on the received frame) the driver will wake up
 * by sending null frame with PS=0.
 */
int ieee80211_sta_power_set_pspoll_moredata_handling(struct ieee80211vap *vap, ieee80211_pspoll_moredata_handling mode)
{
    ieee80211_sta_power_t powersta = vap->iv_pwrsave_sta;

    if (!powersta) {
        return EOK;
    }

    if (mode == IEEE80211_CONTINUE_PSPOLL_FOR_MORE_DATA) {
        powersta->ips_pspoll_moredata= true;
    } else {
        powersta->ips_pspoll_moredata = false;
    }
    return EOK;
}

/*
* get state of pspoll.
*/
u_int32_t ieee80211_sta_power_get_pspoll(struct ieee80211vap *vap)
{
    ieee80211_sta_power_t powersta = vap->iv_pwrsave_sta;

    if (!powersta) {
        return 0;
    }

    return powersta->ips_use_pspoll;
}

/*
* get the state of pspoll_moredata.
*/
ieee80211_pspoll_moredata_handling ieee80211_sta_power_get_pspoll_moredata_handling(struct ieee80211vap *vap)
{
    ieee80211_sta_power_t powersta = vap->iv_pwrsave_sta;

    if (!powersta) {
        return 0;
    }

    return (powersta->ips_pspoll_moredata ? IEEE80211_CONTINUE_PSPOLL_FOR_MORE_DATA : 
                                                              IEEE80211_WAKEUP_FOR_MORE_DATA );
}

static int ieee80211_sta_power_get_mode(ieee80211_sta_power_t powersta)
{
    return powersta->ips_sta_psmode;
}

static u_int32_t
ieee80211_sta_power_get_max_sleeptime(ieee80211_sta_power_t powersta)
{
    return powersta->ips_max_sleeptime;
}

static u_int32_t
ieee80211_sta_power_get_normal_sleeptime(ieee80211_sta_power_t powersta)
{
    return powersta->ips_normal_sleeptime;
}

static u_int32_t
ieee80211_sta_power_get_low_sleeptime(ieee80211_sta_power_t powersta)
{
    return powersta->ips_low_sleeptime;
}


/**
 * @enalbe/disable  uapsd sleep 
 * 
 *  @param vaphandle     : handle to the vap.
 *  @param enable        : enable/disable 
 *  @return EOK  on success and non zero on failure.
 */
static int ieee80211_sta_power_force_sleep (ieee80211_sta_power_t powersta, bool enable)
{
    struct ieee80211vap    *vap;

    if (! powersta || ! powersta->ips_connected)
    {
        return false;
    }
    vap = powersta->ips_vap;
    if ((wlan_vdev_is_up(vap->vdev_obj) != QDF_STATUS_SUCCESS) ||
        (vap->iv_opmode != IEEE80211_M_STA)) 
    {
        return false;
    }

    if (enable) {
        ieee80211_sm_dispatch(powersta->ips_hsm_handle,IEEE80211_POWER_STA_EVENT_FORCE_SLEEP , 0, NULL);
    } else {
        ieee80211_sm_dispatch(powersta->ips_hsm_handle,IEEE80211_POWER_STA_EVENT_FORCE_AWAKE , 0, NULL);
    }
    return EOK;
}

int
ieee80211_set_powersave(struct ieee80211vap *vap, ieee80211_pwrsave_mode mode)
{
    struct ieee80211com      *ic = vap->iv_ic;
    struct ieee80211_node    *ni = vap->iv_bss;

    if (!vap->iv_pwrsave_sta || !ni) {
        return EINVAL;
    }

    if (mode == IEEE80211_PWRSAVE_WNM && !ieee80211_vap_wnm_is_set(vap))
        return EINVAL;

    if (mode == IEEE80211_PWRSAVE_NONE) {
        ic->ic_flags &= ~IEEE80211_F_PMGTON;
        ic->ic_lintval = 1;
        ni->ni_lintval = ic->ic_lintval;
    } else if (ic->ic_caps & IEEE80211_C_PMGT) {
        ieee80211com_set_flags(ic, IEEE80211_F_PMGTON);
        if (mode == IEEE80211_PWRSAVE_WNM) {
#if UMAC_SUPPORT_WNM
            ic->ic_lintval = vap->iv_wnmsleep_intval * ni->ni_dtim_period;
            ni->ni_lintval = ic->ic_lintval;
#endif
        } else if (mode == IEEE80211_PWRSAVE_LOW) {
            ic->ic_lintval = ieee80211_sta_power_get_low_sleeptime(vap->iv_pwrsave_sta)/ic->ic_intval;
            ni->ni_lintval = ic->ic_lintval;
        } else if (mode == IEEE80211_PWRSAVE_NORMAL) {
            ic->ic_lintval = ieee80211_sta_power_get_normal_sleeptime(vap->iv_pwrsave_sta)/ic->ic_intval;
            ni->ni_lintval = ic->ic_lintval;
        } else {
            ic->ic_lintval = ieee80211_sta_power_get_max_sleeptime(vap->iv_pwrsave_sta)/ic->ic_intval;
            ni->ni_lintval = ic->ic_lintval;
        }
    }

    ieee80211_sta_power_set_mode(vap->iv_pwrsave_sta, mode);
    return EOK;
}

ieee80211_pwrsave_mode
ieee80211_get_powersave(struct ieee80211vap *vap)
{
    if (!vap->iv_pwrsave_sta) {
        return IEEE80211_PWRSAVE_NONE;
    }
    return ieee80211_sta_power_get_mode(vap->iv_pwrsave_sta);
}

u_int32_t
ieee80211_get_apps_powersave_state(struct ieee80211vap *vap)
{
    ieee80211_sta_power_t    powersta = vap->iv_pwrsave_sta;
    u_int32_t                pwr_state;

    if (!powersta) {
       return APPS_AWAKE;
    }
    switch (ieee80211_sm_get_curstate(powersta->ips_hsm_handle)) {

    case IEEE80211_POWER_STA_STATE_INIT:
    case IEEE80211_POWER_STA_STATE_ACTIVE:
        if (powersta->ips_connected ) { 
            pwr_state = APPS_AWAKE;
        } else {
            pwr_state = APPS_SLEEP;
        }
        break;

    case IEEE80211_POWER_STA_STATE_PENDING_TX:
    case IEEE80211_POWER_STA_STATE_NULL_SENT:
        if (!powersta->ips_pause_request) {
            pwr_state = APPS_PENDING_SLEEP;
        } else {
            pwr_state = APPS_FAKE_SLEEP;
        }
        break;
        
    case IEEE80211_POWER_STA_STATE_SLEEP:
    case IEEE80211_POWER_STA_STATE_PSPOLL:
        pwr_state = APPS_SLEEP;
        break;

    case IEEE80211_POWER_STA_STATE_PAUSE:
        pwr_state = APPS_FAKING_SLEEP;
        break;                          
        
    default:
        pwr_state = APPS_UNKNOWN_PWRSAVE;
        break;
    }    
            
    return pwr_state;       
}

int
ieee80211_pwrsave_force_sleep(struct ieee80211vap *vap, bool enable)
{
    if (!vap->iv_pwrsave_sta) {
        return EINVAL; 
    }
    return ieee80211_sta_power_force_sleep(vap->iv_pwrsave_sta, enable);
}


/**
 * @set Powersave mode inactivity time .
 *
 *  @param vaphandle     : handle to the vap.
 *  @param mode          : power save mode .
 *  @param inact_time    : inactivity time in msec to enter powersave.
 *  @return 0  on success and -ve on failure.
 */
int ieee80211_set_powersave_inactive_time(struct ieee80211vap *vap, ieee80211_pwrsave_mode mode, u_int32_t inactive_time)
{
    ieee80211_sta_power_t    powersta = vap->iv_pwrsave_sta;

    switch (mode) {
    case IEEE80211_PWRSAVE_LOW:
        powersta->ips_low_inactivitytime = inactive_time;
        break;
            
    case IEEE80211_PWRSAVE_NORMAL:
        powersta->ips_normal_inactivitytime = inactive_time;
        break;
            
    case IEEE80211_PWRSAVE_MAXIMUM:
        powersta->ips_max_inactivitytime = inactive_time;
        break;
            
    default:
         break;
    }
    
    return EOK;

}

/**
 * @get Powersave mode inactivity time .
 *
 *  @param vaphandle     : handle to the vap.
 *  @param mode          : power save mode .
 *  @return iinactive time for the given mode.
 */
u_int32_t ieee80211_get_powersave_inactive_time(struct ieee80211vap *vap, ieee80211_pwrsave_mode mode)
{
    ieee80211_sta_power_t    powersta = vap->iv_pwrsave_sta;
    u_int32_t val=0;

    switch (mode) {
    case IEEE80211_PWRSAVE_LOW:
        val = powersta->ips_low_inactivitytime;
        break;
            
    case IEEE80211_PWRSAVE_NORMAL:
        val = powersta->ips_normal_inactivitytime;
        break;
            
    case IEEE80211_PWRSAVE_MAXIMUM:
        val = powersta->ips_max_inactivitytime;
        break;
            
    default:
         break;
    }
    
    return val;
}

/**
 * @set timeout for pause notification .
 * 
 *  @param vaphandle     : handle to the vap
 *  @param inact_time    : inactivity time in msec to enter powersave. 
 *  @return 0  on success and -ve on failure.
 */
int ieee80211_power_set_ips_pause_notif_timeout(struct ieee80211vap *vap, u_int16_t pause_notif_timeout)
{
    ieee80211_sta_power_t    powersta = vap->iv_pwrsave_sta;
    
    if(!powersta){
        return EINVAL;
    }

    powersta->ips_pause_notif_timeout = pause_notif_timeout;
    return EOK;
}

/**
 * @get timeout for pause notification .
 * 
 *  @param vaphandle     : handle to the vap.
 *  @return pause notification timeout.
 */
u_int16_t ieee80211_power_get_ips_pause_notif_timeout(struct ieee80211vap *vap)
{
    ieee80211_sta_power_t    powersta = vap->iv_pwrsave_sta;
    
    if(!powersta){
        return 0;
    }
    
    return (powersta->ips_pause_notif_timeout);
}
/*
 * called when queueing tx packet  starts. to synchronize the SM with tx thread and make
 * sure that the SM will not change the chip power state while a tx queuing  is in progress.
 */
void _ieee80211_sta_power_tx_start(struct ieee80211vap *vap)
{
    ieee80211_sta_power_t powersta = vap->iv_pwrsave_sta;

    if (!powersta) return;  /* For non STA Vaps */

    atomic_inc(&powersta->ips_tx_in_progress_count);
}

/*
 * called when tx packet queuing ends . 
 */
void _ieee80211_sta_power_tx_end(struct ieee80211vap *vap)
{
    ieee80211_sta_power_t powersta = vap->iv_pwrsave_sta;

    if (!powersta) return;  /* For non STA Vaps */

    atomic_dec(&powersta->ips_tx_in_progress_count);
}

#endif

/**
 * @set max service period length filed in uapsd .
 * 
 *  @param vaphandle     : handle to the vap.
 *  @param max_sp_len    : max service period length (0:unlimited,2,4,6) 
 *  @return EOK  on success and non zero on failure.
 */
int ieee80211_pwrsave_uapsd_set_max_sp_length(struct ieee80211vap *vap,u_int8_t val)
{
    if (!(val < 4)) 
        return EINVAL;

    vap->iv_uapsd  &= ~(0x3 << WME_CAPINFO_UAPSD_MAXSP_SHIFT);
    vap->iv_uapsd  |= (val << WME_CAPINFO_UAPSD_MAXSP_SHIFT);

    return 0;
}

int
wlan_set_powersave(wlan_if_t vaphandle, ieee80211_pwrsave_mode mode)
{
    struct ieee80211vap      *vap = vaphandle;
    struct ieee80211com      *ic = vap->iv_ic;

    if (ic->ic_power_set_mode) {
        return ic->ic_power_set_mode(vap, mode);
    }

    return EOK;
}

ieee80211_pwrsave_mode
wlan_get_powersave(wlan_if_t vaphandle)
{
    struct ieee80211vap      *vap = vaphandle;
    struct ieee80211com      *ic = vap->iv_ic;

    if (ic->ic_power_get_mode) {
        return ic->ic_power_get_mode(vap);
    }

    return IEEE80211_PWRSAVE_NONE;
}

int wlan_sta_power_set_pspoll(wlan_if_t vaphandle, u_int32_t pspoll)
{
    struct ieee80211vap      *vap = vaphandle;
    struct ieee80211com      *ic = vap->iv_ic;

    if (ic->ic_power_sta_set_pspoll) {
        return ic->ic_power_sta_set_pspoll(vap, pspoll);
    }

    return ENXIO;

}

int wlan_sta_power_set_pspoll_moredata_handling(
        wlan_if_t vaphandle, 
        ieee80211_pspoll_moredata_handling mode)
{
    struct ieee80211vap      *vap = vaphandle;
    struct ieee80211com      *ic = vap->iv_ic;

    if (ic->ic_power_sta_set_pspoll_moredata_handling) {
        return ic->ic_power_sta_set_pspoll_moredata_handling(vap, mode);
    }

    return ENXIO;
}

u_int32_t wlan_sta_power_get_pspoll(wlan_if_t vaphandle)
{
    struct ieee80211vap      *vap = vaphandle;
    struct ieee80211com      *ic = vap->iv_ic;

    if (ic->ic_power_sta_get_pspoll) {
        return ic->ic_power_sta_get_pspoll(vap);
    }

    return 0;
}

ieee80211_pspoll_moredata_handling  
wlan_sta_power_get_pspoll_moredata_handling(wlan_if_t vaphandle)
{
    struct ieee80211vap      *vap = vaphandle;
    struct ieee80211com      *ic = vap->iv_ic;

    if (ic->ic_power_sta_get_pspoll_moredata_handling) {
        return ic->ic_power_sta_get_pspoll_moredata_handling(vap);
    }

    return IEEE80211_WAKEUP_FOR_MORE_DATA;
}

u_int32_t
wlan_get_apps_powersave_state(wlan_if_t vaphandle)
{
    struct ieee80211vap      *vap = vaphandle;
    struct ieee80211com      *ic = vap->iv_ic;

    if (ic->ic_power_get_apps_state) {
        return ic->ic_power_get_apps_state(vap);
    }

    return APPS_AWAKE;
}

int wlan_set_powersave_inactive_time(wlan_if_t vaphandle, ieee80211_pwrsave_mode mode, u_int32_t inactive_time)
{
    struct ieee80211vap      *vap = vaphandle;
    struct ieee80211com      *ic = vap->iv_ic;

    if (ic->ic_power_set_inactive_time) {
        return ic->ic_power_set_inactive_time(vap, mode, inactive_time);
    }

    return EINVAL;
}

u_int32_t wlan_get_powersave_inactive_time(wlan_if_t vaphandle, ieee80211_pwrsave_mode mode)
{
    struct ieee80211vap      *vap = vaphandle;
    struct ieee80211com      *ic = vap->iv_ic;

    if (ic->ic_power_get_inactive_time) {
        return ic->ic_power_get_inactive_time(vap, mode);
    }

    return 0;
}

int
wlan_pwrsave_force_sleep(wlan_if_t vaphandle, bool enable)
{
    struct ieee80211vap      *vap = vaphandle;
    struct ieee80211com      *ic = vap->iv_ic;

    if (ic->ic_power_force_sleep) {
        return ic->ic_power_force_sleep(vap, enable);
    }

    return EOK;
}

int wlan_set_ips_pause_notif_timeout(wlan_if_t vaphandle, u_int16_t pause_notif_timeout)
{
    struct ieee80211vap      *vap = vaphandle;
    struct ieee80211com      *ic = vap->iv_ic;

    if (ic->ic_power_set_ips_pause_notif_timeout) {
        return ic->ic_power_set_ips_pause_notif_timeout(vap, pause_notif_timeout);
    }

    return EINVAL;
}

u_int16_t wlan_get_ips_pause_notif_timeout(wlan_if_t vaphandle)
{
    struct ieee80211vap      *vap = vaphandle;
    struct ieee80211com      *ic = vap->iv_ic;

    if (ic->ic_power_get_ips_pause_notif_timeout) {
        return ic->ic_power_get_ips_pause_notif_timeout(vap);
    }

    return 0;
}

int ieee80211_sta_power_send_keepalive(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;

    if (ic->ic_power_sta_send_keepalive) {
        return ic->ic_power_sta_send_keepalive(vap);
    }
    return ENXIO;
}

int ieee80211_sta_power_pause(struct ieee80211vap *vap, u_int32_t timeout)
{
    struct ieee80211com *ic = vap->iv_ic;

    if (ic->ic_power_sta_pause) {
        return ic->ic_power_sta_pause(vap, timeout);
    }

    return ENXIO;
}

int ieee80211_sta_power_unpause(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;

    if (ic->ic_power_sta_unpause) {
        return ic->ic_power_sta_unpause(vap);
    }

    return ENXIO;

}

int ieee80211_sta_power_register_event_handler(
        struct ieee80211vap *vap,
        ieee80211_sta_power_event_handler evhandler, 
        void *arg)
{
    struct ieee80211com *ic = vap->iv_ic;

    if (ic->ic_power_sta_register_event_handler) {
        return ic->ic_power_sta_register_event_handler(vap, evhandler, arg);
    }

    return EOK;
}

int ieee80211_sta_power_unregister_event_handler(
        struct ieee80211vap *vap,
        ieee80211_sta_power_event_handler evhandler, 
        void *arg)
{
    struct ieee80211com *ic = vap->iv_ic;

    if (ic->ic_power_sta_unregister_event_handler) {
        return ic->ic_power_sta_unregister_event_handler(vap, evhandler, arg);
    }

    return EOK;
}

void ieee80211_sta_power_event_tim(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;

    if (ic->ic_power_sta_event_tim) {
        ic->ic_power_sta_event_tim(vap);
    }
}

void ieee80211_sta_power_event_dtim(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;

    if (ic->ic_power_sta_event_dtim) {
        ic->ic_power_sta_event_dtim(vap);
    }
}

void ieee80211_sta_power_tx_start(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;

    if (ic->ic_power_sta_tx_start) {
        ic->ic_power_sta_tx_start(vap);
    }
}

void ieee80211_sta_power_tx_end(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;

    if (ic->ic_power_sta_tx_end) {
        ic->ic_power_sta_tx_end(vap);
    }
}

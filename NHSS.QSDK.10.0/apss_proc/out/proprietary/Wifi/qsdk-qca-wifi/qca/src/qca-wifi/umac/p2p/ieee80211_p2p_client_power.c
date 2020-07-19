/*
 * Copyright (c) 2011,2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#include "ieee80211_p2p_client_power.h"
#include "ieee80211_p2p_client.h"
#include <ieee80211_tsftimer.h>
#include <wlan_mlme_dp_dispatcher.h>

#if UMAC_SUPPORT_P2P

struct ieee80211_p2p_client_power {
    struct ieee80211vap              *pcp_vap;              /* vap handle */
    ieee80211_hsm_t                  pcp_hsm_handle;        /* the SM handle */
    wlan_p2p_client_t                pcp_client_handle;     /* p2 client handle */
    ieee80211_p2p_go_schedule_t      pcp_go_schedule;       /* handle to go schedule */
    os_timer_t                       pcp_ctwindow_timer;        /* fired at the end of ct window  */
    os_timer_t                       pcp_async_null_event_timer;    /* timer used to delivering async null complete failed event*/  
    tsftimer_handle_t                pcp_tbtt_tsf_timer;    /* tbtt tsf timer */
    u_int32_t                        pcp_ctwindow;          /* ctwindow in msec */
    u_int32_t                        pcp_mlme_swbmiss_id;          /* ctwindow in msec */
    u_int32_t                        pcp_sta_ps_state:1,    /* powersave state as seen by the AP */
                                     pcp_sm_started:1,      /* powersave state as seen by the AP */
                                     pcp_go_oppPS_state:1,  /* opp PS state of the GO */
                                     pcp_go_present:1,      /* 1: present, 0:absent */
                                     pcp_uapsd_sp_in_progress:1,      /* 1: uapsd service period is in progress, 0:no uapsd service period is in progress */
                                     pcp_connected:1,       /* are we connected */
                                     pcp_swbmiss_disabled:1,/* is swbmiss disabled */
                                     pcp_node_pause_pend_null:1,/* node is paused pending Tx of NULL frame with PS=1 */
                                     pcp_vap_force_paused:1;/* current force pause state of the vap */
    /* A queue for handling some events asyncronously to avoid dead locks */
    os_mesg_queue_t                 event_mesg_q;
    u_int32_t                        pcp_pwr_arbiter_id;    /* Power Arbiter requestor ID */


    
    u_int32_t                        pcp_tbtt;              /* tbtt in tsf32 */

#define IEEE80211_P2P_CLIENT_POWER_TSF_TBTT         0
#define IEEE80211_P2P_CLIENT_POWER_TSF_CTWINDOW     1
    u_int32_t                        pcp_tsf_state;         /* Next TSF timer state */

};
static void ieee80211_p2p_client_power_p2p_event_handler (wlan_p2p_client_t p2p_client_handle, ieee80211_p2p_client_event *event, void *arg);

typedef enum {
    IEEE80211_P2P_CLIENT_POWER_STATE_INIT,          /* init state SM is not enabled */ 
    IEEE80211_P2P_CLIENT_POWER_STATE_GO_PRESENT,    /* GO is present */ 
    IEEE80211_P2P_CLIENT_POWER_STATE_GO_ABSENT,    /*  GO is absent  */ 
    IEEE80211_P2P_CLIENT_POWER_STATE_GO_ABSENT_TBTT_WAIT,    /*  GO is absent and waiting for TBTT to move it to present  */ 
} ieee80211_p2p_client_power_state; 

typedef enum {
    IEEE80211_P2P_CLIENT_POWER_EVENT_START,               /* start the  SM*/
    IEEE80211_P2P_CLIENT_POWER_EVENT_STOP,                /* stop the  SM*/
    IEEE80211_P2P_CLIENT_POWER_EVENT_GO_PRESENT,          /* GO is present now*/
    IEEE80211_P2P_CLIENT_POWER_EVENT_GO_ABSENT,           /* GO is absent now*/
    IEEE80211_P2P_CLIENT_POWER_EVENT_TBTT,                /* TBTT event */
    IEEE80211_P2P_CLIENT_POWER_EVENT_EOF_CTWINDOW,        /* end of ctwindow */
    IEEE80211_P2P_CLIENT_POWER_EVENT_POWER_SAVE,          /* p2p client entered power save (sent null with PS=1) */
    IEEE80211_P2P_CLIENT_POWER_EVENT_PEND_TX_PS_NULL,     /* Pending the Tx completion of null frame with PS=1 */
    IEEE80211_P2P_CLIENT_POWER_EVENT_TX_PS_NULL_FAIL      /* Failed to send null frame with PS=1 */
} ieee89211_p2p_client_powet_event;

static const char*    p2p_event_name[] = {
    /* IEEE80211_P2P_CLIENT_POWER_EVENT_START        */ "START",
    /* IEEE80211_P2P_CLIENT_POWER_EVENT_STOP         */ "STOP",
    /* IEEE80211_P2P_CLIENT_POWER_EVENT_GO_PRESENT   */ "GO_PRESENT",
    /* IEEE80211_P2P_CLIENT_POWER_EVENT_GO_ABSENT    */ "GO_ABSENT",
    /* IEEE80211_P2P_CLIENT_POWER_EVENT_TBTT         */ "TBTT",
    /* IEEE80211_P2P_CLIENT_POWER_EVENT_EOF_CTWINDOW */ "EOF_CTWINDOW",
    /* IEEE80211_P2P_CLIENT_POWER_EVENT_POWER_SAVE   */ "POWER_SAVE",
    /* IEEE80211_P2P_CLIENT_POWER_EVENT_PEND_TX_PS_NULL */  "PEND_TX_PS_NULL",
    /* IEEE80211_P2P_CLIENT_POWER_EVENT_TX_PS_NULL_FAIL */  "TX_PS_NULL_FAIL"
};

#define MAX_QUEUED_EVENTS  4
#define MIN_TSF_TIMER_TIME  5000 /* 5 msec */

#define P2P_CLIENT_POWER_MAX_EVENT_QUEUE_DEPTH 5

/*
 * function to control force pause (on/off).
 */
static void ieee80211_p2p_client_power_force_pause_control (ieee80211_p2p_client_power_t p2p_cli_power, bool val) 
{
    /* execute the pause/unpause operation only if it was not in the right state */
    if (p2p_cli_power->pcp_vap_force_paused != val) {
        if (val) {
            ieee80211_vap_force_pause(p2p_cli_power->pcp_vap, 0);
            if (p2p_cli_power->pcp_node_pause_pend_null) {
                /*
                 * Do the Node unpause. Note that we do the node unpaused after
                 * the VAP forced pause so that no data frame get out.
                 */
                ASSERT(p2p_cli_power->pcp_vap->iv_bss);
                ieee80211node_unpause(p2p_cli_power->pcp_vap->iv_bss);
                p2p_cli_power->pcp_node_pause_pend_null = false;
            }
            /* request network sleep */
            ieee80211_power_arbiter_enter_nwsleep(p2p_cli_power->pcp_vap,
                    p2p_cli_power->pcp_pwr_arbiter_id);
        } else {
            if (p2p_cli_power->pcp_node_pause_pend_null) {
                ASSERT(p2p_cli_power->pcp_vap->iv_bss);
                ieee80211node_unpause(p2p_cli_power->pcp_vap->iv_bss);
                p2p_cli_power->pcp_node_pause_pend_null = false;
            }
            /* request active state*/
            ieee80211_power_arbiter_exit_nwsleep(p2p_cli_power->pcp_vap,
                    p2p_cli_power->pcp_pwr_arbiter_id);
            ieee80211_vap_force_unpause(p2p_cli_power->pcp_vap,0 );
        }
        p2p_cli_power->pcp_vap_force_paused = val;
    }
}

/*
* restart the one shot tbtt timer
*/ 
static void ieee80211_p2p_client_power_oppps_timer_reset(ieee80211_p2p_client_power_t p2p_cli_power)
{
   int                 retval;
   u_int32_t           nexttbtt_tsf32;
   /* 
    * we do not need tbtt timer if we are connected and not in power save (or) if the GO is not using oppPS.
    */
   if( (!p2p_cli_power->pcp_sta_ps_state && p2p_cli_power->pcp_connected) 
         || !p2p_cli_power->pcp_go_oppPS_state) {
       IEEE80211_DPRINTF(p2p_cli_power->pcp_vap,IEEE80211_MSG_STATE, "IGNORE %s  %d %d %d \n", __func__,
                         p2p_cli_power->pcp_connected , p2p_cli_power->pcp_sta_ps_state , p2p_cli_power->pcp_go_oppPS_state);
       return;
   }

   nexttbtt_tsf32 = ieee80211_p2p_client_get_next_tbtt_tsf_32(p2p_cli_power->pcp_vap);


   p2p_cli_power->pcp_tbtt = nexttbtt_tsf32;


   /* Start this timer */
   retval = ieee80211_tsftimer_start(p2p_cli_power->pcp_tbtt_tsf_timer, nexttbtt_tsf32, 0);
   if (retval != 0) {
        IEEE80211_DPRINTF(p2p_cli_power->pcp_vap,IEEE80211_MSG_STATE,
               "%s: ERROR: ieee80211_tsftimer_start returns error=%d \n", __func__, retval);
   }

}

/*
 * function to start SM by sending EVENT_START.
 *  to SM.
 */
static void ieee80211_p2p_client_power_sm_start(ieee80211_p2p_client_power_t p2p_cli_power)
{
    struct ieee80211vap              *vap = p2p_cli_power->pcp_vap;              /* vap handle */

    if (p2p_cli_power->pcp_sm_started == 0) {

        p2p_cli_power->pcp_sm_started = 1;
        /* 
         * determine the initial state of the GO 
         * if the GO is using opp PS if the current time is with in CTWINDOW then GO is present
         * else go is absent. the go scheduler will send a GO_ABSENT/PRESENT event based on the NOA schedule.  
         */ 
        if (p2p_cli_power->pcp_go_oppPS_state ) {
            u_int64_t           cur_tsf64;
            u_int32_t           bintval; /* beacon interval in micro seconds */
            u_int32_t           offset_tbtt; /* corrent time offsent from the tbtt in milli seconds */
            u_int32_t           remaining_ctwin; /* time remaining in ctwindow in msec */
            cur_tsf64 = vap->iv_ic->ic_get_TSF64(vap->iv_ic);
            bintval = IEEE80211_TU_TO_USEC(vap->iv_bss->ni_intval); /* convert TU to micro seconds */
            offset_tbtt = (OS_MOD64_TBTT_OFFSET(cur_tsf64, bintval)) / 1000;
            if (offset_tbtt > p2p_cli_power->pcp_ctwindow) {
                p2p_cli_power->pcp_go_present= false;
                IEEE80211_DPRINTF(vap,IEEE80211_MSG_STATE, "%s  GO absent(opppPS, outside ctwindow) \n", __func__);
            } else {
                remaining_ctwin  =  p2p_cli_power->pcp_ctwindow - offset_tbtt;
                p2p_cli_power->pcp_go_present= true;
                IEEE80211_DPRINTF(vap,IEEE80211_MSG_STATE,
                          "%s  GO present(oppPS, inside ctwindow) reamining ctwin %d msec \n",__func__,remaining_ctwin );

                OS_SET_TIMER(&p2p_cli_power->pcp_ctwindow_timer, remaining_ctwin); /* arm the ctwindow timer */
            }
        } else {
            p2p_cli_power->pcp_go_present= true;
        }

        ieee80211_sm_dispatch(p2p_cli_power->pcp_hsm_handle,IEEE80211_P2P_CLIENT_POWER_EVENT_START, 0, NULL);
        ieee80211_p2p_client_power_oppps_timer_reset(p2p_cli_power);
    }
}

/*
 * function to stop SM by sending EVENT_STOP.
 *  to SM.
 */
static void ieee80211_p2p_client_power_sm_stop(ieee80211_p2p_client_power_t p2p_cli_power)
{
    if (p2p_cli_power->pcp_sm_started == 1) {
        p2p_cli_power->pcp_sm_started = 0;
        IEEE80211_DPRINTF(p2p_cli_power->pcp_vap, IEEE80211_MSG_STATE, "%s: Clear all P2P client PS settings\n", __func__);
        p2p_cli_power->pcp_ctwindow = 0;
        p2p_cli_power->pcp_go_oppPS_state = 0;

        ieee80211_sm_dispatch(p2p_cli_power->pcp_hsm_handle,IEEE80211_P2P_CLIENT_POWER_EVENT_STOP, 0, NULL);
    }
}

/*
 * per state handlers.
 */

/*
 * INIT state.
 */
static void ieee80211_p2p_client_power_state_init_entry(void *ctx) 
{
    ieee80211_p2p_client_power_t p2p_cli_power = (ieee80211_p2p_client_power_t) ctx;
    /* unpause the vap if it was paused */
    ieee80211_p2p_client_power_force_pause_control(p2p_cli_power,false); 
}


static bool ieee80211_p2p_client_power_state_init_event (void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_p2p_client_power_t p2p_cli_power = (ieee80211_p2p_client_power_t) ctx;
    bool ret_val=true;
    switch(event) {
    case IEEE80211_P2P_CLIENT_POWER_EVENT_START:
        if (p2p_cli_power->pcp_go_present ) {
           /*
            * if GO is present and the client (we) is in power save then 
            * enter GO_ABSENT_TBBTT_WAIT state. if the go announces oppPS , then 
            * GO could be in opp PS.
            * we need to wait for TBTT wheere the GO is going to be awake.    
            */
            if (p2p_cli_power->pcp_sta_ps_state && p2p_cli_power->pcp_go_oppPS_state) {
               ieee80211_sm_transition_to(p2p_cli_power->pcp_hsm_handle,IEEE80211_P2P_CLIENT_POWER_STATE_GO_ABSENT_TBTT_WAIT);
            } else {
               ieee80211_sm_transition_to(p2p_cli_power->pcp_hsm_handle,IEEE80211_P2P_CLIENT_POWER_STATE_GO_PRESENT);
            }
        } else {
            ieee80211_sm_transition_to(p2p_cli_power->pcp_hsm_handle,IEEE80211_P2P_CLIENT_POWER_STATE_GO_ABSENT);
        }
        break;
    default :
        ret_val=false;
        break;
    }
    return ret_val;
}

/*
 * GO_PRESENT state.
 */
static void ieee80211_p2p_client_power_state_go_present_entry(void *ctx) 
{
    ieee80211_p2p_client_power_t p2p_cli_power = (ieee80211_p2p_client_power_t) ctx;
    /* 
     * just unpause the  vap this should unpause 
     * the data on both the ath layer as well as
     * the umac layer. need vap unpause with force flag.
     */
    ieee80211_p2p_client_power_force_pause_control(p2p_cli_power,false); 
    if (p2p_cli_power->pcp_uapsd_sp_in_progress ) {
        int ac,uapsd;
        struct ieee80211_node *ni;

        ac = WME_AC_BE;
        /* send the trigger on highest trigger enabled ac */
        uapsd = p2p_cli_power->pcp_vap->iv_uapsd;
        if (uapsd & WME_CAPINFO_UAPSD_VO) {
            ac = WME_AC_VO;
        } else if (uapsd & WME_CAPINFO_UAPSD_VI) {
            ac = WME_AC_VI;
        } else if (uapsd & WME_CAPINFO_UAPSD_BE) {
            ac = WME_AC_BE;
        } else if (uapsd & WME_CAPINFO_UAPSD_BK) {
            ac = WME_AC_BK;
        }
        ni = ieee80211_try_ref_bss_node(p2p_cli_power->pcp_vap);
        if (ni) {
            IEEE80211_DPRINTF(p2p_cli_power->pcp_vap,IEEE80211_MSG_STATE, "%s: UAPSD send qos null \n", __func__);
            ieee80211_send_qosnulldata(ni,ac,0);
            ieee80211_free_node(ni);
        }
    }
}

static bool ieee80211_p2p_client_power_state_go_present_event (void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_p2p_client_power_t p2p_cli_power = (ieee80211_p2p_client_power_t) ctx;
    bool ret_val=true;
    switch(event) {
    case IEEE80211_P2P_CLIENT_POWER_EVENT_GO_ABSENT:
        ieee80211_sm_transition_to(p2p_cli_power->pcp_hsm_handle,IEEE80211_P2P_CLIENT_POWER_STATE_GO_ABSENT);
        break;
    case IEEE80211_P2P_CLIENT_POWER_EVENT_POWER_SAVE:           
        if (p2p_cli_power->pcp_sta_ps_state && p2p_cli_power->pcp_go_oppPS_state) {
            ieee80211_sm_transition_to(p2p_cli_power->pcp_hsm_handle,IEEE80211_P2P_CLIENT_POWER_STATE_GO_ABSENT_TBTT_WAIT);
        }
        break;
    case IEEE80211_P2P_CLIENT_POWER_EVENT_EOF_CTWINDOW:
        /* 
         * if we are in power save and the GO is using oppPS then we need ct window 
         * event. (OR)
         * if we are not connected and the GO is using oppPS then we need ct window 
         * event.
         */
        if (p2p_cli_power->pcp_go_oppPS_state && (p2p_cli_power->pcp_sta_ps_state || !p2p_cli_power->pcp_connected) ) {
            ieee80211_sm_transition_to(p2p_cli_power->pcp_hsm_handle,IEEE80211_P2P_CLIENT_POWER_STATE_GO_ABSENT_TBTT_WAIT);
        }
        break;
    case IEEE80211_P2P_CLIENT_POWER_EVENT_STOP:
        if (p2p_cli_power->pcp_node_pause_pend_null) {
            /* Undo the node unpause due to pending Tx of NULL frame with PS=1 */
            ASSERT(p2p_cli_power->pcp_vap->iv_bss);
            ieee80211node_unpause(p2p_cli_power->pcp_vap->iv_bss);
            p2p_cli_power->pcp_node_pause_pend_null = false;
        }
        ieee80211_sm_transition_to(p2p_cli_power->pcp_hsm_handle,IEEE80211_P2P_CLIENT_POWER_STATE_INIT);
        break;
    case IEEE80211_P2P_CLIENT_POWER_EVENT_PEND_TX_PS_NULL:
        if (p2p_cli_power->pcp_go_oppPS_state && (!p2p_cli_power->pcp_node_pause_pend_null)) {
            /*
             * With Opp PS and sending a NULL with PS=1 to tell the GO
             * that this station is going to sleep. Paused the data traffic
             * to make sure that no normal data frame get transmitted until
             * we know the completion of this NULL data frame.
             */
            IEEE80211_DPRINTF(p2p_cli_power->pcp_vap, IEEE80211_MSG_STATE,
                      "%s: with Opp PS and Pending Tx Null PS=1 frame. Do node-pause.\n", __func__);
            ASSERT(p2p_cli_power->pcp_vap->iv_bss);
            ieee80211node_pause(p2p_cli_power->pcp_vap->iv_bss);
            p2p_cli_power->pcp_node_pause_pend_null = true;
        }
        break;
    case IEEE80211_P2P_CLIENT_POWER_EVENT_TX_PS_NULL_FAIL:
        if (p2p_cli_power->pcp_node_pause_pend_null) {
            /* Waiting for the completion of this Tx NULL frame with PS=1 */
            IEEE80211_DPRINTF(p2p_cli_power->pcp_vap, IEEE80211_MSG_STATE,
                      "%s: Tx Null PS=1 frame is unsuccessful. Unpause the data now by node-unpause.\n", __func__);
            p2p_cli_power->pcp_node_pause_pend_null = false;
            ASSERT(p2p_cli_power->pcp_vap->iv_bss);
            ieee80211node_unpause(p2p_cli_power->pcp_vap->iv_bss);
        }
        break;
    default:
        ret_val=false;
        break;
    }
    return ret_val;
}

/*
 * GO_ABSENT state.
 */
static void ieee80211_p2p_client_power_state_go_absent_entry(void *ctx) 
{
    ieee80211_p2p_client_power_t p2p_cli_power = (ieee80211_p2p_client_power_t) ctx;
    /* 
     * just pause the data (can not send null frame). this should pause 
     * the data on both the ath layer as well as
     * the umac layer. need vap pause with force flag.
     */
    ieee80211_p2p_client_power_force_pause_control(p2p_cli_power,true); 
}

static bool ieee80211_p2p_client_power_state_go_absent_event (void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_p2p_client_power_t p2p_cli_power = (ieee80211_p2p_client_power_t) ctx;
    bool ret_val=true;
    switch(event) {
    case IEEE80211_P2P_CLIENT_POWER_EVENT_GO_PRESENT:
        if (p2p_cli_power->pcp_sta_ps_state && p2p_cli_power->pcp_go_oppPS_state) {
            ieee80211_sm_transition_to(p2p_cli_power->pcp_hsm_handle,IEEE80211_P2P_CLIENT_POWER_STATE_GO_ABSENT_TBTT_WAIT);
        } else {
            ieee80211_sm_transition_to(p2p_cli_power->pcp_hsm_handle,IEEE80211_P2P_CLIENT_POWER_STATE_GO_PRESENT);
        }
        break;
    case IEEE80211_P2P_CLIENT_POWER_EVENT_STOP:
        ieee80211_sm_transition_to(p2p_cli_power->pcp_hsm_handle,IEEE80211_P2P_CLIENT_POWER_STATE_INIT);
        break;
    default:
        ret_val=false;
        break;
    }
    return ret_val;
}

/*
 * GO_ABSENT_TBTT_WAIT state.
 */
static void ieee80211_p2p_client_power_state_go_absent_tbtt_wait_entry(void *ctx) 
{
    ieee80211_p2p_client_power_t p2p_cli_power = (ieee80211_p2p_client_power_t) ctx;
    /* 
     * just pause the data (can not send null frame). this should pause 
     * the data on both the ath layer as well as
     * the umac layer. need vap pause with force flag.
     */
    ieee80211_p2p_client_power_force_pause_control(p2p_cli_power,true); 
}

static bool ieee80211_p2p_client_power_state_go_absent_tbtt_wait_event (void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data) 
{
    ieee80211_p2p_client_power_t p2p_cli_power = (ieee80211_p2p_client_power_t) ctx;
    bool ret_val=true;
    switch(event) {
    case IEEE80211_P2P_CLIENT_POWER_EVENT_TBTT:
        ieee80211_sm_transition_to(p2p_cli_power->pcp_hsm_handle,IEEE80211_P2P_CLIENT_POWER_STATE_GO_PRESENT);
        break;
    case IEEE80211_P2P_CLIENT_POWER_EVENT_GO_ABSENT:
        ieee80211_sm_transition_to(p2p_cli_power->pcp_hsm_handle,IEEE80211_P2P_CLIENT_POWER_STATE_GO_ABSENT);
        break;
    case IEEE80211_P2P_CLIENT_POWER_EVENT_STOP:
        ieee80211_sm_transition_to(p2p_cli_power->pcp_hsm_handle,IEEE80211_P2P_CLIENT_POWER_STATE_INIT);
        break;
    default:
        ret_val=false;
        break;
    }
    return ret_val;
}


static void ieee80211_p2p_client_power_sm_debug_print (void *ctx,const char *fmt,...) 
{
#if 0 /* DEBUG : remove unnecesssary time of curr_tsf64. */
    ieee80211_p2p_client_power_t handle = (ieee80211_p2p_client_power_t) ctx;
    char tmp_buf[256];
    va_list ap;
    u_int64_t cur_tsf64;

    cur_tsf64 = handle->pcp_vap->iv_ic->ic_get_TSF64(handle->pcp_vap->iv_ic);

    va_start(ap, fmt);
    vsnprintf (tmp_buf,256, fmt, ap);
    va_end(ap);
    IEEE80211_DPRINTF(handle->pcp_vap, IEEE80211_MSG_POWER, "tsf:%lld TU:%lld: %s",cur_tsf64, cur_tsf64>>10, tmp_buf);
#else
    ieee80211_p2p_client_power_t handle = (ieee80211_p2p_client_power_t) ctx;
    char tmp_buf[256];
    va_list ap;

    /* TBD : ic->ic_get_TSF64 involve 2 WMI commands and cause large timing delay even debug is OFF. 
     *       Need another way to debug for the split driver with TSF timestamp.
     */
    va_start(ap, fmt);
    vsnprintf (tmp_buf,256, fmt, ap);
    va_end(ap);

    IEEE80211_DPRINTF(handle->pcp_vap, IEEE80211_MSG_POWER, " %s", tmp_buf);
#endif       
}

ieee80211_state_info ieee80211_p2p_client_power_sm_info[] = {
    { 
        IEEE80211_P2P_CLIENT_POWER_STATE_INIT, 
        IEEE80211_HSM_STATE_NONE,
        IEEE80211_HSM_STATE_NONE,
        false,
        "INIT",
        ieee80211_p2p_client_power_state_init_entry,
        NULL,
        ieee80211_p2p_client_power_state_init_event
    },
    { 
        IEEE80211_P2P_CLIENT_POWER_STATE_GO_PRESENT, 
        IEEE80211_HSM_STATE_NONE,
        IEEE80211_HSM_STATE_NONE,
        false,
        "GO_PRESENT",
        ieee80211_p2p_client_power_state_go_present_entry,
        NULL,
        ieee80211_p2p_client_power_state_go_present_event
    },
    {
        IEEE80211_P2P_CLIENT_POWER_STATE_GO_ABSENT, 
        IEEE80211_HSM_STATE_NONE,
        IEEE80211_HSM_STATE_NONE,
        false,
        "GO_ABSENT",
        ieee80211_p2p_client_power_state_go_absent_entry,
        NULL,
        ieee80211_p2p_client_power_state_go_absent_event
    },
    {
        IEEE80211_P2P_CLIENT_POWER_STATE_GO_ABSENT_TBTT_WAIT, 
        IEEE80211_HSM_STATE_NONE,
        IEEE80211_HSM_STATE_NONE,
        false,
        "GO_ABSENT_TBTT",
        ieee80211_p2p_client_power_state_go_absent_tbtt_wait_entry,
        NULL,
        ieee80211_p2p_client_power_state_go_absent_tbtt_wait_event
    },
};

/*
 * txrx event handler to find out when the client enters in to sleep 
 * wrt AP.
 */
static void ieee80211_p2p_client_power_txrx_event_handler (struct wlan_objmgr_vdev *vdev, ieee80211_vap_txrx_event *event, void *arg)
{
    ieee80211_p2p_client_power_t p2p_cli_power = (ieee80211_p2p_client_power_t) arg;
    u_int32_t event_filter=0;

    switch(event->type) {
    case IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_PS_NULL:

        /*
         * for UAPSD , if the trigger frame is send while we are in UAPSD powersave, then
         * we need to mark that service period has started. while a service period in progresss
         * if we enter absent period, the GO terminates the service period and when the GO
         * enters presence period  then driver needs  to retrigger service period with QOS NULL frame. 
         */
        if (event->u.status == 0 && p2p_cli_power->pcp_vap->iv_uapsd) { 
            int type,subtype,dir;
            struct ieee80211_frame *wh;

            wh = event->wh;
            type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
            subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
            dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;
            event_filter = IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_PS_NULL;

            /* QOS data/null frame with PS bit set is a trigger frame */
            if (type == IEEE80211_FC0_TYPE_DATA &&  (subtype == IEEE80211_FC0_SUBTYPE_QOS_NULL || subtype == IEEE80211_FC0_SUBTYPE_QOS )) {
                u_int8_t tid,ac;
                u_int16_t wme_capinfo_ac = 0;
                if (dir == IEEE80211_FC1_DIR_DSTODS) {
                    tid = ((struct ieee80211_qosframe_addr4 *)wh)->
                        i_qos[0] & IEEE80211_QOS_TID;
                } else {
                    tid = ((struct ieee80211_qosframe *)wh)->
                        i_qos[0] & IEEE80211_QOS_TID;
                }
                ac = TID_TO_WME_AC(tid);
                if (ac == WME_AC_VO) {
                    wme_capinfo_ac = WME_CAPINFO_UAPSD_VO;
                } else if (ac == WME_AC_VI) {
                    wme_capinfo_ac = WME_CAPINFO_UAPSD_VI;
                } else if (ac == WME_AC_BE) {
                    wme_capinfo_ac = WME_CAPINFO_UAPSD_BE;
                } else if (ac == WME_AC_BK) {
                    wme_capinfo_ac = WME_CAPINFO_UAPSD_BK;
                }
                IEEE80211_DPRINTF(p2p_cli_power->pcp_vap,IEEE80211_MSG_STATE, "%s: UAPSD check uapsd 0x%x acinfo 0x%x \n", __func__, p2p_cli_power->pcp_vap->iv_uapsd, wme_capinfo_ac); 
                /* if that perticular AC is trigger enabled then we are in SP */
                if (wme_capinfo_ac &  p2p_cli_power->pcp_vap->iv_uapsd) { 
                    p2p_cli_power->pcp_uapsd_sp_in_progress = 1;
                    event_filter |= IEEE80211_VAP_INPUT_EVENT_EOSP;
                    wlan_vdev_txrx_register_event_handler(p2p_cli_power->pcp_vap->vdev_obj, ieee80211_p2p_client_power_txrx_event_handler,p2p_cli_power,
                                                              event_filter | IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_TX);
                    IEEE80211_DPRINTF(p2p_cli_power->pcp_vap,IEEE80211_MSG_STATE, "%s: UAPSD SP start \n", __func__);
                }
            }
        }

        /*
         * if the client is already in power save wrt AP
         * just ignore this event.
         */
        if (p2p_cli_power->pcp_sta_ps_state)  return;

        if (event->u.status == 0) {
            /*
             * client has sent a null frame to AP with PS bit set to 1.
             * with respect to AP the client has entered power save.
             */
            IEEE80211_DPRINTF(p2p_cli_power->pcp_vap,IEEE80211_MSG_STATE, "%s: sta ps state ON \n", __func__);
            p2p_cli_power->pcp_sta_ps_state=true;
            wlan_vdev_txrx_register_event_handler(p2p_cli_power->pcp_vap->vdev_obj,ieee80211_p2p_client_power_txrx_event_handler,p2p_cli_power,
                                              event_filter | IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_TX);
           if (OS_MESGQ_SEND(&p2p_cli_power->event_mesg_q,IEEE80211_P2P_CLIENT_POWER_EVENT_POWER_SAVE , 0, NULL) != 0) {
               IEEE80211_DPRINTF(p2p_cli_power->pcp_vap,IEEE80211_MSG_STATE, 
                                             "%s: message queue send failed \n", __func__);
            }
            ieee80211_p2p_client_power_oppps_timer_reset(p2p_cli_power);
        }
        else {
            /* Some failure in sending the Null PS=1 frame */
            IEEE80211_DPRINTF(p2p_cli_power->pcp_vap,IEEE80211_MSG_STATE,
                              "%s: indicated failed to send NULL frame with PS=1.\n", __func__);

            /* 
             * this could have been failed synchronously from the SM with the ieee80211_send_nulldata() function call
             * do not dispatch the event synchronously instead send it asynchronously.
             */
             OS_SET_TIMER(&p2p_cli_power->pcp_async_null_event_timer,0);
        }
        break;
    case IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_TX:
        /*    
         * completed a frame without PS bit state , 
         * with respect to AP the client is awake.
         */ 
         if (!IEEE80211_IS_MULTICAST(event->wh->i_addr1)  && event->u.status == 0) {
            p2p_cli_power->pcp_sta_ps_state=false;
            IEEE80211_DPRINTF(p2p_cli_power->pcp_vap,IEEE80211_MSG_STATE, "%s: sta ps state OFF \n", __func__);
            wlan_vdev_txrx_register_event_handler(p2p_cli_power->pcp_vap->vdev_obj,ieee80211_p2p_client_power_txrx_event_handler,p2p_cli_power,
                                                      IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_PS_NULL);
            p2p_cli_power->pcp_uapsd_sp_in_progress = 0; /* chip is awake, no more UAPSD service period */
            IEEE80211_DPRINTF(p2p_cli_power->pcp_vap,IEEE80211_MSG_STATE, "%s: STA awake, UAPSD SP end \n", __func__);
        }
        break;
    case IEEE80211_VAP_INPUT_EVENT_EOSP:
        p2p_cli_power->pcp_uapsd_sp_in_progress = 0; /* EOSP, so no service period in progress */
        IEEE80211_DPRINTF(p2p_cli_power->pcp_vap,IEEE80211_MSG_STATE, "%s: EOSP: UAPSD SP end \n", __func__);
        break;
    default:
        break;
    }

}


/*
 * GO Schedule event handler.
 */
static void ieee80211_p2p_client_power_go_schedule_evhandler(
    ieee80211_p2p_go_schedule_t go_schedule, 
    ieee80211_p2p_go_schedule_event *event, 
    void *arg, 
    bool one_shot_noa)
{
    ieee80211_p2p_client_power_t p2p_cli_power = (ieee80211_p2p_client_power_t) arg;
    switch(event->type) {
    case IEEE80211_P2P_GO_SCHEDULE_EVENT_GO_ABSENT:
        p2p_cli_power->pcp_go_present= false;
        ieee80211_sm_dispatch(p2p_cli_power->pcp_hsm_handle,IEEE80211_P2P_CLIENT_POWER_EVENT_GO_ABSENT, 0, NULL);
        if (one_shot_noa) {
            /* during one shot NOA disable bmiss */
            p2p_cli_power->pcp_swbmiss_disabled=true;
            ieee80211_mlme_sta_swbmiss_timer_disable(p2p_cli_power->pcp_vap,
                                          p2p_cli_power->pcp_mlme_swbmiss_id );
        }
        break;
    case IEEE80211_P2P_GO_SCHEDULE_EVENT_GO_PRESENT:
        p2p_cli_power->pcp_go_present= true;
        if (p2p_cli_power->pcp_swbmiss_disabled) {
            ieee80211_mlme_sta_swbmiss_timer_enable(p2p_cli_power->pcp_vap,
                                          p2p_cli_power->pcp_mlme_swbmiss_id );
            p2p_cli_power->pcp_swbmiss_disabled=false;
        }
        ieee80211_sm_dispatch(p2p_cli_power->pcp_hsm_handle,IEEE80211_P2P_CLIENT_POWER_EVENT_GO_PRESENT, 0, NULL);
        break;
    }
}


/*
 * VAP state event handler.
 */
void ieee80211_p2p_client_power_vap_event_handler (ieee80211_p2p_client_power_t p2p_cli_power, ieee80211_vap_event *event)
{
    switch(event->type) 
    {
    case IEEE80211_VAP_UP:
        p2p_cli_power->pcp_connected=1;
        /*
         * monitor completed null frames with PS=1 .
         */
        wlan_vdev_txrx_register_event_handler(p2p_cli_power->pcp_vap->vdev_obj,ieee80211_p2p_client_power_txrx_event_handler,p2p_cli_power,
                                              IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_PS_NULL);
        /**
         * the SM should have been started when the vap enters join state from
         * ieee80211_p2p_client_power_p2p_event_handler(). is this necessary.
         */ 
        ieee80211_p2p_client_power_sm_start(p2p_cli_power);
        ieee80211_p2p_client_power_oppps_timer_reset(p2p_cli_power);
        IEEE80211_DPRINTF(p2p_cli_power->pcp_vap,IEEE80211_MSG_STATE, "%s: VAP UP \n", __func__);

        break;
    case IEEE80211_VAP_FULL_SLEEP:
    case IEEE80211_VAP_DOWN:
        /*
         * vap is down/in full sleep.
         * reset the connected, power save flags.
         */ 
        p2p_cli_power->pcp_connected=0;
        p2p_cli_power->pcp_sta_ps_state=false;
        p2p_cli_power->pcp_go_present= true;
        if (ieee80211_tsftimer_stop(p2p_cli_power->pcp_tbtt_tsf_timer)) {
           IEEE80211_DPRINTF(p2p_cli_power->pcp_vap,IEEE80211_MSG_STATE,
               "%s: ERROR: ieee80211_tsftimer_stop returns error \n", __func__);
        }
        wlan_vdev_txrx_unregister_event_handler(p2p_cli_power->pcp_vap->vdev_obj,ieee80211_p2p_client_power_txrx_event_handler,p2p_cli_power);
        ieee80211_p2p_client_power_sm_stop(p2p_cli_power);
        IEEE80211_DPRINTF(p2p_cli_power->pcp_vap,IEEE80211_MSG_STATE, "%s: VAP DOWN \n", __func__);
        OS_MESGQ_DRAIN(&p2p_cli_power->event_mesg_q,NULL);
        break;

    default:
        break;
    }
}


/*
 * handler for tbtt timer.
 */
#if 1
static void ieee80211_p2p_client_power_tbtt_handler(tsftimer_handle_t h_tsftime, int arg1, void *arg2, u_int32_t curr_tsf)
{
#define CTWINDOW_OVERHEAD  2 /* 2 msec */
    ieee80211_p2p_client_power_t p2p_cli_power = (ieee80211_p2p_client_power_t) arg2;
    u_int32_t curr_tsf32, ctwindow_tsf32, maxint32 = 0xffffffff, timer_ms = 0;
    struct ieee80211com *ic = p2p_cli_power->pcp_vap->iv_ic;
    int retval;
#if 0
    static int count=0, timer[100];
#endif

    if(p2p_cli_power->pcp_tsf_state == IEEE80211_P2P_CLIENT_POWER_TSF_TBTT)
    {
        if(p2p_cli_power->pcp_ctwindow >= 10){
            curr_tsf32 = ic->ic_get_TSF32(ic);
            ctwindow_tsf32 = ((p2p_cli_power->pcp_ctwindow - CTWINDOW_OVERHEAD)*1000) + p2p_cli_power->pcp_tbtt;
            if(curr_tsf32 > ctwindow_tsf32){
                if((curr_tsf32-ctwindow_tsf32) < (maxint32 >> 1)){
                    timer_ms = 0;
                    IEEE80211_DPRINTF(p2p_cli_power->pcp_vap, IEEE80211_MSG_STATE, "curr_tsf32=%d, ctwindow_tsf32=%d\n", curr_tsf32, ctwindow_tsf32);
                } else {
                    timer_ms = (maxint32-curr_tsf32+ctwindow_tsf32+1)/1000;
                }
            } else {
                if((ctwindow_tsf32-curr_tsf32) < (maxint32 >> 1)) {
                    timer_ms = (ctwindow_tsf32-curr_tsf32)/1000;
                } else {
                    timer_ms = 0;
                    IEEE80211_DPRINTF(p2p_cli_power->pcp_vap,IEEE80211_MSG_STATE,"curr_tsf32=%d, ctwindow_tsf32=%d\n", curr_tsf32, ctwindow_tsf32);
                }
            }

            if(timer_ms > (p2p_cli_power->pcp_ctwindow - CTWINDOW_OVERHEAD))
                timer_ms = p2p_cli_power->pcp_ctwindow - CTWINDOW_OVERHEAD;
            
            if(timer_ms >= CTWINDOW_OVERHEAD){
                /* Time is enough to do CTWindow */
                ieee80211_sm_dispatch(p2p_cli_power->pcp_hsm_handle, IEEE80211_P2P_CLIENT_POWER_EVENT_TBTT, 0, NULL);

                /* Change next TSF timer to CTwindows */
                p2p_cli_power->pcp_tsf_state = IEEE80211_P2P_CLIENT_POWER_TSF_CTWINDOW;
                retval = ieee80211_tsftimer_start(p2p_cli_power->pcp_tbtt_tsf_timer, ctwindow_tsf32, 0);
                if (retval != 0) {
                     IEEE80211_DPRINTF(p2p_cli_power->pcp_vap, IEEE80211_MSG_STATE,
                            "%s: ERROR: ieee80211_tsftimer_start returns error=%d \n", __func__, retval);
                }
            } else {
                /* No time to do CTwindow, skip it */
                ieee80211_p2p_client_power_oppps_timer_reset(p2p_cli_power);
            }
        } else {
            ieee80211_sm_dispatch(p2p_cli_power->pcp_hsm_handle, IEEE80211_P2P_CLIENT_POWER_EVENT_TBTT, 0, NULL);
            ieee80211_p2p_client_power_oppps_timer_reset(p2p_cli_power);
        }
#if 0
        timer[count] = timer_ms;
        count = (count+1)%100;

        if(count == 0){
            int i;
            for(i = 0; i < 100; i++)
                printk("=timer[%d]=%d\n", i, timer[i]);
        }
#endif
    } else if (p2p_cli_power->pcp_tsf_state == IEEE80211_P2P_CLIENT_POWER_TSF_CTWINDOW){
        if (p2p_cli_power->pcp_go_oppPS_state && (p2p_cli_power->pcp_sta_ps_state || !p2p_cli_power->pcp_connected)) {
            ieee80211_sm_dispatch(p2p_cli_power->pcp_hsm_handle, IEEE80211_P2P_CLIENT_POWER_EVENT_EOF_CTWINDOW, 0, NULL);
        }

        /* Change next TSF timer to TBTT */
        p2p_cli_power->pcp_tsf_state = IEEE80211_P2P_CLIENT_POWER_TSF_TBTT;
        ieee80211_p2p_client_power_oppps_timer_reset(p2p_cli_power);
    }
}
#else
static void ieee80211_p2p_client_power_tbtt_handler(tsftimer_handle_t h_tsftime, int arg1, void *arg2, u_int32_t curr_tsf)
{
    ieee80211_p2p_client_power_t p2p_cli_power = (ieee80211_p2p_client_power_t) arg2;

    /* deliver TBTT event and restart the timer */
    ieee80211_sm_dispatch(p2p_cli_power->pcp_hsm_handle,IEEE80211_P2P_CLIENT_POWER_EVENT_TBTT, 0, NULL);
    OS_SET_TIMER(&p2p_cli_power->pcp_ctwindow_timer, p2p_cli_power->pcp_ctwindow); /* arm the ctwindow timer */
    ieee80211_p2p_client_power_oppps_timer_reset(p2p_cli_power);

}
#endif

/*
* tsf changed event,could be called after reset (or) when the tsf jumps around.
* reprogram the tsf timer.
* also wake us up if we are sleeping.
*/  
static void ieee80211_p2p_client_power_tsf_changed_handler(tsftimer_handle_t h_tsftime, int arg1, void *arg2, u_int32_t curr_tsf)
{
    ieee80211_p2p_client_power_t p2p_cli_power = (ieee80211_p2p_client_power_t) arg2;

    IEEE80211_DPRINTF(p2p_cli_power->pcp_vap,IEEE80211_MSG_STATE,
        "%s: called: curr_tsf=0x%x\n", __func__, curr_tsf);

    /* restart the tsf timer */
    ieee80211_p2p_client_power_oppps_timer_reset(p2p_cli_power);
    ieee80211_p2p_client_power_force_pause_control (p2p_cli_power, false); 
}

static OS_TIMER_FUNC(ieee80211_p2p_client_power_ctwindow_timer_handler)
{
    ieee80211_p2p_client_power_t p2p_cli_power;
    OS_GET_TIMER_ARG(p2p_cli_power, ieee80211_p2p_client_power_t);

    /* 
     * if we are in power save and the GO is using oppPS then we need ct window 
     * event. (OR)
     * if we are not connected and the GO is using oppPS then we need ct window 
     * event.
     */
    if (p2p_cli_power->pcp_go_oppPS_state && (p2p_cli_power->pcp_sta_ps_state || !p2p_cli_power->pcp_connected) ) {
        ieee80211_sm_dispatch(p2p_cli_power->pcp_hsm_handle,IEEE80211_P2P_CLIENT_POWER_EVENT_EOF_CTWINDOW, 0, NULL);
    }

}

static OS_TIMER_FUNC(ieee80211_p2p_client_power_async_null_event_timer)
{
    ieee80211_p2p_client_power_t p2p_cli_power;
    OS_GET_TIMER_ARG(p2p_cli_power, ieee80211_p2p_client_power_t);

    ieee80211_sm_dispatch(p2p_cli_power->pcp_hsm_handle, 
                          IEEE80211_P2P_CLIENT_POWER_EVENT_TX_PS_NULL_FAIL, 0, NULL);
}

static void  ieee80211_p2p_client_power_mesgq_event_handler(void *ctx, u_int16_t event,
                                           u_int16_t event_data_len, void *event_data)
{
    ieee80211_p2p_client_power_t p2p_cli_power= (ieee80211_p2p_client_power_t) ctx;
    ieee80211_sm_dispatch(p2p_cli_power->pcp_hsm_handle,event, 0, NULL);
}

/*
 * create P2P client power save  SM object.
 */
ieee80211_p2p_client_power_t ieee80211_p2p_client_power_create(osdev_t os_handle, wlan_p2p_client_t p2p_client_handle, wlan_if_t vap, ieee80211_p2p_go_schedule_t go_schedule)
{
    ieee80211_p2p_client_power_t handle;
    int8_t requestor_name_noa_sm[]="P2P_CLI_NOA_SM";
    char requestor_name_cli[]="P2P_CLI";

    handle = (ieee80211_p2p_client_power_t)OS_MALLOC(os_handle,sizeof(struct ieee80211_p2p_client_power),0);
    if (!handle) {
       return NULL;
    }
    OS_MEMZERO(handle, sizeof(struct ieee80211_p2p_client_power));

    /*
     * create snchronous SM.
     */
    handle->pcp_hsm_handle = ieee80211_sm_create(os_handle, 
                                                 "p2p_client_power",
                                                 (void *) handle, 
                                                 IEEE80211_P2P_CLIENT_POWER_STATE_INIT,
                                                 ieee80211_p2p_client_power_sm_info, 
                                                 sizeof(ieee80211_p2p_client_power_sm_info)/sizeof(ieee80211_state_info),
                                                 MAX_QUEUED_EVENTS, 
                                                 0 /* event data of size int */, 
                                                 MESGQ_PRIORITY_HIGH,
                                                 OS_MESGQ_CAN_SEND_SYNC() ? MESGQ_SYNCHRONOUS_EVENT_DELIVERY : MESGQ_ASYNCHRONOUS_EVENT_DELIVERY,
                                                 ieee80211_p2p_client_power_sm_debug_print,
                                                 p2p_event_name,
                                                 IEEE80211_N(p2p_event_name));
    if (!handle->pcp_hsm_handle) {
       OS_FREE(handle);
       return NULL;
    }
    handle->pcp_tbtt_tsf_timer = ieee80211_tsftimer_alloc(vap->iv_ic->ic_tsf_timer , 0, 
                                                           ieee80211_p2p_client_power_tbtt_handler, 
                                                           0,handle,
                                                           ieee80211_p2p_client_power_tsf_changed_handler); 
    if (!handle->pcp_tbtt_tsf_timer) {
       ieee80211_sm_delete(handle->pcp_hsm_handle); 
       OS_FREE(handle);
       return NULL;
    }

    if (OS_MESGQ_INIT(os_handle, &handle->event_mesg_q,
                      0, 
                      P2P_CLIENT_POWER_MAX_EVENT_QUEUE_DEPTH, 
                      ieee80211_p2p_client_power_mesgq_event_handler,
                      handle, 
                      MESGQ_PRIORITY_NORMAL, 
                      MESGQ_ASYNCHRONOUS_EVENT_DELIVERY) != 0)
    {
       IEEE80211_DPRINTF(vap,IEEE80211_MSG_STATE,
                             "%s : OS_MESGQ_INIT  failed.\n", __func__);
       if (ieee80211_tsftimer_free(handle->pcp_tbtt_tsf_timer, true)) {
           IEEE80211_DPRINTF(vap,IEEE80211_MSG_STATE,
               "%s: ERROR: ieee80211_tsftimer_free returns error \n", __func__);
       }
       ieee80211_sm_delete(handle->pcp_hsm_handle); 
       OS_FREE(handle);
       return NULL;
    }
    handle->pcp_mlme_swbmiss_id = ieee80211_mlme_sta_swbmiss_timer_alloc_id(vap,requestor_name_noa_sm);
    OS_INIT_TIMER(os_handle, &handle->pcp_ctwindow_timer, ieee80211_p2p_client_power_ctwindow_timer_handler, handle , QDF_TIMER_TYPE_WAKE_APPS);
    OS_INIT_TIMER(os_handle, &handle->pcp_async_null_event_timer, 
                  ieee80211_p2p_client_power_async_null_event_timer, handle, QDF_TIMER_TYPE_WAKE_APPS); 
    handle->pcp_go_schedule = go_schedule;
    handle->pcp_vap = vap;
    handle->pcp_client_handle = p2p_client_handle;
    handle->pcp_go_present = true; /* assume go is present to start with */

    handle->pcp_pwr_arbiter_id = ieee80211_power_arbiter_alloc_id(vap, 
                                                                  requestor_name_cli, IEEE80211_PWR_ARBITER_TYPE_P2P);
    if (handle->pcp_pwr_arbiter_id != 0) {
        (void) ieee80211_power_arbiter_enable(vap, handle->pcp_pwr_arbiter_id);
    }

    ieee80211_p2p_go_schedule_register_event_handler(go_schedule,ieee80211_p2p_client_power_go_schedule_evhandler, handle);
    ieee80211_p2p_client_register_event_handler(p2p_client_handle,ieee80211_p2p_client_power_p2p_event_handler , 
                                                handle, IEEE80211_P2P_CLIENT_EVENT_BEACON | IEEE80211_P2P_CLIENT_EVENT_PROBE_RESP |
                                                IEEE80211_P2P_CLIENT_EVENT_UPDATE_CTWINDOW | IEEE80211_P2P_CLIENT_EVENT_UPDATE_OPPPS);

    return handle;
}

/*
 * delete P2P client power save  SM object.
 */
void ieee80211_p2p_client_power_delete(ieee80211_p2p_client_power_t p2p_cli_power) 
{
    OS_CANCEL_TIMER(&p2p_cli_power->pcp_ctwindow_timer);
    OS_CANCEL_TIMER(&p2p_cli_power->pcp_async_null_event_timer);

    if (ieee80211_tsftimer_stop(p2p_cli_power->pcp_tbtt_tsf_timer)) {
           IEEE80211_DPRINTF(p2p_cli_power->pcp_vap,IEEE80211_MSG_STATE,
               "%s: ERROR: ieee80211_tsftimer_stop returns error \n", __func__);
    }
    if (ieee80211_tsftimer_free(p2p_cli_power->pcp_tbtt_tsf_timer, true) ) {
           IEEE80211_DPRINTF(p2p_cli_power->pcp_vap,IEEE80211_MSG_STATE,
               "%s: ERROR: ieee80211_tsftimer_free returns error \n", __func__);
    }
    if (p2p_cli_power->pcp_pwr_arbiter_id != 0) {
        (void) ieee80211_power_arbiter_disable(p2p_cli_power->pcp_vap, p2p_cli_power->pcp_pwr_arbiter_id);
        ieee80211_power_arbiter_free_id(p2p_cli_power->pcp_vap, p2p_cli_power->pcp_pwr_arbiter_id);
        p2p_cli_power->pcp_pwr_arbiter_id = 0;
    }
    ieee80211_mlme_sta_swbmiss_timer_free_id(p2p_cli_power->pcp_vap,p2p_cli_power->pcp_mlme_swbmiss_id);
    ieee80211_p2p_go_schedule_unregister_event_handler(p2p_cli_power->pcp_go_schedule,
                   ieee80211_p2p_client_power_go_schedule_evhandler, p2p_cli_power);
    wlan_vdev_txrx_unregister_event_handler(p2p_cli_power->pcp_vap->vdev_obj,ieee80211_p2p_client_power_txrx_event_handler,p2p_cli_power);
    ieee80211_p2p_client_unregister_event_handler(p2p_cli_power->pcp_client_handle,ieee80211_p2p_client_power_p2p_event_handler , 
                                                            p2p_cli_power);
    ieee80211_sm_delete(p2p_cli_power->pcp_hsm_handle); 

    OS_FREE_TIMER(&p2p_cli_power->pcp_ctwindow_timer);
    OS_FREE_TIMER(&p2p_cli_power->pcp_async_null_event_timer);

    OS_MESGQ_DESTROY(&p2p_cli_power->event_mesg_q);
    OS_FREE(p2p_cli_power);
}

/*
* handle a null frame  with PS=1.
* if the Station (PM SM) trying to enter Power save then
* intercept the frame and fail the frame. the PS SM 
* will try the frame 3 times and will go back to
* INIT. when will it try to send the null frame again.  
*/ 
int ieee80211_p2p_client_power_check_and_filter_null(ieee80211_p2p_client_power_t p2p_cli_power, wbuf_t wbuf)
{
    u_int8_t cur_state = ieee80211_sm_get_curstate(p2p_cli_power->pcp_hsm_handle);
    u_int8_t filter=0;
    struct ieee80211_tx_status ts;

    switch(cur_state) {
        case IEEE80211_P2P_CLIENT_POWER_STATE_GO_ABSENT:
        case IEEE80211_P2P_CLIENT_POWER_STATE_GO_ABSENT_TBTT_WAIT:
           /* force the frame to fail , because the GO is absent now */
           ts.ts_flags = IEEE80211_TX_ERROR;
           ts.ts_retries=0;
#if 0
           ieee80211_complete_wbuf(wbuf,&ts);
           filter=1;
#endif
        break;

        case IEEE80211_P2P_CLIENT_POWER_STATE_GO_PRESENT:
            /*
             * We sending a NULL with PS=1 to tell the GO
             * that this station is going to sleep.
             */
            ieee80211_sm_dispatch(p2p_cli_power->pcp_hsm_handle,
                                  IEEE80211_P2P_CLIENT_POWER_EVENT_PEND_TX_PS_NULL, 0, NULL);
        break;

        default:
        break;
    }
    return filter;
}

static void ieee80211_p2p_client_power_p2p_event_handler (wlan_p2p_client_t p2p_client_handle, ieee80211_p2p_client_event *event, void *arg)
{
    ieee80211_p2p_client_power_t p2p_cli_power = (ieee80211_p2p_client_power_t) arg;
    switch(event->type) {
    case IEEE80211_P2P_CLIENT_EVENT_UPDATE_OPPPS:
        p2p_cli_power->pcp_go_oppPS_state = event->u.oppPS;
        ieee80211_p2p_client_power_oppps_timer_reset(p2p_cli_power);
        IEEE80211_DPRINTF(p2p_cli_power->pcp_vap,IEEE80211_MSG_STATE, "%s: oppps %s \n", __func__, event->u.oppPS ? "ON":"OFF");
        break;
    case IEEE80211_P2P_CLIENT_EVENT_UPDATE_CTWINDOW:
        p2p_cli_power->pcp_ctwindow = IEEE80211_TU_TO_MS(event->u.ctwindow);
        break;
    case IEEE80211_P2P_CLIENT_EVENT_BEACON:
    case IEEE80211_P2P_CLIENT_EVENT_PROBE_RESP:
        /*
         * if we are not connected yet and we are joining 
         * (active,not scanning ),  
         * we still  need to honor NOA and oppPS to successfully
         * connect otherwise all our AUTH/ASSOC management frames will
         * lost beacuse of exessive retries as AP might be sleeping.
         */
        if (p2p_cli_power->pcp_connected == 0 &&
            ieee80211_vap_scanning_is_clear(p2p_cli_power->pcp_vap) && 
            ieee80211_vap_active_is_set(p2p_cli_power->pcp_vap) ) {
            /*
             * determine the current state of GO and also determine how much time left
             * in the CTWINDOW and then program the CTWINDOW timer accordingly.
             */
            ieee80211_p2p_client_power_sm_start(p2p_cli_power);
        }
        break;
    default:
        break;
    }
}

#endif

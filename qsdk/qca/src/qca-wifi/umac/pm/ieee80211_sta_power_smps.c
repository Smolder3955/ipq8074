/*
 * Copyright (c) 2011,2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2010, Atheros Communications Inc.
 * All Rights Reserved.
 */

#include "ieee80211_power_priv.h"
#include "ieee80211_data.h"
#include <wlan_mlme_dp_dispatcher.h>

#if UMAC_SUPPORT_STA_SMPS 

#define IEEE80211_PWRSAVE_TIMER_INTERVAL   200 /* 200 msec */
/*
 * SM power save state.  
 */  
typedef enum ieee80211_smps_state {
    IEEE80211_SM_PWRSAVE_WAIT,
    IEEE80211_SM_PWRSAVE_DISABLED,  
    IEEE80211_SM_PWRSAVE_ENABLED
} IEEE80211_SM_PWRSAVE_STATE;

/*  
 * SM power save state machine events  
 */  
typedef enum ieee80211_smpsevent {
    IEEE80211_SMPS_ENABLE,             /* SM power save can be enabled */
    IEEE80211_SMPS_DISABLE,            /* SM power save to be disabled */
    IEEE80211_SMPS_HW_SM_MODE,         /* Chip is out of SM power save */
    IEEE80211_SMPS_ACTION_FRAME_OK,    /* SM power save action frame successful */
    IEEE80211_SMPS_ACTION_FRAME_FAIL,  /* SM power save action frame failed */
    IEEE80211_SMPS_DISCONNECTION       /* Station disconnected */
} IEEE80211_SM_PWRSAVE_EVENT;

#define IEEE80211_CTS_SMPS 1
#define IEEE80211_SMPS_THRESH_DIFF   4
#define IEEE80211_SMPS_DATAHIST_NUM  5
struct ieee80211_pwrsave_smps {
    IEEE80211_SM_PWRSAVE_STATE  ips_smPowerSaveState;  /* Current dynamic MIMO power save state */
    u_int16_t               ips_smpsDataHistory[IEEE80211_SMPS_DATAHIST_NUM]; /* Throughput history buffer used for enabling MIMO ps */
    u_int8_t                ips_smpsCurrDataIndex;     /* Index in throughput history buffer to be updated */
    struct ieee80211vap     *ips_vap;
    u_int8_t                ips_connected;
    os_timer_t              ips_timer;                   /* to monitor vap activity */ 
} ;

static void ieee80211_pwrsave_smps_txrx_event_handler (struct wlan_objmgr_vdev *vdev, ieee80211_vap_txrx_event *event, void *arg);
/* 
 * SM Power Save Management Action frame 
 */
static void
ieee80211_pwrsave_smps_action_frame(struct ieee80211vap *vap, int smpwrsave)
{
    struct ieee80211_action_mgt_args actionargs;
    
    actionargs.category     = IEEE80211_ACTION_CAT_HT;
    actionargs.action       = IEEE80211_ACTION_HT_SMPOWERSAVE;
    actionargs.arg1         = smpwrsave;    /* SM Power Save state */
    actionargs.arg2         = 1;            /* SM Mode - Dynamic */
    actionargs.arg3         = 0;
    ieee80211_send_action(vap->iv_bss, &actionargs, NULL);
}

/*
 * SM Power Save state machine handler
 */
static void
ieee80211_pwrsave_smps_event(ieee80211_pwrsave_smps_t smps, IEEE80211_SM_PWRSAVE_EVENT event)
{
    struct ieee80211vap *vap = smps->ips_vap;
    struct ieee80211com *ic = vap->iv_ic;

    if (ieee80211_vap_dynamic_mimo_ps_is_clear(vap)) 
        return;

    switch (event) {
    case IEEE80211_SMPS_ENABLE:
        if (smps->ips_smPowerSaveState == IEEE80211_SM_PWRSAVE_DISABLED) {
            /* Send SMPS action frame to AP and wait till it gets ack'ed */
            ieee80211_pwrsave_smps_action_frame(vap, 1);
            wlan_vdev_txrx_register_event_handler(vap->vdev_obj,ieee80211_pwrsave_smps_txrx_event_handler,smps,
                                               IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_SMPS_ACT);
            smps->ips_smPowerSaveState = IEEE80211_SM_PWRSAVE_WAIT;
        }
        break;
    case IEEE80211_SMPS_HW_SM_MODE:
        break;
    case IEEE80211_SMPS_DISABLE:
    case IEEE80211_SMPS_DISCONNECTION:
        if (smps->ips_smPowerSaveState != IEEE80211_SM_PWRSAVE_DISABLED) {
            if (event == IEEE80211_SMPS_DISABLE)
                ieee80211_pwrsave_smps_action_frame(vap, 0);
            ieee80211_vap_smps_clear(vap);                 /* Clear status flag */
            wlan_vdev_set_smps(vdev, 0);
            ic->ic_sm_pwrsave_update(vap->iv_bss, TRUE, TRUE, TRUE);
            smps->ips_smPowerSaveState = IEEE80211_SM_PWRSAVE_DISABLED;
            wlan_vdev_txrx_unregister_event_handler(vap->vdev_obj,ieee80211_pwrsave_smps_txrx_event_handler,smps);
        }
        break;
    case IEEE80211_SMPS_ACTION_FRAME_OK:
        if (smps->ips_smPowerSaveState == IEEE80211_SM_PWRSAVE_WAIT) {
            ieee80211_vap_smps_set(vap);                 /* Clear status flag */
            wlan_vdev_set_smps(vdev, 1);
            wlan_vdev_txrx_unregister_event_handler(vap->vdev_obj,ieee80211_pwrsave_smps_txrx_event_handler,smps);
            ic->ic_sm_pwrsave_update(vap->iv_bss, FALSE, TRUE, TRUE);
            /* Send self-CTS frame to put chip in SMPS mode */
            ieee80211_send_cts(vap->iv_bss, IEEE80211_CTS_SMPS);
            smps->ips_smPowerSaveState = IEEE80211_SM_PWRSAVE_ENABLED;
        }
        break;
    case IEEE80211_SMPS_ACTION_FRAME_FAIL:
        if (smps->ips_smPowerSaveState == IEEE80211_SM_PWRSAVE_ENABLED) {
            /* Resend SMPS disable action frame */
            ieee80211_pwrsave_smps_action_frame(vap, 0);
        } else if (smps->ips_smPowerSaveState == IEEE80211_SM_PWRSAVE_WAIT) {
            smps->ips_smPowerSaveState = IEEE80211_SM_PWRSAVE_DISABLED;
            wlan_vdev_txrx_unregister_event_handler(vap->vdev_obj,ieee80211_pwrsave_smps_txrx_event_handler,smps);
        }
        break;
    default:
        break;
    }
}


static void ieee80211_pwrsave_smps_txrx_event_handler (struct wlan_objmgr_vdev *vdev, ieee80211_vap_txrx_event *event, void *arg)
{
    ieee80211_pwrsave_smps_t smps = (ieee80211_pwrsave_smps_t) arg;

    if (!smps->ips_connected)
        return;

    if (event->u.status == 0) {
        ieee80211_pwrsave_smps_event(smps, IEEE80211_SMPS_ACTION_FRAME_OK);
    } else {
        ieee80211_pwrsave_smps_event(smps, IEEE80211_SMPS_ACTION_FRAME_FAIL);
    }
}

static bool 
ieee80211_pwrsave_smps_check(ieee80211_pwrsave_smps_t smps)
{
    u_int16_t i, throughput = 0, rssi;
    struct ieee80211vap *vap = smps->ips_vap;
    struct ieee80211com *ic = vap->iv_ic;

    if (!smps->ips_connected ||
        (wlan_vdev_is_up(vap->vdev_obj) != QDF_STATUS_SUCCESS) ||
        (vap->iv_opmode != IEEE80211_M_STA) ||
        (!ieee80211node_has_flag(vap->iv_bss, IEEE80211_NODE_HT))) {
        return FALSE;
    }

    /* SM power save check.
     * SM Power save is enabled if,
     * - There is no data traffic or
     * - Throughput is less than threshold and RSSI is greater than threshold.
     */
    throughput = (wlan_vdev_get_txrxbytes(vap->vdev_obj)) / (IEEE80211_PWRSAVE_TIMER_INTERVAL * 125);  // in Mbps
    rssi = ic->ic_node_getrssi(vap->iv_bss, -1, IEEE80211_RSSI_BEACON);
    wlan_vdev_set_txrxbytes(vap->vdev_obj, 0);   // Clear byte counter

    smps->ips_smpsDataHistory[smps->ips_smpsCurrDataIndex++] = throughput;
    smps->ips_smpsCurrDataIndex = smps->ips_smpsCurrDataIndex % IEEE80211_SMPS_DATAHIST_NUM;

    
    /* We calculate average throughput over the past samples */
    throughput = 0;
    for (i = 0; i < IEEE80211_SMPS_DATAHIST_NUM;i++) {
        throughput += smps->ips_smpsDataHistory[i];
    }
    throughput /= IEEE80211_SMPS_DATAHIST_NUM;

    /* 
     * We make the thresholds slightly different for SM power save enable & disable to get 
     * over the ping-pong effect when calculated throughput is close to the threshold value.
     * SMPS Enable Threshold = Registry Value
     * SMPS Disable Threshold = Registry Value + IEEE80211_SMPS_THRESH_DIFF
     */
    if (!throughput || ((throughput < vap->iv_smps_datathresh) &&
                        (rssi > vap->iv_smps_rssithresh))) {
        /* Receive criteria met, do SM power save. */
        ieee80211_pwrsave_smps_event(smps, IEEE80211_SMPS_ENABLE);
    } else if ((throughput > (vap->iv_smps_datathresh + IEEE80211_SMPS_THRESH_DIFF)) ||
               (rssi < vap->iv_smps_rssithresh)) {
        ieee80211_pwrsave_smps_event(smps, IEEE80211_SMPS_DISABLE);
    }
    return TRUE;
}

static OS_TIMER_FUNC(ieee80211_pwrsave_smps_timer)
{
    ieee80211_pwrsave_smps_t smps;
    OS_GET_TIMER_ARG(smps, ieee80211_pwrsave_smps_t);
    if (ieee80211_vap_dynamic_mimo_ps_is_set(smps->ips_vap)) {
        if (ieee80211_pwrsave_smps_check(smps)) {
            OS_SET_TIMER(&smps->ips_timer,IEEE80211_PWRSAVE_TIMER_INTERVAL);
        }
    }
}

static void ieee80211_pwrsave_smps_vap_event_handler (ieee80211_vap_t vap, ieee80211_vap_event *event, void *arg)
{
    ieee80211_pwrsave_smps_t smps = (ieee80211_pwrsave_smps_t) arg;

    switch(event->type) {
    case IEEE80211_VAP_UP:
        if (!smps->ips_connected ) {
            smps->ips_connected=TRUE;
            OS_SET_TIMER(&smps->ips_timer,IEEE80211_PWRSAVE_TIMER_INTERVAL);
        } 
        break;
    case IEEE80211_VAP_FULL_SLEEP:
    case IEEE80211_VAP_DOWN:
        OS_CANCEL_TIMER(&smps->ips_timer);
        smps->ips_connected = FALSE;
        break;
    default:
        break;
    }
}


ieee80211_pwrsave_smps_t ieee80211_pwrsave_smps_attach(struct ieee80211vap *vap, u_int32_t smpsDynamic)
{
    ieee80211_pwrsave_smps_t smps;
    osdev_t os_handle = vap->iv_ic->ic_osdev;
    smps = (ieee80211_pwrsave_smps_t)OS_MALLOC(os_handle,sizeof(struct ieee80211_pwrsave_smps),0);
    if (smps) {
        OS_MEMZERO(smps, sizeof(struct ieee80211_pwrsave_smps));
        /*
         * Initialize pwrsave timer 
         */
        OS_INIT_TIMER(os_handle,
                      &smps->ips_timer,                         
                      ieee80211_pwrsave_smps_timer,
                      smps, QDF_TIMER_TYPE_WAKE_APPS);
        if (smpsDynamic && IEEE80211_HAS_DYNAMIC_SMPS_CAP(vap->iv_ic)) {
            ieee80211_vap_dynamic_mimo_ps_set(vap);
        } else {
            ieee80211_vap_dynamic_mimo_ps_clear(vap);
        }
        smps->ips_smPowerSaveState      = IEEE80211_SM_PWRSAVE_DISABLED;
        smps->ips_connected = false;
        smps->ips_vap =  vap;
        ieee80211_vap_register_event_handler(vap,ieee80211_pwrsave_smps_vap_event_handler,(void *)smps );
    }

    return smps;
}

void ieee80211_pwrsave_smps_detach(ieee80211_pwrsave_smps_t smps)
{
    wlan_vdev_txrx_unregister_event_handler(smps->ips_vap->vdev_obj,ieee80211_pwrsave_smps_txrx_event_handler,smps);
    ieee80211_vap_unregister_event_handler(smps->ips_vap,ieee80211_pwrsave_smps_vap_event_handler,(void *)smps );
    OS_FREE_TIMER(&smps->ips_timer);                         
    OS_FREE(smps);
}
#else
/* dummy declraration to keep compiler happy */
typedef int ieee80211_pwrsave_smps_dummy;

#endif

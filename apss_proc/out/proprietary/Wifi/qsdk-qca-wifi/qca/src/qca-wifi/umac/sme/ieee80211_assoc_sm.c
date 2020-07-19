/*
 * Copyright (c) 2011,2017-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.
 */

#include "ieee80211_var.h"
#include "ieee80211_sme_api.h"
#include "ieee80211_sm.h"
#include "ieee80211_assoc_private.h"
#include <ieee80211_channel.h>
#include <ieee80211_mlme_dfs_dispatcher.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_vdev_mlme_api.h>
/*
 * default max assoc and auth attemtps.
 */
#define MAX_ASSOC_ATTEMPTS 6
#define MAX_AUTH_ATTEMPTS  6
#define MAX_SAE_AUTH_ATTEMPTS  2
#if defined(QCA_WIFI_NAPIER_EMULATION) || defined(QCA_WIFI_QCA8074_VP)
#define MAX_TSFSYNC_TIME   6000     /* msec */
#else
#define MAX_TSFSYNC_TIME   1500     /* msec */
#endif
#define MAX_TXCSA_TIME     1000    /* msec */
#ifdef QCA_WIFI_QCA8074_VP
#define MAX_MGMT_TIME      600   /* msec */
#define AUTH_RETRY_TIME    100
#define ASSOC_RETRY_TIME   100
#define DISASSOC_WAIT_TIME 100 /* msec */
#define FTIE_UPDATE_WAIT_TIME          10 /* msec */
#define MAX_FTIE_UPDATE_WAIT_ATTEMPTS  50
#else
#define MAX_MGMT_TIME      200   /* msec */
#define AUTH_RETRY_TIME    90
#define ASSOC_RETRY_TIME   30
#define DISASSOC_WAIT_TIME 10 /* msec */
#define FTIE_UPDATE_WAIT_TIME          10 /* msec */
#define MAX_FTIE_UPDATE_WAIT_ATTEMPTS  50
#endif
#define MAX_QUEUED_EVENTS  16
#define MLME_OP_CHECK_TIME 10 /* msec */
#define REJOIN_CHECKING_TIME 1000   /* Cisco AP workaround */
#define REJOIN_ATTEMP_TIME      5   /* Cisco AP workaround */



static OS_TIMER_FUNC(assoc_sm_timer_handler);

struct _wlan_assoc_sm {
    osdev_t           os_handle;
    wlan_if_t         vap_handle;
    ieee80211_hsm_t   hsm_handle;
    wlan_scan_entry_t scan_entry;
    wlan_assoc_sm_event_handler sm_evhandler;
    os_if_t           osif;
    os_timer_t        sm_timer; /* generic timer */
    u_int8_t          max_assoc_attempts; /* maxmimum assoc attempts */
    u_int8_t          cur_assoc_attempts; /* current assoc attempt */
    u_int8_t          max_auth_attempts;  /* maxmimum auth attempts */
    u_int8_t          cur_auth_attempts;  /* current auth attempt */
    u_int16_t         max_mgmt_time;      /* maxmimum time to wait for response */
    u_int16_t         max_tsfsync_time;   /* max time to wait for a beacon in msec */
    u_int8_t          last_reason;
    wlan_assoc_sm_event_disconnect_reason   last_failure;
    u_int8_t          prev_bssid[IEEE80211_ADDR_LEN];
    u_int8_t          timeout_event;      /* event to dispacth when timer expires */
    u_int32_t         is_bcn_recvd:1,
                      is_stop_requested:1,
                      sync_stop_requested:1,
                      is_running:1,
                      is_join:1,
                      is_sm_run:1,
                      is_sm_repeater_cac:1;
    /* Cisco AP workaround */
    u_int32_t         last_connected_time;
    u_int8_t          cur_rejoin_attempts;
    u_int8_t          ftie_update_wait_attempts;
    struct ieee80211_ath_channel *chosen_chan;
};

static void ieee80211_assoc_sm_debug_print (void *ctx,const char *fmt,...)
{
    char tmp_buf[256];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf (tmp_buf,256, fmt, ap);
    va_end(ap);
    IEEE80211_DPRINTF(((wlan_assoc_sm_t) ctx)->vap_handle, IEEE80211_MSG_STATE,
        "%s", tmp_buf);
}
/*
 * 802.11 station association state machine implementation.
 */
static void ieee80211_send_event(wlan_assoc_sm_t sm, wlan_assoc_sm_event_type type,wlan_assoc_sm_event_disconnect_reason reason)
{
    wlan_assoc_sm_event sm_event;
    sm_event.event = type;
    sm_event.reason = reason ;
    sm_event.reasoncode = sm->last_reason;
    (* sm->sm_evhandler) (sm, sm->osif, &sm_event);
}

/*
 * different state related functions.
 */


/*
 * INIT
 */
static void ieee80211_assoc_state_init_entry(void *ctx)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;
    enum conn_reset_reason reset_reason = SM_DISCONNECTED_STATE;

    if (sm->is_join) {
       sm->is_join=0;
       /* cancel any pending mlme operation */
       wlan_mlme_cancel(sm->vap_handle);
       if (sm->sync_stop_requested) {
           wlan_mlme_stop_bss(sm->vap_handle, WLAN_MLME_STOP_BSS_F_FORCE_STOP_RESET);
       } else {
           if (sm->is_sm_run)
               reset_reason = SM_CONNECTED_STATE;
           else if (sm->is_sm_repeater_cac)
               reset_reason = SM_REPEATER_CAC_STATE;
           wlan_mlme_connection_reset(sm->vap_handle, reset_reason);
       }
    }
    if (sm->scan_entry) {
        util_scan_free_cache_entry(sm->scan_entry);
        sm->scan_entry = NULL;
    }
    if (sm->last_failure == WLAN_ASSOC_SM_REASON_ASSOC_REJECT) {
        ieee80211_send_event(sm, WLAN_ASSOC_SM_EVENT_ASSOC_REJECT, sm->last_failure);
    } else if (sm->is_stop_requested ) {
        ieee80211_send_event(sm, WLAN_ASSOC_SM_EVENT_DISCONNECT, 0);
    } else {
        ieee80211_send_event(sm, WLAN_ASSOC_SM_EVENT_FAILED, sm->last_failure);
    }
    sm->cur_auth_attempts = 0;
    sm->cur_assoc_attempts = 0;
    sm->last_reason = 0;
    sm->last_failure = 0;
    sm->is_bcn_recvd = 0;
    sm->is_stop_requested = 0;
    sm->sync_stop_requested=0;
    sm->is_running = 0;
    sm->is_sm_run = 0;
    sm->is_sm_repeater_cac = 0;
    sm->ftie_update_wait_attempts = 0;

    /* Cisco AP workaround */
    sm->last_connected_time = 0;
    sm->cur_rejoin_attempts = 0;
}

static void ieee80211_assoc_state_init_exit(void *ctx)
{
    /* NONE */
}

static bool ieee80211_assoc_state_init_event(void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;

    enum ieee80211_phymode phymode = IEEE80211_MODE_AUTO;
    struct ieee80211_ath_channel *chosen_chan = NULL, *ni_chan = NULL;
    struct wlan_objmgr_pdev *pdev;

    switch(event) {
    case IEEE80211_ASSOC_EVENT_CONNECT_REQUEST:
    case IEEE80211_ASSOC_EVENT_REASSOC_REQUEST:

        ASSERT(sm->scan_entry != NULL);
        ni_chan = chosen_chan = wlan_util_scan_entry_channel(sm->scan_entry);

        phymode = ieee80211_chan2mode(chosen_chan);

        if (sm->vap_handle->iv_des_mode != IEEE80211_MODE_AUTO) {
            /* if desired mode is not auto then find if the requested mode
               is supported by the AP */
            phymode = ieee80211_get_phy_mode(sm->vap_handle->iv_ic,chosen_chan, sm->vap_handle->iv_des_mode,phymode);
            if (phymode != IEEE80211_MODE_AUTO) {
                chosen_chan = ieee80211_find_dot11_channel(sm->vap_handle->iv_ic, chosen_chan->ic_ieee,
                        chosen_chan->ic_vhtop_ch_freq_seg2, phymode | sm->vap_handle->iv_ic->ic_chanbwflag);
                if ((chosen_chan == NULL) || (chosen_chan == IEEE80211_CHAN_ANYC)) {
                    IEEE80211_DPRINTF(sm->vap_handle, IEEE80211_MSG_MLME, "%s ,can not find a channel with the desired mode \n", __func__);
                    chosen_chan = ni_chan;
                }
            }
        }
        sm->chosen_chan = chosen_chan;

        pdev = sm->vap_handle->iv_ic->ic_pdev_obj;
        if(pdev == NULL) {
            qdf_print("%s : pdev is null", __func__);
            return false;
        }

        /* Go for Tx CSA when all of the following are true:
         * 1)Tx CSA is set by the user.
         * 2)Enhanced Independent Repeater  is set by the user.
         * 3)The STA is the main STA (either regular STA or main proxy STA).
         * 4)There is at least on AP running/beaconing.
         * 5)When the chosen phy channel and current channel is different.
         */
        if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                QDF_STATUS_SUCCESS) {
            return false;
        }
        IEEE80211_DPRINTF(sm->vap_handle, IEEE80211_MSG_MLME,
                "%s : TXCSA = %d RepCAC = %d Enh_ind = %d Mainsta = %d apvap = %d chosen_diff = %d apcac = %d\n", __func__,
                wlan_mlme_is_txcsa_set(sm->vap_handle) ,
                wlan_mlme_is_repeater_cac_set(sm->vap_handle),
                wlan_get_param(sm->vap_handle,IEEE80211_FEATURE_VAP_ENHIND) ,
                wlan_mlme_is_vap_main_sta(sm->vap_handle) ,
                wlan_mlme_num_apvap_running(sm->vap_handle),
                wlan_mlme_is_chosen_diff_from_curchan(sm->vap_handle,sm->chosen_chan),
                mlme_dfs_is_ap_cac_timer_running(pdev)
                );
        wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);

        if(wlan_get_param(sm->vap_handle,IEEE80211_FEATURE_VAP_ENHIND) &&
           wlan_mlme_is_vap_main_sta(sm->vap_handle) &&
           wlan_mlme_num_apvap_running(sm->vap_handle) &&
           wlan_mlme_is_chosen_diff_from_curchan(sm->vap_handle,sm->chosen_chan)) {
            ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_TXCHANSWITCH);
        } else {
            /* If it is coming here for second time that is after txcsa is over
             * we need to see if the CAC timer is running then wait for the CAC
             */
            if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                    QDF_STATUS_SUCCESS) {
                return false;
            }
            if(wlan_mlme_is_repeater_cac_set(sm->vap_handle) &&
                    wlan_mlme_is_scanentry_dfs(sm->chosen_chan) &&
                    mlme_dfs_is_ap_cac_timer_running(pdev)) {
                wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
                ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_REPEATER_CAC);
            } else {
                wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
                ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_JOIN);
            }
        }
        return true;
        break;
    case IEEE80211_ASSOC_EVENT_DISCONNECT_REQUEST:
    case IEEE80211_ASSOC_EVENT_DISASSOC_REQUEST:
        return true;

    default:
        return false;
    }
}

/*
 *TXCHANSWITCH
 */
static void ieee80211_assoc_state_txchanswitch_entry(void *ctx)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;
    if (wlan_mlme_start_txchanswitch(sm->vap_handle, sm->chosen_chan, MAX_TXCSA_TIME) != 0 ) {
        IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s: txchanswitch failed \n",__func__);
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_ASSOC_EVENT_TXCHANSWITCH_FAIL,0,NULL);
        return;
    }
}

static void ieee80211_assoc_state_txchanswitch_exit(void *ctx)
{
    /* NONE */
}

static bool ieee80211_assoc_state_txchanswitch_event(void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;
    struct ieee80211com *ic = sm->vap_handle->iv_ic;

    switch(event) {
    case IEEE80211_ASSOC_EVENT_TXCHANSWITCH_SUCCESS:
    case IEEE80211_ASSOC_EVENT_TXCHANSWITCH_FAIL: /* Whether CSA happened or not we need to Join */

        /* Wait for AP CAC if Channel is DFS.
         * NOTE:- Since if we had started TXCHANSWITCH the AP VAPs must be doing CAC
         */
#if ATH_SUPPORT_DFS
        if(wlan_mlme_is_repeater_cac_set(sm->vap_handle) &&
           wlan_mlme_is_scanentry_dfs(sm->chosen_chan)) {
            ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_REPEATER_CAC);
        } else
#endif
        {
            ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_JOIN);
        }
        return true;
        break;
    case IEEE80211_ASSOC_EVENT_DISCONNECT_REQUEST:
    case IEEE80211_ASSOC_EVENT_DISASSOC_REQUEST:
        /* cancel pending mlme operation */
        wlan_mlme_cancel(sm->vap_handle);
        /* Clear flag that was set during entry in txchanswitch state */
        ieee80211com_clear_flags(ic,IEEE80211_F_CHANSWITCH);
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_INIT);
        return true;
        break;

    default:
        return false;
    }
}
#if ATH_SUPPORT_DFS
/*
 *REPEATER_CAC
 */
static void ieee80211_assoc_state_repeater_cac_entry(void *ctx)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;

    sm->is_sm_repeater_cac = 1;
    /* We do not do anything here. We do not start STA VAP here. Let AP VAP(s) do the
     * CAC and send us the CAC completetion event. We have our own time in case CAC
     * take too long to complete for some unforseen scenario
     */
    if (wlan_mlme_start_repeater_cac(sm->vap_handle, sm->chosen_chan) != 0 ) {
        IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s: repeater cac failed \n",__func__);
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_ASSOC_EVENT_REPEATER_CAC_FAIL,0,NULL);
        return;
    }
}

static void ieee80211_assoc_state_repeater_cac_exit(void *ctx)
{
    /* NONE */
}

static bool ieee80211_assoc_state_repeater_cac_event(void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;

    switch(event) {
    case IEEE80211_ASSOC_EVENT_REPEATER_CAC_SUCCESS:
        /* If AP VAP comes up first, let's say in channel 100, then AP VAP is
         * doing CAC. In the mean time if STA VAP found Root AP in channel 60
         * then STA thinks that AP is doing CAC in channel 60. Since AP VAP is
         * still in DFS_WAIT number of AP VAP running is zero and therefore
         * TXCSA does not happen. If TXCSA does not happen after CAC AP VAP
         * comes up in channel 100 and STA comes up in channel 60. And STA
         * does not detect a beacon miss.
         */
        if(wlan_mlme_is_chosen_diff_from_curchan(sm->vap_handle,sm->chosen_chan)/* if STA's target channel and AP channels are different after CAC then
             * go to TXSwitch again */) {
            ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_TXCHANSWITCH);
        } else {
            ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_JOIN);
        }
        return true;
        break;
    case IEEE80211_ASSOC_EVENT_REPEATER_CAC_FAIL:
        if (sm->scan_entry) wlan_util_scan_entry_mlme_set_assoc_state(sm->scan_entry, AP_ASSOC_STATE_NONE);
        /* cancel pending mlme operation */
        wlan_mlme_cancel(sm->vap_handle);
        if (wlan_mlme_operation_in_progress(sm->vap_handle)) {
            ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_MLME_WAIT);
        } else {
            ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_INIT);
        }
        return true;
        break;
    case IEEE80211_ASSOC_EVENT_DISCONNECT_REQUEST:
    case IEEE80211_ASSOC_EVENT_DISASSOC_REQUEST:
       /* cancel pending mlme operation */
        wlan_mlme_cancel(sm->vap_handle);
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_INIT);
        return true;
        break;

    default:
        return false;
    }
}
#endif

/*
 *JOIN
 */
static void ieee80211_assoc_state_join_init_entry(void *ctx)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;
    struct ieee80211vap *vap = sm->vap_handle;
    u_int8_t *se_macaddr = util_scan_entry_macaddr(sm->scan_entry);
    struct ieee80211_node *ni = NULL;
    enum wlan_vdev_state vdev_state;

    IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s \n",__func__);

    /* assoc sm RUN to JOIN state transition requires to wait
     * for previous peer being deleted and new connection
     * attempt being started once peer delete resonse is received.
     */
    ni = ieee80211_vap_find_node(vap, se_macaddr);
    if (ni) {
        qdf_print("%s: vap:%d(0x%pK), previous node: 0x%pK:%s found."
               " delete it first", __func__, vap->iv_unit, vap,
               ni, ether_sprintf(se_macaddr));
        qdf_print("vap->iv_myaddr: %s", ether_sprintf(vap->iv_myaddr));
        /* Free previous peer */
        ieee80211_sta_leave(ni);
        ieee80211_free_node(ni);
    }
    sm->is_sm_run = 0;

    ieee80211_reset_bss(vap);
    vdev_state = wlan_vdev_mlme_get_state(vap->vdev_obj);
    if (vdev_state != WLAN_VDEV_S_INIT) {
        /* Wait for peer delete response and vdev moving to init state.
         * once previous bss node is freed, BSS_NODE_FREED event is posted
         * which in turn  moves assoc sm to JOIN_COMPLETE and connetion
         * process will continue.
         */
        qdf_print("%s: vap:%d(0x%pK), logically deleted ni: 0x%pK, se_mac: %s"
                " vdev_state: %d. wait for BSS_NODE_FREED event", __func__,
                vap->iv_unit, vap, ni, ether_sprintf(se_macaddr), vdev_state);
    } else {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE,
                "%s: vap:%d(0x%pK), continue to JOIN_COMPLETE\n",
                __func__, vap->iv_unit, vap);
        /* move to next state */
        ieee80211_sm_dispatch(sm->hsm_handle,
                IEEE80211_ASSOC_EVENT_JOIN_COMPLETE, 0, NULL);
    }
}

static void ieee80211_assoc_state_join_init_exit(void *ctx)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;
    IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s \n",__func__);
}

static bool ieee80211_assoc_state_join_init_event(void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;
    IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s \n",__func__);
    switch(event) {
        case IEEE80211_ASSOC_EVENT_JOIN_COMPLETE:
            ieee80211_sm_transition_to(sm->hsm_handle, IEEE80211_ASSOC_STATE_JOIN_SETTING_COUNTRY);
            break;
        case IEEE80211_ASSOC_EVENT_BEACON_WAIT_TIMEOUT:
        case IEEE80211_ASSOC_EVENT_BEACON_MISS:
        case IEEE80211_ASSOC_EVENT_JOIN_FAIL:
        case IEEE80211_ASSOC_EVENT_DISCONNECT_REQUEST:
        case IEEE80211_ASSOC_EVENT_DISASSOC_REQUEST:
            /* cancel pending mlme operation */
            wlan_mlme_cancel(sm->vap_handle);
            if (wlan_mlme_operation_in_progress(sm->vap_handle)) {
                ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_MLME_WAIT);
            } else {
                ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_INIT);
            }
            break;
        default:
            return false;
    }

	return true;
}

static void ieee80211_assoc_state_join_setting_country_entry(void *ctx)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;

    sm->is_join=1;
    if (wlan_mlme_join_infra(sm->vap_handle, sm->scan_entry, sm->max_tsfsync_time) != 0 ) {
        IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s: join_infra failed \n",__func__);
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_ASSOC_EVENT_JOIN_FAIL,0,NULL);
        return;
    }
}

static void ieee80211_assoc_state_join_setting_country_exit(void *ctx)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;

    IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s \n",__func__);
}

static bool ieee80211_assoc_state_join_setting_country_event(void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;

    switch(event) {
        case IEEE80211_ASSOC_EVENT_JOIN_SUCCESS_SET_COUNTRY:
            ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_JOIN_COMPLETED);
            return true;
            break;
        case IEEE80211_ASSOC_EVENT_BEACON_WAIT_TIMEOUT:
        case IEEE80211_ASSOC_EVENT_BEACON_MISS:
        case IEEE80211_ASSOC_EVENT_JOIN_FAIL:
        case IEEE80211_ASSOC_EVENT_DISCONNECT_REQUEST:
        case IEEE80211_ASSOC_EVENT_DISASSOC_REQUEST:
            /* cancel pending mlme operation */
            wlan_mlme_cancel(sm->vap_handle);

            if (wlan_mlme_operation_in_progress(sm->vap_handle)) {
                ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_MLME_WAIT);
            } else {
                ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_INIT);
            }
            break;
        default:
            return false;
    }

    return true;
}

static void ieee80211_assoc_state_join_entry(void *ctx)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;

    sm->is_join=1;
    if (wlan_mlme_join_infra_continue(sm->vap_handle, sm->scan_entry, sm->max_tsfsync_time) != 0 ) {
        IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s: join_infra failed \n",__func__);
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_ASSOC_EVENT_JOIN_FAIL,0,NULL);
        return;
    }
}

static void ieee80211_assoc_state_join_exit(void *ctx)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;

    IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s \n",__func__);
    wlan_mlme_cancel(sm->vap_handle);/* cancel any pending mlme join req */
}

static bool ieee80211_assoc_state_join_event(void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;

    switch(event) {
        case IEEE80211_ASSOC_EVENT_JOIN_SUCCESS:
            if (wlan_util_scan_entry_mlme_assoc_state(sm->scan_entry) >= AP_ASSOC_STATE_AUTH) {
                ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_ASSOC);
            } else {
                ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_AUTH);
            }
            return true;
            break;

        case IEEE80211_ASSOC_EVENT_BEACON_WAIT_TIMEOUT:
        case IEEE80211_ASSOC_EVENT_BEACON_MISS:
        case IEEE80211_ASSOC_EVENT_JOIN_FAIL:
        case IEEE80211_ASSOC_EVENT_DISCONNECT_REQUEST:
        case IEEE80211_ASSOC_EVENT_DISASSOC_REQUEST:
            /* cancel pending mlme operation */
            wlan_mlme_cancel(sm->vap_handle);
            if (wlan_mlme_operation_in_progress(sm->vap_handle)) {
                ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_MLME_WAIT);
            } else {
                ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_INIT);
            }
            return true;
            break;

        default:
            return false;
    }
}

/*
 * AUTH
 */
static void ieee80211_assoc_state_auth_entry(void *ctx)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;
    ++sm->cur_auth_attempts;

    if (wlan_mlme_auth_request(sm->vap_handle,sm->max_mgmt_time) !=0 ) {
        IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s: auth_request failed retrying ...\n",__func__);
        sm->timeout_event = IEEE80211_ASSOC_EVENT_TIMEOUT,
        OS_SET_TIMER(&sm->sm_timer,AUTH_RETRY_TIME);
        return;
    }
}

static void ieee80211_assoc_state_auth_exit(void *ctx)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;
    wlan_mlme_cancel(sm->vap_handle); /* cancel any pending mlme auth req*/
    OS_CANCEL_TIMER(&sm->sm_timer);
}

static bool ieee80211_assoc_state_auth_event(void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;

    switch(event) {

    case IEEE80211_ASSOC_EVENT_AUTH_SUCCESS:
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_ASSOC);
        return true;
        break;

    case IEEE80211_ASSOC_EVENT_AUTH_FAIL:
    case IEEE80211_ASSOC_EVENT_TIMEOUT:
        sm->last_failure = WLAN_ASSOC_SM_REASON_AUTH_FAILED;
        if (sm->cur_auth_attempts < sm->max_auth_attempts) {
            ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_AUTH);
            return true;
            break;
        }

        IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s: max auth attempts reached \n",__func__);
        if (sm->scan_entry) {
               wlan_util_scan_entry_mlme_set_assoc_state(sm->scan_entry, AP_ASSOC_STATE_NONE);
        }
        /* fall thru */

    case IEEE80211_ASSOC_EVENT_DISCONNECT_REQUEST:
    case IEEE80211_ASSOC_EVENT_DISASSOC_REQUEST:
       /* cancel pending mlme operation */
        wlan_mlme_cancel(sm->vap_handle);
        if (wlan_mlme_operation_in_progress(sm->vap_handle)) {
            ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_MLME_WAIT);
        } else {
            ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_INIT);
        }
        return true;
        break;

    case IEEE80211_ASSOC_EVENT_DEAUTH:
        ieee80211_send_event(sm, WLAN_ASSOC_SM_EVENT_REJOINING, WLAN_ASSOC_SM_REASON_DEAUTH);
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_AUTH);
        return true;
        break;

    default:
        return false;

    }
}

/*
 * ASSOC
 */
static void ieee80211_assoc_state_assoc_entry(void *ctx)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;
    wlan_if_t vap = sm->vap_handle;
    u_int8_t zero_mac[IEEE80211_ADDR_LEN] = { 0, 0, 0, 0, 0, 0 };

    if (vap->iv_roam.iv_wait_for_ftie_update) {
        sm->ftie_update_wait_attempts++;
        sm->timeout_event = IEEE80211_ASSOC_EVENT_WAIT_FOR_FTIE_UPDATE;
        OS_SET_TIMER(&sm->sm_timer,FTIE_UPDATE_WAIT_TIME);
        return;
    }

    ++sm->cur_assoc_attempts;
    if (!IEEE80211_ADDR_EQ(sm->prev_bssid, zero_mac) ||
            wlan_util_scan_entry_mlme_assoc_state(sm->scan_entry) == AP_ASSOC_STATE_ASSOC) {
        if (wlan_mlme_reassoc_request(sm->vap_handle,sm->prev_bssid, sm->max_mgmt_time) !=0 ) {
            IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s: reassoc request failed retrying ...\n",__func__);
            sm->timeout_event = IEEE80211_ASSOC_EVENT_TIMEOUT,
            OS_SET_TIMER(&sm->sm_timer,ASSOC_RETRY_TIME);
            return;
        }
    } else {
        if (wlan_mlme_assoc_request(sm->vap_handle, sm->max_mgmt_time) !=0 ) {
            IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s: assoc request failed retrying ...\n",__func__);
            sm->timeout_event = IEEE80211_ASSOC_EVENT_TIMEOUT,
            OS_SET_TIMER(&sm->sm_timer,ASSOC_RETRY_TIME);
            return;
        }
    }
}

static void ieee80211_assoc_state_assoc_exit(void *ctx)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;
    wlan_mlme_cancel(sm->vap_handle); /* cancel any pending mlme assoc req */
    OS_CANCEL_TIMER(&sm->sm_timer);
}

static bool ieee80211_assoc_state_assoc_event(void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;

    switch(event) {
    case IEEE80211_ASSOC_EVENT_ASSOC_SUCCESS:
        sm->last_connected_time = OS_GET_TIMESTAMP();
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_RUN);
        return true;
        break;

    case IEEE80211_ASSOC_EVENT_ASSOC_FAIL:
	if ((sm->last_reason == IEEE80211_STATUS_REJECT_TEMP) ||
            (sm->last_reason == IEEE80211_STATUS_INVALID_PMKID) ||
            (sm->last_reason == IEEE80211_STATUS_ANTI_CLOGGING_TOKEN_REQ) ||
            (sm->last_reason == IEEE80211_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED)) {

            IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s: Assoc reject status: %d \n",__func__, sm->last_reason);
            if (sm->scan_entry) wlan_util_scan_entry_mlme_set_assoc_state(sm->scan_entry, AP_ASSOC_STATE_NONE);

            sm->last_failure = WLAN_ASSOC_SM_REASON_ASSOC_REJECT;
            ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_INIT);

            return true;
	}
       /* fall through if assoc failure is not due to above checked reasons */
    case IEEE80211_ASSOC_EVENT_TIMEOUT:
        if (sm->cur_assoc_attempts >= sm->max_assoc_attempts) {
            IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s: max assoc attempts reached \n",__func__);
            if (sm->scan_entry) wlan_util_scan_entry_mlme_set_assoc_state(sm->scan_entry, AP_ASSOC_STATE_NONE);
            ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_INIT);
            return true;
        }
        sm->last_failure = WLAN_ASSOC_SM_REASON_ASSOC_FAILED;
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_ASSOC);
        return true;
        break;

    case IEEE80211_ASSOC_EVENT_DISCONNECT_REQUEST:
    case IEEE80211_ASSOC_EVENT_DISASSOC_REQUEST:
        if (sm->scan_entry) wlan_util_scan_entry_mlme_set_assoc_state(sm->scan_entry, AP_ASSOC_STATE_NONE);
        /* cancel pending mlme operation */
        wlan_mlme_cancel(sm->vap_handle);
        if (wlan_mlme_operation_in_progress(sm->vap_handle)) {
            ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_MLME_WAIT);
        } else {
            ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_INIT);
        }
        break;

    case IEEE80211_ASSOC_EVENT_DEAUTH:
        ieee80211_send_event(sm, WLAN_ASSOC_SM_EVENT_REJOINING, WLAN_ASSOC_SM_REASON_DEAUTH);
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_AUTH);
        return true;
        break;

    case IEEE80211_ASSOC_EVENT_WAIT_FOR_FTIE_UPDATE:
        if (sm->ftie_update_wait_attempts >= MAX_FTIE_UPDATE_WAIT_ATTEMPTS) {
            if (sm->scan_entry) wlan_util_scan_entry_mlme_set_assoc_state(sm->scan_entry, AP_ASSOC_STATE_NONE);
            ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_INIT);
            return true;
        }
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_ASSOC);
        return true;
        break;

    default:
        return false;
    }
    return false;
}

/*
 *RUN
 */
static void ieee80211_assoc_state_run_entry(void *ctx)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;
    wlan_assoc_sm_event sm_event;

    wlan_mlme_connection_up(sm->vap_handle);
    sm_event.event = WLAN_ASSOC_SM_EVENT_SUCCESS;
    sm_event.reason = WLAN_ASSOC_SM_REASON_NONE;
    sm_event.reasoncode = 0;
    sm->is_sm_run = 1;
    (* sm->sm_evhandler) (sm, sm->osif, &sm_event);
    ieee80211_update_custom_scan_chan_list(sm->vap_handle, true);
}

static void ieee80211_assoc_state_run_exit(void *ctx)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;
    wlan_mlme_connection_down(sm->vap_handle);
    sm->cur_auth_attempts = 0;
    sm->cur_assoc_attempts = 0;
    sm->last_reason = 0;
    sm->is_bcn_recvd = 0;
   ieee80211_update_custom_scan_chan_list(sm->vap_handle, false);
}

static bool ieee80211_assoc_state_run_event(void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;
    wlan_if_t vap = sm->vap_handle;
    u_int8_t cur_bssid[IEEE80211_ADDR_LEN];

    qdf_print("%s: vap: %d(0x%pK) event: %d", __func__, vap->iv_unit, vap, event);
    switch(event) {
    case IEEE80211_ASSOC_EVENT_BEACON_MISS:
        sm->last_failure = WLAN_ASSOC_SM_REASON_BEACON_MISS;    /* beacon miss */
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_INIT);
        return true;
        break;
    case IEEE80211_ASSOC_EVENT_DISASSOC:
        sm->last_failure = WLAN_ASSOC_SM_REASON_DISASSOC;
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_INIT);
        return true;
        break;

    case IEEE80211_ASSOC_EVENT_DEAUTH:
        sm->last_failure = WLAN_ASSOC_SM_REASON_DEAUTH;
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_INIT);
        return true;
        break;

    case IEEE80211_ASSOC_EVENT_DISCONNECT_REQUEST:
        /* no need to send disassoc usully caaled while romaing */
        if (sm->scan_entry){
            wlan_util_scan_entry_mlme_set_assoc_state(sm->scan_entry, AP_ASSOC_STATE_NONE);
            wlan_util_scan_entry_update_mlme_info(vap, sm->scan_entry);
        }
        wlan_vap_get_bssid(sm->vap_handle, cur_bssid);
        vap->iv_roam.iv_roam_disassoc = 1;
        wlan_mlme_disassoc_request(sm->vap_handle, cur_bssid, IEEE80211_REASON_ASSOC_LEAVE);
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_INIT);
        return true;
        break;
    case IEEE80211_ASSOC_EVENT_DISASSOC_REQUEST:
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_DISASSOC);
        return true;
        break;

    default:
        return false;
    }
}

/* Callback function for the Completion of Disassoc request frame. */
static void
tx_disassoc_req_completion(wlan_if_t vaphandle, wbuf_t wbuf, void *arg,
                           u_int8_t *dst_addr, u_int8_t *src_addr,
                           u_int8_t *bssid, ieee80211_xmit_status *ts)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) arg;

    if (wlan_vap_delete_in_progress(vaphandle)) {
        qdf_print("%s: vap %pK (id %d) delete in progress", __func__, vaphandle, vaphandle->iv_unit);
        return;
    }

    IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s: Tx disassoc. status=%0xX\n",
                      __func__, ts? ts->ts_flags:0);

    ieee80211_sm_dispatch(sm->hsm_handle, IEEE80211_ASSOC_EVENT_DISASSOC_SENT,0,NULL);
}

/*
 *DISASSOC
 */
static void ieee80211_assoc_state_disassoc_entry(void *ctx)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;
    u_int8_t          cur_bssid[IEEE80211_ADDR_LEN];
    wlan_vap_complete_buf_handler req_completion_handler = NULL;
    void               *req_completion_arg = NULL;
    struct ieee80211com *ic = sm->vap_handle->iv_ic;

    wlan_vap_get_bssid(sm->vap_handle, cur_bssid);

    if(ic && ic->ic_is_mode_offload(ic)) {
        req_completion_handler = tx_disassoc_req_completion;
        req_completion_arg = sm;
    }

    if (wlan_mlme_mark_delayed_node_cleanup(sm->vap_handle, cur_bssid) == 0) {
        if (wlan_mlme_disassoc_request_with_callback(sm->vap_handle, cur_bssid,
                                                     IEEE80211_REASON_ASSOC_LEAVE,
                                                     req_completion_handler, req_completion_arg) !=0 ) {
            IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s: ignore send disassoc failure \n",__func__);
        }
    }

    if (sm->scan_entry) wlan_util_scan_entry_mlme_set_assoc_state(sm->scan_entry, AP_ASSOC_STATE_NONE);
    sm->timeout_event = IEEE80211_ASSOC_EVENT_TIMEOUT;
    if (sm->sync_stop_requested) {
        /* wait for some time for disassoc frame to go out */
        OS_SLEEP(DISASSOC_WAIT_TIME*1000);
        ieee80211_sm_dispatch(sm->hsm_handle, sm->timeout_event,0,NULL);
    } else {
        OS_SET_TIMER(&sm->sm_timer,sm->max_mgmt_time);
    }
}

static void ieee80211_assoc_state_disassoc_exit(void *ctx)
{
    u_int8_t  bssid[IEEE80211_ADDR_LEN];
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;

    /* delete any unicast key installed */
    wlan_vap_get_bssid(sm->vap_handle,bssid);
    wlan_del_key(sm->vap_handle,IEEE80211_KEYIX_NONE,bssid);
    OS_CANCEL_TIMER(&sm->sm_timer);
}

static bool ieee80211_assoc_state_disassoc_event(void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;
    switch(event) {
    case IEEE80211_ASSOC_EVENT_DISASSOC_SENT:
    case IEEE80211_ASSOC_EVENT_TIMEOUT:
        ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_INIT);
        return true;
        break;

    default:
        return false;
    }
}

/*
 * MLME_WAIT
 */
static void ieee80211_assoc_state_mlme_wait_entry(void *ctx)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;
    sm->timeout_event = IEEE80211_ASSOC_EVENT_TIMEOUT;
    OS_SET_TIMER(&sm->sm_timer,MLME_OP_CHECK_TIME);

}

static bool ieee80211_assoc_state_mlme_wait_event(void *ctx, u_int16_t event, u_int16_t event_data_len, void *event_data)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) ctx;
    switch(event) {
    case IEEE80211_ASSOC_EVENT_TIMEOUT:
        if (wlan_mlme_operation_in_progress(sm->vap_handle)) {
#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
            /*
             * When STA is in CAC period the print below keeps coming.
             * Until wpa_supplicant is STA-CAC aware we need to avoid
             * the print.
             */
#else
            printk("%s: waiting for mlme cancel to complete \n",__func__);
#endif
            OS_SET_TIMER(&sm->sm_timer,MLME_OP_CHECK_TIME);
        } else {
            ieee80211_sm_transition_to(sm->hsm_handle,IEEE80211_ASSOC_STATE_INIT);
        }
        return true;
        break;
    default:
        return false;

    }

}

ieee80211_state_info ieee80211_assoc_sm_info[] = {
    {
        (u_int8_t) IEEE80211_ASSOC_STATE_INIT,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "INIT",
        ieee80211_assoc_state_init_entry,
        ieee80211_assoc_state_init_exit,
        ieee80211_assoc_state_init_event
    },
    {
        (u_int8_t) IEEE80211_ASSOC_STATE_TXCHANSWITCH,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "TXCHANSWITCH",
        ieee80211_assoc_state_txchanswitch_entry,
        ieee80211_assoc_state_txchanswitch_exit,
        ieee80211_assoc_state_txchanswitch_event
    },
#if ATH_SUPPORT_DFS
    {
        (u_int8_t) IEEE80211_ASSOC_STATE_REPEATER_CAC,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "REPEATER_CAC",
        ieee80211_assoc_state_repeater_cac_entry,
        ieee80211_assoc_state_repeater_cac_exit,
        ieee80211_assoc_state_repeater_cac_event
    },
#endif
    {
        (u_int8_t) IEEE80211_ASSOC_STATE_JOIN,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "JOIN",
        ieee80211_assoc_state_join_init_entry,
        ieee80211_assoc_state_join_init_exit,
        ieee80211_assoc_state_join_init_event
    },
   {
        (u_int8_t) IEEE80211_ASSOC_STATE_JOIN_SETTING_COUNTRY,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "JOIN SETTING_COUNTRY",
        ieee80211_assoc_state_join_setting_country_entry,
        ieee80211_assoc_state_join_setting_country_exit,
        ieee80211_assoc_state_join_setting_country_event
    },
   {
        (u_int8_t) IEEE80211_ASSOC_STATE_JOIN_COMPLETED,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "JOIN COMPLETED",
        ieee80211_assoc_state_join_entry,
        ieee80211_assoc_state_join_exit,
        ieee80211_assoc_state_join_event
    },
    {
        (u_int8_t) IEEE80211_ASSOC_STATE_AUTH,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "AUTH",
        ieee80211_assoc_state_auth_entry,
        ieee80211_assoc_state_auth_exit,
        ieee80211_assoc_state_auth_event
    },
    {
        (u_int8_t) IEEE80211_ASSOC_STATE_ASSOC,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "ASSOC",
        ieee80211_assoc_state_assoc_entry,
        ieee80211_assoc_state_assoc_exit,
        ieee80211_assoc_state_assoc_event
    },
    {
        (u_int8_t) IEEE80211_ASSOC_STATE_RUN,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "RUN",
        ieee80211_assoc_state_run_entry,
        ieee80211_assoc_state_run_exit,
        ieee80211_assoc_state_run_event
    },
    {
        (u_int8_t) IEEE80211_ASSOC_STATE_DISASSOC,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "DISASSOC",
        ieee80211_assoc_state_disassoc_entry,
        ieee80211_assoc_state_disassoc_exit,
        ieee80211_assoc_state_disassoc_event
    },
    {
        (u_int8_t) IEEE80211_ASSOC_STATE_MLME_WAIT,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "MLME_WAIT",
        ieee80211_assoc_state_mlme_wait_entry,
        NULL,
        ieee80211_assoc_state_mlme_wait_event,
    },
};

static const char *assoc_event_names[] = {      "CONNECT_REQUEST",
                                                "DISCONNECT_REQUEST",
                                                "DISASSOC_REQUEST",
                                                "REASSOC_REQUEST",
                                                "JOIN_INIT_COMPLETED",
                                                "JOIN_SUCCESS",
                                                "JOIN_SUCCESS_SET_COUNTRY",
                                                "JOIN_FAIL",
                                                "AUTH_SUCCESS",
                                                "AUTH_FAIL",
                                                "ASSOC_FAIL",
                                                "ASSOC_SUCCESS",
                                                "BEACON_WAIT_TIMEOUT",
                                                "BEACON_MISS",
                                                "DISASSOC",
                                                "DEAUTH",
                                                "DISASSOC_SENT",
                                                "TIMEOUT",
                                                "RECONNECT_REQUEST",
                                                "TXCHANSWITCH_SUCCESS",
                                                "TXCHANSWITCH_FAIL",
                                                "REPEATER_CAC_SUCCESS",
                                                "REPEATER_CAC_FAIL",
                                              };
static OS_TIMER_FUNC(assoc_sm_timer_handler)
{
    wlan_assoc_sm_t sm;

    OS_GET_TIMER_ARG(sm, wlan_assoc_sm_t);
    IEEE80211_DPRINTF(sm->vap_handle,IEEE80211_MSG_STATE,"%s: timed out cur state %s \n",
                      __func__, ieee80211_assoc_sm_info[ieee80211_sm_get_curstate(sm->hsm_handle)].name);
    ieee80211_sm_dispatch(sm->hsm_handle, sm->timeout_event,0,NULL);

}

/*
 * mlme event handlers.
 */
static void sm_join_complete_set_country(os_handle_t osif, IEEE80211_STATUS status)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) osif;
    sm->last_reason=status;
    if (status == IEEE80211_STATUS_SUCCESS) {
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_ASSOC_EVENT_JOIN_SUCCESS_SET_COUNTRY,0,NULL);
    } else {
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_ASSOC_EVENT_JOIN_FAIL,0,NULL);
    }
}

static void sm_join_complete(os_handle_t osif, IEEE80211_STATUS status)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) osif;
    sm->last_reason=status;
    if (status == IEEE80211_STATUS_SUCCESS) {
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_ASSOC_EVENT_JOIN_SUCCESS,0,NULL);
    } else {
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_ASSOC_EVENT_JOIN_FAIL,0,NULL);
    }
}

static void sm_auth_complete(os_handle_t osif, u_int8_t *macaddr, IEEE80211_STATUS status)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) osif;
    sm->last_reason=status;
    if (status == IEEE80211_STATUS_SUCCESS) {
        if (sm->scan_entry) wlan_util_scan_entry_mlme_set_assoc_state(sm->scan_entry, AP_ASSOC_STATE_AUTH);
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_ASSOC_EVENT_AUTH_SUCCESS,0,NULL);
    } else {
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_ASSOC_EVENT_AUTH_FAIL,0,NULL);
    }
}

static void sm_assoc_complete(os_handle_t osif, IEEE80211_STATUS status,u_int16_t aid, wbuf_t wbuf)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) osif;
    sm->last_reason=status;
    if (status == IEEE80211_STATUS_SUCCESS) {
        if (sm->scan_entry) wlan_util_scan_entry_mlme_set_assoc_state(sm->scan_entry, AP_ASSOC_STATE_ASSOC);
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_ASSOC_EVENT_ASSOC_SUCCESS,0,NULL);
    } else {
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_ASSOC_EVENT_ASSOC_FAIL,0,NULL);
    }

}

static void sm_reassoc_complete(os_handle_t osif, IEEE80211_STATUS status,u_int16_t aid, wbuf_t wbufs)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) osif;
    sm->last_reason=status;
    if (status == IEEE80211_STATUS_SUCCESS) {
        if (sm->scan_entry) wlan_util_scan_entry_mlme_set_assoc_state(sm->scan_entry, AP_ASSOC_STATE_ASSOC);
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_ASSOC_EVENT_ASSOC_SUCCESS,0,NULL);
    } else {
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_ASSOC_EVENT_ASSOC_FAIL,0,NULL);
    }

}

static void sm_chanswitch_complete(os_handle_t osif,IEEE80211_STATUS status)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) osif;

    sm->last_reason=status;
    if (status == IEEE80211_STATUS_SUCCESS) {
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_ASSOC_EVENT_TXCHANSWITCH_SUCCESS,0,NULL);
    } else {
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_ASSOC_EVENT_TXCHANSWITCH_FAIL,0,NULL);
    }
    wlan_mlme_cancel_txchanswitch_timer(sm->vap_handle);

}

static void sm_repeater_cac_complete(os_handle_t osif,IEEE80211_STATUS status)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) osif;
    sm->last_reason=status;

    if (status == IEEE80211_STATUS_SUCCESS) {
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_ASSOC_EVENT_REPEATER_CAC_SUCCESS,0,NULL);
    } else {
        ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_ASSOC_EVENT_REPEATER_CAC_FAIL,0,NULL);
    }
    wlan_mlme_cancel_repeater_cac_timer(sm->vap_handle);

}

static void sm_deauth_indication(os_handle_t osif,u_int8_t *macaddr, u_int16_t associd, u_int16_t reason)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) osif;
    sm->last_reason = reason;
    if (sm->scan_entry) wlan_util_scan_entry_mlme_set_assoc_state(sm->scan_entry, AP_ASSOC_STATE_NONE); // we need to auth
    ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_ASSOC_EVENT_DEAUTH,0,NULL);
}

static void sm_disassoc_indication(os_handle_t osif,u_int8_t *macaddr, u_int16_t associd, u_int32_t reason)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) osif;
    sm->last_reason = reason;
    /*
     * In theory we should still be auth'ed
     * But many APs send disassoc when they mean deauth
     * so just start again to save an extra deauth
     */
    if (sm->scan_entry) wlan_util_scan_entry_mlme_set_assoc_state(sm->scan_entry, AP_ASSOC_STATE_NONE);
    ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_ASSOC_EVENT_DISASSOC,0,NULL);
}

wlan_mlme_event_handler_table sta_mlme_evt_handler = {
    sm_join_complete_set_country,   /* mlme_join_complete_set_country */
    sm_join_complete,
    NULL,
    sm_auth_complete,
    NULL,
    sm_assoc_complete,
    sm_reassoc_complete,
    NULL,
    NULL,
    sm_chanswitch_complete,     /* mlme_txchanswitch_complete */
    sm_repeater_cac_complete,   /* mlme_repeater_cac_complete */
    NULL,
    sm_deauth_indication,
    NULL,
    NULL,
    sm_disassoc_indication,     /* mlme_disassoc_indication */
};

/*
 * misc event handlers
 */
static void sm_beacon_miss(os_handle_t osif)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) osif;

    if (sm->scan_entry) wlan_util_scan_entry_mlme_set_assoc_state(sm->scan_entry, AP_ASSOC_STATE_NONE);
    ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_ASSOC_EVENT_BEACON_MISS,0,NULL);
}

static void sm_clonemac(os_handle_t osif)
{
    wlan_assoc_sm_t sm = (wlan_assoc_sm_t) osif;
    ieee80211_sm_dispatch(sm->hsm_handle,IEEE80211_ASSOC_EVENT_RECONNECT_REQUEST,0,NULL);
}


static wlan_misc_event_handler_table sta_misc_evt_handler = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    sm_beacon_miss,
    NULL,                               /* wlan_beacon_rssi_indication */
    NULL,
    sm_clonemac,
    NULL,                               /* wlan_sta_scan_entry_update */
    NULL,                               /* wlan_ap_stopped */
#if ATH_SUPPORT_WAPI
    NULL,                               /* wlan_sta_rekey_indication */
#endif
#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
    NULL,                 /* wlan_sta_cac_started */
#endif
};

wlan_assoc_sm_t wlan_assoc_sm_create(osdev_t oshandle, wlan_if_t vaphandle)
{
    wlan_assoc_sm_t sm;
    sm = (wlan_assoc_sm_t) OS_MALLOC(oshandle,sizeof(struct _wlan_assoc_sm),0);
    if (!sm) {
        return NULL;
    }
    OS_MEMZERO(sm, sizeof(struct _wlan_assoc_sm));
    sm->os_handle = oshandle;
    sm->vap_handle = vaphandle;
    sm->hsm_handle = ieee80211_sm_create(oshandle,
                                         "assoc",
                                         (void *) sm,
                                         IEEE80211_ASSOC_STATE_INIT,
                                         ieee80211_assoc_sm_info,
                                         sizeof(ieee80211_assoc_sm_info)/sizeof(ieee80211_state_info),
                                         MAX_QUEUED_EVENTS,
                                         0 /* no event data */,
                                         MESGQ_PRIORITY_HIGH,
                                         IEEE80211_HSM_ASYNCHRONOUS, /* run the SM asynchronously */
                                         ieee80211_assoc_sm_debug_print,
                                         assoc_event_names,
                                         IEEE80211_N(assoc_event_names)
                                         );
    if (!sm->hsm_handle) {
        OS_FREE(sm);
        IEEE80211_DPRINTF(vaphandle, IEEE80211_MSG_STATE,
            "%s : ieee80211_sm_create failed\n", __func__);
        return NULL;
    }

    sm->max_assoc_attempts = MAX_ASSOC_ATTEMPTS;
    sm->max_auth_attempts =  MAX_AUTH_ATTEMPTS;
    sm->max_tsfsync_time =  MAX_TSFSYNC_TIME;
    sm->max_mgmt_time =  MAX_MGMT_TIME;
    sm->ftie_update_wait_attempts = 0;
    OS_INIT_TIMER(oshandle, &(sm->sm_timer), assoc_sm_timer_handler, (void *)sm, QDF_TIMER_TYPE_WAKE_APPS);

    wlan_vap_register_mlme_event_handlers(vaphandle,(os_if_t) sm, &sta_mlme_evt_handler);
    wlan_vap_register_misc_event_handlers(vaphandle,(os_if_t)sm,&sta_misc_evt_handler);

    return sm;
}



void  wlan_assoc_sm_delete(wlan_assoc_sm_t smhandle)
{
    if (smhandle->is_running) {
        IEEE80211_DPRINTF(smhandle->vap_handle,IEEE80211_MSG_STATE,"%s : can not delete while still runing \n", __func__);
    }
    OS_CANCEL_TIMER(&(smhandle->sm_timer));
    OS_FREE_TIMER(&(smhandle->sm_timer));
    if (wlan_vap_unregister_misc_event_handlers(smhandle->vap_handle,(os_if_t)smhandle,&sta_misc_evt_handler)) {
        IEEE80211_DPRINTF(smhandle->vap_handle,IEEE80211_MSG_STATE,"%s : unregister nusc evt handler failed \n", __func__);
    }
    if (wlan_vap_unregister_mlme_event_handlers(smhandle->vap_handle,(os_if_t)smhandle,&sta_mlme_evt_handler)) {
        IEEE80211_DPRINTF(smhandle->vap_handle,IEEE80211_MSG_STATE,"%s : unregister mlme evt handler failed \n", __func__);
    }
    ieee80211_sm_delete(smhandle->hsm_handle);
    OS_FREE(smhandle);
}

void wlan_assoc_sm_register_event_handlers(wlan_assoc_sm_t smhandle, os_if_t osif,
                                            wlan_assoc_sm_event_handler sm_evhandler)
{
    smhandle->osif = osif;
    smhandle->sm_evhandler = sm_evhandler;

}

/*
 * start the state machine and handling the events.
 */
int wlan_assoc_sm_start(wlan_assoc_sm_t smhandle, wlan_scan_entry_t scan_entry, u_int8_t *curbssid)
{
    u_int8_t zero_mac[IEEE80211_ADDR_LEN] = { 0, 0, 0, 0, 0, 0 };

    if (scan_entry == NULL) {
        return -EINVAL;
    }
    if ( smhandle->is_running ) {
        IEEE80211_DPRINTF(smhandle->vap_handle,IEEE80211_MSG_STATE,"%s: association SM is already running!!  \n", __func__);
        return -EINPROGRESS;
    }

    if (smhandle->scan_entry) {
        qdf_err("previous candidate entry 0x%p bss: %pM not freed",
                smhandle->scan_entry, util_scan_entry_bssid(smhandle->scan_entry));
        qdf_err("New candidate entry 0x%p bss: %pM not freed",
                scan_entry, util_scan_entry_bssid(scan_entry));
        util_scan_free_cache_entry(smhandle->scan_entry);
        smhandle->scan_entry = NULL;
    }

    smhandle->scan_entry = util_scan_copy_cache_entry(scan_entry);
    if (!smhandle->scan_entry) {
        return -ENOMEM;
    }

    /* mark it as running */
    smhandle->is_running = 1;
    if (curbssid) {
           IEEE80211_ADDR_COPY(smhandle->prev_bssid,curbssid);
    } else {
        IEEE80211_ADDR_COPY(smhandle->prev_bssid, zero_mac);
    }

    /* Reset HSM to INIT state. */
    ieee80211_sm_reset(smhandle->hsm_handle, IEEE80211_ASSOC_STATE_INIT, NULL);

    if (curbssid || wlan_util_scan_entry_mlme_assoc_state(smhandle->scan_entry) == AP_ASSOC_STATE_ASSOC) {
        ieee80211_sm_dispatch(smhandle->hsm_handle,
                              IEEE80211_ASSOC_EVENT_REASSOC_REQUEST,0,NULL);
    } else {
        ieee80211_sm_dispatch(smhandle->hsm_handle,
                              IEEE80211_ASSOC_EVENT_CONNECT_REQUEST,0,NULL);
    }
    return EOK;
}

/*
 * stop handling the events.
 */
int wlan_assoc_sm_stop(wlan_assoc_sm_t smhandle, u_int32_t flags)
{
    /*
     * return an error if it is already stopped (or)
     * there is a stop request is pending.
     */
    if (!smhandle->is_running ) {
        IEEE80211_DPRINTF(smhandle->vap_handle,IEEE80211_MSG_STATE,"%s: association SM is already stopped  !!  \n",__func__);
        return -EALREADY;
    }
    if (smhandle->is_stop_requested) {
        IEEE80211_DPRINTF(smhandle->vap_handle,IEEE80211_MSG_STATE,"%s: association SM is already being stopped !!  \n",__func__);
        return -EALREADY;
    }
    smhandle->is_stop_requested = 1;
    if (flags & IEEE80211_ASSOC_SM_STOP_SYNC) {
        smhandle->sync_stop_requested=1;
    }
    if (flags & IEEE80211_ASSOC_SM_STOP_DISASSOC) {
        if (smhandle->sync_stop_requested) {
            ieee80211_sm_dispatch_sync(smhandle->hsm_handle,
                                       IEEE80211_ASSOC_EVENT_DISASSOC_REQUEST,0,NULL,true);
        } else {
            ieee80211_sm_dispatch(smhandle->hsm_handle,
                                  IEEE80211_ASSOC_EVENT_DISASSOC_REQUEST,0,NULL);
        }

    } else {
        if (smhandle->sync_stop_requested) {
            ieee80211_sm_dispatch_sync(smhandle->hsm_handle,
                                       IEEE80211_ASSOC_EVENT_DISCONNECT_REQUEST,0,NULL,true);
        } else {
            ieee80211_sm_dispatch(smhandle->hsm_handle,
                                  IEEE80211_ASSOC_EVENT_DISCONNECT_REQUEST,0,NULL);
        }
    }
    return EOK;
}

/*
 * set a assoc ame param.
 */
int wlan_assoc_sm_set_param(wlan_assoc_sm_t smhandle, wlan_assoc_sm_param param,u_int32_t val)
{

    switch(param) {
    case PARAM_MAX_ASSOC_ATTEMPTS:
        smhandle->max_assoc_attempts = (u_int8_t)val;
        if (val == 0) return -EINVAL;
        break;
    case PARAM_MAX_AUTH_ATTEMPTS:
        smhandle->max_auth_attempts = (u_int8_t)val;
        if (val == 0) return -EINVAL;
        break;
    case PARAM_MAX_TSFSYNC_TIME:
        smhandle->max_tsfsync_time = (u_int16_t)val;
        if (val == 0) return -EINVAL;
        break;
    case PARAM_MAX_MGMT_TIME:
        smhandle->max_mgmt_time = (u_int16_t)val;
        if (val == 0) return -EINVAL;
        break;
    case PARAM_CURRENT_STATE:
    return -EINVAL;
        break;
    }

    return EOK;
}

/*
 * get an assoc ame param.
 */
u_int32_t wlan_assoc_sm_get_param(wlan_assoc_sm_t smhandle, wlan_assoc_sm_param param)
{
    u_int32_t val=0;

    switch(param) {
    case PARAM_MAX_ASSOC_ATTEMPTS:
        val = smhandle->max_assoc_attempts;
        break;
    case PARAM_MAX_AUTH_ATTEMPTS:
        val = smhandle->max_auth_attempts;
        break;
    case PARAM_MAX_TSFSYNC_TIME:
        val = smhandle->max_tsfsync_time;
        break;
    case PARAM_MAX_MGMT_TIME:
        val = smhandle->max_mgmt_time;
        break;
    case PARAM_CURRENT_STATE:
        switch((ieee80211_connection_state)ieee80211_sm_get_curstate(smhandle->hsm_handle)) {
        case IEEE80211_ASSOC_STATE_INIT:
            val=WLAN_ASSOC_STATE_INIT;
            break;
        case IEEE80211_ASSOC_STATE_JOIN:
        case IEEE80211_ASSOC_STATE_JOIN_SETTING_COUNTRY:
        case IEEE80211_ASSOC_STATE_JOIN_COMPLETED:
        case IEEE80211_ASSOC_STATE_AUTH:
        case IEEE80211_ASSOC_STATE_MLME_WAIT:
            val=WLAN_ASSOC_STATE_AUTH;
            break;
        case IEEE80211_ASSOC_STATE_ASSOC:
            val=WLAN_ASSOC_STATE_ASSOC;
            break;
        case IEEE80211_ASSOC_STATE_RUN:
            val=WLAN_ASSOC_STATE_RUN;
            break;
        case IEEE80211_ASSOC_STATE_DISASSOC:
            val=WLAN_ASSOC_STATE_RUN;
            break;
        case IEEE80211_ASSOC_STATE_TXCHANSWITCH:
            val=WLAN_ASSOC_STATE_TXCHANSWITCH;
            break;
        case IEEE80211_ASSOC_STATE_REPEATER_CAC:
            val=WLAN_ASSOC_STATE_REPEATER_CAC;
            break;
        }
        break;
    }

    return val;
}

int wlan_assoc_bss_node_freed_handler(wlan_assoc_sm_t sm)
{
    ieee80211_connection_state state;
    struct ieee80211vap *vap = sm->vap_handle;

    state = ieee80211_sm_get_curstate(sm->hsm_handle);
    if (state == IEEE80211_ASSOC_STATE_JOIN) {
        ieee80211_sm_dispatch(sm->hsm_handle,
                IEEE80211_ASSOC_EVENT_JOIN_COMPLETE, 0, NULL);
    } else {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_STATE,
                "%s:SKIP dispatching JOIN_COMPLETE: vap:%d(0x%pK), state:%d\n",
                __func__, vap->iv_unit, vap, state);
    }

    return 0;
}
/*
 * go back to initial state to start new associaiton.
 */
int wlan_assoc_sm_restart(wlan_assoc_sm_t smhandle, u_int32_t flags)
{
    /*
     * return an error if it is already stopped (or)
     * there is a stop request is pending.
     */
    if (!smhandle->is_running ) {
        IEEE80211_DPRINTF(smhandle->vap_handle,IEEE80211_MSG_STATE,"%s: association SM is already stopped  !!  \n",__func__);
        return -EALREADY;
    }
    if (smhandle->is_stop_requested) {
        IEEE80211_DPRINTF(smhandle->vap_handle,IEEE80211_MSG_STATE,"%s: association SM is already being stopped !!  \n",__func__);
        return -EALREADY;
    }
    smhandle->is_stop_requested = 0;
    if (flags & IEEE80211_ASSOC_SM_STOP_SYNC) {
        smhandle->sync_stop_requested=1;
        ieee80211_sm_dispatch_sync(smhandle->hsm_handle,
                                   IEEE80211_ASSOC_EVENT_DISCONNECT_REQUEST,0,NULL,true);
    } else {
        ieee80211_sm_dispatch(smhandle->hsm_handle,
                              IEEE80211_ASSOC_EVENT_DISCONNECT_REQUEST,0,NULL);
    }
    return EOK;
}

void wlan_assoc_set_scan_buf(wlan_assoc_sm_t smhandle, wlan_scan_entry_t scan_result)
{
    smhandle->scan_entry = scan_result;
}

#if UMAC_SUPPORT_WPA3_STA
void wlan_assoc_sm_sae_max_auth_retry(wlan_assoc_sm_t smhandle, bool is_sae)
{
    if (is_sae) {
        if (smhandle->vap_handle->iv_sae_max_auth_attempts)
            smhandle->max_auth_attempts =  smhandle->vap_handle->iv_sae_max_auth_attempts;
        else
            smhandle->max_auth_attempts =  MAX_SAE_AUTH_ATTEMPTS;
    } else
        smhandle->max_auth_attempts =  MAX_AUTH_ATTEMPTS;
}
#endif

void wlan_assoc_sm_msgq_drain(wlan_assoc_sm_t smhandle)
{
    if (!smhandle)
        return;

    ieee80211_sm_msgq_drain(smhandle->hsm_handle, NULL);
}

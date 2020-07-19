/*
 * Copyright (c) 2011-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.
 *
 *
 */

#include <ieee80211_var.h>
#include <ieee80211_channel.h>
#include <ieee80211_mlme.h>
#include <wlan_mlme_vdev_mgmt_ops.h>
#include <ieee80211_target.h>
#include <ieee80211_rateset.h>
#include <ieee80211_wds.h>
#if UNIFIED_SMARTANTENNA
#include <wlan_sa_api_utils_api.h>
#include <wlan_sa_api_utils_defs.h>
#endif
#if QCA_LTEU_SUPPORT
#include <ieee80211_nl.h>
#endif
#include <qdf_lock.h>
#if ATH_PERF_PWR_OFFLOAD
#include <ol_if_athvar.h>
#endif
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include <osif_nss_wifiol_vdev_if.h>
#endif
#if QCA_AIRTIME_FAIRNESS
#include <wlan_atf_utils_api.h>
#endif
#include <wlan_lmac_if_api.h>
#include "cfg_ucfg_api.h"
#if ACFG_NETLINK_TX
void acfg_clean(struct ieee80211com *ic);
#endif
#include <wlan_son_pub.h>
#include <ieee80211_mlme_dfs_dispatcher.h>
#if WLAN_SUPPORT_GREEN_AP
#include <wlan_green_ap_api.h>
#endif
#include <wlan_mlme_dp_dispatcher.h>

#if DBDC_REPEATER_SUPPORT
#include "target_type.h"
#endif

#include "target_if.h"

#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_ucfg_api.h>
#endif

#include <wlan_utility.h>
#include <wlan_mlme_if.h>
#include <wlan_vdev_mlme.h>
#include <wlan_vdev_mgr_utils_api.h>

/* flags used during vap delete, whether to stop BSS or not */
#define NO_STOP 0
#define DO_STOP 1

static void ieee80211_roam_initparams(wlan_if_t vap);

#if DYNAMIC_BEACON_SUPPORT
static OS_TIMER_FUNC(ieee80211_dbeacon_suspend_beacon)
{
    struct ieee80211vap  *vap = NULL;
    OS_GET_TIMER_ARG(vap, struct ieee80211vap *);

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG, "Enter timer:%s, Kickout all sta under this VAP \n", __FUNCTION__);
    wlan_deauth_all_stas(vap);

    qdf_spin_lock_bh(&vap->iv_dbeacon_lock);
    if (!ieee80211_mlme_beacon_suspend_state(vap)) {
        ieee80211_mlme_set_beacon_suspend_state(vap, true);
    }
    qdf_spin_unlock_bh(&vap->iv_dbeacon_lock);

}
#endif

#if QCN_IE
/*
 * Callback function that will be called at the expiration of bpr_timer.
 */
static enum qdf_hrtimer_restart_status ieee80211_send_bcast_resp_handler(qdf_hrtimer_data_t *arg)
{
    void *ptr = (void *) arg;
    qdf_hrtimer_data_t *thr = container_of(ptr, qdf_hrtimer_data_t, u);
    struct ieee80211vap *vap = container_of(thr, struct ieee80211vap, bpr_timer);
    struct ieee80211_framing_extractx extractx;

    OS_MEMZERO(&extractx, sizeof(extractx));

    if ((!vap) || (ieee80211_vap_deleted_is_set(vap))) {
       return QDF_HRTIMER_NORESTART;
    }

    if (wlan_vdev_is_up(vap->vdev_obj) != QDF_STATUS_SUCCESS) {
       IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,
                         "%s |%d vdev is not in UP state", __func__, __LINE__);
       return QDF_HRTIMER_NORESTART;
    }
    /* Set probe-response rate, if configured */
    if (vap->iv_prb_rate) {
        extractx.datarate = vap->iv_prb_rate;
    }

    /* Send the broadcast probe response and reset the state to default */
    ieee80211_send_proberesp(vap->iv_bss, IEEE80211_GET_BCAST_ADDR(vap->iv_ic), NULL, 0, &extractx);
    vap->iv_bpr_callback_count++;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,
        "Callback running : %s | %d | Delay: %d | Current time: %lld | Next beacon tstamp: %lld | "
        "beacon interval: %d ms \n",
        __func__, __LINE__, vap->iv_bpr_delay, qdf_ktime_to_ns(qdf_ktime_get()),
        qdf_ktime_to_ns(vap->iv_next_beacon_tstamp), vap->iv_ic->ic_intval);

    return QDF_HRTIMER_NORESTART;
}
#endif

int
ieee80211_vap_setup(struct ieee80211com *ic, struct ieee80211vap *vap,
                    struct vdev_mlme_obj *vdev_mlme,
                    int opmode, int scan_priority_base, u_int32_t flags,
                    const u_int8_t bssid[IEEE80211_ADDR_LEN])
{
#define DEFAULT_WATERMARK_THRESHOLD 50
#define IEEE80211_C_OPMODE                                              \
    (IEEE80211_C_IBSS | IEEE80211_C_HOSTAP | IEEE80211_C_AHDEMO |       \
     IEEE80211_C_MONITOR)
    uint32_t i;
#if UMAC_SUPPORT_APONLY
    struct ieee80211_vap_opmode_count vap_opmode_count;
#endif
#if ATH_SUPPORT_WRAP
    struct ieee80211vap *mpsta_vap = (struct ieee80211vap *) ic->ic_mpsta_vap;
#endif
#if 1 /* DISABLE_FOR_PLUGFEST_GSTANTON */
    vap->iv_debug |= (IEEE80211_MSG_POWER | IEEE80211_MSG_STATE);
#endif

    vap->iv_ic = ic;
    vap->iv_create_flags=flags;
    vap->iv_flags = 0;       /* reset flags */
    ieee80211vap_set_flag(vap, ic->ic_flags); /* propagate common flags */
    vap->iv_flags_ext = 0; /* reset flags */
    ieee80211vap_set_flag_ext(vap, ic->ic_flags_ext); /* propagate common flags*/
    /* Enable AMSDU in AMPDU Support by default */
    ieee80211vap_set_flag_ext(vap, IEEE80211_FEXT_AMSDU);
    vap->iv_ath_cap = ic->ic_ath_cap;
    vap->iv_htflags = ic->ic_htflags;
    /* Default Multicast traffic to lowest rate of 1000 Kbps*/
    vap->iv_mcast_fixedrate = 0;
    vap->iv_caps = 0;
    ieee80211vap_set_cap(vap, ic->ic_caps &~ IEEE80211_C_OPMODE);
    vap->iv_ath_cap &= ~IEEE80211_ATHC_WME;
    vap->iv_node_count = 0;
    vap->iv_ccmpsw_seldec = 1;
    vap->iv_vap_is_down = 1;
#if defined(WLAN_DFS_FULL_OFFLOAD) && defined(QCA_DFS_NOL_OFFLOAD)
    vap->vap_start_failure = 0;
    vap->vap_start_failure_action_taken = 0;
#endif
#if ATH_SSID_STEERING
    vap->iv_vap_ssid_config = 0xff; /* In ssid steering Default is 0xff private is 1 public is 0 */
#endif
#if UMAC_SUPPORT_ACFG
    if (opmode == IEEE80211_M_HOSTAP) {
        vap->iv_diag_warn_threshold = 65; /* default 65Mbps */
        vap->iv_diag_err_threshold = 26; /* default 26Mbps */
    }
#endif
    atomic_set(&vap->iv_rx_gate,0);

    for (i = 0; i < IEEE80211_APPIE_MAX_FRAMES; i++) {
        STAILQ_INIT(&vap->iv_app_ie_list[i]);
    }

    /* Initialize MUEDCA params */
    ieee80211_muedca_initparams(vap);
    /* The flag should be initialized to 1 to ensure STA updates the target
     * with muedca_params the first time it receives MUEDCA IE in beacons from
     * AP. */
    if(opmode == IEEE80211_M_STA && vap->iv_he_ul_muofdma) {
        vap->iv_update_muedca_params = 1;
    }

    /*
     * By default, supports sending our Country IE and 802.11h
     * informations (but also depends on the lower ic flags).
     */
    ieee80211_vap_country_ie_set(vap);
    ieee80211_vap_doth_set(vap);
    ieee80211_vap_off_channel_support_set(vap);
    switch (opmode) {
    case IEEE80211_M_STA:
        /* turn on sw bmiss timer by default */
        ieee80211_vap_sw_bmiss_set(vap);
        break;
    case IEEE80211_M_IBSS:
        ieee80211vap_set_cap(vap, IEEE80211_C_IBSS);
        vap->iv_ath_cap &= ~IEEE80211_ATHC_XR;
        break;
    case IEEE80211_M_AHDEMO:
        ieee80211vap_set_cap(vap, IEEE80211_C_AHDEMO);
        vap->iv_ath_cap &= ~IEEE80211_ATHC_XR;
        break;
    case IEEE80211_M_HOSTAP:
        ieee80211vap_set_cap(vap, IEEE80211_C_HOSTAP);
        vap->iv_ath_cap &= ~(IEEE80211_ATHC_XR | IEEE80211_ATHC_TURBOP);
        break;
    case IEEE80211_M_MONITOR:
        ieee80211vap_set_cap(vap, IEEE80211_C_MONITOR);
        vap->iv_ath_cap &= ~(IEEE80211_ATHC_XR | IEEE80211_ATHC_TURBOP);
        break;
    case IEEE80211_M_WDS:
        ieee80211vap_set_cap(vap, IEEE80211_C_WDS);
        vap->iv_ath_cap &= ~(IEEE80211_ATHC_XR | IEEE80211_ATHC_TURBOP);
        IEEE80211_VAP_WDS_ENABLE(vap);
        break;
    }
    vap->iv_opmode = opmode;
#ifdef ATH_SUPPORT_TxBF
    vap->iv_txbfmode = 1;
#endif
    vap->iv_scan_priority_base = scan_priority_base;

    vap->iv_chanchange_count = 0;
    vap->channel_change_done = 0;
    vap->appie_buf_updated = 0;
    #ifdef ATH_SUPPORT_QUICK_KICKOUT
    vap->iv_sko_th = ATH_TX_MAX_CONSECUTIVE_XRETRIES;
    #endif

    qdf_atomic_set(&(vap->kickout_in_progress), 0);
    qdf_atomic_set(&(vap->stop_action_progress), 0);
    spin_lock_init(&vap->iv_lock);
    spin_lock_init(&vap->init_lock);
    qdf_semaphore_init(&vap->iv_sem_lock);
    if (!vdev_mlme) {
        mlme_err(" VDEV MLME component object is NULL");
        return -EINVAL;
    }
    vap->iv_bsschan = ic->ic_curchan; /* initialize bss chan to cur chan */
    /*
     * Enable various functionality by default if we're capable.
     */

    /* NB: bg scanning only makes sense for station mode right now */
    if ((ic->ic_opmode == IEEE80211_M_STA) &&
        (vap->iv_caps & IEEE80211_C_BGSCAN))
        vap->iv_flags |= IEEE80211_F_BGSCAN;
    /* If lp_iot_mode we want to ensure DTIM is 4x beacon interval use IEEE80211_DTIM_DEFAULT_LP_IOT */
    /* If hostap mode, set DTIM interval as 1 to the FW */
    if (vap->iv_create_flags & IEEE80211_LP_IOT_VAP) {
        if (opmode == IEEE80211_M_HOSTAP)
            wlan_set_param(vap, IEEE80211_DTIM_INTVAL, IEEE80211_DTIM_DEFAULT_LP_IOT);
        else
            vdev_mlme->proto.generic.dtim_period = IEEE80211_DTIM_DEFAULT_LP_IOT;
    } else {
        if (opmode == IEEE80211_M_HOSTAP)
            wlan_set_param(vap, IEEE80211_DTIM_INTVAL, IEEE80211_DTIM_DEFAULT);
        else
            vdev_mlme->proto.generic.dtim_period = IEEE80211_DTIM_DEFAULT;
    }

    vap->iv_bmiss_count_for_reset = IEEE80211_DEFAULT_BMISS_COUNT_RESET;
    vap->iv_bmiss_count_max = IEEE80211_DEFAULT_BMISS_COUNT_MAX;

    vap->iv_des_mode = IEEE80211_MODE_AUTO;
    vap->iv_cur_mode = IEEE80211_MODE_AUTO;
    vap->iv_des_modecaps = (1 << IEEE80211_MODE_AUTO);
    vap->iv_des_chan[IEEE80211_MODE_AUTO]           = IEEE80211_CHAN_ANYC;
    if (opmode != IEEE80211_M_HOSTAP)
    {
       vap->iv_des_chan[IEEE80211_MODE_11A]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11A);

       vap->iv_des_chan[IEEE80211_MODE_11B]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11B);

       vap->iv_des_chan[IEEE80211_MODE_11G]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11G);

       vap->iv_des_chan[IEEE80211_MODE_11NA_HT20]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11NA_HT20);

       vap->iv_des_chan[IEEE80211_MODE_11NG_HT20]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11NG_HT20);

       vap->iv_des_chan[IEEE80211_MODE_11NA_HT40PLUS]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11NA_HT40PLUS);

       vap->iv_des_chan[IEEE80211_MODE_11NA_HT40MINUS]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11NA_HT40MINUS);

       vap->iv_des_chan[IEEE80211_MODE_11NG_HT40PLUS]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11NG_HT40PLUS);

       vap->iv_des_chan[IEEE80211_MODE_11NG_HT40MINUS]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11NG_HT40MINUS);

       vap->iv_des_chan[IEEE80211_MODE_11NG_HT40]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11NG_HT40);

       vap->iv_des_chan[IEEE80211_MODE_11NA_HT40]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11NA_HT40);

       vap->iv_des_chan[IEEE80211_MODE_11AC_VHT20]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11AC_VHT20);

       vap->iv_des_chan[IEEE80211_MODE_11AC_VHT40]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11AC_VHT40);

       vap->iv_des_chan[IEEE80211_MODE_11AC_VHT40PLUS]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11AC_VHT40PLUS);

       vap->iv_des_chan[IEEE80211_MODE_11AC_VHT40MINUS]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11AC_VHT40MINUS);

       vap->iv_des_chan[IEEE80211_MODE_11AC_VHT80]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11AC_VHT80);

       vap->iv_des_chan[IEEE80211_MODE_11AC_VHT160]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11AC_VHT160);

       vap->iv_des_chan[IEEE80211_MODE_11AC_VHT80_80]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11AC_VHT80_80);

       vap->iv_des_chan[IEEE80211_MODE_11AXA_HE20]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11AXA_HE20);

       vap->iv_des_chan[IEEE80211_MODE_11AXG_HE20]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11AXG_HE20);

       vap->iv_des_chan[IEEE80211_MODE_11AXA_HE40PLUS]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11AXA_HE40PLUS);

       vap->iv_des_chan[IEEE80211_MODE_11AXA_HE40MINUS]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11AXA_HE40MINUS);

       vap->iv_des_chan[IEEE80211_MODE_11AXG_HE40PLUS]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11AXG_HE40PLUS);

       vap->iv_des_chan[IEEE80211_MODE_11AXG_HE40MINUS]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11AXG_HE40MINUS);

       vap->iv_des_chan[IEEE80211_MODE_11AXA_HE40]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11AXA_HE40);

       vap->iv_des_chan[IEEE80211_MODE_11AXG_HE40]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11AXG_HE40);

       vap->iv_des_chan[IEEE80211_MODE_11AXA_HE80]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11AXA_HE80);

       vap->iv_des_chan[IEEE80211_MODE_11AXA_HE160]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11AXA_HE160);

       vap->iv_des_chan[IEEE80211_MODE_11AXA_HE80_80]  =
                         ieee80211_find_dot11_channel(ic, 0, 0,
                                                 IEEE80211_MODE_11AXA_HE80_80);
    }
    else {
       vap->iv_des_chan[IEEE80211_MODE_11A]              = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11B]              = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11G]              = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11NA_HT20]        = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11NG_HT20]        = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11NA_HT40PLUS]    = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11NA_HT40MINUS]   = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11NG_HT40PLUS]    = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11NG_HT40MINUS]   = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11NG_HT40]        = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11NA_HT40]        = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11AC_VHT20]       = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11AC_VHT40]       = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11AC_VHT40PLUS]   = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11AC_VHT40MINUS]  = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11AC_VHT80]       = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11AC_VHT160]      = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11AC_VHT80_80]    = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11AXA_HE20]       = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11AXG_HE20]       = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11AXA_HE40PLUS]   = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11AXA_HE40MINUS]  = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11AXG_HE40PLUS]   = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11AXG_HE40MINUS]  = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11AXA_HE40]       = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11AXG_HE40]       = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11AXA_HE80]       = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11AXA_HE160]      = IEEE80211_CHAN_ANYC;
       vap->iv_des_chan[IEEE80211_MODE_11AXA_HE80_80]    = IEEE80211_CHAN_ANYC;
    }
    /* apply registry settings */
    vap->iv_des_ibss_chan     = ic->ic_reg_parm.defaultIbssChannel;
    vap->iv_rateCtrlEnable    = ic->ic_reg_parm.rateCtrlEnable;
    if (opmode == IEEE80211_M_STA)
       vap->iv_rc_txrate_fast_drop_en  = ic->ic_reg_parm.rc_txrate_fast_drop_en;

    vap->iv_fixed_rateset     = ic->ic_reg_parm.transmitRateSet;
    vap->iv_fixed_retryset    = ic->ic_reg_parm.transmitRetrySet;

    if(wlan_pdev_nif_feat_cap_get(ic->ic_pdev_obj, WLAN_PDEV_F_MBSS_IE_ENABLE))
        vap->iv_max_aid = ic->ic_num_clients + (1 << ic->ic_mbss.max_bssid);
    else
        /* default max aid(should be a config param ?) */
        vap->iv_max_aid = ic->ic_num_clients + 1;

    /* keep alive time out */
    vap->iv_keep_alive_timeout    =  IEEE80211_DEFULT_KEEP_ALIVE_TIMEOUT;
    vap->iv_inact_count =  (vap->iv_keep_alive_timeout +
                            IEEE80211_INACT_WAIT -1)/IEEE80211_INACT_WAIT;
    vap->watermark_threshold      = vap->iv_max_aid;
    vap->watermark_threshold_flag = true ;

    if(vap->iv_max_aid == 0) {
        /* Default threshold value is 50 when the
         * maximum number of clients is 0 */
        vap->watermark_threshold = DEFAULT_WATERMARK_THRESHOLD;
    }
#ifdef ATH_SUPPORT_TxBF
    vap->iv_autocvupdate      = ic->ic_reg_parm.autocvupdate;
    vap->iv_cvupdateper    = ic->ic_reg_parm.cvupdateper;
#endif

    /* set WMM-related parameters */
    vap->iv_wmm_enable        = ic->ic_reg_parm.wmeEnabled;
    if (ieee80211com_has_cap(ic, IEEE80211_C_WME)) {
        if (vap->iv_wmm_enable) {
            ieee80211_vap_wme_set(vap);
        } else  {
            ieee80211_vap_wme_clear(vap);
        }
    } else {
        ieee80211_vap_wme_clear(vap);
    }

    vap->iv_wmm_power_save    = 0;
    vap->iv_smps_rssithresh   = ic->ic_reg_parm.smpsRssiThreshold;
    vap->iv_smps_datathresh   = ic->ic_reg_parm.smpsDataThreshold;

    {
        u_int8_t    uapsd_flag;

        uapsd_flag = (ic->ic_reg_parm.uapsd.vo ? WME_CAPINFO_UAPSD_VO : 0) |
                     (ic->ic_reg_parm.uapsd.vi ? WME_CAPINFO_UAPSD_VI : 0) |
                     (ic->ic_reg_parm.uapsd.bk ? WME_CAPINFO_UAPSD_BK : 0) |
                     (ic->ic_reg_parm.uapsd.be ? WME_CAPINFO_UAPSD_BE : 0);

        ieee80211_set_uapsd_flags(vap, uapsd_flag);
    }

    OS_RWLOCK_INIT(&vap->iv_tim_update_lock);

    IEEE80211_ADDR_COPY(vap->iv_myaddr, ic->ic_myaddr);
    IEEE80211_ADDR_COPY(vap->iv_my_hwaddr, ic->ic_my_hwaddr);

    for(i=0;i<IEEE80211_MAX_VAP_EVENT_HANDLERS; ++i) {
        vap->iv_event_handler[i] = NULL;
    }

    vdev_mlme->proto.generic.nss = ieee80211_getstreams(ic, ic->ic_tx_chainmask);
    vap->iv_he_ul_nss = ieee80211_getstreams(ic, ic->ic_rx_chainmask);

#if ATH_SUPPORT_WRAP
    if (vap->iv_psta && !vap->iv_mpsta) {
        /*For PSTA, copy NSS value that is configured for MPSTA*/
        if (mpsta_vap) {
            vdev_mlme->proto.generic.nss = mpsta_vap->vdev_mlme->proto.generic.nss;
        }
    }
#endif
    /* Set the SGI if the hardware supports SGI for
     * any of the modes. Note that the hardware will
     * support this for all modes or none at all
     */
    vap->iv_sgi         = IEEE80211_GI_0DOT4_US;
    vap->iv_data_sgi    = IEEE80211_GI_0DOT4_US;
    vap->iv_he_sgi      = IEEE80211_GI_0DOT8_US;
    vap->iv_he_data_sgi = IEEE80211_GI_0DOT8_US;
    vap->iv_he_ul_sgi   = IEEE80211_GI_0DOT8_US;
    wlan_set_param(vap, IEEE80211_SHORT_GI, vap->iv_sgi);

    /* Set LTF to 0 for the time being */
    vap->iv_he_ltf       = IEEE80211_HE_LTF_DEFAULT;
    vap->iv_he_ul_ltf    = IEEE80211_HE_LTF_DEFAULT;
    /* All 1 values for auto rate HE GI and LTF combinations
     * will allow the FW to choose the best combination from
     * all possible set of combinations
     */
    vap->iv_he_ar_gi_ltf = IEEE80211_HE_AR_DEFAULT_LTF_SGI_COMBINATION;

    /* Enable BSR Support by default */
    vap->iv_he_bsr_supp = IEEE80211_BSR_SUPPORT_ENABLE;

     /* Initialize to 0, it should be set to 1 on vap up */
    vap->iv_needs_up_on_acs = 0;
    vap->ald_pid = WLAN_DEFAULT_NETLINK_PID;
     /* Explicit disabling */
    son_vdev_fext_capablity(vap->vdev_obj,
                            SON_CAP_CLEAR,
                            WLAN_VDEV_FEXT_SON_SPL_RPT);
    /* Default VHT SGI mask is enabled for high stream MCS 7, 8, 9 */
    vap->iv_vht_sgimask = 0x380;

    /* Default VHT80 rate mask support all MCS rates except NSS=3 MCS=6 */
    vap->iv_vht80_ratemask = MAX_VHT80_RATE_MASK;

    /* By default support for VHT MCS10/11 should be enabled for lithium targets*/
    if(ic->ic_get_tgt_type(ic) >= TARGET_TYPE_QCA8074) {
        vap->iv_vht_mcs10_11_supp = ENABLE_VHT_MCS_10_11_SUPP;
    }
    else {
        vap->iv_vht_mcs10_11_supp = !ENABLE_VHT_MCS_10_11_SUPP;
    }

    /* By default support for non Q-Q peer for VHT MCS10/11 should be disabled */
    vap->iv_vht_mcs10_11_nq2q_peer_supp = !ENABLE_VHT_MCS_10_11_SUPP;
#if ATH_SUPPORT_WRAP
    if (ic->ic_htcap) {
        if ((vap->iv_ic->ic_wrap_vap_sgi_cap) && !(vap->iv_mpsta)&&(vap->iv_psta))
           vap->iv_sgi = vap->iv_ic->ic_wrap_vap_sgi_cap;
    }
#endif

    if (ic->ic_vhtcap) {
#if ATH_SUPPORT_WRAP
        if ((vap->iv_ic->ic_wrap_vap_sgi_cap) && !(vap->iv_mpsta)&&(vap->iv_psta))
           vap->iv_sgi = vap->iv_ic->ic_wrap_vap_sgi_cap;
#endif

        if (!(vap->iv_create_flags & IEEE80211_LP_IOT_VAP)) {
            vdev_mlme->proto.vht_info.subfee = ((ic->ic_vhtcap &
                 IEEE80211_VHTCAP_SU_BFORMEE) >> IEEE80211_VHTCAP_SU_BFORMEE_S);
            vdev_mlme->proto.vht_info.subfer = ((ic->ic_vhtcap & IEEE80211_VHTCAP_SU_BFORMER)
                                            >> IEEE80211_VHTCAP_SU_BFORMER_S);

            vdev_mlme->proto.vht_info.mubfer = (opmode == IEEE80211_M_HOSTAP)
                            ? ((ic->ic_vhtcap & IEEE80211_VHTCAP_MU_BFORMER)
                                    >> IEEE80211_VHTCAP_MU_BFORMER_S) : 0;

#if ATH_SUPPORT_WRAP
            /* Set mubfee only for main proxy sta and not for pstas */
            if (vap->iv_mpsta || (!vap->iv_mpsta && !vap->iv_psta))
#endif
                vdev_mlme->proto.vht_info.mubfee = (opmode == IEEE80211_M_STA) ?
                            ((ic->ic_vhtcap & IEEE80211_VHTCAP_MU_BFORMEE) >>
                             IEEE80211_VHTCAP_MU_BFORMEE_S) : 0;

            vdev_mlme->proto.vht_info.bfee_sts_cap = (ic->ic_vhtcap >>
                    IEEE80211_VHTCAP_STS_CAP_S) & IEEE80211_VHTCAP_STS_CAP_M;

            vdev_mlme->proto.vht_info.sounding_dimension = (ic->ic_vhtcap &
                    IEEE80211_VHTCAP_SOUND_DIM) >> IEEE80211_VHTCAP_SOUND_DIM_S;
        }
    }

    /* LDPC, TX_STBC, RX_STBC : With VHT, HT will be set as well. Just use that */
    vdev_mlme->proto.generic.ldpc = ieee80211com_get_ldpccap(ic);
    vap->iv_he_ul_ldpc = ieee80211com_get_ldpccap(ic);

    /* Enabling  LDPC by default */
    wlan_set_param(vap, IEEE80211_SUPPORT_LDPC, vdev_mlme->proto.generic.ldpc);

    if (ic->ic_htcap) {
        vap->iv_tx_stbc = ((ic->ic_htcap & IEEE80211_HTCAP_C_TXSTBC)
                                >> IEEE80211_HTCAP_C_TXSTBC_S);
            vap->iv_rx_stbc = ((ic->ic_htcap & IEEE80211_HTCAP_C_RXSTBC)
                                >> IEEE80211_HTCAP_C_RXSTBC_S);
    }

    /* BA Buffer Size will be initialized to the IC default for
     * he target and 64 for legacy target.
     */
    if (ic->ic_he_target) {
        vap->iv_ba_buffer_size = ic->ic_he_default_ba_buf_size;
    } else {
        vap->iv_ba_buffer_size = IEEE80211_MIN_BA_BUFFER_SIZE;
    }

    if (ic->ic_vap_set_param) {
        ic->ic_vap_set_param(vap,
                IEEE80211_CONFIG_BA_BUFFER_SIZE, vap->iv_ba_buffer_size);
    }

    /* 11AX TODO (Phase II) Vap default settings
     * To Do - Keep until all the
     * user controls are added.
     * Add required checks for WRAP modes
     */
     if (ic->ic_he_target) {
        uint64_t ic_hecap_mac;
        qdf_mem_copy(&ic_hecap_mac,
                ic->ic_he.hecap_macinfo, sizeof(ic->ic_he.hecap_macinfo));

        vap->iv_he_ctrl           = ENABLE_HE_CTRL_FIELD;
        vap->iv_he_twtreq         = ENABLE_HE_TWT_REQ;
        vap->iv_he_twtres         = ENABLE_HE_TWT_RES;
        vap->iv_he_frag           = ENABLE_HE_FRAG;
        vap->iv_he_max_frag_msdu  = ENABLE_HE_MAX_FRAG_MSDU;
        vap->iv_he_min_frag_size  = ENABLE_HE_MIN_FRAG_SIZE;
        vap->iv_he_multi_tid_aggr = ENABLE_HE_MULTITID;
        vap->iv_he_link_adapt     = ENABLE_HE_LINK_ADAPT;
        vap->iv_he_all_ack        = ENABLE_HE_ALL_ACK;
        vap->iv_he_ul_mu_sched    = ENABLE_HE_UL_MU_SCHEDULING;
        vap->iv_he_actrl_bsr      = ENABLE_HE_ACTRL_BSR;
        vap->iv_he_32bit_ba       = ENABLE_HE_32BIT_BA;
        vap->iv_he_mu_cascade     = ENABLE_HE_MU_CASCADE;
        vap->iv_he_omi            = ENABLE_HE_OMI;
        vap->iv_he_ofdma_ra       = ENABLE_HE_OFDMA_RA;
        vap->iv_he_amsdu_frag     = ENABLE_HE_AMSDU_FRAG;
        vap->iv_he_flex_twt       = ENABLE_HE_FLEX_TWT;
        vap->iv_he_relaxed_edca   = ENABLE_HE_RELAXED_EDCA;
        vap->iv_he_spatial_reuse  = ENABLE_HE_SPATIAL_REUSE;
        vap->iv_he_multi_bss      = ENABLE_HE_MULTI_BSS;
        vap->iv_he_twt_required   = ENABLE_HE_TWT_REQUIRED;

        vap->iv_he_su_bfee        = HECAP_PHY_SUBFME_GET_FROM_IC
                                        ((&(ic->ic_he.hecap_phyinfo
                                            [IC_HECAP_PHYDWORD_IDX0])));
        vap->iv_he_su_bfer        = HECAP_PHY_SUBFMR_GET_FROM_IC
                                        ((&(ic->ic_he.hecap_phyinfo
                                            [IC_HECAP_PHYDWORD_IDX0])));
        vap->iv_he_ul_muofdma     = !ENABLE_HE_UL_MUOFDMA;
        vap->iv_he_muedca         = !ENABLE_HE_MUEDCA;

        if(vap->iv_opmode == IEEE80211_M_HOSTAP) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_HE,
                    "%s: Enable HE mu-bfer by default "
                    "only in AP mode\n", __FUNCTION__);
            vap->iv_he_mu_bfer    = HECAP_PHY_MUBFMR_GET_FROM_IC
                                        ((&(ic->ic_he.hecap_phyinfo
                                            [IC_HECAP_PHYDWORD_IDX0])));
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_HE,
                    "%s: Enable HE dl-ofdma by default "
                    "only in AP mode\n", __FUNCTION__);
            vap->iv_he_dl_muofdma     = ENABLE_HE_DL_MUOFDMA;
            /* Enable ULOFDMA for HKv2 */
            if(ic->ic_get_tgt_type(ic) == TARGET_TYPE_QCA8074V2) {
                vap->iv_he_ul_muofdma     = ENABLE_HE_UL_MUOFDMA;
                vap->iv_he_muedca         = ENABLE_HE_MUEDCA;
            }
        } else {
            vap->iv_he_mu_bfer    = !ENABLE_HE_MU_BFER;;
            vap->iv_he_dl_muofdma = !ENABLE_HE_DL_MUOFDMA;
        }

        vap->iv_he_ul_mumimo      = HECAP_PHY_UL_MU_MIMO_GET_FROM_IC
                                        ((&(ic->ic_he.hecap_phyinfo
                                            [IC_HECAP_PHYDWORD_IDX0])));

        vap->iv_he_default_pe_durn = IEEE80211_HE_PE_DURATION_MAX;
        /* By default HE duration-based RTS will be disabled */
        vap->iv_he_rts_threshold  = HEOP_PARAM_RTS_THRSHLD_DURATION_DISABLED_VALUE;
        /* Following features are not supported in target yet */
        vap->iv_he_mu_bfee        = !ENABLE_HE_MU_BFEE;

        vap->iv_he_amsdu_in_ampdu_suprt = HECAP_MAC_AMSDUINAMPDU_GET_FROM_IC
                                                            (ic_hecap_mac);
        vap->iv_he_1xltf_800ns_gi = HECAP_PHY_SU_1XLTFAND800NSECSGI_GET_FROM_IC
                                            ((&(ic->ic_he.hecap_phyinfo
                                                [IC_HECAP_PHYDWORD_IDX0])));
        vap->iv_he_subfee_sts_lteq80 = HECAP_PHY_BFMENSTSLT80MHZ_GET_FROM_IC
                                        ((&(ic->ic_he.hecap_phyinfo
                                            [IC_HECAP_PHYDWORD_IDX0])));
        vap->iv_he_subfee_sts_gt80   = HECAP_PHY_BFMENSTSGT80MHZ_GET_FROM_IC
                                            ((&(ic->ic_he.hecap_phyinfo
                                                [IC_HECAP_PHYDWORD_IDX0])));

        vap->iv_he_4xltf_800ns_gi = HECAP_PHY_4XLTFAND800NSECSGI_GET_FROM_IC
                                            ((&(ic->ic_he.hecap_phyinfo
                                                [IC_HECAP_PHYDWORD_IDX0])));
        vap->iv_he_max_nc         = HECAP_PHY_MAX_NC_GET_FROM_IC
                                            ((&(ic->ic_he.hecap_phyinfo
                                                [IC_HECAP_PHYDWORD_IDX0])));
        vap->iv_twt_rsp           = HECAP_MAC_TWTRSP_GET_FROM_IC(ic_hecap_mac);

#if ATH_SUPPORT_WRAP
        if (vap->iv_psta || vap->iv_mpsta) {
            vap->iv_he_ul_mumimo  = !ENABLE_HE_UL_MUMIMO;
            vap->iv_he_dl_muofdma = !ENABLE_HE_DL_MUOFDMA;
            vap->iv_he_ul_muofdma = !ENABLE_HE_UL_MUOFDMA;
            vap->iv_he_su_bfee    = !ENABLE_HE_SU_BFEE;
            vap->iv_he_su_bfer    = !ENABLE_HE_SU_BFER;
            vap->iv_he_mu_bfee    = !ENABLE_HE_MU_BFEE;
            vap->iv_he_mu_bfer    = !ENABLE_HE_MU_BFER;
        }
#endif
        /* Set vedev params he su bfee, he su bfer, he mu bfee,
         * he mu bfer, he dl/ul muofdma and he ul mumimo by default.
         * Note that setting one of these params will internally set
         * all of these vdev params since we are using the same
         * wmi_vdev_interface to set these params */
        if (ic->ic_vap_set_param) {
            ic->ic_vap_set_param(vap, IEEE80211_CONFIG_HE_SU_BFEE, vap->iv_he_su_bfee);
        }

        /* initialize the mcsnssmap in ieee80211vap structure */
        vap->iv_he_tx_mcsnssmap = HE_INVALID_MCSNSSMAP;
        vap->iv_he_rx_mcsnssmap = HE_INVALID_MCSNSSMAP;

        /* Set the default sounding mode to SU AX sounding */
        vap->iv_he_sounding_mode = ENABLE_SU_AX_SOUNDING;
        if(ic->ic_vap_set_param) {
            ic->ic_vap_set_param(vap, IEEE80211_CONFIG_HE_SOUNDING_MODE,
                                    vap->iv_he_sounding_mode);
        }

        /* In case of ap, basic mcs/nss are known.
         * We need to handle STA part differently as
         * STA does not advertise HE basic mcsnss map.
         * For a STA, it has to check whether the
         * AP sent mcsnss map in HECAP meets the basic
         * requirement advertised in the HEOP by the same AP.
         * The initialization for AP is done here. For,
         * sta these values will be derived from HEOP
         * sent by the AP.
         */
        if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
            vap->iv_he_basic_mcs_for_bss = HE_MCS_VALUE_FOR_MCS_0_7;
            vap->iv_he_basic_nss_for_bss = HE_DEFAULT_SS_IN_MCS_NSS_SET;
        }
    }

    vap->iv_tsf_offset.offset = 0;
    vap->iv_tsf_offset.offset_negative = false;

    /*by defualt wep key cache will not be allocated in first four slots */
    vap->iv_wep_keycache = 1;

#if ATH_SUPPORT_WPA_SUPPLICANT_CHECK_TIME
    vap->iv_rejoint_attemp_time = 20;
#endif
    /* by defualt we will send disassoc while doing ath0 down */
    vap->iv_send_deauth= 0;

#if UMAC_SUPPORT_APONLY
    vap->iv_aponly = true;
    OS_MEMZERO(&vap_opmode_count, sizeof(vap_opmode_count));
    ieee80211_get_vap_opmode_count(ic, &vap_opmode_count);
    if (vap->iv_opmode == IEEE80211_M_IBSS &&
        vap_opmode_count.total_vaps == 0) {
        printk("Disabling aponly path for ad-hoc mode\n");
        vap->iv_aponly = false;
        ic->ic_aponly = false;
    }
#else
    vap->iv_aponly = false;
#endif

    /*initialization for ratemask*/
    vap->iv_ratemask_default = 0;
    vap->iv_legacy_ratemasklower32 = 0;
    vap->iv_ht_ratemasklower32 = 0;
    vap->iv_vht_ratemasklower32 = 0;
    vap->iv_vht_ratemaskhigher32 = 0;
    vap->min_dwell_time_passive = 200;
    vap->max_dwell_time_passive = 300;
#if QCA_LTEU_SUPPORT
    vap->scan_rest_time =
        vap->scan_idle_time = (u_int32_t)-1;
    vdev_mlme->mgmt.generic.repeat_probe_time =
        vdev_mlme->mgmt.generic.probe_delay = (u_int32_t)-1;

    vap->scan_probe_spacing_interval = LTEU_DEFAULT_PRB_SPC_INT;
    vap->mu_start_delay = 5000;
    vap->wifi_tx_power = 22;
#endif
    vap->iv_ena_vendor_ie = 1;	/* Enable vendoe Ie by default */
    vap->iv_update_vendor_ie = 0;  /* configure for the default value */
    vap->iv_force_onetxchain = false;
    vap->iv_mesh_mgmt_txsend_config  = 0;
    vap->iv_assoc_denial_notify = 1; /* Enable assoc denial notification by default*/
    vdev_mlme->mgmt.generic.rts_threshold = IEEE80211_RTS_MAX;
    vdev_mlme->mgmt.generic.frag_threshold = IEEE80211_FRAGMT_THRESHOLD_MAX;


    /* attach other modules */
    ieee80211_rateset_vattach(vap);
    ieee80211_proto_vattach(vap);
    ieee80211_node_vattach(vap, vdev_mlme);
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    ieee80211_crypto_vattach(vap);
#endif
    ieee80211_vap_pause_vattach(ic,vap);
    ieee80211_rrm_vattach(ic,vap);
    ieee80211_wnm_vattach(ic,vap);
    ieee80211_power_vattach(vap,
                            1,  /* enable full sleep */
                            ic->ic_reg_parm.sleepTimePwrSaveMax,
                            ic->ic_reg_parm.sleepTimePwrSave,
                            ic->ic_reg_parm.sleepTimePerf,
                            ic->ic_reg_parm.inactivityTimePwrSaveMax,
                            ic->ic_reg_parm.inactivityTimePwrSave,
                            ic->ic_reg_parm.inactivityTimePerf,
                            ic->ic_reg_parm.smpsDynamic,
                            ic->ic_reg_parm.psPollEnabled);
    ieee80211_mlme_vattach(vap);
    if (ieee80211_mlme_register_event_handler( vap, ieee80211_mlme_event_callback, NULL ) != EOK) {
        mlme_err("%s : Unable to register event handler with MLME!", __func__);
    }

    ieee80211_aplist_vattach(&(vap->iv_candidate_aplist),
                             vap,
                             vap->iv_ic->ic_osdev);
    ieee80211_aplist_config_vattach(&(vap->iv_aplist_config), vap->iv_ic->ic_osdev);
    ieee80211_acl_attach(vap);
    ieee80211_scs_vattach(vap);
    ieee80211_ald_vattach(vap);
    /*MBO & OCE functionality init */
    ieee80211_mbo_vattach(vap);
    ieee80211_oce_vattach(vap);
    vap->nbr_scan_period = NBR_SCAN_PERIOD;
#if ATH_SUPPORT_WIFIPOS && !(ATH_SUPPORT_LOWI)
    if (wifiposenable)
    {
#if ATH_SUPPORT_WIFIPOS_ONE_VAP
{
        struct ieee80211vap *first_vap;
        first_vap = TAILQ_FIRST(&ic->ic_vaps);
        if ( NULL == first_vap ){
            printk("In %s, line:%d, wifi-pos enabled vap:%d\n", __func__, __LINE__, vap->iv_unit );
            ieee80211_wifipos_vattach(vap);
        }
	else {
            printk("In %s, line:%d, wifi-pos disabled vap:%d\n", __func__, __LINE__, vap->iv_unit );
        }
}
#else
        ieee80211_wifipos_vattach(vap);
#endif
    }
    else
    {
        printk("%s:%d: wifipos disabled\n", __func__,__LINE__);
    }
#endif

#if DYNAMIC_BEACON_SUPPORT
        vap->iv_dbeacon = 0;
        vap->iv_dbeacon_runtime = 0;
        vap->iv_dbeacon_rssi_thr = DEFAULT_DBEACON_RSSI_THRESHOLD;
        vap->iv_dbeacon_timeout = DEFAULT_DBEACON_TIMEOUT;
        OS_INIT_TIMER(ic->ic_osdev, &vap->iv_dbeacon_suspend_beacon,
                ieee80211_dbeacon_suspend_beacon, vap,QDF_TIMER_TYPE_WAKE_APPS);
        qdf_spinlock_create(&vap->iv_dbeacon_lock);
#endif
#if QCN_IE
        vap->iv_bpr_delay = DEFAULT_BCAST_PROBE_RESP_DELAY;
        ic->ic_bpr_latency_comp = DEFAULT_BCAST_PROBE_RESP_LATENCY_COMPENSATION;
        ic->ic_bcn_latency_comp = DEFAULT_BEACON_LATENCY_COMPENSATION;
        if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
            qdf_hrtimer_init(&vap->bpr_timer, ieee80211_send_bcast_resp_handler,
                QDF_CLOCK_MONOTONIC, QDF_HRTIMER_MODE_REL, QDF_CONTEXT_TASKLET);
        }
#endif

    qdf_spinlock_create(&vap->iv_mgmt_offchan_tx_lock);
    qdf_spinlock_create(&vap->peer_free_notif_lock);
    qdf_list_create(&vap->iv_mgmt_offchan_req_list, IEEE80211_MGMT_OFFCHAN_LIST_MAX);
    qdf_atomic_init(&vap->iv_mgmt_offchan_cmpl_pending);

    vap->iv_csmode = IEEE80211_CSA_MODE_AUTO; /* csmode will be calculated dynamically */
    vap->iv_enable_ecsaie = true; /* Extended channel switch ie is enabled by default */
    vap->iv_enable_max_ch_sw_time_ie = true; /* Maximum channel switch Time ie is enabled by default */

    vap->iv_sta_assoc_resp_ie = NULL;
    vap->iv_sta_assoc_resp_len = 0;
#if UMAC_SUPPORT_WPA3_STA
    vap->iv_sae_max_auth_attempts = 0;
    qdf_timer_init(vap->iv_ic->ic_osdev, &vap->iv_sta_external_auth_timer,
                   wlan_mlme_external_auth_timer_fn, vap, QDF_TIMER_TYPE_WAKE_APPS);
#endif

    if(opmode == IEEE80211_M_STA)
        ieee80211_roam_initparams(vap);

    OS_MEMSET(vap->iv_txpow_mgt_frm, 0xff, sizeof(vap->iv_txpow_mgt_frm));
    vap->force_cleanup_peers = 0;
    return 1;
#undef  IEEE80211_C_OPMODE
}

int
ieee80211_vap_attach(struct ieee80211vap *vap)
{
    int ret;
#if WLAN_SUPPORT_GREEN_AP
    struct wlan_objmgr_pdev *pdev = wlan_vap_get_pdev(vap);
#endif

    IEEE80211_ADD_VAP_TARGET(vap);

    /*
     * XXX: It looks like we always need a bss node around
     * for transmit before association (e.g., probe request
     * in scan operation). When we actually join a BSS, we'll
     * create a new node and free the old one.
     */
    ret = ieee80211_node_latevattach(vap);


    if (ret == 0) {
        IEEE80211_UPDATE_TARGET_IC(vap->iv_bss);
    }

    /*
     * If IQUE is NOT enabled at compiling, ieee80211_me_attach attaches
     * the empty op table to vap->iv_me_ops;
     * If IQUE is enabled, the initialization is done by the following
     * function, and the op table is correctly attached.
     */
    if(ret == 0){
        ieee80211_ique_attach(ret,vap);
        wlan_vdev_ique_attach(vap->vdev_obj);
    }
    if(ret == 0)
    {
        ieee80211_nawds_attach(vap);
        vap->iv_vap_ath_info_handle = ieee80211_vap_ath_info_attach(vap);
        ieee80211_quiet_vattach(vap);

#if WLAN_SUPPORT_GREEN_AP
        if (vap->iv_opmode == IEEE80211_M_HOSTAP)
            /* If it is a HOSTAP start the Green AP feature */
             wlan_green_ap_start(pdev);
#endif
    }

    vap->iv_sta_assoc_resp_ie = NULL;
    vap->iv_sta_assoc_resp_len = 0;
#if UMAC_SUPPORT_WPA3_STA
    vap->iv_sae_max_auth_attempts = 0;
#endif
    vap->iv_dpp_vap_mode = 0;
    return ret;
}

void
ieee80211_vap_detach(struct ieee80211vap *vap)
{
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    int i;
#endif
#if WLAN_SUPPORT_GREEN_AP
    struct wlan_objmgr_pdev *pdev = wlan_vap_get_pdev(vap);
#endif

    ieee80211_quiet_vdetach(vap);
    ieee80211_vap_ath_info_detach(vap->iv_vap_ath_info_handle);
    ieee80211_node_latevdetach(vap);
    ieee80211_proto_vdetach(vap);
    ieee80211_mlme_vdetach(vap);
    ieee80211_vap_pause_vdetach(vap->iv_ic, vap);
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    ieee80211_crypto_vdetach(vap);
#endif
    ieee80211_ald_vdetach(vap);
    ieee80211_scs_vdetach(vap);
    ieee80211_rrm_vdetach(vap);
    ieee80211_power_vdetach(vap);
    ieee80211_wnm_vdetach(vap);
    /*
     * detach ique features/functions
     */
    if (vap->iv_ique_ops.me_detach) {
        vap->iv_ique_ops.me_detach(vap);
    }
    if (vap->iv_ique_ops.hbr_detach) {
        vap->iv_ique_ops.hbr_detach(vap);
    }
    ieee80211_aplist_vdetach(&(vap->iv_candidate_aplist));
    ieee80211_aplist_config_vdetach(&(vap->iv_aplist_config));
    ieee80211_resmgr_vdetach(vap->iv_ic->ic_resmgr, vap);
    ieee80211_acl_detach(vap);
    ieee80211_acl_detach(vap);
    /*MBO & OCE functionality deinit */
    ieee80211_mbo_vdetach(vap);
    ieee80211_oce_vdetach(vap);
#if ATH_SUPPORT_WIFIPOS && !(ATH_SUPPORT_LOWI)
    ieee80211_wifipos_vdetach(vap);
#endif
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    for (i = 0; i < IEEE80211_WEP_NKID; i++) {
        ieee80211_crypto_freekey(vap, &vap->iv_nw_keys[i]);
    }
#endif
    ieee80211_vendorie_vdetach(vap);
#if DYNAMIC_BEACON_SUPPORT
    OS_FREE_TIMER(&vap->iv_dbeacon_suspend_beacon);
    qdf_spinlock_destroy(&vap->iv_dbeacon_lock);
#endif

#if WLAN_SUPPORT_GREEN_AP
    if (vap->iv_opmode == IEEE80211_M_HOSTAP)
        wlan_green_ap_stop(pdev);
#endif
    if(vap->iv_sta_assoc_resp_ie)
	qdf_mem_free(vap->iv_sta_assoc_resp_ie);

#if UMAC_SUPPORT_WPA3_STA
    qdf_timer_sync_cancel(&vap->iv_sta_external_auth_timer);
    qdf_timer_free(&vap->iv_sta_external_auth_timer);
#endif
    if (vap->iv_roam.iv_ft_ie.ie) {
        qdf_mem_free(vap->iv_roam.iv_ft_ie.ie);
    }

    if (vap->iv_roam.iv_ft_params.fties) {
        qdf_mem_free(vap->iv_roam.iv_ft_params.fties);
    }

    spin_lock_destroy(&vap->iv_lock);
    spin_lock_destroy(&vap->init_lock);
    while (!qdf_list_empty(&vap->iv_mgmt_offchan_req_list)) {
        qdf_list_node_t *tnode = NULL;
        struct ieee80211_offchan_list *offchan_list;

        if (qdf_list_remove_front(
                   &vap->iv_mgmt_offchan_req_list, &tnode) == QDF_STATUS_SUCCESS) {
            offchan_list = qdf_container_of(tnode, struct ieee80211_offchan_list, next_request);
            if (offchan_list->offchan_tx_frm)
                qdf_nbuf_free(offchan_list->offchan_tx_frm);
            qdf_mem_free(offchan_list);
        }
    }

    qdf_list_destroy(&vap->iv_mgmt_offchan_req_list);
    qdf_spinlock_destroy(&vap->iv_mgmt_offchan_tx_lock);
    qdf_spinlock_destroy(&vap->peer_free_notif_lock);
}

int
ieee80211_vap_register_events(struct ieee80211vap *vap, wlan_event_handler_table *evtab)
{
    vap->iv_evtable = evtab;
    wlan_vdev_register_osif_events(vap->vdev_obj, evtab);
    return 0;
}

int
ieee80211_vap_register_mlme_events(struct ieee80211vap *vap, os_handle_t oshandle, wlan_mlme_event_handler_table *evtab)
{
    int i;
    /* unregister if there exists one already */
    ieee80211_vap_unregister_mlme_events(vap,oshandle,evtab);
    IEEE80211_VAP_LOCK(vap);
    for (i=0;i<IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS; ++i) {
        if ( vap->iv_mlme_evtable[i] == NULL) {
            vap->iv_mlme_evtable[i] = evtab;
            vap->iv_mlme_arg[i] = oshandle;
            IEEE80211_VAP_UNLOCK(vap);
            return 0;
        }
    }
    IEEE80211_VAP_UNLOCK(vap);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "%s mlme evtable is full.\n", __func__);
    return ENOMEM;
}

int
ieee80211_vap_unregister_mlme_events(struct ieee80211vap *vap,os_handle_t oshandle, wlan_mlme_event_handler_table *evtab)
{
    int i;
    IEEE80211_VAP_LOCK(vap);
    for (i=0;i<IEEE80211_MAX_VAP_MLME_EVENT_HANDLERS; ++i) {
        if ( vap->iv_mlme_evtable[i] == evtab && vap->iv_mlme_arg[i] == oshandle) {
            vap->iv_mlme_evtable[i] = NULL;
            vap->iv_mlme_arg[i] = NULL;
            IEEE80211_VAP_UNLOCK(vap);
            return 0;
        }
    }
    IEEE80211_VAP_UNLOCK(vap);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "%s The handler is not in the evtable.\n", __func__);
    return EEXIST;
}

int
ieee80211_vap_register_misc_events(struct ieee80211vap *vap, os_handle_t oshandle, wlan_misc_event_handler_table *evtab)
{
    int i;
    /* unregister if there exists one already */
    ieee80211_vap_unregister_misc_events(vap,oshandle,evtab);
    IEEE80211_VAP_LOCK(vap);
    for (i=0;i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {
        if ( vap->iv_misc_evtable[i] == NULL) {
            vap->iv_misc_evtable[i] = evtab;
            vap->iv_misc_arg[i] = oshandle;
            IEEE80211_VAP_UNLOCK(vap);
            return 0;
        }
    }
    IEEE80211_VAP_UNLOCK(vap);
    return ENOMEM;
}

int
ieee80211_vap_unregister_misc_events(struct ieee80211vap *vap,os_handle_t oshandle, wlan_misc_event_handler_table *evtab)
{
    int i;
    IEEE80211_VAP_LOCK(vap);
    for (i=0;i<IEEE80211_MAX_MISC_EVENT_HANDLERS; ++i) {
        if ( vap->iv_misc_evtable[i] == evtab && vap->iv_misc_arg[i] == oshandle) {
            vap->iv_misc_evtable[i] = NULL;
            vap->iv_misc_arg[i] = NULL;
            IEEE80211_VAP_UNLOCK(vap);
            return 0;
        }
    }
    IEEE80211_VAP_UNLOCK(vap);
    return EEXIST;
}

int ieee80211_vap_register_ccx_events(struct ieee80211vap *vap, os_if_t osif, wlan_ccx_handler_table *evtab)
{
    vap->iv_ccx_arg = osif;
    vap->iv_ccx_evtable = evtab;
    wlan_vdev_register_ccx_event_handlers(vap->vdev_obj, osif, evtab);
    return 0;
}

int
ieee80211_vap_match_ssid(struct ieee80211vap *vap, const u_int8_t *ssid, u_int8_t ssidlen)
{
    int i;

    if (vap == NULL) {
        return 0;
    }
    for (i = 0; i < vap->iv_des_nssid; i++) {
        if ((vap->iv_des_ssid[i].len == ssidlen) &&
            (OS_MEMCMP(vap->iv_des_ssid[i].ssid, ssid, ssidlen) == 0)) {
            /* find a matching entry */
            return 1;
        }
    }

    return 0;
}

/*
 * free the vap and deliver event.
 */
void ieee80211_vap_free(struct ieee80211vap *vap)
{
    struct ieee80211com *ic  = vap->iv_ic;
    os_if_t             osif = vap->iv_ifp;

    if (osif != NULL) {
       IEEE80211COM_DELIVER_VAP_EVENT(ic, osif, IEEE80211_VAP_DELETED);
    }
}

/*
 * deliver the stop event to osif layer.
 */
void ieee80211_vap_deliver_stop(struct ieee80211vap *vap)
{
    struct ieee80211com *ic;
    os_if_t             osif;

    if (vap) {
        ic = vap->iv_ic;
        osif = vap->iv_ifp;

        IEEE80211COM_DELIVER_VAP_EVENT(ic, osif, IEEE80211_VAP_STOPPED);
    }
}

/*
 * deliver the error event to osif layer.
 */
void ieee80211_vap_deliver_stop_error(struct ieee80211vap *vap)
{
    struct ieee80211com *ic;
    os_if_t             osif;

    if (vap) {
        ic = vap->iv_ic;
        osif = vap->iv_ifp;

        IEEE80211COM_DELIVER_VAP_EVENT(ic, osif, IEEE80211_VAP_STOP_ERROR);
    }
}

/**
 * @register a vap event handler.
 * ARGS :
 *  ieee80211_vap_event_handler : vap event handler
 *  arg                         : argument passed back via the evnt handler
 * RETURNS:
 *  on success returns 0.
 *  on failure returns a negative value.
 * allows more than one event handler to be registered.
 */
int ieee80211_vap_unregister_event_handler(ieee80211_vap_t vap,ieee80211_vap_event_handler evhandler, void *arg)
{
    int i;
    IEEE80211_VAP_LOCK(vap);
    for (i=0;i<IEEE80211_MAX_VAP_EVENT_HANDLERS; ++i) {
        if ( vap->iv_event_handler[i] == evhandler &&  vap->iv_event_handler_arg[i] == arg ) {
            vap->iv_event_handler[i] = NULL;
            vap->iv_event_handler_arg[i] = NULL;
            IEEE80211_VAP_UNLOCK(vap);
            return 0;
        }
    }
    IEEE80211_VAP_UNLOCK(vap);
    return EEXIST;

}

/**
 * @unregister a vap event handler.
 * ARGS :
 *  ieee80211_vap_event_handler : vap event handler
 *  arg                         : argument passed back via the evnt handler
 * RETURNS:
 *  on success returns 0.
 *  on failure returns a negative value.
 */
int ieee80211_vap_register_event_handler(ieee80211_vap_t vap,ieee80211_vap_event_handler evhandler, void *arg)
{
    int i;
    /* unregister if there exists one already */
    ieee80211_vap_unregister_event_handler(vap,evhandler,arg);

    IEEE80211_VAP_LOCK(vap);
    for (i=0;i<IEEE80211_MAX_VAP_EVENT_HANDLERS; ++i) {
        if ( vap->iv_event_handler[i] == NULL) {
            vap->iv_event_handler[i] = evhandler;
            vap->iv_event_handler_arg[i] = arg;
            IEEE80211_VAP_UNLOCK(vap);
            return 0;
        }
    }
    IEEE80211_VAP_UNLOCK(vap);
    return ENOMEM;
}

void ieee80211_vap_deliver_event(struct ieee80211vap *vap, ieee80211_vap_event *event)
{
    int i;
    void *arg;
    ieee80211_vap_event_handler evhandler;
    IEEE80211_VAP_LOCK(vap);
    for(i=0;i<IEEE80211_MAX_VAP_EVENT_HANDLERS; ++i) {
        if (vap->iv_event_handler[i]) {
            evhandler =  vap->iv_event_handler[i];
            arg = vap->iv_event_handler_arg[i];
            IEEE80211_VAP_UNLOCK(vap);
            (* evhandler) (vap, event,arg);
            IEEE80211_VAP_LOCK(vap);
        }
    }
    IEEE80211_VAP_UNLOCK(vap);
}

/* Return true if any VAP is in RUNNING state */
int
ieee80211_vap_is_any_running(struct ieee80211com *ic)
{

    struct ieee80211vap *vap;
    int running = 0;
    IEEE80211_COMM_LOCK(ic);
    TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
        if (vap->iv_opmode != IEEE80211_M_HOSTAP && vap->iv_opmode != IEEE80211_M_IBSS) {
            continue;
        }
        if (vap->channel_switch_state) {
            continue;
        }
        if (wlan_vdev_is_up(vap->vdev_obj) == QDF_STATUS_SUCCESS) {
            running++;
            break;
        }
    }
    IEEE80211_COMM_UNLOCK(ic);
    return running;
}

int
ieee80211_num_apvap_running(struct ieee80211com *ic)
{

    struct ieee80211vap *vap;
    int running = 0;
    IEEE80211_COMM_LOCK(ic);
    TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
        if ((wlan_vdev_is_up(vap->vdev_obj) == QDF_STATUS_SUCCESS) &&
                vap->iv_opmode == IEEE80211_M_HOSTAP) {
            running++;
        }
    }
    IEEE80211_COMM_UNLOCK(ic);
    return running;
}

/**
 * @ get opmode
 * ARGS:
 *  vap    : handle to vap.
 * RETURNS: returns opmode of the vap.
 */
enum ieee80211_opmode ieee80211_vap_get_opmode(ieee80211_vap_t vap)
{
    return  vap->iv_opmode;
}

const char* ieee80211_opmode2string( enum ieee80211_opmode opmode)
{
    switch ( opmode )
    {
    case IEEE80211_M_STA:
         return "IEEE80211_M_STA";
    case IEEE80211_M_IBSS:
     return "IEEE80211_M_IBSS";
    case IEEE80211_M_AHDEMO:
     return "IEEE80211_M_AHDEMO";
    case IEEE80211_M_HOSTAP:
         return "IEEE80211_M_HOSTAP";
    case IEEE80211_M_MONITOR:
     return "IEEE80211_M_MONITOR";
    case IEEE80211_M_WDS:
     return "IEEE80211_M_WDS";
    case IEEE80211_M_BTAMP:
     return "IEEE80211_M_BTAMP";
    case IEEE80211_M_ANY:
     return "IEEE80211_M_ANY";

    default:
     return "Unknown ieee80211_opmode";
    }
};


void wlan_vap_up_check_beacon_interval(struct ieee80211vap *vap, enum ieee80211_opmode opmode)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ieee80211vap *tmpvap;
    ol_ath_soc_softc_t *soc = scn->soc;

    IEEE80211_COMM_LOCK_BH(ic);

    if (opmode == IEEE80211_M_HOSTAP) {
        ic->ic_num_ap_vaps++;
        /* Increment lp_iot_mode vap count if flags indicate. */
        if (vap->iv_create_flags & IEEE80211_LP_IOT_VAP)  {
            if (!ic->ic_num_lp_iot_vaps) {
                /* Maintain LP IOT VAP status in soc per pdev */
                soc->soc_lp_iot_vaps_mask |= (uint8_t)(1 << ic->ic_get_pdev_idx(ic));
                ic->ic_num_lp_iot_vaps=1;
            } else {
                 IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_DEBUG,
                         "Only one Low power vap is supported \n");
            };
        }
#if ATH_PERF_PWR_OFFLOAD
        if (MBSSID_BINTVAL_CHECK(ic->ic_intval, ic->ic_num_ap_vaps, scn->bcn_mode)) {
            ic->ic_need_vap_reinit = 0;
        }
        else {
            /*
             * If a new VAP is created on runtime , we need to ensure that beacon interval is atleast 50 when NoOfVAPS<=2 ,
             *  100 when NoOfVAPS<=8 and 200 when NoOfVAPS<=16
             */
            if(ic->ic_num_ap_vaps <= IEEE80211_BINTVAL_VAPCOUNT1){
                ic->ic_intval = IEEE80211_OL_BINTVAL_MINVAL_RANGE1;
                ic->ic_need_vap_reinit = 1;
            }
            else if(ic->ic_num_ap_vaps <= IEEE80211_BINTVAL_VAPCOUNT2) {
                ic->ic_intval = IEEE80211_BINTVAL_MINVAL_RANGE2;
                ic->ic_need_vap_reinit = 1;

            }
            else {
                if(scn->bcn_mode == 1) { //burst mode enabled so limit the beacon interval value to 100
                    ic->ic_intval = IEEE80211_BINTVAL_MINVAL_RANGE2;
                }
                else { // burst mode disabled (staggered) , limit the beacon interval to 200
                    ic->ic_intval = IEEE80211_BINTVAL_MINVAL_RANGE3;
                }

                ic->ic_need_vap_reinit = 1;

            }


        }
#endif
        /* There is a need to upconvert all vap's bintval to a factor of the LP IOT vap's beacon intval */
        if (vap->iv_create_flags & IEEE80211_LP_IOT_VAP)  {
            u_int8_t lp_vap_is_present=0;
            u_int16_t lp_bintval = ic->ic_intval;

            /* Iterate to find if a LP IOT vap is there */
            TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
                if (tmpvap->iv_create_flags & IEEE80211_LP_IOT_VAP) {
                    lp_vap_is_present = 1;
                    /* If multiple lp iot vaps are present pick the least */
                    if (lp_bintval > tmpvap->iv_bss->ni_intval)  {
                        lp_bintval = tmpvap->iv_bss->ni_intval;
                    }
                }
            }

            /* up convert beacon interval in ic to a factor of LP vap */
            if (lp_vap_is_present)  {
                UP_CONVERT_TO_FACTOR_OF(ic->ic_intval, lp_bintval);
            }

            TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
                if (lp_vap_is_present)  {
                    if (!(tmpvap->iv_create_flags & IEEE80211_LP_IOT_VAP)) {
                        /* up convert vap beacon interval in ni to a factor of LP vap */
                        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                                "Current beacon interval %d: Checking if up conversion is needed as lp_iot vap is present. ", ic->ic_intval);
                        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                                "New beacon interval  %d \n", ic->ic_intval);
                    }
                    UP_CONVERT_TO_FACTOR_OF(tmpvap->iv_bss->ni_intval, lp_bintval);
                }
            }
            ic->ic_need_vap_reinit = 1;  /* Better to reinit */
        }

        if (!(ic->ic_is_mode_offload(ic))) {
            /*TODO SRK: Direct Attach? Increment lp_iot_mode vap count if flags indicate. */
            LIMIT_BEACON_PERIOD(ic->ic_intval);
            ic->ic_set_beacon_interval(ic);
        }
    }

    IEEE80211_COMM_UNLOCK_BH(ic);
}

void wlan_vap_down_check_beacon_interval(struct ieee80211vap *vap, enum ieee80211_opmode opmode)
{
    struct ieee80211com *ic = vap->iv_ic;

    IEEE80211_COMM_LOCK_BH(ic);

    if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
        if (vap->iv_create_flags & IEEE80211_LP_IOT_VAP)  {
            /* Even though it is not expected we do see multiple down calls which ends up in wrapround
               So resetting instead of downcount.  */
            ic->ic_num_lp_iot_vaps=0;
        }
        if ((ic->ic_is_mode_offload(ic)) && ic->ic_num_ap_vaps) {
            ic->ic_need_vap_reinit = 0;
        }
        if (ic->ic_num_ap_vaps)
        {
            ic->ic_num_ap_vaps--;
        }

    }

    IEEE80211_COMM_UNLOCK_BH(ic);
}


/*
 * @ check if vap is in running state
 * ARGS:
 *  vap    : handle to vap.
 * RETURNS:
 *  TRUE if current state of the vap is IEE80211_S_RUN.
 *  FALSE otherwise.
 */
bool ieee80211_is_vap_state_running(ieee80211_vap_t vap)
{
    return (wlan_vdev_is_up(vap->vdev_obj) == QDF_STATUS_SUCCESS) ? true : false;
}
EXPORT_SYMBOL(ieee80211_is_vap_state_running);

/*
 * External UMAC Interface
 */
wlan_if_t
wlan_vap_create(wlan_dev_t            devhandle,
                enum ieee80211_opmode opmode,
                int                   scan_priority_base,
                u_int32_t             flags,
                u_int8_t              *bssid,
                u_int8_t              *mataddr,
                void                   *osifp_handle)
{
    struct ieee80211com *ic = devhandle;
    struct ieee80211vap *vap;
    struct vdev_mlme_obj *vdev_mlme;

    qdf_info("enter. devhandle=0x%pK, opmode=%s, flags=0x%x\n",
             devhandle,
             ieee80211_opmode2string(opmode),
             flags);

    /* For now restrict lp_iot vaps to just 1 */
    if (!ic ||((opmode == IEEE80211_M_HOSTAP) &&
               (flags & IEEE80211_LP_IOT_VAP) &&
               (ic->ic_num_lp_iot_vaps))) {
        printk("%s: cant create more than 1 lp_iot mode vap object \n", __func__);
        return NULL;
    }

    /* Legacy MLME init */
    vdev_mlme = (struct vdev_mlme_obj *)osifp_handle;
    vap = mlme_ext_vap_create(ic, opmode, scan_priority_base, flags, bssid,
                              mataddr, vdev_mlme);
    if (vap == NULL) {
        printk("%s: failed to create a vap object\n", __func__);
        return NULL;
    }

    ieee80211_vap_pause_late_vattach(ic,vap);
    ieee80211_resmgr_vattach(ic->ic_resmgr, vap);

    ieee80211_vap_deleted_clear(vap); /* clear the deleted */

    /* when all  done,  add vap to queue */
    IEEE80211_COMM_LOCK_BH(ic);
    TAILQ_INSERT_TAIL(&ic->ic_vaps, vap, iv_next);

    /*
     * Ensure that beacon interval is atleast 50 when NoOfVAPS<=2 , 100 when NoOfVAPS<=8 and 200 when NoOfVAPS<=16
     *
     */
    IEEE80211_COMM_UNLOCK_BH(ic);

    qdf_info("exit");

#if ATH_SUPPORT_FLOWMAC_MODULE
    /*
     * Due to platform dependency the flow control need to check the status
     * of the lmac layer before enabling the flow mac at vap layer.
     *
     * Enabling of the flowmac happens after creating the wifi interface
     */
    vap->iv_flowmac = ic->ic_get_flowmac_enabled_State(ic);
#endif
#if ATH_PERF_PWR_OFFLOAD
#if ATH_SUPPORT_DSCP_OVERRIDE
    vap->iv_dscp_map_id = 0x00;
#endif
#endif
#if UNIFIED_SMARTANTENNA
    ic->sta_not_connected_cfg = TRUE;

    vdev_mlme->mgmt.chainmask_info.tx_chainmask = devhandle->ic_tx_chainmask;
    vdev_mlme->mgmt.chainmask_info.rx_chainmask = devhandle->ic_rx_chainmask;
    wlan_sa_api_start(ic->ic_pdev_obj, vap->vdev_obj, SMART_ANT_STA_NOT_CONNECTED | SMART_ANT_NEW_CONFIGURATION);
#endif
    return vap;
}

void ic_mon_vap_update(struct ieee80211com *ic, struct ieee80211vap *vap)
{
    if(vap->iv_opmode == IEEE80211_M_MONITOR)
    {
        ic->ic_mon_vap = vap;
        wlan_pdev_set_mon_vdev(ic->ic_pdev_obj, vap->vdev_obj);
    }
}

void ic_sta_vap_update(struct ieee80211com *ic, struct ieee80211vap *vap)
{
    STA_VAP_DOWNUP_LOCK(ic);
    if(vap->iv_opmode == IEEE80211_M_STA) {
        /*
         * In QWRAP mode remember the main proxy STA
         * In non QWRAP mode remember the only STA
         */
#if ATH_SUPPORT_WRAP
    if(wlan_is_psta(vap)) {
        if(wlan_is_mpsta(vap))
            ic->ic_sta_vap = vap;
    } else  {
            ic->ic_sta_vap = vap;
    }
#else
    ic->ic_sta_vap = vap;
#endif
    }
    STA_VAP_DOWNUP_UNLOCK(ic);
}

void ic_reset_params(struct ieee80211com *ic)
{
    struct wlan_objmgr_pdev *pdev = NULL;

    if (ic->ic_acs) {
        ieee80211_acs_deinit(&(ic->ic_acs));
        ieee80211_acs_init(&(ic->ic_acs), ic, ic->ic_osdev);
        wlan_register_scantimer_handler(ic);
    }

    pdev = ic->ic_pdev_obj;

    if (pdev == NULL){
        qdf_print("\npdev is NULL");
    }else{
	ucfg_scan_flush_results(pdev, NULL);
    }

#if ACFG_NETLINK_TX
    acfg_clean(ic);
#endif

#if WLAN_SUPPORT_PRIMARY_ALLOWED_CHAN
    if(ic->ic_primary_allowed_enable) {
            qdf_mem_free(ic->ic_primary_chanlist->chan);
            qdf_mem_free(ic->ic_primary_chanlist);
            ic->ic_primary_allowed_enable = false;
            ic->ic_primary_chanlist = NULL;
    }
#endif

#if ATH_SUPPORT_DSCP_OVERRIDE
    ic->ic_override_hmmc_dscp               = 0;
    ic->ic_dscp_hmmc_tid                    = 0;
    ic->ic_dscp_igmp_tid                    = 0;
    ic->ic_override_igmp_dscp               = 0;
#endif
#if UMAC_SUPPORT_PERIODIC_PERFSTATS
    ic->ic_thrput.is_enab                   = 0;
    ic->ic_thrput.histogram_size            = 15;
    ic->ic_per.is_enab                      = 0;
    ic->ic_per.histogram_size               = 300;
#endif
    ic->ic_blkreportflood                   = 0;
    ic->ic_dropstaquery                     = 0;
    ic->ic_consider_obss_long_slot          = CONSIDER_OBSS_LONG_SLOT_DEFAULT;
    ic->ic_sta_vap_amsdu_disable            = 0;
#if QCA_AIRTIME_FAIRNESS
    wlan_atf_pdev_reset(ic->ic_pdev_obj);
#endif /* QCA_AIRTIME_FAIRNESS */

    if(!ic->ic_is_mode_offload(ic)) {
#ifdef QCA_LOWMEM_PLATFORM
        ic->ic_num_clients = IEEE80211_33_AID;
#else
        ic->ic_num_clients = IEEE80211_128_AID;
#endif
    } else {
#ifdef QCA_LOWMEM_PLATFORM
        ic->ic_num_clients = IEEE80211_33_AID;
#else
        ic->ic_num_clients = ic->ic_def_num_clients;
#endif
    }

    ieee80211com_set_cap(ic,
                        IEEE80211_C_WEP
                        | IEEE80211_C_AES
                        | IEEE80211_C_AES_CCM
                        | IEEE80211_C_CKIP
                        | IEEE80211_C_TKIP
                        | IEEE80211_C_TKIPMIC
#if ATH_SUPPORT_WAPI
                        | IEEE80211_C_WAPI
#endif
                        | IEEE80211_C_WME);

    ieee80211com_clear_cap_ext(ic,-1);
    ieee80211com_set_cap_ext(ic,
                            IEEE80211_CEXT_PERF_PWR_OFLD
                            | IEEE80211_CEXT_MULTICHAN
                            | IEEE80211_CEXT_11AC);
    /* ic_flags */
    ieee80211com_clear_flags(ic,-1);
    IEEE80211_ENABLE_SHPREAMBLE(ic);
    IEEE80211_ENABLE_SHSLOT(ic);
    IEEE80211_ENABLE_DATAPAD(ic);
    ieee80211com_clear_flags(ic, IEEE80211_F_COEXT_DISABLE);

    /* ic_flags_ext */
    ieee80211com_clear_flags_ext(ic,-1);
    IEEE80211_ENABLE_COUNTRYIE(ic);
    IEEE80211_UAPSD_ENABLE(ic);
    IEEE80211_FEXT_MARKDFS_ENABLE(ic);
    IEEE80211_SET_DESIRED_COUNTRY(ic);
    IEEE80211_ENABLE_11D(ic);
    IEEE80211_ENABLE_AMPDU(ic);
    IEEE80211_ENABLE_IGNORE_11D_BEACON(ic);
    /* ic_flags_ext2 */
    ic->ic_flags_ext2 &= ~IEEE80211_FEXT2_CSA_WAIT;

    ieee80211_ic_wep_tkip_htrate_clear(ic);
    ieee80211_ic_non_ht_ap_clear(ic);
    ieee80211_ic_block_dfschan_clear(ic);
    ieee80211_ic_doth_set(ic);             /* Default 11h to start enabled  */
    ieee80211_ic_off_channel_support_clear(ic);
    ieee80211_ic_ht20Adhoc_clear(ic);
    ieee80211_ic_ht40Adhoc_clear(ic);
    ieee80211_ic_htAdhocAggr_clear(ic);
    ieee80211_ic_disallowAutoCCchange_clear(ic);
    ieee80211_ic_p2pDevEnable_clear(ic);
    ieee80211_ic_ignoreDynamicHalt_clear(ic);
    ieee80211_ic_override_proberesp_ie_set(ic);
    ieee80211_ic_wnm_set(ic);
    ieee80211_ic_2g_csa_clear(ic);
    ieee80211_ic_offchanscan_clear(ic);
    ieee80211_ic_enh_ind_rpt_clear(ic);
    ieee80211com_clear_htflags(ic, IEEE80211_HTF_SHORTGI40 | IEEE80211_HTF_SHORTGI20);
    ieee80211com_set_htflags(ic, IEEE80211_HTF_SHORTGI40 | IEEE80211_HTF_SHORTGI20);
    ieee80211com_set_maxampdu(ic, IEEE80211_HTCAP_MAXRXAMPDU_65536);
    /*Default wradar channel filtering is disabled  */
    ic->ic_no_weather_radar_chan = 0;
    ic->ic_intval = IEEE80211_BINTVAL_DEFAULT; /* beacon interval */
    ic->ic_lintval = 1;         /* listen interval */
    ic->ic_lintval_assoc = IEEE80211_LINTVAL_MAX; /* listen interval to use in association */
    ic->ic_bmisstimeout = IEEE80211_BMISS_LIMIT * ic->ic_intval;
    ic->ic_txpowlimit = IEEE80211_TXPOWER_MAX;
    ic->ic_multiDomainEnabled = 0;

#if UMAC_SUPPORT_ACFG
    ic->ic_diag_enable=0;
#endif
    ic->ic_chan_stats_th = IEEE80211_CHAN_STATS_THRESOLD;
    ic->ic_strict_pscan_enable = 0;
    ic->ic_min_rssi_enable = false;
    ic->ic_min_rssi = 0;
    ic->ic_tx_next_ch = NULL;
    ic->ic_strict_doth = 0;
    ic->ic_non_doth_sta_cnt = 0;
#if DBDC_REPEATER_SUPPORT
    ic->fast_lane = 0;
    ic->fast_lane_ic = NULL;
    ic->ic_extender_connection = 0;
    OS_MEMZERO(ic->ic_preferred_bssid, IEEE80211_ADDR_LEN);
    ic->ic_extender_connection = 0;
#endif
    /* Initialize default to the first available channel, AUTO mode */
    ic->ic_curchan = ieee80211_ath_get_channel(ic, 0);
    ic->ic_auth_tx_xretry = 0;
    ic->no_chans_available = 0;

    /* Reset CSA related variables. */
    qdf_mem_zero(ic->ic_csa_vdev_ids, WLAN_UMAC_PDEV_MAX_VDEVS);
    ic->ic_csa_num_vdevs = 0;
    ic->ic_nr_share_radio_flag = 0;
    ic->ic_nr_share_enable = 0;
    ic->ic_num_csa_bcn_tmp_sent = 0;
    ic->schedule_bringup_vaps = false;
    ic->ic_ht40intol_scan_running = 0;
    ic->ic_user_coext_disable = 0;
#if ATH_SUPPORT_DFS && QCA_DFS_NOL_VAP_RESTART
    ic->ic_pause_stavap_scan = 0;
#endif
#if QCN_ESP_IE
    ic->ic_esp_air_time_fraction = 0;
    ic->ic_esp_ppdu_duration = 0;
    ic->ic_esp_ba_window = 0;
    ic->ic_esp_periodicity = 0;
    ic->ic_fw_esp_air_time = 0;
    ic->ic_esp_flag = 0;
#endif /* QCN_ESP_IE */
    ic->ru26_tolerant = true;
}

void global_ic_list_reset_params(struct ieee80211com *ic) {

#if DBDC_REPEATER_SUPPORT
    int i;
    struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(ic->ic_pdev_obj);
    GLOBAL_IC_LOCK_BH(ic->ic_global_list);
    ic->ic_global_list->dbdc_process_enable = 1;
    ic->ic_global_list->force_client_mcast_traffic = 0;

    ic->ic_global_list->delay_stavap_connection = cfg_get(psoc, CFG_TGT_DBDC_TIME_DELAY);
    ic->ic_global_list->num_stavaps_up = 0;
    ic->ic_global_list->is_dbdc_rootAP = 0;
    ic->ic_global_list->iface_mgr_up = 0;
    ic->ic_global_list->disconnect_timeout = 10;
    ic->ic_global_list->reconfiguration_timeout = 60;
    ic->ic_global_list->always_primary = 0;
    ic->ic_global_list->num_fast_lane_ic = 0;
    ic->ic_global_list->max_priority_stavap_up = NULL;
    ic->ic_global_list->num_rptr_clients = 0;
    ic->ic_global_list->same_ssid_support = 0;
    ic->ic_global_list->ap_preferrence = 0;
    ic->ic_global_list->extender_info = 0;
    ic->ic_global_list->rootap_access_downtime = 0;
    ic->ic_global_list->num_l2uf_retries = 0;
    for (i=0; i < MAX_RADIO_CNT; i++) {
       OS_MEMZERO(&ic->ic_global_list->preferred_list_stavap[i][0], IEEE80211_ADDR_LEN);
       OS_MEMZERO(&ic->ic_global_list->denied_list_apvap[i][0], IEEE80211_ADDR_LEN);
    }
    GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
#endif
}

void ieee80211_dfs_reset(struct ieee80211com *ic)
{
    struct wlan_objmgr_pdev *pdev = ic->ic_pdev_obj;

    if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
            QDF_STATUS_SUCCESS) {
        return;
    }
    mlme_dfs_stop(pdev);
    mlme_dfs_reset(pdev);
    wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
}
EXPORT_SYMBOL(ieee80211_dfs_reset);

int
wlan_vap_delete_stop_bss(struct ieee80211vap *vap, int stop)
{
    struct ieee80211com *ic = vap->iv_ic;
    uint32_t flags = 0;

    IEEE80211_COMM_LOCK_BH(ic);

    if (ieee80211_vap_deleted_is_clear(vap)) /* if not deleted then it is on the list */
        {
            TAILQ_REMOVE(&ic->ic_vaps, vap, iv_next);
            if (TAILQ_EMPTY(&ic->ic_vaps))      /* reset to supported mode */
                ic->ic_opmode = IEEE80211_M_STA;
            ieee80211_vap_deleted_set(vap); /* mark it as deleted */

            IEEE80211_COMM_UNLOCK_BH(ic);

            if (stop) {
                if (vap->iv_opmode == IEEE80211_M_MONITOR) {
                    flags = WLAN_MLME_STOP_BSS_F_FORCE_STOP_RESET |
                            WLAN_MLME_STOP_BSS_F_NO_RESET;
                } else {
                    flags = WLAN_MLME_STOP_BSS_F_SEND_DEAUTH        |
                            WLAN_MLME_STOP_BSS_F_CLEAR_ASSOC_STATE  |
                            WLAN_MLME_STOP_BSS_F_WAIT_RX_DONE       |
                            WLAN_MLME_STOP_BSS_F_NO_RESET;
                }
                wlan_mlme_stop_vdev(vap->vdev_obj, flags, WLAN_MLME_NOTIFY_OSIF);
            }
        } else {
        IEEE80211_COMM_UNLOCK_BH(ic);
    }

    return 0;
}

int
wlan_vap_delete_start(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;

    ieee80211_vap_pause_vdetach(ic,vap);

    return 0;
}

int
wlan_vap_delete_complete(struct ieee80211vap *vap)
{
#if UNIFIED_SMARTANTENNA
    struct ieee80211com *ic = vap->iv_ic;
    int nacvaps = 0;
    u_int32_t osif_get_num_active_vaps( wlan_dev_t  comhandle);
    QDF_STATUS status = QDF_STATUS_E_FAILURE;

    nacvaps = osif_get_num_active_vaps(ic);
    if (nacvaps == 0) {
        status = wlan_objmgr_vdev_try_get_ref(vap->vdev_obj, WLAN_SA_API_ID);
        if (QDF_IS_STATUS_ERROR(status)) {
            qdf_print("%s, %d unable to get reference", __func__, __LINE__);
        } else {
            wlan_sa_api_stop(ic->ic_pdev_obj, vap->vdev_obj, SMART_ANT_NEW_CONFIGURATION);
            wlan_objmgr_vdev_release_ref(vap->vdev_obj, WLAN_SA_API_ID);
        }
    }
#endif
    return 0;
}

int
wlan_vap_recover(wlan_if_t vap)
{
    wlan_vap_delete_start(vap);

    wlan_vap_delete_stop_bss(vap, NO_STOP);

    return 0;
}

int
wlan_vap_delete(wlan_if_t vap)
{
    wlan_vap_delete_start(vap);

    wlan_vap_delete_stop_bss(vap, DO_STOP);

    return 0;
}

static void wlan_soc_vap_count(struct wlan_objmgr_psoc *psoc, void *obj, void *args)
{
    int *vap_count = NULL;

    vap_count = (int *) args;
    (*vap_count)++;
}

void wlan_stop(struct ieee80211com *ic)
{
    struct wlan_objmgr_pdev *pdev = ic->ic_pdev_obj;
    int vap_count = 0;

    wlan_objmgr_iterate_obj_list(wlan_pdev_get_psoc(pdev), WLAN_VDEV_OP, wlan_soc_vap_count,
                                 &vap_count, 1, WLAN_MLME_NB_ID);

#if ATH_SUPPORT_DFS
    ieee80211_dfs_reset(ic);
#endif

    if(vap_count == 0) {
        ic->ic_stop(ic, 1);
    }

    ic_reset_params(ic);
    global_ic_list_reset_params(ic);
}


int wlan_vap_allocate_mac_addr(wlan_dev_t devhandle, u_int8_t *bssid)
{
    struct ieee80211com *ic = devhandle;
    return ic->ic_vap_alloc_macaddr(ic,bssid);
}

int wlan_vap_free_mac_addr(wlan_dev_t devhandle, u_int8_t *bssid)
{
    struct ieee80211com *ic = devhandle;
    return ic->ic_vap_free_macaddr(ic,bssid);
}
int
wlan_vap_register_event_handlers(wlan_if_t vaphandle,
                                 wlan_event_handler_table *evtable)
{
    return ieee80211_vap_register_events(vaphandle,evtable);
}
int
wlan_vap_register_mlme_event_handlers(wlan_if_t vaphandle,
                                 os_handle_t oshandle,
                                 wlan_mlme_event_handler_table *evtable)
{
    return ieee80211_vap_register_mlme_events(vaphandle, oshandle, evtable);
}

int
wlan_vap_unregister_mlme_event_handlers(wlan_if_t vaphandle,
                                 os_handle_t oshandle,
                                 wlan_mlme_event_handler_table *evtable)
{
    return ieee80211_vap_unregister_mlme_events(vaphandle, oshandle, evtable);
}

int
wlan_vap_register_misc_event_handlers(wlan_if_t vaphandle,
                                 os_handle_t oshandle,
                                 wlan_misc_event_handler_table *evtable)
{
    return ieee80211_vap_register_misc_events(vaphandle, oshandle, evtable);
}

int
wlan_vap_unregister_misc_event_handlers(wlan_if_t vaphandle,
                                 os_handle_t oshandle,
                                 wlan_misc_event_handler_table *evtable)
{
    return ieee80211_vap_unregister_misc_events(vaphandle, oshandle, evtable);
}

int
wlan_vap_register_ccx_event_handlers(wlan_if_t vaphandle,
                                 os_if_t osif,
                                 wlan_ccx_handler_table *evtable)
{
    return ieee80211_vap_register_ccx_events(vaphandle, osif, evtable);
}

os_if_t
wlan_vap_get_registered_handle(wlan_if_t vap)
{
    return (os_if_t)vap->iv_ifp;
}

void
wlan_vap_set_registered_handle(wlan_if_t vap, os_if_t osif)
{
    vap->iv_ifp = osif;
}

wlan_dev_t
wlan_vap_get_devhandle(wlan_if_t vap)
{
    return (wlan_dev_t)vap->iv_ic;
}

struct wlan_iter_func_arg {
    ieee80211_vap_iter_func iter_func;
    void  *iter_arg;
    u_int32_t iter_num_vap;
};

static INLINE void
wlan_iter_func(void *arg, struct ieee80211vap *vap, bool is_last_vap)
{
    struct wlan_iter_func_arg *params =  (struct wlan_iter_func_arg *) arg;
    if (params->iter_func) {
        params->iter_func(params->iter_arg,vap);
    }
}

static void wlan_vdev_iter_op(struct wlan_objmgr_pdev *pdev, void *obj, void *args)
{
    struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)obj;
    struct wlan_iter_func_arg *params = (struct wlan_iter_func_arg *)args;
    struct ieee80211vap *vap;

    vap = (wlan_if_t)wlan_vdev_get_mlme_ext_obj(vdev);

    if (vap == NULL)
       return;

    params->iter_num_vap++;
    params->iter_func(params->iter_arg, vap);
}

u_int32_t wlan_iterate_vap_list(wlan_dev_t ic,ieee80211_vap_iter_func iter_func,void *arg)
{
    struct wlan_iter_func_arg params;

    params.iter_func = iter_func;
    params.iter_arg = arg;
    params.iter_num_vap = 0;

    if (wlan_objmgr_pdev_try_get_ref(ic->ic_pdev_obj, WLAN_MLME_NB_ID) ==
                                     QDF_STATUS_SUCCESS) {
          wlan_objmgr_pdev_iterate_obj_list(ic->ic_pdev_obj, WLAN_VDEV_OP,
                                   wlan_vdev_iter_op, &params, 1,
                                   WLAN_MLME_NB_ID);
          wlan_objmgr_pdev_release_ref(ic->ic_pdev_obj, WLAN_MLME_NB_ID);
    }

    return params.iter_num_vap;
}

u_int32_t wlan_iterate_vap_list_lock(wlan_dev_t ic,ieee80211_vap_iter_func iter_func,void *arg)
{
    struct wlan_iter_func_arg params;

    params.iter_func = iter_func;
    params.iter_arg = arg;
    params.iter_num_vap = 0;

    if (wlan_objmgr_pdev_try_get_ref(ic->ic_pdev_obj, WLAN_MLME_NB_ID) ==
                                     QDF_STATUS_SUCCESS) {
          wlan_objmgr_pdev_iterate_obj_list(ic->ic_pdev_obj, WLAN_VDEV_OP,
                                   wlan_vdev_iter_op, &params, 0,
                                   WLAN_MLME_NB_ID);
          wlan_objmgr_pdev_release_ref(ic->ic_pdev_obj, WLAN_MLME_NB_ID);
    }

    return params.iter_num_vap;
}


enum ieee80211_opmode
wlan_vap_get_opmode(wlan_if_t vaphandle)
{
    return vaphandle->iv_opmode;
}

u_int8_t *
wlan_vap_get_macaddr(wlan_if_t vaphandle)
{
    return vaphandle->iv_myaddr;
}

u_int8_t *
wlan_vap_get_hw_macaddr(wlan_if_t vaphandle)
{
    return vaphandle->iv_my_hwaddr;
}

ieee80211_aplist_config_t
ieee80211_vap_get_aplist_config(struct ieee80211vap *vap)
{
    return vap->iv_aplist_config;
}

ieee80211_candidate_aplist_t
ieee80211_vap_get_aplist(struct ieee80211vap *vap)
{
    return vap->iv_candidate_aplist;
}

static void
ieee80211_reset_stats(struct ieee80211vap *vap, int reset_hw)
{
#if !ATH_SUPPORT_STATS_APONLY
    struct ieee80211com *ic = vap->iv_ic;
#endif

#if !ATH_SUPPORT_STATS_APONLY

    if (reset_hw) {
        OS_MEMZERO(&ic->ic_phy_stats[0],
                   sizeof(struct ieee80211_phy_stats) * IEEE80211_MODE_MAX);

        /* clear H/W phy counters */
        ic->ic_clear_phystats(ic);
    }
#endif
}

#ifdef QCA_SUPPORT_CP_STATS
const struct ieee80211_mac_stats *
wlan_mac_stats_from_cp_stats(wlan_if_t vaphandle, int is_mcast,
                             struct ieee80211_mac_stats *mac_stats)
{
    struct vdev_ic_cp_stats vdev_cp_stats;
    struct vdev_80211_mac_stats *vdev_mac_stats;

    if (!mac_stats || !vaphandle) {
        qdf_print("Invalid input");
        return NULL;
    }

    if (wlan_ucfg_get_vdev_cp_stats(vaphandle->vdev_obj,
                                    &vdev_cp_stats) != 0) {
        qdf_print("Failed to fetch vdev cp stats");
        return NULL;
    }

    if (is_mcast) {
        vdev_mac_stats = &vdev_cp_stats.mcast_stats;
    } else {
        vdev_mac_stats = &vdev_cp_stats.ucast_stats;
    }

    mac_stats->ims_rx_badkeyid = vdev_mac_stats->cs_rx_badkeyid;
    mac_stats->ims_rx_decryptok = vdev_mac_stats->cs_rx_decryptok;
    mac_stats->ims_rx_wepfail = vdev_mac_stats->cs_rx_wepfail;
    mac_stats->ims_rx_tkipreplay = vdev_mac_stats->cs_rx_tkipreplay;
    mac_stats->ims_rx_tkipformat = vdev_mac_stats->cs_rx_tkipformat;
    mac_stats->ims_rx_tkipicv = vdev_mac_stats->cs_rx_tkipicv;
    mac_stats->ims_rx_ccmpreplay = vdev_mac_stats->cs_rx_ccmpreplay;
    mac_stats->ims_rx_ccmpformat = vdev_mac_stats->cs_rx_ccmpformat;
    mac_stats->ims_rx_ccmpmic = vdev_mac_stats->cs_rx_ccmpmic;
    mac_stats->ims_rx_wpireplay = vdev_mac_stats->cs_rx_wpireplay;
    mac_stats->ims_rx_wpimic = vdev_mac_stats->cs_rx_wpimic;
    mac_stats->ims_rx_countermeasure = vdev_mac_stats->cs_rx_countermeasure;
    mac_stats->ims_tx_mgmt = vdev_mac_stats->cs_tx_mgmt;
    mac_stats->ims_rx_mgmt = vdev_mac_stats->cs_rx_mgmt;
    mac_stats->ims_tx_discard = vdev_mac_stats->cs_tx_discard;
    mac_stats->ims_rx_discard = vdev_mac_stats->cs_rx_discard;
    return mac_stats;
}

int wlan_stats_from_cp_stats(wlan_if_t vaphandle,
                             struct ieee80211_stats *stats)
{
    struct vdev_ic_cp_stats vdev_cp_stats;

    if (!stats || !vaphandle) {
        qdf_print("Invalid input");
        return -EINVAL;
    }

    if (wlan_ucfg_get_vdev_cp_stats(vaphandle->vdev_obj,
                                    &vdev_cp_stats) != 0) {
        qdf_print("Failed to fetch vdev cp stats");
        return -EFAULT;
    }

    stats->is_rx_wrongbss = vdev_cp_stats.stats.cs_rx_wrongbss;
    stats->is_rx_wrongdir = vdev_cp_stats.stats.cs_rx_wrongdir;
    stats->is_rx_mcastecho = vdev_cp_stats.stats.cs_rx_mcast_echo;
    stats->is_rx_notassoc = vdev_cp_stats.stats.cs_rx_not_assoc;
    stats->is_rx_noprivacy = vdev_cp_stats.stats.cs_rx_noprivacy;
    stats->is_rx_mgtdiscard = vdev_cp_stats.stats.cs_rx_mgmt_discard;
    stats->is_rx_ctl = vdev_cp_stats.stats.cs_rx_ctl;
    stats->is_rx_rstoobig = vdev_cp_stats.stats.cs_rx_rs_too_big;
    stats->is_rx_elem_missing = vdev_cp_stats.stats.cs_rx_elem_missing;
    stats->is_rx_elem_toobig = vdev_cp_stats.stats.cs_rx_elem_too_big;
    stats->is_rx_badchan = vdev_cp_stats.stats.cs_rx_chan_err;
    stats->is_rx_nodealloc = vdev_cp_stats.stats.cs_rx_node_alloc;
    stats->is_rx_ssidmismatch =
                               vdev_cp_stats.stats.cs_rx_ssid_mismatch;
    stats->is_rx_auth_unsupported =
                               vdev_cp_stats.stats.cs_rx_auth_unsupported;
    stats->is_rx_auth_fail = vdev_cp_stats.stats.cs_rx_auth_fail;
    stats->is_rx_auth_countermeasures =
                           vdev_cp_stats.stats.cs_rx_auth_countermeasures;
    stats->is_rx_assoc_bss = vdev_cp_stats.stats.cs_rx_assoc_bss;
    stats->is_rx_assoc_notauth =
                           vdev_cp_stats.stats.cs_rx_assoc_notauth;
    stats->is_rx_assoc_capmismatch =
                           vdev_cp_stats.stats.cs_rx_assoc_cap_mismatch;
    stats->is_rx_assoc_norate = vdev_cp_stats.stats.cs_rx_assoc_norate;
    stats->is_rx_assoc_badwpaie =
                           vdev_cp_stats.stats.cs_rx_assoc_wpaie_err;
    stats->is_rx_action = vdev_cp_stats.stats.cs_rx_action;
    stats->is_rx_bad_auth = vdev_cp_stats.stats.cs_rx_auth_err;
    stats->is_tx_nodefkey = vdev_cp_stats.stats.cs_tx_nodefkey;
    stats->is_tx_noheadroom = vdev_cp_stats.stats.cs_tx_noheadroom;
    stats->is_rx_acl = vdev_cp_stats.stats.cs_rx_acl;
    stats->is_rx_nowds = vdev_cp_stats.stats.cs_rx_nowds;
    stats->is_tx_nonode = vdev_cp_stats.stats.cs_tx_nonode;
    stats->is_tx_badcipher = vdev_cp_stats.stats.cs_tx_cipher_err;
    stats->is_tx_not_ok = vdev_cp_stats.stats.cs_tx_not_ok;
    stats->tx_beacon_swba_cnt = vdev_cp_stats.stats.cs_tx_bcn_swba;
    stats->is_node_timeout = vdev_cp_stats.stats.cs_node_timeout;
    stats->is_crypto_nomem = vdev_cp_stats.stats.cs_crypto_nomem;
    stats->is_crypto_tkip = vdev_cp_stats.stats.cs_crypto_tkip;
    stats->is_crypto_tkipenmic =
                            vdev_cp_stats.stats.cs_crypto_tkipenmic;
    stats->is_crypto_tkipcm = vdev_cp_stats.stats.cs_crypto_tkipcm;
    stats->is_crypto_ccmp = vdev_cp_stats.stats.cs_crypto_ccmp;
    stats->is_crypto_wep = vdev_cp_stats.stats.cs_crypto_wep;
    stats->is_crypto_setkey_cipher =
                            vdev_cp_stats.stats.cs_crypto_setkey_cipher;
    stats->is_crypto_setkey_nokey =
                            vdev_cp_stats.stats.cs_crypto_setkey_nokey;
    stats->is_crypto_delkey = vdev_cp_stats.stats.cs_crypto_delkey;
    stats->is_crypto_badcipher =
                            vdev_cp_stats.stats.cs_crypto_cipher_err;
    stats->is_crypto_attachfail =
                            vdev_cp_stats.stats.cs_crypto_attach_fail;
    stats->is_crypto_swfallback =
                            vdev_cp_stats.stats.cs_crypto_swfallback;
    stats->is_crypto_keyfail = vdev_cp_stats.stats.cs_crypto_keyfail;
    stats->is_ibss_capmismatch = vdev_cp_stats.stats.cs_ibss_capmismatch;
    stats->is_ps_unassoc = vdev_cp_stats.stats.cs_ps_unassoc;
    stats->is_ps_badaid = vdev_cp_stats.stats.cs_ps_aid_err;
    stats->padding = vdev_cp_stats.stats.cs_padding;
    stats->total_num_offchan_tx_mgmt =
                                vdev_cp_stats.stats.cs_tx_offchan_mgmt;
    stats->total_num_offchan_tx_data =
                                vdev_cp_stats.stats.cs_tx_offchan_data;
    stats->num_offchan_tx_failed =
                                vdev_cp_stats.stats.cs_tx_offchan_fail;
    stats->total_invalid_macaddr_nodealloc_failcnt =
                          vdev_cp_stats.stats.cs_invalid_macaddr_nodealloc_fail;
    stats->tx_bcn_succ_cnt = vdev_cp_stats.stats.cs_tx_bcn_success;
    stats->tx_bcn_outage_cnt = vdev_cp_stats.stats.cs_tx_bcn_outage;
    stats->sta_xceed_rlim = vdev_cp_stats.stats.cs_sta_xceed_rlim;
    stats->sta_xceed_vlim = vdev_cp_stats.stats.cs_sta_xceed_vlim;
    stats->mlme_auth_attempt =
                          vdev_cp_stats.stats.cs_mlme_auth_attempt;
    stats->mlme_auth_success =
                          vdev_cp_stats.stats.cs_mlme_auth_success;
    stats->authorize_attempt =
                           vdev_cp_stats.stats.cs_authorize_attempt;
    stats->authorize_success =
                           vdev_cp_stats.stats.cs_authorize_success;
    return 0;
}
#endif

#if !ATH_SUPPORT_STATS_APONLY
const struct ieee80211_phy_stats *
wlan_phy_stats(wlan_dev_t devhandle, enum ieee80211_phymode mode)
{
    struct ieee80211com *ic = devhandle;

    KASSERT(mode != IEEE80211_MODE_AUTO && mode < IEEE80211_MODE_MAX,
            ("Invalid PHY mode\n"));

    if (ic->ic_update_phystats) {
    ic->ic_update_phystats(ic, mode);
	}
    return &ic->ic_phy_stats[mode];
}
#endif

systime_t ieee80211_get_last_data_timestamp(wlan_if_t vaphandle)
{
    return vaphandle->iv_lastdata;
}

systime_t ieee80211_get_directed_frame_timestamp(wlan_if_t vaphandle)
{
    /*
     * Now that we have an API it's a good opportunity to synchronize access
     * to this field.
     */
    wlan_vdev_update_last_traffic_indication(vaphandle->vdev_obj);
    return vaphandle->iv_last_directed_frame;
}

systime_t ieee80211_get_last_ap_frame_timestamp(wlan_if_t vaphandle)
{
    /*
     * Now that we have an API it's a good opportunity to synchronize access
     * to this field.
     */
    wlan_vdev_update_last_ap_frame(vaphandle->vdev_obj);
    return vaphandle->iv_last_ap_frame;
}

systime_t wlan_get_directed_frame_timestamp(wlan_if_t vaphandle)
{
    return ieee80211_get_directed_frame_timestamp(vaphandle);
}

systime_t wlan_get_last_ap_frame_timestamp(wlan_if_t vaphandle)
{
    return ieee80211_get_last_ap_frame_timestamp(vaphandle);
}

systime_t ieee80211_get_traffic_indication_timestamp(wlan_if_t vaphandle)
{
    return vaphandle->iv_last_traffic_indication;
}

systime_t wlan_get_traffic_indication_timestamp(wlan_if_t vaphandle)
{
    return ieee80211_get_traffic_indication_timestamp(vaphandle);
}

bool ieee80211_is_connected(wlan_if_t vaphandle)
{
   return (wlan_vdev_is_up(vaphandle->vdev_obj) == QDF_STATUS_SUCCESS);
}

bool wlan_is_connected(wlan_if_t vaphandle)
{
    return (wlan_vdev_is_up(vaphandle->vdev_obj) == QDF_STATUS_SUCCESS);
}
EXPORT_SYMBOL(wlan_is_connected);

static void ieee80211_get_active_vap_iter_func(void *arg, wlan_if_t vap)
{

    u_int16_t *active_vaps = (u_int16_t *) arg;

    if (wlan_vdev_chan_config_valid(vap->vdev_obj) == QDF_STATUS_SUCCESS) {
        ++(*active_vaps);
    }
}

u_int32_t  ieee80211_get_num_active_vaps(wlan_dev_t  comhandle) {

    u_int16_t num_active_vaps=0;
    wlan_iterate_vap_list(comhandle,ieee80211_get_active_vap_iter_func,(void *)&num_active_vaps);
    return num_active_vaps;

}

static void ieee80211_get_interop_phy_vap_iter_func(void *arg, wlan_if_t vap)
{
    uint16_t *interop_phy_vaps = (u_int16_t *) arg;

    if ((vap->iv_opmode == IEEE80211_M_HOSTAP) && (vap->iv_csa_interop_phy)) {
        ++(*interop_phy_vaps);
    }
}

uint16_t ieee80211_get_num_csa_interop_phy_vaps(wlan_dev_t  comhandle) {

    uint16_t csa_interop_phy_vaps = 0;
    wlan_iterate_vap_list(comhandle, ieee80211_get_interop_phy_vap_iter_func,
            (void *)&csa_interop_phy_vaps);

    return csa_interop_phy_vaps;
}

#if ATH_SUPPORT_WRAP
bool wlan_is_psta(wlan_if_t vaphandle)
{
    return (ieee80211_vap_psta_is_set(vaphandle));
}
EXPORT_SYMBOL(wlan_is_psta);

bool wlan_is_mpsta(wlan_if_t vaphandle)
{
    return (ieee80211_vap_mpsta_is_set(vaphandle));
}
EXPORT_SYMBOL(wlan_is_mpsta);

bool wlan_is_wrap(wlan_if_t vaphandle)
{
    return (ieee80211_vap_wrap_is_set(vaphandle));
}

static INLINE void ieee80211_wrap_iter_func(void *arg, wlan_if_t vap)
{
   u_int16_t *num_wraps = (u_int16_t *) arg;

   if (ieee80211_vap_wrap_is_set(vap))
       ++(*num_wraps);
}

int wlan_vap_get_num_wraps(wlan_dev_t  comhandle)
{
    u_int16_t num_wraps=0;
    wlan_iterate_vap_list(comhandle,ieee80211_wrap_iter_func,(void *)&num_wraps);
    return num_wraps;
}

#endif

int wlan_vap_get_bssid(wlan_if_t vaphandle, u_int8_t *bssid)
{
    /* need locking to prevent changing the iv_bss */
    IEEE80211_VAP_LOCK(vaphandle);
    if (vaphandle->iv_bss) {
        IEEE80211_ADDR_COPY(bssid, vaphandle->iv_bss->ni_bssid);
        IEEE80211_VAP_UNLOCK(vaphandle);
        return EOK;
    }
    IEEE80211_VAP_UNLOCK(vaphandle);
    return -EINVAL;
}

int wlan_reset_start(wlan_if_t vaphandle, ieee80211_reset_request *reset_req)
{
    struct ieee80211com *ic = vaphandle->iv_ic;

    /*
     * TBD: Flush data queues only for vaphandle if
     * IEEE80211_RESET_TYPE_DOT11_INTF is set.
     */
    return ic->ic_reset_start(ic, reset_req->no_flush);
}
#if ATH_SUPPORT_WRAP
int wlan_wrap_set_key(wlan_if_t vaphandle)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;

    return ic->ic_wrap_set_key(vap);
}

int wlan_wrap_del_key(wlan_if_t vaphandle)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;

    return ic->ic_wrap_del_key(vap);
}
#endif

static int
ieee80211_vap_reset(struct ieee80211vap *vap, ieee80211_reset_request *reset_req)
{
    struct ieee80211com *ic = vap->iv_ic;

    /* Cancel pending MLME requests */
    wlan_mlme_cancel(vap);

    /*
     * Reset node table include all nodes.
     * NB: pairwise keys will be deleted during node cleanup.
     */
    if (vap->iv_opmode == IEEE80211_M_STA)
        ieee80211_reset_bss(vap);
    else
        ieee80211_node_reset(vap->iv_bss);

    /* Reset aplist configuration parameters */
    ieee80211_aplist_config_init(ieee80211_vap_get_aplist_config(vap));

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    /* Reset RSN settings */
    ieee80211_rsn_reset(vap);
#endif
    /* Reset statistics */
    ieee80211_reset_stats(vap, reset_req->reset_hw);

    /* Reset some of the misc. vap settings */
    vap->iv_des_modecaps = (1 << IEEE80211_MODE_AUTO);
    vap->iv_des_nssid = 0;
    OS_MEMZERO(&vap->iv_des_ssid[0], (sizeof(ieee80211_ssid) * IEEE80211_SCAN_MAX_SSID));

    /* Because reset_start has graspped a mutex which chan_set
     *will also try to grasp, so don't call ieee80211_set_channel here.
     */
#if !ATH_RESET_SERIAL
    /* Reset some MIB variables if required */
    if (reset_req->set_default_mib) {
        /*
         * NB: Only IEEE80211_RESET_TYPE_DOT11_INTF can reset MIB variables
         */
        KASSERT(reset_req->type == IEEE80211_RESET_TYPE_DOT11_INTF, ("invalid reset request\n"));

        if (reset_req->reset_mac) {
            /* reset regdmn module */
            ieee80211_regdmn_reset(ic);
        }

        if (reset_req->reset_phy) {
            /* set the desired PHY mode to 11b */
            vap->iv_des_mode = reset_req->phy_mode;

            /* change to the default PHY mode if required */
            /* set wireless mode */
            ieee80211_setmode(ic, vap->iv_des_mode, vap->iv_opmode);

            /* set default channel */
            ASSERT(vap->iv_des_chan[vap->iv_des_mode] != IEEE80211_CHAN_ANYC);
            ieee80211_set_channel(ic, vap->iv_des_chan[vap->iv_des_mode]);
            vap->iv_bsschan = ic->ic_curchan;
        }
    }
#endif

    return 0;
}

struct ieee80211_vap_iter_reset_arg {
    ieee80211_reset_request *reset_req;
    int  err;
};

static INLINE void ieee80211_vap_iter_reset(void *arg, wlan_if_t vap, bool is_last_vap)
{
    struct ieee80211_vap_iter_reset_arg *params= (struct ieee80211_vap_iter_reset_arg *) arg;
    /*
     * In case iv_bss was not stopped.
     */
    if (vap->iv_opmode == IEEE80211_M_STA) {
        wlan_mlme_stop_bss(vap,
                           WLAN_MLME_STOP_BSS_F_FORCE_STOP_RESET |
                           WLAN_MLME_STOP_BSS_F_WAIT_RX_DONE);
    } else {
        wlan_mlme_stop_bss(vap,
                           WLAN_MLME_STOP_BSS_F_FORCE_STOP_RESET |
                           WLAN_MLME_STOP_BSS_F_WAIT_RX_DONE |
                           WLAN_MLME_STOP_BSS_F_NO_RESET);
    }

    params->err = ieee80211_vap_reset(vap, params->reset_req);
}

int
wlan_reset(wlan_if_t vaphandle, ieee80211_reset_request *reset_req)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    int err = 0;

    /* NB: must set H/W MAC address before chip reset */
    if (reset_req->reset_mac && IEEE80211_ADDR_IS_VALID(reset_req->macaddr) &&
        !IEEE80211_ADDR_EQ(reset_req->macaddr, ic->ic_myaddr)) {

        IEEE80211_ADDR_COPY(ic->ic_myaddr, reset_req->macaddr);
        ic->ic_set_macaddr(ic, reset_req->macaddr);
        IEEE80211_ADDR_COPY(vap->iv_myaddr, ic->ic_myaddr);
        /*
         * TBD: if OS tries to set mac addr when multiple VAP co-exist,
         * we need to notify other VAPs and the corresponding ports
         * so that the port owner can change source address!!
         */
    }

    /* reset UMAC software states */
    if (reset_req->type == IEEE80211_RESET_TYPE_DOT11_INTF) {
        /*
         * In case iv_bss was not stopped.
         */
        wlan_mlme_stop_bss(vap,
                           WLAN_MLME_STOP_BSS_F_FORCE_STOP_RESET |
                           WLAN_MLME_STOP_BSS_F_WAIT_RX_DONE);

        err = ieee80211_vap_reset(vap, reset_req);
    } else if (reset_req->type == IEEE80211_RESET_TYPE_DEVICE) {
        u_int32_t num_vaps;
        struct ieee80211_vap_iter_reset_arg params;
        params.err=0;
        params.reset_req = reset_req;
        ieee80211_iterate_vap_list_internal(ic,ieee80211_vap_iter_reset,((void *) &params),num_vaps);
        err = params.err;
    }

    /* TBD: Always reset the hardware? */
    err = ic->ic_reset(ic);
    if (err)
        return err;

    return err;
}

int wlan_reset_end(wlan_if_t vaphandle, ieee80211_reset_request *reset_req)
{
    struct ieee80211com *ic = vaphandle->iv_ic;
    int ret;

    ret = ic->ic_reset_end(ic, reset_req->no_flush);
#if ATH_RESET_SERIAL
    /* Reset some MIB variables if required */
    if (reset_req->set_default_mib) {
        struct ieee80211vap *vap = vaphandle;
        /*
         * NB: Only IEEE80211_RESET_TYPE_DOT11_INTF can reset MIB variables
         */
        KASSERT(reset_req->type == IEEE80211_RESET_TYPE_DOT11_INTF, ("invalid reset request\n"));

        if (reset_req->reset_mac) {
            /* reset regdmn module */
            ieee80211_regdmn_reset(ic);
        }

        if (reset_req->reset_phy) {
            /* set the desired PHY mode to 11b */
            vap->iv_des_mode = reset_req->phy_mode;

            /* change to the default PHY mode if required */
            /* set wireless mode */
            ieee80211_setmode(ic, vap->iv_des_mode, vap->iv_opmode);

            /* set default channel */
            ASSERT(vap->iv_des_chan[vap->iv_des_mode] != IEEE80211_CHAN_ANYC);
            ieee80211_set_channel(ic, vap->iv_des_chan[vap->iv_des_mode]);
            vap->iv_bsschan = ic->ic_curchan;
        }
    }
#endif

    return ret;

}

int wlan_getrssi(wlan_if_t vaphandle, wlan_rssi_info *rssi_info, u_int32_t flags)
{
    struct ieee80211vap   *vap = vaphandle;
    struct ieee80211_node *bss_node = NULL;
    int err = ENXIO;

    bss_node = ieee80211_try_ref_bss_node(vap);
    if (bss_node) {
        err =  wlan_node_getrssi(bss_node,rssi_info,flags);
        ieee80211_free_node(bss_node);
    }
    return err;
}

int wlan_send_probereq(
    wlan_if_t       vaphandle,
    const u_int8_t  *destination,
    const u_int8_t  *bssid,
    const u_int8_t  *ssid,
    const u_int32_t ssidlen,
    const void      *optie,
    const size_t    optielen)
{
    struct ieee80211vap     *vap = vaphandle;
    struct ieee80211_node   *bss_node = NULL;
    int                     err = ENXIO;

    bss_node = ieee80211_try_ref_bss_node(vap);
    if (bss_node) {
        err = ieee80211_send_probereq(bss_node,
                                      vaphandle->iv_myaddr,
                                      destination,
                                      bssid,
                                      ssid,
                                      ssidlen,
                                      optie,
                                      optielen);

        ieee80211_free_node(bss_node);
    }
    return err;
}

int wlan_get_txrate_info(wlan_if_t vaphandle, ieee80211_rate_info *rate_info)
{
    struct ieee80211vap     *vap = vaphandle;
    struct ieee80211_node   *bss_node = NULL;
    int err = ENXIO;

    bss_node = ieee80211_try_ref_bss_node(vap);
    if (bss_node) {
        err =  wlan_node_txrate_info(bss_node,rate_info);
        ieee80211_free_node(bss_node);
    }
    return err;
}

int wlan_vap_create_flags(wlan_if_t vaphandle)
{
    return vaphandle->iv_create_flags;
}


ieee80211_resmgr_vap_priv_t
ieee80211vap_get_resmgr(ieee80211_vap_t vap)
{
    return vap->iv_resmgr_priv;
}

void
ieee80211vap_set_resmgr(ieee80211_vap_t vap, ieee80211_resmgr_vap_priv_t resmgr_priv)
{
    vap->iv_resmgr_priv = resmgr_priv;
}

/**
 * @set vap beacon interval.
 * ARGS :
 *  ieee80211_vap_event_handler : vap event handler
 *  intval                      : beacon interval.
 * RETURNS:
 *  on success returns 0.
 */
int ieee80211_vap_set_beacon_interval(ieee80211_vap_t vap, u_int16_t intval)
{
  ieee80211_node_set_beacon_interval(vap->iv_bss,intval);
  return 0;
}

/**
 * @get vap beacon interval.
 * ARGS :
 *  ieee80211_vap_event_handler : vap event handler
 * RETURNS:
 *   returns beacon interval.
 */
u_int16_t ieee80211_vap_get_beacon_interval(ieee80211_vap_t vap)
{
  return ieee80211_node_get_beacon_interval(vap->iv_bss);
}


/*
 * @Set station tspec.
 * ARGS :
 *  wlan_if_t           : vap handle
 *  u_int_8             : value of tspec state
 * RETURNS:             : void.
 */
void wlan_set_tspecActive(wlan_if_t vaphandle, u_int8_t val)
{
    struct ieee80211com *ic = vaphandle->iv_ic;
    ieee80211_set_tspecActive(ic, val);
    wlan_vdev_set_tspecActive(vaphandle->vdev_obj, val);
}

/*
 * @Indicates whether station tspec is negotiated or not.
 * ARGS :
 *  wlan_if_t           : vap handle
 * RETURNS:             : value of tspec state.
 */
int wlan_is_tspecActive(wlan_if_t vaphandle)
{
    struct ieee80211com *ic = vaphandle->iv_ic;
    return ieee80211_is_tspecActive(ic);
}

u_int32_t wlan_get_tsf32(wlan_if_t vaphandle)
{
    struct ieee80211com *ic = vaphandle->iv_ic;
    return ieee80211_get_tsf32(ic);
}

int wlan_is_proxysta(wlan_if_t vaphandle)
{
     struct ieee80211vap     *vap = vaphandle;
     return vap->iv_proxySTA;
}

int wlan_set_proxysta(wlan_if_t vaphandle, int enable)
{
    struct ieee80211vap     *vap = vaphandle;
    return (vap->iv_set_proxysta(vap, enable));
}

int wlan_vap_get_tsf_offset(wlan_if_t vaphandle, u_int64_t *tsf_offset)
{
    *tsf_offset = vaphandle->iv_tsf_offset.offset;
    return EOK;
}

void wlan_deauth_all_stas(wlan_if_t vaphandle)
{
    struct ieee80211vap             *vap = vaphandle;
    struct ieee80211_mlme_priv      *mlme_priv;
    extern void sta_deauth (void *arg, struct ieee80211_node *ni);
	if ( vap == NULL ) {
		return;
	}
	mlme_priv = vap->iv_mlme_priv;

    switch(vap->iv_opmode) {
    case IEEE80211_M_HOSTAP:
            wlan_iterate_station_list(vap, sta_deauth, NULL);
        break;
    case IEEE80211_M_ANY:
    case IEEE80211_OPMODE_MAX:
    case IEEE80211_M_P2P_DEVICE:
    case IEEE80211_M_P2P_CLIENT:
    case IEEE80211_M_P2P_GO:
    case IEEE80211_M_WDS:
    case IEEE80211_M_MONITOR:
    case IEEE80211_M_AHDEMO:
    case IEEE80211_M_IBSS:
    case IEEE80211_M_STA:
    default:
       break;
    }
return;
}



#if ATH_WOW_OFFLOAD
int wlan_update_protocol_offload(wlan_if_t vaphandle)
{
    struct ieee80211_node *iv_bss = vaphandle->iv_bss;
    struct ieee80211_key *k = &iv_bss->ni_ucastkey;
    u_int32_t tid_num;

    if (iv_bss->ni_flags & IEEE80211_NODE_QOS) {
        /* Use TID 0 by default */
        tid_num = 0;
    }
    else {
        tid_num = IEEE80211_NON_QOS_SEQ;
    }

    vaphandle->iv_vap_wow_offload_info_get(vaphandle->iv_ic, &k->wk_keytsc, WOW_OFFLOAD_KEY_TSC);
    vaphandle->iv_vap_wow_offload_info_get(vaphandle->iv_ic, &iv_bss->ni_txseqs[tid_num], WOW_OFFLOAD_TX_SEQNUM);
    /* Update seq num in ATH layer as well. Not sure why, but seqnum
     * from UMAC layer are over-written with ATH layer seq num for HT
     * stations. */
    vaphandle->iv_vap_wow_offload_txseqnum_update(vaphandle->iv_ic, iv_bss, tid_num, iv_bss->ni_txseqs[tid_num]);
    return 0;
}

int wlan_vap_prep_wow_offload_info(wlan_if_t vaphandle)
{
    struct ieee80211_node *iv_bss = vaphandle->iv_bss;
    struct ieee80211_key *k = &iv_bss->ni_ucastkey;
    struct wow_offload_misc_info wo_info;
    u_int32_t tid_num;

    wo_info.flags = 0;
    OS_MEMCPY(wo_info.myaddr, vaphandle->iv_myaddr, IEEE80211_ADDR_LEN);
    OS_MEMCPY(wo_info.bssid, iv_bss->ni_macaddr, IEEE80211_ADDR_LEN);
    if (iv_bss->ni_flags & IEEE80211_NODE_QOS) {
        /* Use TID 0 by default */
        tid_num = 0;
        wo_info.flags |= WOW_NODE_QOS;
    }
    else {
        tid_num = IEEE80211_NON_QOS_SEQ;
    }
    wo_info.tx_seqnum = iv_bss->ni_txseqs[tid_num];
    wo_info.ucast_keyix = k->wk_keyix;
    if (k->wk_cipher->ic_cipher == IEEE80211_CIPHER_AES_CCM) {
        wo_info.cipher = WOW_CIPHER_AES;
    }
    else if (k->wk_cipher->ic_cipher == IEEE80211_CIPHER_TKIP) {
        wo_info.cipher = WOW_CIPHER_TKIP;
    }
    else if (k->wk_cipher->ic_cipher == IEEE80211_CIPHER_WEP) {
        wo_info.cipher = WOW_CIPHER_WEP;
    }
    else {
        wo_info.cipher = WOW_CIPHER_NONE;
    }
    wo_info.keytsc = k->wk_keytsc;

    vaphandle->iv_vap_wow_offload_rekey_misc_info_set(vaphandle->iv_ic, &wo_info);

    return EOK;
}
#endif /* ATH_WOW_OFFLOAD */

int ieee80211_vendorie_vdetach(wlan_if_t vap)
{
    if(vap && vap->vie_handle != NULL) {
        wlan_mlme_remove_ie_list(vap->vie_handle);
        vap->vie_handle = NULL;
    }
    return 0;
}

struct ieee80211_iter_new_opmode_arg {
    struct ieee80211vap *vap;
    u_int8_t num_sta_vaps_active;
    u_int8_t num_ibss_vaps_active;
    u_int8_t num_ap_vaps_active;
    u_int8_t num_ap_vaps_dfswait;
    u_int8_t num_ap_sleep_vaps_active; /* ap vaps that can sleep (P2P GO) */
    u_int8_t num_sta_nobeacon_vaps_active; /* sta vap for repeater application */
    u_int8_t num_btamp_vaps_active;
    bool     vap_active;
};


static void ieee80211_vap_iter_new_opmode(void *arg, wlan_if_t vap)
{
    struct ieee80211_iter_new_opmode_arg *params = (struct ieee80211_iter_new_opmode_arg *) arg;
    bool vap_active = false;

        if ( vap == NULL ) {
            return;
        }

        if (vap != params->vap) {
            if (wlan_vdev_mlme_is_active(vap->vdev_obj) == QDF_STATUS_SUCCESS) {
                vap_active = true;
            }
        } else {
            vap_active = params->vap_active;
        }
        if (vap_active) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "%s: vap %pK tmpvap %pK opmode %d\n", __func__,
                        params->vap, vap, ieee80211vap_get_opmode(vap));

            switch(ieee80211vap_get_opmode(vap)) {
            case IEEE80211_M_HOSTAP:
            case IEEE80211_M_BTAMP:
                if (ieee80211_vap_cansleep_is_set(vap)) {
                   params->num_ap_sleep_vaps_active++;
                } else {
                   if (ieee80211vap_get_opmode(vap) == IEEE80211_M_BTAMP) {
                       params->num_btamp_vaps_active++;
                   }
                }
                if(ieee80211_vap_dfswait_is_set(vap)){
                    params->num_ap_vaps_dfswait++;
                } else {
                   params->num_ap_vaps_active++;
                }
                break;
            case IEEE80211_M_IBSS:
                params->num_ibss_vaps_active++;
                break;

            case IEEE80211_M_STA:
                if (vap->iv_create_flags & IEEE80211_NO_STABEACONS) {
                    params->num_sta_nobeacon_vaps_active++;
                }
                else {
                    params->num_sta_vaps_active++;
                }
                break;

            default:
                break;

            }
        }
}

/*
 * determine the new opmode whhen one of the vaps
 * changes its state.
 */
enum ieee80211_opmode
ieee80211_new_opmode(struct ieee80211vap *vap, bool vap_active)
{
    struct ieee80211com *ic = vap->iv_ic;
    enum ieee80211_opmode opmode;
    struct ieee80211_iter_new_opmode_arg params;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "%s: active state %d\n", __func__, vap_active);

    params.num_sta_vaps_active = params.num_ibss_vaps_active = 0;
    params.num_ap_vaps_active = params.num_ap_sleep_vaps_active = 0;
    params.num_ap_vaps_dfswait = 0;
    params.num_sta_nobeacon_vaps_active = 0;
    params.vap = vap;
    params.vap_active = vap_active;
    wlan_iterate_vap_list(ic,ieee80211_vap_iter_new_opmode,(void *) &params);
    /*
     * we cant support all 3 vap types active at the same time.
     */
    ASSERT(!(params.num_ap_vaps_active && params.num_sta_vaps_active && params.num_ibss_vaps_active));

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "%s: ibss %d, ap %d, sta %d sta_nobeacon %d \n", __func__, params.num_ibss_vaps_active, params.num_ap_vaps_active, params.num_sta_vaps_active, params.num_sta_nobeacon_vaps_active);


    if (params.num_ibss_vaps_active) {
        opmode = IEEE80211_M_IBSS;
        return opmode;
    }
    if (params.num_ap_vaps_active) {
        opmode = IEEE80211_M_HOSTAP;
        return opmode;
    }
    if (params.num_ap_vaps_dfswait) {
        opmode = IEEE80211_M_HOSTAP;
        return opmode;
    }
    if (params.num_sta_nobeacon_vaps_active) {
        opmode = IEEE80211_M_HOSTAP;
        return opmode;
    }
    opmode = ieee80211vap_get_opmode(vap);
    if ((opmode != IEEE80211_M_MONITOR) && (params.num_sta_vaps_active ||
                                            params.num_ap_sleep_vaps_active)) {
        opmode = IEEE80211_M_STA;
    }
    return opmode;
}

enum ieee80211_cwm_width ieee80211_get_vap_max_chwidth (struct ieee80211vap *vap)
{
    enum ieee80211_phymode phymode = vap->iv_des_mode;
    enum ieee80211_cwm_width width = IEEE80211_CWM_WIDTHINVALID;

    if (phymode == IEEE80211_MODE_AUTO) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG,
            "%s: VAP(0x%pK) dest Phymode is AUTO\n", __func__, vap);
        return IEEE80211_CWM_WIDTHINVALID;
    }

    switch (phymode) {
        case IEEE80211_MODE_11A:
        case IEEE80211_MODE_11B:
        case IEEE80211_MODE_11G:
        case IEEE80211_MODE_FH:
        case IEEE80211_MODE_TURBO_A:
        case IEEE80211_MODE_TURBO_G:
        case IEEE80211_MODE_11NA_HT20:
        case IEEE80211_MODE_11NG_HT20:
        case IEEE80211_MODE_11AC_VHT20:
        case IEEE80211_MODE_11AXA_HE20:
        case IEEE80211_MODE_11AXG_HE20:
            {
                width = IEEE80211_CWM_WIDTH20;
                break;
            }
        case IEEE80211_MODE_11NA_HT40PLUS:
        case IEEE80211_MODE_11NA_HT40MINUS:
        case IEEE80211_MODE_11NG_HT40PLUS:
        case IEEE80211_MODE_11NG_HT40MINUS:
        case IEEE80211_MODE_11NA_HT40:
        case IEEE80211_MODE_11NG_HT40:
        case IEEE80211_MODE_11AC_VHT40PLUS:
        case IEEE80211_MODE_11AC_VHT40MINUS:
        case IEEE80211_MODE_11AC_VHT40:
        case IEEE80211_MODE_11AXA_HE40PLUS:
        case IEEE80211_MODE_11AXA_HE40MINUS:
        case IEEE80211_MODE_11AXG_HE40PLUS:
        case IEEE80211_MODE_11AXG_HE40MINUS:
        case IEEE80211_MODE_11AXA_HE40:
        case IEEE80211_MODE_11AXG_HE40:
            {
                width = IEEE80211_CWM_WIDTH40;
                break;
            }
        case IEEE80211_MODE_11AC_VHT80:
        case IEEE80211_MODE_11AXA_HE80:
            {
                width = IEEE80211_CWM_WIDTH80;
                break;
            }
        case IEEE80211_MODE_11AC_VHT160:
        case IEEE80211_MODE_11AXA_HE160:
            {
                width = IEEE80211_CWM_WIDTH160;
                break;
            }
        case IEEE80211_MODE_11AC_VHT80_80:
        case IEEE80211_MODE_11AXA_HE80_80:
            {
                width = IEEE80211_CWM_WIDTH80_80;
                break;
            }
        default:
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG,
                "%s: Invalid phymode received: %d\n", __func__, phymode);
            break;
    }

    return width;
}

/*
 * Upon some parameter changes, vap will be restartd, then fw will reset some paramters to default.
 * This function will restore the needed parameters.
*/
void wlan_restore_vap_params(wlan_if_t vap)
{
    uint32_t bcn_rate;
    uint32_t mgmt_rate;

    if (IEEE80211_M_HOSTAP == vap->iv_opmode) {
        wlan_util_vdev_mlme_get_param(vap->vdev_mlme,
                WLAN_MLME_CFG_TX_MGMT_RATE, &mgmt_rate);
        wlan_util_vdev_mlme_get_param(vap->vdev_mlme,
                WLAN_MLME_CFG_BCN_TX_RATE, &bcn_rate);
        /* If rates set, use previous rates */
        /* After vdev restart, fw will reset rates to default */
        if(mgmt_rate){
            wlan_set_param(vap, IEEE80211_MGMT_RATE, mgmt_rate);
        }
        if (vap->iv_disabled_legacy_rate_set) {
            wlan_set_param(vap, IEEE80211_RTSCTS_RATE, mgmt_rate);
        }
        if(vap->iv_bcast_fixedrate){
            wlan_set_param(vap, IEEE80211_BCAST_RATE, vap->iv_bcast_fixedrate);
        }
        if(vap->iv_mcast_fixedrate){
            wlan_set_param(vap, IEEE80211_MCAST_RATE, vap->iv_mcast_fixedrate);
        }

        if(bcn_rate) {
            wlan_set_param(vap, IEEE80211_BEACON_RATE_FOR_VAP, bcn_rate);
        }
    }
}

#if DBDC_REPEATER_SUPPORT
void
wlan_update_radio_priorities(struct ieee80211com *ic)
{
    struct ieee80211com *tmp_ic = NULL;
    struct ieee80211com *primary_fast_lane_ic = NULL;
    struct ieee80211com *fast_lane_ic = NULL;
    struct ieee80211vap *tmpvap = NULL, *maxpriority_stavap_up = NULL;
    int i=0,k=2;
    int max_radio_priority = MAX_RADIO_CNT+1;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    struct ol_ath_softc_net80211 *scn = NULL;
#endif

    for (i=0; i < MAX_RADIO_CNT; i++) {
        GLOBAL_IC_LOCK_BH(ic->ic_global_list);
        tmp_ic = ic->ic_global_list->global_ic[i];
        GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
        if (tmp_ic) {
            spin_lock(&tmp_ic->ic_lock);
            if (tmp_ic->ic_primary_radio) {
                tmp_ic->ic_radio_priority = 1;
                /*update primary radio fast lane ic*/
                primary_fast_lane_ic = tmp_ic->fast_lane_ic;
            } else {
                tmp_ic->ic_radio_priority = 0;
            }
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
            scn = OL_ATH_SOFTC_NET80211(tmp_ic);
            if (tmp_ic->nss_vops) {
                tmp_ic->nss_vops->ic_osif_nss_store_other_pdev_stavap(tmp_ic);
                tmp_ic->nss_vops->ic_osif_nss_enable_dbdc_process(tmp_ic, tmp_ic->ic_global_list->dbdc_process_enable);
            }
#endif

            spin_unlock(&tmp_ic->ic_lock);
        }
    }
    for (i=0; i < MAX_RADIO_CNT; i++) {
        GLOBAL_IC_LOCK_BH(ic->ic_global_list);
        tmp_ic = ic->ic_global_list->global_ic[i];
        GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
        if (tmp_ic && (tmp_ic->ic_radio_priority == 0)) {
            if (tmp_ic == primary_fast_lane_ic) {
                spin_lock(&tmp_ic->ic_lock);
                tmp_ic->ic_radio_priority = 1;
                spin_unlock(&tmp_ic->ic_lock);
            } else {
                spin_lock(&tmp_ic->ic_lock);
                tmp_ic->ic_radio_priority = k;
                spin_unlock(&tmp_ic->ic_lock);

                /*update same priority for fast lane ic*/
                fast_lane_ic = tmp_ic->fast_lane_ic;
                if (fast_lane_ic) {
                    spin_lock(&fast_lane_ic->ic_lock);
                    fast_lane_ic->ic_radio_priority = k;
                    spin_unlock(&fast_lane_ic->ic_lock);
                }
                k++;
            }
        }
       if (tmp_ic) {
          qdf_print("radio mac:%s priority:%d",ether_sprintf(tmp_ic->ic_my_hwaddr),tmp_ic->ic_radio_priority)    ;
          if (max_radio_priority > tmp_ic->ic_radio_priority) {
              if (tmp_ic->ic_sta_vap &&
                  (wlan_vdev_is_up(((struct ieee80211vap *)tmp_ic->ic_sta_vap)->vdev_obj) ==
                                       QDF_STATUS_SUCCESS)) {
                  tmpvap = tmp_ic->ic_sta_vap;
                  maxpriority_stavap_up = tmp_ic->ic_sta_vap;
                  max_radio_priority = tmp_ic->ic_radio_priority;
               }
           }
       }
    }
    GLOBAL_IC_LOCK_BH(ic->ic_global_list);
    if (maxpriority_stavap_up && (maxpriority_stavap_up != ic->ic_global_list->max_priority_stavap_up)) {
       ic->ic_global_list->max_priority_stavap_up = maxpriority_stavap_up;
    }
    GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
    return;
}
#endif

bool ieee80211_is_vap_state_stopping (struct ieee80211vap *vap)
{
    return (wlan_vdev_is_going_down(vap->vdev_obj) == QDF_STATUS_SUCCESS);
}

bool
ieee80211_vap_is_connecting(wlan_if_t vap)
{
    osif_dev *osifp;

    if (!vap || !vap->iv_ifp) {
        qdf_print("%s: vap: 0x%pK  ", __func__, vap);
        return false;
    }

    if (vap->iv_opmode != IEEE80211_M_STA) {
        return false;
    }

    osifp = (osif_dev *)vap->iv_ifp;

    return wlan_connection_sm_is_connecting(osifp->sm_handle);
}

bool
ieee80211_vap_is_connected(wlan_if_t vap)
{
    osif_dev *osifp;

    if (!vap || !vap->iv_ifp) {
        qdf_print("%s: vap: 0x%pK ", __func__, vap);
        return false;
    }

    if (vap->iv_opmode != IEEE80211_M_STA) {
        return false;
    }

    osifp = (osif_dev *)vap->iv_ifp;
    return wlan_connection_sm_is_connected(osifp->sm_handle);
}

inline bool
wlan_vap_delete_in_progress(struct ieee80211vap *vap)
{
    osif_dev *osifp = NULL;

    if (!vap)
	return true;

    osifp = (osif_dev *)vap->iv_ifp;
    if (!osifp) {
	return true;
    } else {
	return osifp->is_delete_in_progress;
    }
}

static void find_ap_mode_vap(void *arg, struct ieee80211vap *vap)
{
    ieee80211_vap_t *ptr_apvap=(ieee80211_vap_t *)arg;

    if (IEEE80211_M_HOSTAP == vap->iv_opmode) {
        *ptr_apvap = vap;
    }

    return;
}

struct ieee80211vap *
wlan_get_ap_mode_vap (struct ieee80211com *ic) {

    ieee80211_vap_t vap = NULL;

    wlan_iterate_vap_list(ic, find_ap_mode_vap ,(void *)&vap );

    return vap;

}

static void ieee80211_roam_initparams(wlan_if_t vap)
{
    vap->iv_roam.iv_roaming = 0;
    vap->iv_roam.iv_ft_enable = 0;
    vap->iv_roam.iv_ft_roam = 0;
    vap->iv_roam.iv_wait_for_ftie_update = 0;
    vap->iv_roam.iv_roam_disassoc = 0;
    qdf_mem_zero(vap->iv_roam.iv_mdie, MD_IE_LEN);
    vap->iv_roam.iv_ft_ie.ie = NULL;
    vap->iv_roam.iv_ft_ie.length = 0;
    vap->iv_roam.iv_ft_params.fties = NULL;
    vap->iv_roam.iv_ft_params.fties_len = 0;
    qdf_mem_zero(vap->iv_roam.iv_ft_params.target_ap, IEEE80211_ADDR_LEN);

    return;
}


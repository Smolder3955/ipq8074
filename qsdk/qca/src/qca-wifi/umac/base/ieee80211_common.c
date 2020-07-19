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
 */

#include <ieee80211_var.h>
#include <ieee80211_channel.h>
#include <ieee80211_rateset.h>
#include <ieee80211_config.h>
#include <ieee80211_tsftimer.h>
#include <ieee80211_notify_tx_bcn.h>
#include <ieee80211P2P_api.h>
#include <ieee80211_wnm_proto.h>
#include <ieee80211_vi_dbg.h>
#include <ieee80211_bsscolor.h>
#include "if_athvar.h"
#include "ol_if_athvar.h"
#include <qdf_lock.h>
#include <ieee80211_mlme_dfs_dispatcher.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_mlme_dp_dispatcher.h>
#include <wlan_utility.h>
#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_utils_api.h>
#endif

#include <target_if.h>
#include <init_deinit_lmac.h>

#include <wlan_vdev_mlme.h>
/* Support for runtime pktlog enable/disable */
unsigned int enable_pktlog_support = 1; /*Enable By Default*/
module_param(enable_pktlog_support, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(enable_pktlog_support,
        "Runtime pktlog enable/disable Support");

#if ACFG_NETLINK_TX
extern int acfg_attach(struct ieee80211com *ic);
extern void acfg_detach(struct ieee80211com *ic);
#else
uint32_t acfg_event_workqueue_init(osdev_t osdev);
#endif

#if UMAC_SUPPORT_ACFG
extern int acfg_diag_attach(struct ieee80211com *ic);
extern int acfg_diag_detach(struct ieee80211com *ic);
#endif


int module_init_wlan(void);
void module_exit_wlan(void);


void print_vap_msg(struct ieee80211vap *vap, unsigned category, const char *fmt, ...)
{
     va_list ap;
     va_start(ap, fmt);
     if (vap) {
        asf_vprint_category(&vap->iv_print, category, fmt, ap);
     } else {
        qdf_vprint(fmt, ap);
     }
     va_end(ap);
}

void print_vap_verbose_msg(struct ieee80211vap *vap, unsigned verbose, unsigned category, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    if (vap) {
        asf_vprint(&vap->iv_print, category, verbose, fmt, args);
    } else {
        qdf_vprint(fmt, args);
    }
    va_end(args);
}

/**
* ASF print support function to print based on category for vap print control object
* @param vap - object of struct ieee80211vap in which asf print control object is declared
* @param category - category of the print message
*/
void IEEE80211_DPRINTF(struct ieee80211vap *vap, unsigned category, const char *fmt, ...)
{
     char                   tmp_buf[OS_TEMP_BUF_SIZE], *tmp;
     va_list                ap;
     struct ieee80211com    *ic = NULL;

     if ((vap) && ieee80211_msg(vap, category)) {
         ic = vap->iv_ic;
         tmp = tmp_buf + snprintf(tmp_buf,OS_TEMP_BUF_SIZE, "[%s] vap-%d(%s):",
                             msg_type_to_str(category), vap->iv_unit, vap->iv_netdev_name);
#if DBG_LVL_MAC_FILTERING
        if (!vap->iv_print.dbgLVLmac_on) {
#endif
             va_start(ap, fmt);
             vsnprintf (tmp,(OS_TEMP_BUF_SIZE - (tmp - tmp_buf)), fmt, ap);
             va_end(ap);
             print_vap_msg(vap, category, (const char *)tmp_buf, ap);
             ic->ic_log_text(ic,tmp_buf);
             OS_LOG_DBGPRINT("%s\n", tmp_buf);
#if DBG_LVL_MAC_FILTERING
        }
#endif
    }

}


/**
* ASF print support function to print based on category for vap print control object
* @param vap - object of struct ieee80211vap in which asf print control object is declared
* @param verbose - verbose level of the print message
* @param category - category of the print message
*/
void IEEE80211_DPRINTF_VB(struct ieee80211vap *vap, unsigned verbose, unsigned category, const char *fmt, ...)
{
     char                   tmp_buf[OS_TEMP_BUF_SIZE], *tmp;
     va_list                ap;
     struct ieee80211com    *ic = NULL;

     if ((vap) && (verbose <= vap->iv_print.verb_threshold) && ieee80211_msg(vap, category)) {
         ic = vap->iv_ic;
         tmp = tmp_buf + snprintf(tmp_buf,OS_TEMP_BUF_SIZE, "[%s] vap-%d(%s):",
                             msg_type_to_str(category), vap->iv_unit, vap->iv_netdev_name);
         va_start(ap, fmt);
         vsnprintf (tmp,(OS_TEMP_BUF_SIZE - (tmp - tmp_buf)), fmt, ap);
         va_end(ap);
         print_vap_verbose_msg(vap, verbose, category, (const char *)tmp_buf, ap);
         ic->ic_log_text(ic,tmp_buf);
         OS_LOG_DBGPRINT("%s\n", tmp_buf);
    }

}

/**
* ASF print support function tp print based on category for ic print control object
* @param ic - object of struct ieee80211com in which asf print control object is declared
* @param category - category of the print message
*/
void IEEE80211_DPRINTF_IC_CATEGORY(struct ieee80211com *ic, unsigned category, const char *fmt, ...)
{
    va_list args;

    if ( (ic) && ieee80211_msg_ic(ic, category)) {
        va_start(args, fmt);
        if (ic) {
            asf_vprint_category(&ic->ic_print, category, fmt, args);
        } else {
            qdf_vprint(fmt, args);
        }
        va_end(args);
    }

}

/**
* ASF print support function tp print based on category and verbose for ic print control object
* @param ic - object of struct ieee80211com in which asf print control object is declared
* @param verbose - verbose level of the print message
* @param category - category of the print message
*/
void IEEE80211_DPRINTF_IC(struct ieee80211com *ic, unsigned verbose, unsigned category, const char *fmt, ...)
{
    va_list args;

    if ((ic) && (verbose <= ic->ic_print.verb_threshold) && ieee80211_msg_ic(ic, category)) {
        va_start(args, fmt);
        if (ic) {
            asf_vprint(&(ic)->ic_print, category, verbose, fmt, args);
        } else {
            qdf_vprint(fmt, args);
        }
        va_end(args);
    }

}

static void ieee80211_vap_iter_mlme_inact_timeout(void *arg, struct ieee80211vap *vap)
{
    mlme_inact_timeout(vap);
}

void ieee80211_vap_mlme_inact_erp_timeout(struct ieee80211com *ic)
{
    wlan_iterate_vap_list(ic, ieee80211_vap_iter_mlme_inact_timeout, NULL);
}
/*
 * Per-ieee80211com inactivity timer callback.
 * used for checking any kind of inactivity in the
 * COM device.
 */
static OS_TIMER_FUNC(ieee80211_inact_timeout)
{
    struct ieee80211com *ic;

    OS_GET_TIMER_ARG(ic, struct ieee80211com *);
    ieee80211_timeout_stations(&ic->ic_sta);
    wlan_pdev_timeout_fragments(ic->ic_pdev_obj, IEEE80211_FRAG_TIMEOUT * 1000);
    wlan_iterate_vap_list(ic, ieee80211_vap_iter_mlme_inact_timeout, NULL);
    if (ic->ic_initialized == 1) {
        OS_SET_TIMER(&ic->ic_inact_timer, IEEE80211_INACT_WAIT * 1000);
    }
}

static OS_TIMER_FUNC(ieee80211_obss_nb_ru_tolerence_timeout)
{
    struct ieee80211com *ic;

    OS_GET_TIMER_ARG(ic, struct ieee80211com *);

    if (ic->ic_initialized == 1) {
        ic->ru26_tolerant = true;
        ic->ic_set_ru26_tolerant(ic, ic->ru26_tolerant);
    }
}

static os_timer_func(ieee80211_csa_max_rx_wait_timer)
{
    struct ieee80211com *ic;

    OS_GET_TIMER_ARG(ic, struct ieee80211com *);

    if (!ic)
        return;

    /* unsubscribe to ppdu stats */
    if (ic->ic_subscribe_csa_interop_phy) {
        ic->ic_subscribe_csa_interop_phy(ic, false);
    }
}

#if UMAC_SUPPORT_WNM
static OS_TIMER_FUNC(ieee80211_bssload_timeout)
{
    struct ieee80211com *ic;
    OS_GET_TIMER_ARG(ic, struct ieee80211com *);
    ieee80211_wnm_bss_validate_inactivity(ic);
    OS_SET_TIMER(&ic->ic_bssload_timer, IEEE80211_BSSLOAD_WAIT);
}
#endif

/*
 * @brief description
 *  function executed from the timer context to update the noise stats
 *  like noise value min value, max value and median value
 *  at the end of each traffic rate.
 *
 */
void update_noise_stats(struct ieee80211com *ic)
{
    struct     ieee80211_node_table *nt = &ic->ic_sta;
    struct     ieee80211_node *ni;
    u_int8_t   *median_array;
    u_int8_t   bin_index, median_index, temp_variable;

    median_array = (u_int8_t *)OS_MALLOC(ic->ic_osdev, (ic->bin_number + 1) * sizeof(u_int8_t), GFP_KERNEL);
    if (median_array == NULL){
        printk("Memory allocation for median array failed \n");
        return;
    }

    OS_RWLOCK_READ_LOCK(&ic->ic_sta.nt_nodelock, &lock_state);
    TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
        if (ni->ni_associd != 0){
            if (ic->bin_number != 0)
            {
                ni->ni_noise_stats[ic->bin_number].noise_value = ni->ni_rssi;
                for(bin_index = 0;bin_index <= ic->bin_number;bin_index++){
                    median_array[bin_index] = ni->ni_noise_stats[bin_index].noise_value;
                }
                for(bin_index = 0;bin_index <= ic->bin_number;bin_index++){
                    for (median_index = 0;median_index < (ic->bin_number - bin_index); median_index++){
                        if (median_array[median_index] >= median_array[median_index+1])
                        {
                            temp_variable = median_array[median_index];
                            median_array[median_index] = median_array[median_index+1];
                            median_array[median_index+1] = temp_variable;
                        }
                    }
                }
                if ((ic->bin_number) %2 == 0)
                {
                    ni->ni_noise_stats[ic->bin_number].median_value = median_array[ic->bin_number/2];
                } else{
                    ni->ni_noise_stats[ic->bin_number].median_value = median_array[(ic->bin_number/2) + 1];
                }

                if(ni->ni_noise_stats[ic->bin_number].noise_value <= ni->ni_noise_stats[ic->bin_number-1].min_value){
                    ni->ni_noise_stats[ic->bin_number].min_value = ni->ni_noise_stats[ic->bin_number].noise_value;
                } else{
                    ni->ni_noise_stats[ic->bin_number].min_value = ni->ni_noise_stats[ic->bin_number-1].min_value;
                }
                if(ni->ni_noise_stats[ic->bin_number].noise_value >= ni->ni_noise_stats[ic->bin_number-1].max_value){
                    ni->ni_noise_stats[ic->bin_number].max_value = ni->ni_noise_stats[ic->bin_number].noise_value;
                } else{
                    ni->ni_noise_stats[ic->bin_number].max_value = ni->ni_noise_stats[ic->bin_number-1].max_value;
                }
            }

            else {
                ni->ni_noise_stats[ic->bin_number].noise_value = ni->ni_rssi;
                ni->ni_noise_stats[ic->bin_number].min_value = ni->ni_noise_stats[ic->bin_number].noise_value;
                ni->ni_noise_stats[ic->bin_number].max_value = ni->ni_noise_stats[ic->bin_number].noise_value;
                ni->ni_noise_stats[ic->bin_number].median_value = ni->ni_noise_stats[ic->bin_number].noise_value;
            }
        }
    }
    OS_RWLOCK_READ_UNLOCK(&ic->ic_sta.nt_nodelock, &lock_state);
    OS_FREE(median_array);
}

/*
 * brief description
 * Timer function which is used to record the noise statistics of each node
 * timer is called ath the end of each traffic rate and is measured until
 * the end of traffic interval
 */
static OS_TIMER_FUNC(ieee80211_noise_stats_update)
{
    struct ieee80211com *ic;
    OS_GET_TIMER_ARG(ic, struct ieee80211com *);
    update_noise_stats(ic);
    ic->bin_number++;
    if(ic->bin_number < ic->traf_bins){
        OS_SET_TIMER(&ic->ic_noise_stats,ic->traf_rate * 1000);
    }
}

#if SUPPORT_11AX_D3
void
ieee80211_heop_init(struct ieee80211com * ic) {
    /* In future we may have to fill in the
     * values of individual fields in heop_param and heop_bsscolor_info */
    ic->ic_he.heop_param = 0;
    ic->ic_he.heop_bsscolor_info = 0;
}
#else
void
ieee80211_heop_param_init(struct ieee80211com * ic) {
    /* In future we may have to fill in the
     * values of individual fields in heop_param */
    ic->ic_he.heop_param = 0;
}
#endif

int ieee80211_mbssid_setup(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    int vap_count;
    struct ieee80211vap *tmpvap;
    struct vdev_mlme_obj *vdev_mlme = vap->vdev_mlme;

    vap->iv_mbss.mbss_set_id = IEEE80211_DEFAULT_MBSS_SET_IDX;
    /* VAP created first is the transmitting VAP */
    if(ic->ic_mbss.transmit_vap == NULL) {
         vdev_mlme->mgmt.mbss_11ax.profile_idx = IEEE80211_INVALID_MBSS_BSSIDX;
         ic->ic_mbss.transmit_vap = vap;
         ic->ic_mbss.num_non_transmit_vaps = 0;
         qdf_mem_zero(
             (void *) &ic->ic_mbss.bssid_index_bmap[IEEE80211_DEFAULT_MBSS_SET_IDX],
              sizeof(ic->ic_mbss.bssid_index_bmap[IEEE80211_DEFAULT_MBSS_SET_IDX]));

         qdf_info("[MBSSID] Added transmitting VAP (%pK), max_bssid_indicator:%d\n",
                   vap, ic->ic_mbss.max_bssid);
    } else {

        /*
         * Check for max profiles. Transmitting VAP is included for comparison.
         * eg. - with max_bssid=4, there can be 16 VAPs in MBSSID set that includes
         * 1 tranmitting VAP and 15 non-transmitting VAPs.
         */
         vap_count = 0;
	 TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
	    if (tmpvap->iv_opmode == IEEE80211_M_HOSTAP) {
	        vap_count++;
	    }
	 }
         if (vap_count == (1 << ic->ic_mbss.max_bssid)) {
             qdf_print("[MBSSID] Cannot add non-transmitting VAP, Max BSS number reached!\n");
	     return -1;
	 }

         /* It is a non-transmitting VAP */
         vdev_mlme->mgmt.mbss_11ax.profile_idx = qdf_ffz(
                 ic->ic_mbss.bssid_index_bmap[IEEE80211_DEFAULT_MBSS_SET_IDX]) + 1;
         if (qdf_unlikely(vdev_mlme->mgmt.mbss_11ax.profile_idx == IEEE80211_INVALID_MBSS_BSSIDX)) {
             qdf_print("[MBSSID] BSSID index bitmap full!\n");
             return -1;
         }

         qdf_set_bit(vdev_mlme->mgmt.mbss_11ax.profile_idx - 1,
             (unsigned long *) &ic->ic_mbss.bssid_index_bmap[IEEE80211_DEFAULT_MBSS_SET_IDX]);
         qdf_atomic_init(&vap->iv_mbss.iv_added_to_mbss_ie);

         IEEE80211_VAP_MBSS_NON_TRANS_ENABLE(vap);

	 qdf_print("[MBSSID] Added non-transmitting VAP (%pK) with BSSID index:%d\n",
                   vap, vdev_mlme->mgmt.mbss_11ax.profile_idx);
    }

    return 0;
}

bool inline ieee80211_mbssid_check_max_profiles(struct ieee80211com *ic, uint8_t count)
{
#define POW_OF_2(x) (1 << x)
    /*
     * Check for max profiles. Transmitting VAP is included for comparison.
     * eg. - with max_bssid=4, there can be 16 VAPs in MBSSID set that includes
     * 1 tranmitting VAP and 15 non-transmitting VAPs.
     */
    if (count == (POW_OF_2(ic->ic_mbss.max_bssid) - 1)) {
        return false;
    } else {
        return true;
    }
}

int
ieee80211_ifattach(struct ieee80211com *ic, IEEE80211_REG_PARAMETERS *ieee80211_reg_parm)
{
    u_int8_t bcast[IEEE80211_ADDR_LEN] = {0xff,0xff,0xff,0xff,0xff,0xff};
    int error = 0;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct target_psoc_info *psoc_info;
    target_resource_config* tgt_cfg;

    psoc_info = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
							       scn->soc->psoc_obj);
    tgt_cfg = target_psoc_get_wlan_res_cfg(psoc_info);

#define DEFAULT_TRAFFIC_INTERVAL 1800
#define DEFAULT_TRAFFIC_RATE     300

    /* set up broadcast address */
    IEEE80211_ADDR_COPY(ic->ic_broadcast, bcast);

    /* initialize channel list */
    ieee80211_update_channellist(ic, 0, false);

    /* initialize rate set */
    ieee80211_init_rateset(ic);

#if SUPPORT_11AX_D3
    ieee80211_heop_init(ic);
#else
    ieee80211_heop_param_init(ic);
#endif

    ic->ic_dfs_start_tx_rcsa_and_waitfor_rx_csa = ieee80211_dfs_start_tx_rcsa_and_waitfor_rx_csa;

    /* validate ic->ic_curmode */
    if (!IEEE80211_SUPPORT_PHY_MODE(ic, ic->ic_curmode))
        ic->ic_curmode = IEEE80211_MODE_AUTO;

    /* setup initial channel settings */
    ic->ic_curchan = ieee80211_ath_get_channel(ic, 0); /* arbitrarily pick the first channel */

    /* Enable marking of dfs by default */
    IEEE80211_FEXT_MARKDFS_ENABLE(ic);

    if (ic->ic_reg_parm.htEnableWepTkip) {
        ieee80211_ic_wep_tkip_htrate_set(ic);
    } else {
        ieee80211_ic_wep_tkip_htrate_clear(ic);
    }

    if (ic->ic_reg_parm.htVendorIeEnable)
        IEEE80211_ENABLE_HTVIE(ic);

    /* whether to ignore 11d beacon */
    if (ic->ic_reg_parm.ignore11dBeacon)
        IEEE80211_ENABLE_IGNORE_11D_BEACON(ic);

    if (ic->ic_reg_parm.disallowAutoCCchange) {
        ieee80211_ic_disallowAutoCCchange_set(ic);
    }
    else {
        ieee80211_ic_disallowAutoCCchange_clear(ic);
    }

    (void) ieee80211_setmode(ic, ic->ic_curmode, ic->ic_opmode);

    ic->ic_he_bsscolor = IEEE80211_BSS_COLOR_INVALID;
    if (EOK != ieee80211_bsscolor_attach(ic)) {
        qdf_print("%s: BSS Color attach failed _investigate__ ",__func__);
    }

    ic->ic_intval = IEEE80211_BINTVAL_DEFAULT; /* beacon interval */
    ic->ic_set_beacon_interval(ic);

    ic->ic_lintval = 1;         /* listen interval */
    ic->ic_lintval_assoc = IEEE80211_LINTVAL_MAX; /* listen interval to use in association */
    ic->ic_bmisstimeout = IEEE80211_BMISS_LIMIT * ic->ic_intval;
    TAILQ_INIT(&ic->ic_vaps);

    ic->ic_txpowlimit = IEEE80211_TXPOWER_MAX;

    /* Intialize WDS Auto Detect mode */
    ieee80211com_set_flags_ext(ic, IEEE80211_FEXT_WDS_AUTODETECT);

	/*
	** Enable the 11d country code IE by default
	*/

    ieee80211com_set_flags_ext(ic, IEEE80211_FEXT_COUNTRYIE);

    /* setup CWM configuration */
    ic->ic_cwm_set_mode(ic, ic->ic_reg_parm.cwmMode);
    ic->ic_cwm_set_extoffset(ic, ic->ic_reg_parm.cwmExtOffset);
    ic->ic_cwm_set_extprotmode(ic, ic->ic_reg_parm.cwmExtProtMode);
    ic->ic_cwm_set_extprotspacing(ic, ic->ic_reg_parm.cwmExtProtSpacing);

    ic->ic_cwm_set_enable(ic, ic->ic_reg_parm.cwmEnable);
    ic->ic_cwm_set_extbusythreshold(ic, ic->ic_reg_parm.cwmExtBusyThreshold);

    ic->ic_enable2GHzHt40Cap = ic->ic_reg_parm.enable2GHzHt40Cap;

#ifdef ATH_COALESCING
    ic->ic_tx_coalescing     = ic->ic_reg_parm.txCoalescingEnable;
    wlan_pdev_set_tx_coalescing(ic->ic_pdev_obj, ic->ic_reg_parm.txCoalescingEnable);
#endif
    ic->ic_ignoreDynamicHalt = ic->ic_reg_parm.ignoreDynamicHalt;

    /* default to auto ADDBA mode */
    ic->ic_addba_mode = ADDBA_MODE_AUTO;
    wlan_pdev_set_addba_mode(ic->ic_pdev_obj, ADDBA_MODE_AUTO);

    if (ic->ic_reg_parm.ht20AdhocEnable) {
        /*
         * Support HT rates in Ad hoc connections.
         */
        if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11NA_HT20) ||
            IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11NG_HT20)) {
            ieee80211_ic_ht20Adhoc_set(ic);

            if (ic->ic_reg_parm.htAdhocAggrEnable) {
                ieee80211_ic_htAdhocAggr_set(ic);
            }
        }
    }

    if (ic->ic_reg_parm.ht40AdhocEnable) {
        /*
         * Support HT rates in Ad hoc connections.
         */
        if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11NA_HT40PLUS) ||
            IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11NA_HT40MINUS) ||
            IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11NG_HT40PLUS) ||
            IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11NG_HT40MINUS)) {
            ieee80211_ic_ht40Adhoc_set(ic);

            if (ic->ic_reg_parm.htAdhocAggrEnable) {
                ieee80211_ic_htAdhocAggr_set(ic);
            }
        }
    }

    OS_INIT_TIMER(ic->ic_osdev, &(ic->ic_inact_timer), ieee80211_inact_timeout, (void *) (ic), QDF_TIMER_TYPE_WAKE_APPS);
    ic->ic_obss_nb_ru_tolerance_time = IEEE80211_OBSS_NB_RU_TOLERANCE_TIME_DEFVAL;
    OS_INIT_TIMER(ic->ic_osdev, &(ic->ic_obss_nb_ru_tolerance_timer), ieee80211_obss_nb_ru_tolerence_timeout, (void *) (ic), QDF_TIMER_TYPE_WAKE_APPS);
#if UMAC_SUPPORT_WNM
    OS_INIT_TIMER(ic->ic_osdev, &(ic->ic_bssload_timer), ieee80211_bssload_timeout, (void *) (ic), QDF_TIMER_TYPE_WAKE_APPS);
#endif

    OS_INIT_TIMER(ic->ic_osdev, &(ic->ic_noise_stats), ieee80211_noise_stats_update, (void *) (ic), QDF_TIMER_TYPE_WAKE_APPS);

    qdf_timer_init(NULL, &ic->ic_csa_max_rx_wait_timer,
            ieee80211_csa_max_rx_wait_timer, (void *)(ic),
            QDF_TIMER_TYPE_WAKE_APPS);

    /* Initialization of traffic interval and traffic rate is 1800 and 300 seconds respectively */
    ic->traf_interval = DEFAULT_TRAFFIC_INTERVAL;
    ic->traf_rate = DEFAULT_TRAFFIC_RATE;
    ic->traf_bins = ic->traf_interval/ic->traf_rate;
    if (ic->ic_reg_parm.disable2040Coexist) {
        ieee80211com_set_flags(ic, IEEE80211_F_COEXT_DISABLE);
    } else {
        ieee80211com_clear_flags(ic, IEEE80211_F_COEXT_DISABLE);
    }

    /* setup other modules */

    /* The TSF Timer module is required when P2P or Off-channel support are required */
    ic->ic_tsf_timer = ieee80211_tsf_timer_attach(ic);

    ieee80211_ic_off_channel_support_clear(ic);

    ieee80211_p2p_attach(ic);
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    ieee80211_crypto_attach(ic);
#endif
    ieee80211_node_attach(ic);
    ieee80211_proto_attach(ic);
    ieee80211_power_attach(ic);
    ieee80211_mlme_attach(ic);

#if ATH_SUPPORT_DFS
    ic->set_country = (struct assoc_sm_set_country*)qdf_mem_malloc(
            sizeof(struct assoc_sm_set_country));

    ATH_CREATE_WORK(&ic->dfs_cac_timer_start_work,
            ieee80211_dfs_cac_timer_start_async,
            (void *)ic);
    ATH_CREATE_WORK(&ic->assoc_sm_set_country_code,
            ieee80211_set_country_code_assoc_sm,
            (void *)ic);
    OS_INIT_TIMER(ic->ic_osdev,
            &(ic->ic_dfs_tx_rcsa_and_nol_ie_timer),
            ieee80211_dfs_tx_rcsa_task,
            (void *)(ic),
            QDF_TIMER_TYPE_WAKE_APPS);
    OS_INIT_TIMER(ic->ic_osdev,
            &(ic->ic_dfs_waitfor_csa_timer),
            ieee80211_dfs_waitfor_csa_task,
            (void *)(ic),
            QDF_TIMER_TYPE_WAKE_APPS);
#if QCA_DFS_NOL_VAP_RESTART
    OS_INIT_TIMER(ic->ic_osdev,
            &(ic->ic_dfs_nochan_vap_restart_timer),
            ieee80211_dfs_nochan_vap_restart,
            (void *)(ic),
            QDF_TIMER_TYPE_WAKE_APPS);
#endif
#endif /* ATH_SUPPORT_DFS */

    /*
     * By default overwrite probe response with beacon IE in scan entry.
     */
    ieee80211_ic_override_proberesp_ie_set(ic);

    ic->ic_resmgr = ieee80211_resmgr_create(ic, IEEE80211_RESMGR_MODE_SINGLE_CHANNEL);

    error = ieee80211_acs_attach(&(ic->ic_acs),
                          ic,
                          ic->ic_osdev);
    if (error) {
        /* detach and free already allocated memory for scan */
        ieee80211_node_detach(ic);
        return error;
    }

    error = ieee80211_cbs_attach(&(ic->ic_cbs),
                          ic,
                          ic->ic_osdev);
    if (error) {
        /* detach and free already allocated memory for scan */
        ieee80211_acs_detach(&(ic->ic_acs));
        ieee80211_node_detach(ic);
        return error;
    }

    ic->ic_notify_tx_bcn_mgr = ieee80211_notify_tx_bcn_attach(ic);
#if UMAC_SUPPORT_VI_DBG
    ieee80211_vi_dbg_attach(ic);
#endif
    ieee80211_quiet_attach(ic);
	ieee80211_admctl_attach(ic);

    /*
     * Perform steps that require multiple objects to be initialized.
     * For example, cross references between objects such as ResMgr and Scanner.
     */
    ieee80211_resmgr_create_complete(ic->ic_resmgr);

    ic->ic_get_ext_chan_info = ieee80211_get_extchaninfo;

#if ACFG_NETLINK_TX
    acfg_attach(ic);
#elif UMAC_SUPPORT_ACFG
    acfg_event_workqueue_init(ic->ic_osdev);
#endif
#if UMAC_SUPPORT_ACFG
    acfg_diag_attach(ic);
#endif

    if(!ic->ic_is_mode_offload(ic)) {
#ifdef QCA_LOWMEM_PLATFORM
        ic->ic_num_clients = IEEE80211_33_AID;
#else
        ic->ic_num_clients = IEEE80211_128_AID;
#endif
    }
    ic->ic_chan_stats_th = IEEE80211_CHAN_STATS_THRESOLD;
    ic->ic_chan_switch_cnt = IEEE80211_RADAR_11HCOUNT;
    ic->ic_wb_subelem = 1;
    ic->ic_sec_offsetie = 1;

    if (wlan_pdev_nif_feat_cap_get(ic->ic_pdev_obj,
                                   WLAN_PDEV_F_MBSS_IE_ENABLE)) {
        ic->ic_mbss.max_bssid =  tgt_cfg->max_bssid_indicator;
    }

    /* initialization complete */
    ic->ic_initialized = 1;

    ic->ic_nr_share_radio_flag = 0;
    ic->ic_nr_share_enable = 0;
    if (ic->ic_cfg80211_config) {
        ic->ic_roaming = IEEE80211_ROAMING_MANUAL;
    }

    return 0;
#undef DEFAULT_TRAFFIC_INTERVAL
#undef DEFAULT_TRAFFIC_RATE
}

void
ieee80211_ifdetach(struct ieee80211com *ic)
{
    if (!ic->ic_initialized) {
        return;
    }

    /* Setting zero to aviod re-arming of ic_inact_timer timer */
    ic->ic_initialized = 0;

    /*
     * Preparation for detaching objects.
     * For example, remove and cross references between objects such as those
     * between ResMgr and Scanner.
     */
    ieee80211_resmgr_delete_prepare(ic->ic_resmgr);

    qdf_timer_free(&ic->ic_csa_max_rx_wait_timer);

    OS_FREE_TIMER(&ic->ic_inact_timer);
#if UMAC_SUPPORT_WNM
    OS_FREE_TIMER(&ic->ic_bssload_timer);
#endif

    OS_FREE_TIMER(&ic->ic_obss_nb_ru_tolerance_timer);

    OS_FREE_TIMER(&ic->ic_noise_stats);

    OS_CANCEL_TIMER(&ic->ic_dfs_tx_rcsa_and_nol_ie_timer);
    if(ic->ic_dfs_waitfor_csa_sched) {
        OS_CANCEL_TIMER(&ic->ic_dfs_waitfor_csa_timer);
        ic->ic_dfs_waitfor_csa_sched = 0;
    }
#if ATH_SUPPORT_DFS && QCA_DFS_NOL_VAP_RESTART
    OS_CANCEL_TIMER(&ic->ic_dfs_nochan_vap_restart_timer);
#endif

    /* all the vaps should have been deleted now */
    ASSERT(TAILQ_FIRST(&ic->ic_vaps) == NULL);

    ieee80211_node_detach(ic);
    ieee80211_quiet_detach(ic);
	ieee80211_admctl_detach(ic);

    ieee80211_proto_detach(ic);
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    ieee80211_crypto_detach(ic);
#endif
    ieee80211_power_detach(ic);
    ieee80211_mlme_detach(ic);
    ieee80211_notify_tx_bcn_detach(ic->ic_notify_tx_bcn_mgr);
    ieee80211_resmgr_delete(ic->ic_resmgr);
    ieee80211_p2p_detach(ic);
    ieee80211_cbs_detach(&(ic->ic_cbs));
    ieee80211_acs_detach(&(ic->ic_acs));

    if( EOK != ieee80211_bsscolor_detach(ic)){
        qdf_print("%s: BSS Color detach failed ",__func__);
    }
#if UMAC_SUPPORT_VI_DBG
    ieee80211_vi_dbg_detach(ic);
#endif

#if UMAC_SUPPORT_ACFG
    acfg_diag_detach(ic);
#endif

#if ACFG_NETLINK_TX
    acfg_detach(ic);
#endif
    /* Detach TSF timer at the end to avoid assertion */
    if (ic->ic_tsf_timer) {
        ieee80211_tsf_timer_detach(ic->ic_tsf_timer);
        ic->ic_tsf_timer = NULL;
    }

#if QCA_SUPPORT_GPR
    if(ic->ic_gpr_enable) {
        qdf_hrtimer_kill(&ic->ic_gpr_timer);
        qdf_mem_free(ic->acfg_frame);
        ic->acfg_frame = NULL;
        qdf_err("\nStopping GPR timer as this is last vap with gpr \n");
    }
#endif

    spin_lock_destroy(&ic->ic_lock);
    spin_lock_destroy(&ic->ic_main_sta_lock);
    spin_lock_destroy(&ic->ic_addba_lock);
    IEEE80211_STATE_LOCK_DESTROY(ic);
    spin_lock_destroy(&ic->ic_beacon_alloc_lock);
    spin_lock_destroy(&ic->ic_diag_lock);
    spin_lock_destroy(&ic->ic_radar_found_lock);
    qdf_spinlock_destroy(&ic->ic_channel_stats.lock);
}

/*
 * Start this IC
 */
void ieee80211_start_running(struct ieee80211com *ic)
{
    OS_SET_TIMER(&ic->ic_inact_timer, IEEE80211_INACT_WAIT*1000);
}

/*
 * Stop this IC
 */
void ieee80211_stop_running(struct ieee80211com *ic)
{
    OS_CANCEL_TIMER(&ic->ic_inact_timer);
}

int ieee80211com_register_event_handlers(struct ieee80211com *ic,
                                     void *event_arg,
                                     wlan_dev_event_handler_table *evtable)
{
    int i;
    /* unregister if there exists one already */
    ieee80211com_unregister_event_handlers(ic,event_arg,evtable);
    IEEE80211_COMM_LOCK(ic);
    for (i=0;i<IEEE80211_MAX_DEVICE_EVENT_HANDLERS; ++i) {
        if ( ic->ic_evtable[i] == NULL) {
            ic->ic_evtable[i] = evtable;
            ic->ic_event_arg[i] = event_arg;
            IEEE80211_COMM_UNLOCK(ic);
            return 0;
        }
    }
    IEEE80211_COMM_UNLOCK(ic);
    return -ENOMEM;


}

int ieee80211com_unregister_event_handlers(struct ieee80211com *ic,
                                     void *event_arg,
                                     wlan_dev_event_handler_table *evtable)
{
    int i;
    IEEE80211_COMM_LOCK(ic);
    for (i=0;i<IEEE80211_MAX_DEVICE_EVENT_HANDLERS; ++i) {
        if ( ic->ic_evtable[i] == evtable &&  ic->ic_event_arg[i] == event_arg) {
            ic->ic_evtable[i] = NULL;
            ic->ic_event_arg[i] = NULL;
            IEEE80211_COMM_UNLOCK(ic);
            return 0;
        }
    }
    IEEE80211_COMM_UNLOCK(ic);
    return -EEXIST;
}

/* Clear user defined ADDBA response codes for all nodes. */
static void
ieee80211_addba_clearresponse(void *arg, struct ieee80211_node *ni)
{
    struct ieee80211com *ic = (struct ieee80211com *) arg;
    ic->ic_addba_clearresponse(ni);
}

int wlan_device_register_event_handlers(wlan_dev_t devhandle,
                                     void *event_arg,
                                     wlan_dev_event_handler_table *evtable)
{

    return ieee80211com_register_event_handlers(devhandle,event_arg,evtable);
}


int wlan_device_unregister_event_handlers(wlan_dev_t devhandle,
                                     void *event_arg,
                                     wlan_dev_event_handler_table *evtable)
{
    return ieee80211com_unregister_event_handlers(devhandle,event_arg,evtable);
}


int wlan_set_device_param(wlan_dev_t ic, ieee80211_device_param param, u_int32_t val)
{
    int retval=EOK;
    switch(param) {
    case IEEE80211_DEVICE_TX_CHAIN_MASK:
    case IEEE80211_DEVICE_TX_CHAIN_MASK_LEGACY:
	if(ic->ic_set_chain_mask(ic,param,val) == 0) {
            ic->ic_tx_chainmask = val;
            wlan_objmgr_update_txchainmask_to_allvdevs(ic);
        } else {
            retval=EINVAL;
        }
        break;
    case IEEE80211_DEVICE_RX_CHAIN_MASK:
    case IEEE80211_DEVICE_RX_CHAIN_MASK_LEGACY:
	if(ic->ic_set_chain_mask(ic,param,val) == 0) {
            ic->ic_rx_chainmask = val;
            wlan_objmgr_update_rxchainmask_to_allvdevs(ic);
        } else {
            retval=EINVAL;
        }
        break;

    case IEEE80211_DEVICE_PROTECTION_MODE:
        if (val > IEEE80211_PROT_RTSCTS) {
	    retval=EINVAL;
        } else {
	   ic->ic_protmode = val;
        }
        break;
    case IEEE80211_DEVICE_NUM_TX_CHAIN:
    case IEEE80211_DEVICE_NUM_RX_CHAIN:
    case IEEE80211_DEVICE_COUNTRYCODE:
       /* read only */
	retval=EINVAL;
        break;
    case IEEE80211_DEVICE_BMISS_LIMIT:
    	ic->ic_bmisstimeout = val * ic->ic_intval;
        break;
    case IEEE80211_DEVICE_BLKDFSCHAN:
        if (val == 0) {
            ieee80211_ic_block_dfschan_clear(ic);
        } else {
            ieee80211_ic_block_dfschan_set(ic);
        }
        break;
    case IEEE80211_DEVICE_GREEN_AP_ENABLE_PRINT:
        ic->ic_green_ap_set_print_level(ic, val);
        break;
    case IEEE80211_DEVICE_CWM_EXTPROTMODE:
        if (val < IEEE80211_CWM_EXTPROTMAX) {
            ic->ic_cwm_set_extprotmode(ic, val);
        } else {
            retval = EINVAL;
        }
        break;
    case IEEE80211_DEVICE_CWM_EXTPROTSPACING:
        if (val < IEEE80211_CWM_EXTPROTSPACINGMAX) {
            ic->ic_cwm_set_extprotspacing(ic, val);
        } else {
            retval = EINVAL;
        }
        break;
    case IEEE80211_DEVICE_CWM_ENABLE:
        ic->ic_cwm_set_enable(ic, val);
        break;
    case IEEE80211_DEVICE_CWM_EXTBUSYTHRESHOLD:
        ic->ic_cwm_set_extbusythreshold(ic, val);
        break;
    case IEEE80211_DEVICE_DOTH:
        if (val == 0) {
            ieee80211_ic_doth_clear(ic);
        } else {
            ieee80211_ic_doth_set(ic);
        }
        break;
    case IEEE80211_DEVICE_ADDBA_MODE:
        ic->ic_addba_mode = val;
        wlan_pdev_set_addba_mode(ic->ic_pdev_obj, val);
        /*
        * Clear any user defined ADDBA response codes before switching modes.
        */
        ieee80211_iterate_node(ic, ieee80211_addba_clearresponse, ic);
        break;
    case IEEE80211_DEVICE_MULTI_CHANNEL:
        if (!val) {
            /* Disable Multi-Channel */
            retval = ieee80211_resmgr_setmode(ic->ic_resmgr, IEEE80211_RESMGR_MODE_SINGLE_CHANNEL);
        }
        else if (ic->ic_caps_ext & IEEE80211_CEXT_MULTICHAN) {
            retval = ieee80211_resmgr_setmode(ic->ic_resmgr, IEEE80211_RESMGR_MODE_MULTI_CHANNEL);
        }
        else {
            printk("%s: Unable to enable Multi-Channel Scheduling since device/driver don't support it.\n", __func__);
            retval = EINVAL;
        }
        break;
    case IEEE80211_DEVICE_MAX_AMSDU_SIZE:
        ic->ic_amsdu_max_size = val;
        wlan_pdev_set_amsdu_max_size(ic->ic_pdev_obj, val);
        break;
#if ATH_SUPPORT_IBSS_HT
    case IEEE80211_DEVICE_HT20ADHOC:
        if (val == 0) {
            ieee80211_ic_ht20Adhoc_clear(ic);
        } else {
            ieee80211_ic_ht20Adhoc_set(ic);
        }
        break;
    case IEEE80211_DEVICE_HT40ADHOC:
        if (val == 0) {
            ieee80211_ic_ht40Adhoc_clear(ic);
        } else {
            ieee80211_ic_ht40Adhoc_set(ic);
        }
        break;
    case IEEE80211_DEVICE_HTADHOCAGGR:
        if (val == 0) {
            ieee80211_ic_htAdhocAggr_clear(ic);
        } else {
            ieee80211_ic_htAdhocAggr_set(ic);
        }
        break;
#endif /* end of #if ATH_SUPPORT_IBSS_HT */
    case IEEE80211_DEVICE_PWRTARGET:
        ieee80211com_set_curchanmaxpwr(ic, val);
        break;
    case IEEE80211_DEVICE_P2P:
        if (val == 0) {
            ieee80211_ic_p2pDevEnable_clear(ic);
        }
        else if (ic->ic_caps_ext & IEEE80211_CEXT_P2P) {
            ieee80211_ic_p2pDevEnable_set(ic);
        }
        else {
            printk("%s: Unable to enable P2P since device/driver don't support it.\n", __func__);
            retval = EINVAL;
        }
        break;

    case IEEE80211_DEVICE_OVERRIDE_SCAN_PROBERESPONSE_IE:
      if (val) {
          ieee80211_ic_override_proberesp_ie_set(ic);
      } else {
          ieee80211_ic_override_proberesp_ie_clear(ic);
      }
      break;
    case IEEE80211_DEVICE_2G_CSA:
        if (val == 0) {
            ieee80211_ic_2g_csa_clear(ic);
        } else {
            ieee80211_ic_2g_csa_set(ic);
        }
        break;

    default:
        printk("%s: Error: invalid param=%d.\n", __func__, param);
    }
    return retval;

}

u_int32_t wlan_get_device_param(wlan_dev_t ic, ieee80211_device_param param)
{

    switch(param) {
    case IEEE80211_DEVICE_NUM_TX_CHAIN:
        return (ic->ic_num_tx_chain);
        break;
    case IEEE80211_DEVICE_NUM_RX_CHAIN:
        return (ic->ic_num_rx_chain);
        break;
    case IEEE80211_DEVICE_TX_CHAIN_MASK:
        return (ic->ic_tx_chainmask);
        break;
    case IEEE80211_DEVICE_RX_CHAIN_MASK:
        return (ic->ic_rx_chainmask);
        break;
    case IEEE80211_DEVICE_PROTECTION_MODE:
	return (ic->ic_protmode );
        break;
    case IEEE80211_DEVICE_BMISS_LIMIT:
    	return (ic->ic_bmisstimeout / ic->ic_intval);
        break;
    case IEEE80211_DEVICE_BLKDFSCHAN:
        return (ieee80211_ic_block_dfschan_is_set(ic));
        break;
    case IEEE80211_DEVICE_GREEN_AP_ENABLE_PRINT:
        return ic->ic_green_ap_get_print_level(ic);
        break;
    case IEEE80211_DEVICE_CWM_EXTPROTMODE:
        return ic->ic_cwm_get_extprotmode(ic);
        break;
    case IEEE80211_DEVICE_CWM_EXTPROTSPACING:
        return ic->ic_cwm_get_extprotspacing(ic);
        break;
    case IEEE80211_DEVICE_CWM_ENABLE:
        return ic->ic_cwm_get_enable(ic);
        break;
    case IEEE80211_DEVICE_CWM_EXTBUSYTHRESHOLD:
        return ic->ic_cwm_get_extbusythreshold(ic);
        break;
    case IEEE80211_DEVICE_DOTH:
        return (ieee80211_ic_doth_is_set(ic));
        break;
    case IEEE80211_DEVICE_ADDBA_MODE:
        return ic->ic_addba_mode;
        break;
    case IEEE80211_DEVICE_COUNTRYCODE:
        return ieee80211_getCurrentCountry(ic);
        break;
    case IEEE80211_DEVICE_MULTI_CHANNEL:
        return (ieee80211_resmgr_getmode(ic->ic_resmgr)
                == IEEE80211_RESMGR_MODE_MULTI_CHANNEL);
        break;
    case IEEE80211_DEVICE_MAX_AMSDU_SIZE:
        return(ic->ic_amsdu_max_size);
        break;
#if ATH_SUPPORT_IBSS_HT
    case IEEE80211_DEVICE_HT20ADHOC:
        return (ieee80211_ic_ht20Adhoc_is_set(ic));
        break;
    case IEEE80211_DEVICE_HT40ADHOC:
        return (ieee80211_ic_ht40Adhoc_is_set(ic));
        break;
    case IEEE80211_DEVICE_HTADHOCAGGR:
        return (ieee80211_ic_htAdhocAggr_is_set(ic));
        break;
#endif /* end of #if ATH_SUPPORT_IBSS_HT */
    case IEEE80211_DEVICE_PWRTARGET:
        return (ieee80211com_get_curchanmaxpwr(ic));
        break;
    case IEEE80211_DEVICE_P2P:
        return (ieee80211_ic_p2pDevEnable_is_set(ic));
        break;
    case IEEE80211_DEVICE_OVERRIDE_SCAN_PROBERESPONSE_IE:
        return  ieee80211_ic_override_proberesp_ie_is_set(ic);
        break;
    case IEEE80211_DEVICE_2G_CSA:
        return (ieee80211_ic_2g_csa_is_set(ic));
        break;
    default:
        return 0;
    }
}

int wlan_get_device_mac_addr(wlan_dev_t ic, u_int8_t *mac_addr)
{
   IEEE80211_ADDR_COPY(mac_addr, ic->ic_myaddr);
   return EOK;
}

void wlan_device_note(struct ieee80211com *ic, const char *fmt, ...)
{
     char                   tmp_buf[OS_TEMP_BUF_SIZE];
     va_list                ap;
     va_start(ap, fmt);
     vsnprintf (tmp_buf,OS_TEMP_BUF_SIZE, fmt, ap);
     va_end(ap);
     printk("%s",tmp_buf);
     ic->ic_log_text(ic,tmp_buf);
}

void wlan_get_vap_opmode_count(wlan_dev_t ic,
                               struct ieee80211_vap_opmode_count *vap_opmode_count)
{
    ieee80211_get_vap_opmode_count(ic, vap_opmode_count);
}

static void ieee80211_vap_iter_active_vaps(void *arg, struct ieee80211vap *vap)
{
    u_int16_t *pnactive = (u_int16_t *)arg;
       /* active vap check is used for assigning/updating vap channel with ic_curchan
	 so, it considers active vaps, and Vaps which are in CAC period */
    if (wlan_vdev_chan_config_valid(vap->vdev_obj) == QDF_STATUS_SUCCESS)
        ++(*pnactive);

}

static void ieee80211_vap_iter_vaps_up(void *arg, struct ieee80211vap *vap)
{
    u_int8_t *pnvaps_up = (u_int8_t *)arg;
    /* active vap check is used for assigning/updating vap channel with ic_curchan
       so, it considers active vaps, and Vaps which are in CAC period */
    if (vap->iv_opmode == IEEE80211_M_STA){
        if (wlan_vdev_chan_config_valid(vap->vdev_obj) == QDF_STATUS_SUCCESS) {
            ++(*pnvaps_up);
        }
    } else {
        if (wlan_vdev_mlme_is_active(vap->vdev_obj) == QDF_STATUS_SUCCESS) {
            ++(*pnvaps_up);
        }
    }
}

/* Returns the number of AP vaps up */
static void ieee80211_vap_iter_ap_vaps_up(void *arg, struct ieee80211vap *vap)
{
    u_int8_t *pnvaps_up = (u_int8_t *)arg;
    if ((wlan_vdev_mlme_is_active(vap->vdev_obj) == QDF_STATUS_SUCCESS)  &&
        (vap->iv_opmode == IEEE80211_M_HOSTAP)) {
        ++(*pnvaps_up);
    }
}

/*
 * returns number of vaps active.
 */
u_int16_t
ieee80211_vaps_active(struct ieee80211com *ic)
{
    u_int16_t nactive=0;
    wlan_iterate_vap_list_lock(ic,ieee80211_vap_iter_active_vaps,(void *) &nactive);
    return nactive;
}

/*
 * returns number of vaps active and up.
 */
u_int8_t
ieee80211_get_num_vaps_up(struct ieee80211com *ic)
{
    u_int8_t nvaps_up=0;
    wlan_iterate_vap_list_lock(ic,ieee80211_vap_iter_vaps_up,(void *) &nvaps_up);
    return nvaps_up;
}

/*
 * Returns number of ap vaps active and up.
 */
u_int8_t
ieee80211_get_num_ap_vaps_up(struct ieee80211com *ic)
{
    u_int8_t nvaps_up=0;
    wlan_iterate_vap_list_lock(ic,ieee80211_vap_iter_ap_vaps_up,(void *) &nvaps_up);
    return nvaps_up;
}

static void
ieee80211_iter_vap_opmode(void *arg, struct ieee80211vap *vaphandle)
{
    struct ieee80211_vap_opmode_count    *vap_opmode_count = arg;
    enum ieee80211_opmode                opmode = ieee80211vap_get_opmode(vaphandle);

    vap_opmode_count->total_vaps++;

    switch (opmode) {
    case IEEE80211_M_IBSS:
        vap_opmode_count->ibss_count++;
        break;

    case IEEE80211_M_STA:
        vap_opmode_count->sta_count++;
        break;

    case IEEE80211_M_WDS:
        vap_opmode_count->wds_count++;
        break;

    case IEEE80211_M_AHDEMO:
        vap_opmode_count->ahdemo_count++;
        break;

    case IEEE80211_M_HOSTAP:
        vap_opmode_count->ap_count++;
        break;

    case IEEE80211_M_MONITOR:
        vap_opmode_count->monitor_count++;
        break;

    case IEEE80211_M_BTAMP:
        vap_opmode_count->btamp_count++;
        break;

    default:
        vap_opmode_count->unknown_count++;

        printk("%s vap=%pK unknown opmode=%d\n",
            __func__, vaphandle, opmode);
        break;
    }
}

void
ieee80211_get_vap_opmode_count(struct ieee80211com *ic,
                               struct ieee80211_vap_opmode_count *vap_opmode_count)
{
    wlan_iterate_vap_list_lock(ic, ieee80211_iter_vap_opmode, (void *) vap_opmode_count);
}

static void
ieee80211_vap_iter_associated(void *arg, struct ieee80211vap *vap)
{
    u_int8_t *pis_sta_associated = (u_int8_t *)arg;
    if (vap->iv_opmode == IEEE80211_M_STA) {
        if (wlan_vdev_allow_connect_n_tx(vap->vdev_obj) == QDF_STATUS_SUCCESS) {
                (*pis_sta_associated) = 1;
        }
    }
}

/*
 * returns 1 if STA vap is not in associated state else 0
 */
u_int8_t
ieee80211_sta_assoc_in_progress(struct ieee80211com *ic)
{
    u_int8_t in_progress = 0;
    struct ieee80211_vap_opmode_count vap_opmode_count;

    OS_MEMZERO(&vap_opmode_count, sizeof(vap_opmode_count));
    ieee80211_get_vap_opmode_count(ic, &vap_opmode_count);
    if (vap_opmode_count.sta_count) {
        u_int8_t is_sta_associated = 0;

        wlan_iterate_vap_list_lock(ic,ieee80211_vap_iter_associated,(void *) &is_sta_associated);
        if (!is_sta_associated)
            in_progress = 1;
    }

    return in_progress;
}

static void
ieee80211_vap_iter_last_traffic_timestamp(void *arg, struct ieee80211vap *vap)
{
    systime_t    *p_last_traffic_timestamp = arg;
    systime_t    current_traffic_timestamp = ieee80211_get_traffic_indication_timestamp(vap);

    if (current_traffic_timestamp > *p_last_traffic_timestamp) {
        *p_last_traffic_timestamp = current_traffic_timestamp;
    }
}

systime_t
ieee80211com_get_traffic_indication_timestamp(struct ieee80211com *ic)
{
    systime_t    traffic_timestamp = 0;

    wlan_iterate_vap_list_lock(ic, ieee80211_vap_iter_last_traffic_timestamp,(void *) &traffic_timestamp);

    return traffic_timestamp;
}

struct ieee80211_iter_vaps_ready_arg {
    u_int8_t num_sta_vaps_ready;
    u_int8_t num_ibss_vaps_ready;
    u_int8_t num_ap_vaps_ready;
};

static void ieee80211_vap_iter_ready_vaps(void *arg, wlan_if_t vap)
{
    struct ieee80211_iter_vaps_ready_arg *params = (struct ieee80211_iter_vaps_ready_arg *) arg;
    if (wlan_vdev_is_up(vap->vdev_obj) == QDF_STATUS_SUCCESS) {
        switch(ieee80211vap_get_opmode(vap)) {
        case IEEE80211_M_HOSTAP:
        case IEEE80211_M_BTAMP:
            params->num_ap_vaps_ready++;
            break;

        case IEEE80211_M_IBSS:
            params->num_ibss_vaps_ready++;
            break;

        case IEEE80211_M_STA:
            params->num_sta_vaps_ready++;
            break;

        default:
            break;

        }
    }
}

/*
 * returns number of vaps ready.
 */
u_int16_t
ieee80211_vaps_ready(struct ieee80211com *ic, enum ieee80211_opmode opmode)
{
    struct ieee80211_iter_vaps_ready_arg params;
    u_int16_t nready = 0;
    OS_MEMZERO(&params, sizeof(params));
    wlan_iterate_vap_list_lock(ic,ieee80211_vap_iter_ready_vaps,(void *) &params);
    switch(opmode) {
        case IEEE80211_M_HOSTAP:
        case IEEE80211_M_BTAMP:
            nready = params.num_ap_vaps_ready;
            break;

        case IEEE80211_M_IBSS:
            nready = params.num_ibss_vaps_ready;
            break;

        case IEEE80211_M_STA:
            nready = params.num_sta_vaps_ready;
            break;

        default:
            break;
    }
    return nready;
}

int wlan_set_fips(wlan_if_t vap, void *args)
{

    struct ath_fips_cmd *fips_buf = (struct ath_fips_cmd *)args;
    struct ieee80211com *ic = vap->iv_ic;
    int retval = -1;

    u_int8_t default_key[] =  { 0x2b, 0x7e, 0x15, 0x16,
                                0x28, 0xae, 0xd2, 0xa6,
                                0xab, 0xf7, 0x15, 0x88,
                                0x09, 0xcf, 0x4f, 0x3c
                              };
    u_int8_t default_data[] = { 0xf0, 0xf1, 0xf2, 0xf3,
            0xf4, 0xf5, 0xf6, 0xf7,
            0xf8, 0xf9, 0xfa, 0xfb,
            0xfc, 0xfd, 0xfe, 0xff
    };
    u_int8_t default_iv[] =   { 0xf0, 0xf1, 0xf2, 0xf3,
            0xf4, 0xf5, 0xf6, 0xf7,
            0xf8, 0xf9, 0xfa, 0xfb,
            0xfc, 0xfd, 0xfe, 0xff
    };

    if (!ic || !ic->ic_fips_test) {
        printk("\n %s:%d fips_test function not supported", __func__, __LINE__);
        return -EINVAL;
    }
    if(fips_buf != NULL) {

        if (fips_buf->key == NULL) {
            fips_buf->key_len = sizeof(default_key);
            memcpy(fips_buf->key, default_key, sizeof(default_key));
        }

        if (fips_buf->data == NULL) {
            fips_buf->data_len = sizeof(default_data);
            memcpy(fips_buf->data, default_data, sizeof(default_data));
        }

        if (fips_buf->iv == NULL) {
            memcpy(fips_buf->iv, default_iv, sizeof(default_iv));
        }
        retval = ic->ic_fips_test(ic, fips_buf);
    }
    return retval;
}

int
module_init_wlan(void)
{
    return 0;
}

void
module_exit_wlan(void)
{
}

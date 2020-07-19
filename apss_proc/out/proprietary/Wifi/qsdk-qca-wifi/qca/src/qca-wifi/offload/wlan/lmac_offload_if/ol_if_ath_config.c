/*
 * Copyright (c) 2017-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Radio interface configuration routines for perf_pwr_offload
 */
#include <osdep.h>
#include "ol_if_athvar.h"
#include "ol_if_txrx_handles.h"
#include "ol_if_athpriv.h"
#include "ath_ald.h"
#include "dbglog_host.h"
#include "fw_dbglog_api.h"
#include "ol_ath_ucfg.h"
#include <target_if.h>
#define IF_ID_OFFLOAD (1)
#if ATH_PERF_PWR_OFFLOAD
#if WLAN_SPECTRAL_ENABLE
#include <target_if_spectral.h>
#endif
#include "osif_private.h"

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include <osif_nss_wifiol_if.h>
#include "osif_nss_wifiol_vdev_if.h"
#endif
#include <ieee80211_ioctl_acfg.h>
#if QCA_AIRTIME_FAIRNESS
#include <target_if_atf.h>
#endif /* QCA_AIRTIME_FAIRNESS */
#include "cdp_txrx_ctrl.h"
#include "cdp_txrx_cmn_struct.h"
#include <wlan_lmac_if_api.h>
#include <init_deinit_lmac.h>
#include "dp_txrx.h"
#include "target_type.h"
#include <wlan_utility.h>
#include <ol_regdomain_common.h>
#include <wlan_reg_ucfg_api.h>
/*The value of the threshold is compared against the OBSS RSSI in dB.
* It is a 8-bit value whose
* range is -128 to 127 (after two's complement operation).
* For example, if the parameter value is 0xF5, the target will
* allow spatial reuse if the RSSI detected from other BSS
* is below -10 dB.
*/
#define AP_OBSS_PD_LOWER_THRESH 0
#define AP_OBSS_PD_UPPER_THRESH 255
#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_utils_api.h>
#endif
#include <wlan_vdev_mgr_ucfg_api.h>

#if QCA_SUPPORT_AGILE_DFS
#include <wlan_dfs_ucfg_api.h>
#endif

#ifdef WLAN_DFS_PRECAC_AUTO_CHAN_SUPPORT
#include "../../../cmn_dev/umac/dfs/core/src/dfs_zero_cac.h"
#endif

#if WLAN_CFR_ENABLE
#include <wlan_cfr_ucfg_api.h>
#endif

#include "cfg_ucfg_api.h"

#if ATH_SUPPORT_HYFI_ENHANCEMENTS || ATH_SUPPORT_DSCP_OVERRIDE
/* Do we need to move these to some appropriate header */
void ol_ath_set_hmmc_tid(struct ieee80211com *ic , u_int32_t tid);
void ol_ath_set_hmmc_dscp_override(struct ieee80211com *ic , u_int32_t val);
void ol_ath_set_hmmc_tid(struct ieee80211com *ic , u_int32_t tid);


u_int32_t ol_ath_get_hmmc_tid(struct ieee80211com *ic);
u_int32_t ol_ath_get_hmmc_dscp_override(struct ieee80211com *ic);
#endif
void ol_ath_reset_vap_stat(struct ieee80211com *ic);
uint32_t promisc_is_active (struct ieee80211com *ic);

extern int ol_ath_target_start(ol_ath_soc_softc_t *soc);

void ol_pdev_set_tid_override_queue_mapping(ol_txrx_pdev_handle pdev, int value);
int ol_pdev_get_tid_override_queue_mapping(ol_txrx_pdev_handle pdev);

void ol_ath_wlan_n_cw_interference_handler(struct ol_ath_softc_net80211 *scn,
                                           A_UINT32 interference_type);

static u_int32_t ol_ath_net80211_get_total_per(struct ieee80211com *ic)
{
    u_int32_t total_per = ic->ic_get_tx_hw_retries(ic);

    if ( total_per == 0) {
    return 0;
    }

    return (total_per);
}

#ifndef QCA_WIFI_QCA8074_VP
bool ol_ath_validate_chainmask(struct ol_ath_softc_net80211 *scn,
        uint32_t chainmask, int direction, int phymode)
{
    struct wlan_objmgr_psoc *psoc = scn->soc->psoc_obj;
    struct wlan_psoc_host_service_ext_param *ext_param;
    struct target_psoc_info *tgt_hdl;
    uint8_t pdev_idx;
    struct wlan_psoc_host_mac_phy_caps *mac_phy_cap = NULL;
    struct wlan_psoc_host_mac_phy_caps *mac_phy_cap_arr = NULL;
    struct wlan_psoc_host_chainmask_table *table = NULL;
    struct wlan_psoc_host_chainmask_capabilities *capability = NULL;
#if QCA_SUPPORT_AGILE_DFS
    struct wlan_objmgr_pdev *pdev = NULL;
#endif
    int j = 0;
    bool is_2g_band_supported = false;
    bool is_5g_band_supported = false;
    uint32_t table_id = 0;
    enum ieee80211_cwm_width ch_width;

    tgt_hdl = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(psoc);
    if (!tgt_hdl) {
    	qdf_info("%s: psoc target_psoc_info is null", __func__);
    	return false;
    }

    ext_param = &(tgt_hdl->info.service_ext_param);

    pdev_idx = lmac_get_pdev_idx(scn->sc_pdev);
    mac_phy_cap_arr = target_psoc_get_mac_phy_cap(tgt_hdl);
    mac_phy_cap = &mac_phy_cap_arr[pdev_idx];

    /* get table ID for a given pdev */
    table_id = mac_phy_cap->chainmask_table_id;

    /* table */
    table =  &(ext_param->chainmask_table[table_id]);

    /* Return if table is null, usually should be false */
    if (!table->cap_list){
        qdf_info("%s: Returning due to null table", __func__);
        return false;
    }

    for (j = 0; j < table->num_valid_chainmasks; j++) {
        if (table->cap_list[j].chainmask != chainmask) {
            continue;
        } else {
            capability = &(table->cap_list[j]);

            QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO_LOW,
                    "chainmask num %d: 0x%08x \n",j, capability->chainmask);
            QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO_LOW,
                    "\t supports_chan_width_20: %u \n", capability->supports_chan_width_20);
            QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO_LOW,
                    "\t supports_chan_width_40: %u \n", capability->supports_chan_width_40);
            QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO_LOW,
                    "\t supports_chan_width_80: %u \n", capability->supports_chan_width_80);
            QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO_LOW,
                    "\t supports_chan_width_160: %u \n", capability->supports_chan_width_160);
            QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO_LOW,
                    "\t supports_chan_width_80P80: %u \n", capability->supports_chan_width_80P80);
            QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO_LOW,
                    "\t chain_mask_2G: %u \n", capability->chain_mask_2G);
            QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO_LOW,
                    "\t chain_mask_5G: %u \n", capability->chain_mask_5G);
            QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO_LOW,
                    "\t chain_mask_tx: %u \n", capability->chain_mask_tx);
            QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO_LOW,
                    "\t chain_mask_rx: %u \n", capability->chain_mask_rx);
            QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO_LOW,
                    "\t supports_aDFS: %u \n",  capability->supports_aDFS);
            break;
        }
    }

    if (capability == NULL) {
        QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO_LOW,
                    "Chain Mask is not available for radio \n");
        return false;
    }

    /*
     * 1. check given chain mask supports tx/rx.
     * 2. check given chain mask supports 2G/5G.
     * 3. check given chain mask supports current phymode
     *      20, 40, 80, 160, 80P80.
     * 4. Check given chain mask supprts aDFS.
     * Once given chain mask validation is done, Calculate NSS and update nss for radio.
     */

    if (direction == VALIDATE_TX_CHAINMASK) {
        if (!capability->chain_mask_tx) {
            QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO_LOW,
                    "%s: chain mask does not support TX \n", __func__);
            return false;
        }
    }

    if (direction == VALIDATE_RX_CHAINMASK) {
        if (!capability->chain_mask_rx) {
            QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO_LOW,
                    "%s: chain mask does not support RX \n", __func__);
            return false;
        }
    }

    if ((mac_phy_cap->supported_bands & WMI_HOST_WLAN_2G_CAPABILITY) && capability->chain_mask_2G) {
        is_2g_band_supported = true;
    }
    if ((mac_phy_cap->supported_bands & WMI_HOST_WLAN_5G_CAPABILITY) && capability->chain_mask_5G) {
        is_5g_band_supported = true;
    }

    if (phymode == IEEE80211_MODE_AUTO) {
        if (!is_2g_band_supported && !is_5g_band_supported)
            return false;
        else
            return true;
    }

    /* check BAND for a given chain mask */
    if (is_phymode_2G(phymode)) {
        if (!capability->chain_mask_2G) {
            QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO_LOW,
                    "%s: Invalid chain mask for mode: %d 2.4G band not supported\n", __func__, phymode);
            return false;
        }
    }

    if (is_phymode_5G(phymode)) {
        if (!capability->chain_mask_5G) {
            QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO_LOW,
                    "%s: Invalid chain mask for mode: %d 5G band not supported\n", __func__, phymode);
            return false;
        }
    }

    /* check channel width for a given chain mask */
    ch_width = get_chwidth_phymode(phymode);
    switch(ch_width)
    {
        case IEEE80211_CWM_WIDTH20:
            if (!capability->supports_chan_width_20) {
                QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO_LOW,
                        "%s: Invalid chain mask for mode: %d chwidth20 not supported\n", __func__, phymode);
                return false;
            }
            break;
        case IEEE80211_CWM_WIDTH40:
            if (!capability->supports_chan_width_40) {
                QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO_LOW,
                        "%s: Invalid chain mask for mode: %d chwidth40 not supported\n", __func__, phymode);
                return false;
            }
            break;
        case IEEE80211_CWM_WIDTH80:
            if (!capability->supports_chan_width_80) {
                QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO_LOW,
                        "%s: Invalid chain mask for mode: %d chwidth80 not supported\n", __func__, phymode);
                return false;
            }
            break;
        case IEEE80211_CWM_WIDTH160:
            if (!capability->supports_chan_width_160) {
                QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO_LOW,
                        "%s: Invalid chain mask for mode: %d chwidth160 not supported\n", __func__, phymode);
                return false;
            }
            break;
        case IEEE80211_CWM_WIDTH80_80:
            if (!capability->supports_chan_width_80P80) {
                QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO_LOW,
                        "%s: Invalid chain mask for mode: %d chwidth80+80 not supported\n", __func__, phymode);
                return false;
            }

        default:
            break;

    }

#if QCA_SUPPORT_AGILE_DFS

    pdev = scn->sc_pdev;
    if (is_phymode_5G(phymode)) {

        int is_precac_enabled;
        ucfg_dfs_get_precac_enable(pdev, &is_precac_enabled);
        if (is_precac_enabled) {
            if (!capability->supports_aDFS) {
                QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO_LOW,
                          "%s:Invalid chain mask for mode: %d aDFS not supported\n", __func__,phymode);
                return false;
            }
        }
    }

#endif

    return true;
}
#else
bool ol_ath_validate_chainmask(struct ol_ath_softc_net80211 *scn,
        uint32_t chainmask, int direction, int phymode)
{
       return true;
}
#endif

void
ol_ath_dump_chainmaks_tables(struct ol_ath_softc_net80211 *scn)
{
    struct wlan_objmgr_psoc *psoc = scn->soc->psoc_obj;
    struct wlan_psoc_host_service_ext_param *ext_param;
    struct target_psoc_info *tgt_hdl;
    uint8_t pdev_idx;
    struct wlan_psoc_host_chainmask_table *table = NULL;
    struct wlan_psoc_host_mac_phy_caps *mac_phy_cap_arr = NULL;
    struct wlan_psoc_host_mac_phy_caps *mac_phy_cap = NULL;
    int j = 0, table_id = 0;

    tgt_hdl = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(psoc);
    if(!tgt_hdl) {
    	qdf_info("%s: psoc target_psoc_info is null", __func__);
    	return;
    }

    mac_phy_cap_arr = target_psoc_get_mac_phy_cap(tgt_hdl);
    pdev_idx = lmac_get_pdev_idx(scn->sc_pdev);
    mac_phy_cap = &mac_phy_cap_arr[pdev_idx];
    ext_param = target_psoc_get_service_ext_param(tgt_hdl);

    /* get table ID for a given pdev */
    table_id = mac_phy_cap->chainmask_table_id;

    table =  &(ext_param->chainmask_table[table_id]);
    qdf_info("------------- table ID: %d --------------- ", table->table_id);
    qdf_info("num valid chainmasks: %d ", table->num_valid_chainmasks);
    for (j = 0; j < table->num_valid_chainmasks; j++) {
        qdf_info("chainmask num %d: 0x%08x ",j, table->cap_list[j].chainmask);
        qdf_info("\t supports_chan_width_20: %u ", table->cap_list[j].supports_chan_width_20);
        qdf_info("\t supports_chan_width_40: %u ", table->cap_list[j].supports_chan_width_40);
        qdf_info("\t supports_chan_width_80: %u ", table->cap_list[j].supports_chan_width_80);
        qdf_info("\t supports_chan_width_160: %u ", table->cap_list[j].supports_chan_width_160);
        qdf_info("\t supports_chan_width_80P80: %u ", table->cap_list[j].supports_chan_width_80P80);
        qdf_info("\t chain_mask_2G: %u ", table->cap_list[j].chain_mask_2G);
        qdf_info("\t chain_mask_5G: %u ", table->cap_list[j].chain_mask_5G);
        qdf_info("\t chain_mask_tx: %u ", table->cap_list[j].chain_mask_tx);
        qdf_info("\t chain_mask_rx: %u ", table->cap_list[j].chain_mask_rx);
        qdf_info("\t supports_aDFS: %u ",  table->cap_list[j].supports_aDFS);
    }
}


int config_txchainmask(struct ieee80211com *ic, struct ieee80211_ath_channel *chan)
{
        struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct wlan_vdev_mgr_cfg mlme_cfg;
    int retval = 0;
    uint32_t iv_nss;

    retval = ol_ath_pdev_set_param(scn, wmi_pdev_param_tx_chain_mask,
                                   scn->user_config_val);

    if (retval == EOK) {
        u_int8_t  nss;
        struct ieee80211vap *tmpvap = NULL;
        /* Update the ic_chainmask */
        ieee80211com_set_tx_chainmask(ic, (u_int8_t)(scn->user_config_val));
        /* Get nss from configured tx chainmask */
        nss = ieee80211_getstreams(ic, ic->ic_tx_chainmask);

        TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
            u_int8_t retv;
            /* Update the iv_nss before restart the vap by sending WMI CMD to FW
               to configure the NSS */
            if(ic->ic_is_mode_offload(ic)) {
                if (ic->ic_vap_set_param) {
                    retv = wlan_set_param(tmpvap,IEEE80211_FIXED_NSS,nss);
                    if (retv == EOK) {
                        mlme_cfg.value = nss;
                        ucfg_wlan_vdev_mgr_set_param(tmpvap->vdev_obj, WLAN_MLME_CFG_NSS,
                                    mlme_cfg);
                    } else {
                        ucfg_wlan_vdev_mgr_get_param(tmpvap->vdev_obj, WLAN_MLME_CFG_NSS,
					                                    &iv_nss);
                        qdf_info("%s: vap %d :%pK Failed to configure NSS from %d to %d ",
                                __func__,tmpvap->iv_unit,tmpvap,iv_nss,nss);
                    }
                }
            }
        }
    }

    return 0;
}

int config_rxchainmask(struct ieee80211com *ic, struct ieee80211_ath_channel *chan)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    int retval = 0;


    retval = ol_ath_pdev_set_param(scn, wmi_pdev_param_rx_chain_mask,
                                   scn->user_config_val);

    if (retval == EOK) {
        /* Update the ic_chainmask */
        ieee80211com_set_rx_chainmask(ic, (u_int8_t)(scn->user_config_val));
    }

    return 0;
}
#define MAX_ANTENNA_GAIN 30
int
ol_ath_set_config_param(struct ol_ath_softc_net80211 *scn,
        enum _ol_ath_param_t param, void *buff, bool *restart_vaps)
{
    int retval = 0;
    u_int32_t value = *(u_int32_t *)buff, param_id;
    struct ieee80211com *ic = &scn->sc_ic;
    struct ieee80211vap *tmp_vap = NULL;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
	int thresh = 0;
#endif
#if DBDC_REPEATER_SUPPORT
    struct ieee80211com *tmp_ic = NULL;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    struct ol_ath_softc_net80211 *tmp_scn = NULL;
    u_int32_t nss_soc_cfg;
#endif
    int i = 0;
    struct ieee80211com *fast_lane_ic;
#endif
#if ATH_SUPPORT_WRAP && QCA_NSS_WIFI_OFFLOAD_SUPPORT
    osif_dev *osd = NULL;
    struct ieee80211vap *mpsta_vap = NULL;
#endif
    struct wlan_objmgr_pdev *pdev;
    struct wlan_objmgr_psoc *psoc;
#if QCA_AIRTIME_FAIRNESS
    int atf_sched = 0;
#endif

#if ATH_SUPPORT_DFS
    struct wlan_lmac_if_dfs_rx_ops *dfs_rx_ops;
#endif

#ifdef OBSS_PD
    struct ieee80211_vap_opmode_count vap_opmode_count;
#endif
    target_resource_config *tgt_cfg;
    ol_txrx_pdev_handle pdev_txrx_handle;
    ol_txrx_soc_handle soc_txrx_handle;
    struct wlan_psoc_host_hal_reg_capabilities_ext *reg_cap;
    uint8_t pdev_idx;
    ol_ath_soc_softc_t *soc = scn->soc;
    struct common_wmi_handle *wmi_handle;
    struct common_wmi_handle *pdev_wmi_handle;
#ifdef OBSS_PD
    struct ieee80211vap *vap = NULL;
#endif
    wmi_handle = lmac_get_wmi_hdl(scn->soc->psoc_obj);
    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    pdev = ic->ic_pdev_obj;
    if(pdev == NULL) {
        qdf_info("%s : pdev is NULL", __func__);
        return -1;
    }

    psoc = wlan_pdev_get_psoc(pdev);

    if (psoc == NULL) {
        qdf_info("%s : psoc is NULL", __func__);
        return -1;
    }

    soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
    pdev_txrx_handle = wlan_pdev_get_dp_handle(pdev);
    tgt_cfg = lmac_get_tgt_res_cfg(psoc);
    if (!tgt_cfg) {
        qdf_info("%s: psoc target res cfg is null", __func__);
        return -1;
    }
#if ATH_SUPPORT_ZERO_CAC_DFS
    dfs_rx_ops = wlan_lmac_if_get_dfs_rx_ops(psoc);
#endif

    reg_cap = ucfg_reg_get_hal_reg_cap(psoc);
    pdev_idx = lmac_get_pdev_idx(pdev);
    qdf_assert_always(restart_vaps != NULL);

    if (atomic_read(&scn->soc->reset_in_progress)) {
        qdf_info("Reset in progress, return");
        return -1;
    }

    if (scn->soc->down_complete) {
        qdf_info("Starting the target before sending the command");
        if (ol_ath_target_start(scn->soc)) {
               qdf_info("failed to start the target");
               return -1;
        }
    }

    switch(param)
    {
        case OL_ATH_PARAM_TXCHAINMASK:
        {
            u_int8_t cur_mask = ieee80211com_get_tx_chainmask(ic);
            enum ieee80211_phymode phymode = IEEE80211_MODE_AUTO;

            if (!value) {
                /* value is 0 - set the chainmask to be the default
                 * supported tx_chain_mask value
                 */
                if (cur_mask == tgt_cfg->tx_chain_mask){
                    break;
                }
                scn->user_config_val = tgt_cfg->tx_chain_mask;
                osif_restart_for_config(ic, config_txchainmask, NULL);
            } else if (cur_mask != value) {
                /* Update chainmask only if the current chainmask is different */

                if (ol_target_lithium(scn->soc->psoc_obj)) {

                    if (TAILQ_EMPTY(&ic->ic_vaps)) {
                        /* No vaps present, make phymode as AUTO */
                        phymode = IEEE80211_MODE_AUTO;
                    } else {
                        /*
                         * Currently phymode is per radio and it is same for all VAPS.
                         * So first vap mode alone fine for validaitons.
                         * Note: In future if we support different phymode per VAP then
                         * we need to check for all VAPS here or have per VAP chainmasks.
                         */
                        tmp_vap = TAILQ_FIRST(&ic->ic_vaps);
                        phymode = tmp_vap->iv_des_mode;
                    }

                    if (!ol_ath_validate_chainmask(scn, value, VALIDATE_TX_CHAINMASK, phymode)) {
                        qdf_info("Invalid TX chain mask: %d for phymode: %d", value, phymode);
                        return -1;
                    }
                } else if (value > tgt_cfg->tx_chain_mask) {
                    qdf_info("ERROR - value is greater than supported chainmask 0x%x \n",
                            tgt_cfg->tx_chain_mask);
                    return -1;
                }
                scn->user_config_val = value;
                osif_restart_for_config(ic, config_txchainmask, NULL);
            }
        }
        break;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS && ATH_SUPPORT_DSCP_OVERRIDE
		case OL_ATH_PARAM_HMMC_DSCP_TID_MAP:
			ol_ath_set_hmmc_tid(ic,value);
		break;

		case OL_ATH_PARAM_HMMC_DSCP_OVERRIDE:
			ol_ath_set_hmmc_dscp_override(ic,value);
        break;
#endif
        case OL_ATH_PARAM_RXCHAINMASK:
        {
            u_int8_t cur_mask = ieee80211com_get_rx_chainmask(ic);
            enum ieee80211_phymode phymode = IEEE80211_MODE_AUTO;
#if WLAN_SPECTRAL_ENABLE
            u_int8_t spectral_rx_chainmask;
#endif
            if (!value) {
                /* value is 0 - set the chainmask to be the default
                 * supported rx_chain_mask value
                 */
                if (cur_mask == tgt_cfg->rx_chain_mask){
                    break;
                }
                scn->user_config_val = tgt_cfg->rx_chain_mask;
                osif_restart_for_config(ic, config_rxchainmask, NULL);
            } else if (cur_mask != value) {
                /* Update chainmask only if the current chainmask is different */

                if (ol_target_lithium(scn->soc->psoc_obj)) {
                    if (TAILQ_EMPTY(&ic->ic_vaps)) {
                        /* No vaps present, make phymode as AUTO */
                        phymode = IEEE80211_MODE_AUTO;
                    } else {
                        /*
                         * Currently phymode is per radio and it is same for all VAPS.
                         * So first vap mode alone fine for validaitons.
                         * Note: In future if we support different phymode per VAP then
                         * we need to check for all VAPS here or have per VAP chainmasks.
                         */
                        tmp_vap = TAILQ_FIRST(&ic->ic_vaps);
                        phymode = tmp_vap->iv_des_mode;
                    }

                    if (!ol_ath_validate_chainmask(scn, value, VALIDATE_RX_CHAINMASK, phymode)) {
                        qdf_info("Invalid RX chain mask: %d for phymode ", value, phymode);
                        return -1;
                    }
                } else if (value > tgt_cfg->rx_chain_mask) {
                    qdf_info("ERROR - value is greater than supported chainmask 0x%x \n",
                            tgt_cfg->rx_chain_mask);
                    return -1;
                }
                scn->user_config_val = value;
                osif_restart_for_config(ic, config_rxchainmask, NULL);
            }
#if WLAN_SPECTRAL_ENABLE
            qdf_info("Resetting spectral chainmask to Rx chainmask\n");
            spectral_rx_chainmask = ieee80211com_get_rx_chainmask(ic);
            target_if_spectral_set_rxchainmask(ic->ic_pdev_obj, spectral_rx_chainmask);
#endif
        }
        break;
#if QCA_AIRTIME_FAIRNESS
    case  OL_ATH_PARAM_ATF_STRICT_SCHED:
        {
            if ((value != 0) && (value != 1))
            {
                qdf_info("\n ATF Strict Sched value only accept 1 (Enable) or 0 (Disable)!! ");
                return -1;
            }
            atf_sched = target_if_atf_get_sched(psoc, pdev);
            if ((value == 1) && (!(atf_sched & ATF_GROUP_SCHED_POLICY))
               && (target_if_atf_get_ssid_group(psoc, pdev)))
            {
                qdf_info("\nFair queue across groups is enabled so strict queue within groups is not allowed. Invalid combination ");
                return -EINVAL;
            }
            retval = ol_ath_pdev_set_param(scn,  wmi_pdev_param_atf_strict_sch, value);
            if (retval == EOK) {
                    if (value)
                        target_if_atf_set_sched(psoc, pdev, atf_sched | ATF_SCHED_STRICT);
                    else
                        target_if_atf_set_sched(psoc, pdev, atf_sched & ~ATF_SCHED_STRICT);
            }
        }
        break;
    case  OL_ATH_PARAM_ATF_GROUP_POLICY:
        {
            if ((value != 0) && (value != 1))
            {
                qdf_info("\n ATF Group policy value only accept 1 (strict) or 0 (fair)!! ");
                return -1;
            }
            atf_sched = target_if_atf_get_sched(psoc, pdev);
            if ((value == 0) && (atf_sched & ATF_SCHED_STRICT))
            {
                qdf_info("\n Strict queue within groups is enabled so fair queue across groups is not allowed.Invalid combination ");
                return -EINVAL;
            }
            retval = ol_ath_pdev_set_param(scn, wmi_pdev_param_atf_ssid_group_policy, value);
            if (retval == EOK) {
                if (value)
                    target_if_atf_set_sched(psoc, pdev, atf_sched | ATF_GROUP_SCHED_POLICY);
                else
                    target_if_atf_set_sched(psoc, pdev, atf_sched & ~ATF_GROUP_SCHED_POLICY);
            }
        }
        break;
    case  OL_ATH_PARAM_ATF_OBSS_SCHED:
        {
#if 0 /* remove after FW support */
            retval = ol_ath_pdev_set_param(scn,
                                 wmi_pdev_param_atf_obss_noise_sch, !!value);
#endif
            if (retval == EOK) {
                atf_sched = target_if_atf_get_sched(psoc, pdev);
                if (value)
                    target_if_atf_set_sched(psoc, pdev, atf_sched | ATF_SCHED_OBSS);
                else
                    target_if_atf_set_sched(psoc, pdev, atf_sched & ~ATF_SCHED_OBSS);
            }
        }
        break;
    case  OL_ATH_PARAM_ATF_OBSS_SCALE:
        {
#if 0 /* remove after FW support */
            retval = ol_ath_pdev_set_param(scn,
                                 wmi_pdev_param_atf_obss_noise_scaling_factor, value);
#endif
            if (retval == EOK) {
                target_if_atf_set_obss_scale(psoc, pdev, value);
            }
        }
        break;
#endif
        case OL_ATH_PARAM_TXPOWER_LIMIT2G:
        {
            if (!value) {
                value = scn->max_tx_power;
            }
            ic->ic_set_txPowerLimit(ic, value, value, 1);
        }
        break;

        case OL_ATH_PARAM_TXPOWER_LIMIT5G:
        {
            if (!value) {
                value = scn->max_tx_power;
                }
                ic->ic_set_txPowerLimit(ic, value, value, 0);
            }
        break;
        case OL_ATH_PARAM_RTS_CTS_RATE:
        if(value > 4) {
            qdf_info("Invalid value for setctsrate Disabling it in Firmware \n");
            value = WMI_HOST_FIXED_RATE_NONE;
        }
        scn->ol_rts_cts_rate = value;
        return ol_ath_pdev_set_param(scn,
                wmi_pdev_param_rts_fixed_rate,value);
        break;

        case OL_ATH_PARAM_DEAUTH_COUNT:
#if WDI_EVENT_ENABLE
        if(value) {
            scn->scn_user_peer_invalid_cnt = value;
            scn->scn_peer_invalid_cnt = 0;
        }
#endif
        break;

            case OL_ATH_PARAM_TXPOWER_SCALE:
        {
            if((WMI_HOST_TP_SCALE_MAX <= value) && (value <= WMI_HOST_TP_SCALE_MIN))
            {
                scn->txpower_scale = value;
                return ol_ath_pdev_set_param(scn,
                    wmi_pdev_param_txpower_scale, value);
            } else {
                retval = -EINVAL;
            }
        }
        break;
            case OL_ATH_PARAM_PS_STATE_CHANGE:
        {
            (void)ol_ath_pdev_set_param(scn,
                    wmi_pdev_peer_sta_ps_statechg_enable, value);
            scn->ps_report = value;
        }
        break;
        case OL_ATH_PARAM_NON_AGG_SW_RETRY_TH:
        {
                return ol_ath_pdev_set_param(scn,
                             wmi_pdev_param_non_agg_sw_retry_th, value);
        }
        break;
        case OL_ATH_PARAM_AGG_SW_RETRY_TH:
        {
		return ol_ath_pdev_set_param(scn,
				wmi_pdev_param_agg_sw_retry_th, value);
	}
	break;
	case OL_ATH_PARAM_STA_KICKOUT_TH:
	{
		return ol_ath_pdev_set_param(scn,
				wmi_pdev_param_sta_kickout_th, value);
	}
	break;
    case OL_ATH_PARAM_DYN_GROUPING:
    {
        value = !!value;
        if ((scn->dyngroup != (u_int8_t)value) && (ic->ic_dynamic_grouping_support)) {
            retval = ol_ath_pdev_set_param(scn,
                    wmi_pdev_param_mu_group_policy, value);

            if (retval == EOK) {
                scn->dyngroup = (u_int8_t)value;
            }

        } else {
                retval = -EINVAL;
        }
        break;
    }
        case OL_ATH_PARAM_DBGLOG_RATELIM:
        {
                void *dbglog_handle;
                struct target_psoc_info *tgt_psoc_info;

                tgt_psoc_info = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
                                                    scn->soc->psoc_obj);
                if (tgt_psoc_info == NULL) {
                        qdf_info("%s: target_psoc_info is null ", __func__);
                        return -EINVAL;
                }

                if (!(dbglog_handle = target_psoc_get_dbglog_hdl(tgt_psoc_info))) {
                        qdf_info("%s: dbglog_handle is null ", __func__);
                        return -EINVAL;
                }

                fwdbg_ratelimit_set(dbglog_handle, value);
        }
        break;
	case OL_ATH_PARAM_BCN_BURST:
	{
		/* value is set to either 1 (bursted) or 0 (staggered).
		 * if value passed is non-zero, convert it to 1 with
                 * double negation
                 */
                value = !!value;
                if (ieee80211_vap_is_any_running(ic)) {
                    qdf_info("%s: VAP(s) in running state. Cannot change between burst/staggered beacon modes",
                              __func__);
                    retval = -EINVAL;
                    break;
                }
                if (scn->bcn_mode != (u_int8_t)value) {
		    if (wlan_pdev_nif_feat_cap_get(ic->ic_pdev_obj,
                                                  WLAN_PDEV_F_MBSS_IE_ENABLE)) {
		        if (value != 1) {
		            qdf_err("Disabling bursted mode not allowed when MBSS feature is enabled");
			    retval = -EINVAL;
                            break;
		        }
		    }
                    retval = ol_ath_pdev_set_param(scn,
                                 wmi_pdev_param_beacon_tx_mode, value);
                    if (retval == EOK) {
                        scn->bcn_mode = (u_int8_t)value;
                        *restart_vaps = TRUE;
                    }
                }
                break;
        }
        break;
    case OL_ATH_PARAM_DPD_ENABLE:
        {
            value = !!value;
            if ((scn->dpdenable != (u_int8_t)value) && (ic->ic_dpd_support)) {
                retval = ol_ath_pdev_set_param(scn,
                        wmi_pdev_param_dpd_enable, value);
                if (retval == EOK) {
                    scn->dpdenable = (u_int8_t)value;
                }
            } else {
                retval = -EINVAL;
            }
        }
        break;

    case OL_ATH_PARAM_ARPDHCP_AC_OVERRIDE:
        {
            if ((WME_AC_BE <= value) && (value <= WME_AC_VO)) {
                scn->arp_override = value;
                retval = ol_ath_pdev_set_param(scn,
                        wmi_pdev_param_arp_ac_override, value);
            } else {
                retval = -EINVAL;
            }
        }
        break;

        case OL_ATH_PARAM_IGMPMLD_OVERRIDE:
            if ((0 == value) || (value == 1)) {
                scn->igmpmld_override = value;
                cdp_txrx_set_pdev_param(soc_txrx_handle, (struct cdp_pdev *)pdev_txrx_handle,
                        CDP_CONFIG_IGMPMLD_OVERRIDE, value);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
                if (ic->nss_radio_ops)
                    ic->nss_radio_ops->ic_nss_ol_set_igmpmld_override_tos(scn);
#endif
                retval = ol_ath_pdev_set_param(scn,
                    wmi_pdev_param_igmpmld_override, value);
            } else {
                retval = -EINVAL;
            }
        break;
        case OL_ATH_PARAM_IGMPMLD_TID:
        if ((0 <= value) && (value <= 7)) {
            scn->igmpmld_tid = value;
            cdp_txrx_set_pdev_param(soc_txrx_handle, (struct cdp_pdev *)pdev_txrx_handle,
                    CDP_CONFIG_IGMPMLD_TID, value);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
                if (ic->nss_radio_ops)
                    ic->nss_radio_ops->ic_nss_ol_set_igmpmld_override_tos(scn);
#endif
                retval = ol_ath_pdev_set_param(scn,
                    wmi_pdev_param_igmpmld_tid, value);
            } else {
                retval = -EINVAL;
            }
        break;
        case OL_ATH_PARAM_ANI_ENABLE:
        {
                if (value <= 1) {
                    retval = ol_ath_pdev_set_param(scn,
                             wmi_pdev_param_ani_enable, value);
                } else {
                    retval = -EINVAL;
                }
                if (retval == EOK) {
                    if (!value) {
                        scn->is_ani_enable = false;
                    } else {
                        scn->is_ani_enable = true;
                    }
                }
        }
        break;
        case OL_ATH_PARAM_ANI_POLL_PERIOD:
        {
                if (value > 0) {
                    return ol_ath_pdev_set_param(scn,
                             wmi_pdev_param_ani_poll_period, value);
                } else {
                    retval = -EINVAL;
                }
        }
        break;
        case OL_ATH_PARAM_ANI_LISTEN_PERIOD:
        {
                if (value > 0) {
                    return ol_ath_pdev_set_param(scn,
                             wmi_pdev_param_ani_listen_period, value);
                } else {
                    retval = -EINVAL;
                }
        }
        break;
        case OL_ATH_PARAM_ANI_OFDM_LEVEL:
        {
                return ol_ath_pdev_set_param(scn,
                       wmi_pdev_param_ani_ofdm_level, value);
        }
        break;
        case OL_ATH_PARAM_ANI_CCK_LEVEL:
        {
                return ol_ath_pdev_set_param(scn,
                       wmi_pdev_param_ani_cck_level, value);
        }
        break;
        case OL_ATH_PARAM_BURST_DUR:
        {
            /* In case of Lithium based targets, value = 0 is allowed for
             * burst_dur, whereas for default case, minimum value is 1 and
             * should return error if value  = 0.
             */
            if((value >= ic->ic_burst_min) && (value <= 8192)) {
                retval = ol_ath_pdev_set_param(scn,
                         wmi_pdev_param_burst_dur, value);
                if (retval == EOK)
                    scn->burst_dur = (u_int16_t)value;
            } else {
                retval = -EINVAL;
            }
        }
        break;

        case OL_ATH_PARAM_BURST_ENABLE:
        {
                if (value == 0 || value ==1) {
                    retval = ol_ath_pdev_set_param(scn,
                             wmi_pdev_param_burst_enable, value);

                    if (retval == EOK) {
                        scn->burst_enable = (u_int8_t)value;
                    }
		    if(!scn->burst_dur)
		    {
			retval = ol_ath_pdev_set_param(scn,
                             wmi_pdev_param_burst_dur, 8160);
			if (retval == EOK) {
			    scn->burst_dur = (u_int16_t)value;
			}
		    }
                } else {
                    retval = -EINVAL;
                }
        }
        break;

        case OL_ATH_PARAM_CCA_THRESHOLD:
        {
            if (value > -11) {
                value = -11;
            } else if (value < -94) {
                value = -94;
            }
            if (value) {
                retval = ol_ath_pdev_set_param(scn,
                        wmi_pdev_param_cca_threshold, value);
                if (retval == EOK) {
                    scn->cca_threshold = (u_int16_t)value;
                }
            } else {
                retval = -EINVAL;
            }
        }
        break;

        case OL_ATH_PARAM_DCS:
            {
                value &= OL_ATH_CAP_DCS_MASK;
                if ((value & OL_ATH_CAP_DCS_WLANIM) && !(IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan))) {
                    qdf_info("Disabling DCS-WLANIM for 11G mode\n");
                    value &= (~OL_ATH_CAP_DCS_WLANIM);
                }
                /*
                 * Host and target should always contain the same value. So
                 * avoid talking to target if the values are same.
                 */
                if (value == OL_IS_DCS_ENABLED(scn->scn_dcs.dcs_enable)) {
                    retval = EOK;
                    break;
                }
                /* if already enabled and run state is not running, more
                 * likely that channel change is in progress, do not let
                 * user modify the current status
                 */
                if ((OL_IS_DCS_ENABLED(scn->scn_dcs.dcs_enable)) &&
                        !(OL_IS_DCS_RUNNING(scn->scn_dcs.dcs_enable))) {
                    retval = EINVAL;
                    break;
                }
                retval = ol_ath_pdev_set_param(scn,
                                wmi_pdev_param_dcs, value);

                /*
                 * we do not expect this to fail, if failed, eventually
                 * target and host may not be at agreement. Otherway is
                 * to keep it in same old state.
                 */
                if (EOK == retval) {
                    scn->scn_dcs.dcs_enable = value;
                    qdf_info("DCS: %s dcs enable value %d return value %d", __func__, value, retval );
                } else {
                    qdf_info("DCS: %s target command fail, setting return value %d",
                            __func__, retval );
                }
                (OL_IS_DCS_ENABLED(scn->scn_dcs.dcs_enable)) ? (OL_ATH_DCS_SET_RUNSTATE(scn->scn_dcs.dcs_enable)) :
                                        (OL_ATH_DCS_CLR_RUNSTATE(scn->scn_dcs.dcs_enable));
            }
            break;
        case OL_ATH_PARAM_DCS_SIM:
            switch (value) {
            case ATH_CAP_DCS_CWIM: /* cw interferecne*/
                if (OL_IS_DCS_ENABLED(scn->scn_dcs.dcs_enable) & OL_ATH_CAP_DCS_CWIM) {
                    ol_ath_wlan_n_cw_interference_handler(scn, value);
                }
                break;
            case ATH_CAP_DCS_WLANIM: /* wlan interference stats*/
                if (OL_IS_DCS_ENABLED(scn->scn_dcs.dcs_enable) & OL_ATH_CAP_DCS_WLANIM) {
                    ol_ath_wlan_n_cw_interference_handler(scn, value);
                }
                break;
            default:
                break;
            }
            break;
        case OL_ATH_PARAM_DCS_COCH_THR:
            scn->scn_dcs.coch_intr_thresh = value;
            break;
        case OL_ATH_PARAM_DCS_TXERR_THR:
            scn->scn_dcs.tx_err_thresh = value;
            break;
        case OL_ATH_PARAM_DCS_PHYERR_THR:
            scn->scn_dcs.phy_err_threshold = value;
            break;
        case OL_ATH_PARAM_DCS_PHYERR_PENALTY:
            scn->scn_dcs.phy_err_penalty = value;         /* phy error penalty*/
            break;
        case OL_ATH_PARAM_DCS_RADAR_ERR_THR:
            scn->scn_dcs.radar_err_threshold = value;
            break;
        case OL_ATH_PARAM_DCS_USERMAX_CU_THR:
            scn->scn_dcs.user_max_cu = value;             /* tx_cu + rx_cu */
            break;
        case OL_ATH_PARAM_DCS_INTR_DETECT_THR:
            scn->scn_dcs.intr_detection_threshold = value;
            break;
        case OL_ATH_PARAM_DCS_SAMPLE_WINDOW:
            scn->scn_dcs.intr_detection_window = value;
            break;
        case OL_ATH_PARAM_DCS_DEBUG:
            if (value < 0 || value > 2) {
                qdf_info("0-disable, 1-critical 2-all, %d-not valid option\n", value);
                return -EINVAL;
            }
            scn->scn_dcs.dcs_debug = value;
            break;

        case OL_ATH_PARAM_DYN_TX_CHAINMASK:
            /****************************************
             *Value definition:
             * bit 0        dynamic TXCHAIN
             * bit 1        single TXCHAIN
             * bit 2        single TXCHAIN for ctrl frames
             * For bit 0-1, if value =
             * 0x1  ==>   Dyntxchain enabled,  single_txchain disabled
             * 0x2  ==>   Dyntxchain disabled, single_txchain enabled
             * 0x3  ==>   Both enabled
             * 0x0  ==>   Both disabled
             *
             * bit 3-7      reserved
             * bit 8-11     single txchain mask, only valid if bit 1 set
             *
             * For bit 8-11, the single txchain mask for this radio,
             * only valid if single_txchain enabled, by setting bit 1.
             * Single txchain mask need to be updated when txchainmask,
             * is changed, e.g. 4x4(0xf) ==> 3x3(0x7)
             ****************************************/
#define DYN_TXCHAIN         0x1
#define SINGLE_TXCHAIN      0x2
#define SINGLE_TXCHAIN_CTL  0x4
            if( (value & SINGLE_TXCHAIN) ||
                     (value & SINGLE_TXCHAIN_CTL) ){
                value &= 0xf07;
            }else{
                value &= 0x1;
            }

            if (scn->dtcs != value) {
                retval = ol_ath_pdev_set_param(scn,
                                 wmi_pdev_param_dyntxchain, value);
                if (retval == EOK) {
                    scn->dtcs = value;
                }
            }
        break;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
		case OL_ATH_PARAM_BUFF_THRESH:
			thresh = value;
			if((thresh >= MIN_BUFF_LEVEL_IN_PERCENT) && (thresh<=100))
			{
				scn->soc->buff_thresh.ald_free_buf_lvl = scn->soc->buff_thresh.pool_size - ((scn->soc->buff_thresh.pool_size * thresh) / 100);
				qdf_info("Buff Warning Level=%d\n", (scn->soc->buff_thresh.pool_size - scn->soc->buff_thresh.ald_free_buf_lvl));
			} else {
                qdf_info("ERR: Buff Thresh(in %%) should be >=%d and <=100\n", MIN_BUFF_LEVEL_IN_PERCENT);
            }
			break;
		case OL_ATH_PARAM_DROP_STA_QUERY:
			ic->ic_dropstaquery = !!value;
			break;
		case OL_ATH_PARAM_BLK_REPORT_FLOOD:
			ic->ic_blkreportflood = !!value;
			break;
#endif

        case OL_ATH_PARAM_VOW_EXT_STATS:
            {
                scn->vow_extstats = value;
            }
            break;

        case OL_ATH_PARAM_LTR_ENABLE:
            param_id = wmi_pdev_param_ltr_enable;
            goto low_power_config;
        case OL_ATH_PARAM_LTR_AC_LATENCY_BE:
            param_id = wmi_pdev_param_ltr_ac_latency_be;
            goto low_power_config;
        case OL_ATH_PARAM_LTR_AC_LATENCY_BK:
            param_id = wmi_pdev_param_ltr_ac_latency_bk;
            goto low_power_config;
        case OL_ATH_PARAM_LTR_AC_LATENCY_VI:
            param_id = wmi_pdev_param_ltr_ac_latency_vi;
            goto low_power_config;
        case OL_ATH_PARAM_LTR_AC_LATENCY_VO:
            param_id = wmi_pdev_param_ltr_ac_latency_vo;
            goto low_power_config;
        case OL_ATH_PARAM_LTR_AC_LATENCY_TIMEOUT:
            param_id = wmi_pdev_param_ltr_ac_latency_timeout;
            goto low_power_config;
        case OL_ATH_PARAM_LTR_TX_ACTIVITY_TIMEOUT:
            param_id = wmi_pdev_param_ltr_tx_activity_timeout;
            goto low_power_config;
        case OL_ATH_PARAM_LTR_SLEEP_OVERRIDE:
            param_id = wmi_pdev_param_ltr_sleep_override;
            goto low_power_config;
        case OL_ATH_PARAM_LTR_RX_OVERRIDE:
            param_id = wmi_pdev_param_ltr_rx_override;
            goto low_power_config;
        case OL_ATH_PARAM_L1SS_ENABLE:
            param_id = wmi_pdev_param_l1ss_enable;
            goto low_power_config;
        case OL_ATH_PARAM_DSLEEP_ENABLE:
            param_id = wmi_pdev_param_dsleep_enable;
            goto low_power_config;
low_power_config:
            retval = ol_ath_pdev_set_param(scn,
                         param_id, value);
        case OL_ATH_PARAM_ACS_CTRLFLAG:
            if(ic->ic_acs){
                ieee80211_acs_set_param(ic->ic_acs, IEEE80211_ACS_CTRLFLAG , *(int *)buff);
            }
            break;
        case OL_ATH_PARAM_ACS_ENABLE_BK_SCANTIMEREN:
            if(ic->ic_acs){
                ieee80211_acs_set_param(ic->ic_acs, IEEE80211_ACS_ENABLE_BK_SCANTIMER , *(int *)buff);
            }
            break;
        case OL_ATH_PARAM_CBS:
            if (ic->ic_cbs) {
                ieee80211_cbs_set_param(ic->ic_cbs, IEEE80211_CBS_ENABLE , *(int *)buff);
            }
            break;
        case OL_ATH_PARAM_CBS_DWELL_SPLIT_TIME:
            if (ic->ic_cbs) {
                ieee80211_cbs_set_param(ic->ic_cbs, IEEE80211_CBS_DWELL_SPLIT_TIME , *(int *)buff);
            }
            break;
        case OL_ATH_PARAM_CBS_DWELL_REST_TIME:
            if (ic->ic_cbs) {
                ieee80211_cbs_set_param(ic->ic_cbs, IEEE80211_CBS_DWELL_REST_TIME , *(int *)buff);
            }
            break;
        case OL_ATH_PARAM_CBS_REST_TIME:
            if (ic->ic_cbs) {
                ieee80211_cbs_set_param(ic->ic_cbs, IEEE80211_CBS_REST_TIME , *(int *)buff);
            }
            break;
        case OL_ATH_PARAM_CBS_WAIT_TIME:
            if (ic->ic_cbs) {
                ieee80211_cbs_set_param(ic->ic_cbs, IEEE80211_CBS_WAIT_TIME , *(int *)buff);
            }
            break;
        case OL_ATH_PARAM_CBS_CSA:
            if (ic->ic_cbs) {
                ieee80211_cbs_set_param(ic->ic_cbs, IEEE80211_CBS_CSA_ENABLE , *(int *)buff);
            }
            break;
        case OL_ATH_PARAM_ACS_SCANTIME:
            if(ic->ic_acs){
                ieee80211_acs_set_param(ic->ic_acs, IEEE80211_ACS_SCANTIME , *(int *)buff);
            }
            break;
        case OL_ATH_PARAM_ACS_RSSIVAR:
            if(ic->ic_acs){
                ieee80211_acs_set_param(ic->ic_acs, IEEE80211_ACS_RSSIVAR , *(int *)buff);
            }
            break;
        case OL_ATH_PARAM_ACS_CHLOADVAR:
            if(ic->ic_acs){
                ieee80211_acs_set_param(ic->ic_acs, IEEE80211_ACS_CHLOADVAR , *(int *)buff);
            }
            break;
        case OL_ATH_PARAM_ACS_SRLOADVAR:
            if(ic->ic_acs){
                ieee80211_acs_set_param(ic->ic_acs, IEEE80211_ACS_SRLOADVAR , *(int *)buff);
            }
            break;
        case OL_ATH_PARAM_ACS_LIMITEDOBSS:
            if(ic->ic_acs){
                ieee80211_acs_set_param(ic->ic_acs, IEEE80211_ACS_LIMITEDOBSS , *(int *)buff);
            }
            break;
        case OL_ATH_PARAM_ACS_DEBUGTRACE:
            if(ic->ic_acs){
                ieee80211_acs_set_param(ic->ic_acs, IEEE80211_ACS_DEBUGTRACE , *(int *)buff);
            }
             break;
#if ATH_CHANNEL_BLOCKING
        case OL_ATH_PARAM_ACS_BLOCK_MODE:
            if (ic->ic_acs) {
                ieee80211_acs_set_param(ic->ic_acs, IEEE80211_ACS_BLOCK_MODE , *(int *)buff);
            }
            break;
#endif
        case OL_ATH_PARAM_RESET_OL_STATS:
            ol_ath_reset_vap_stat(ic);
            break;
#if ATH_RX_LOOPLIMIT_TIMER
        case OL_ATH_PARAM_LOOPLIMIT_NUM:
            if (*(int *)buff > 0)
                scn->rx_looplimit_timeout = *(int *)buff;
            break;
#endif
#define ANTENNA_GAIN_2G_MASK    0x0
#define ANTENNA_GAIN_5G_MASK    0x8000
        case OL_ATH_PARAM_ANTENNA_GAIN_2G:
            if (value >= 0 && value <= 30) {
                return ol_ath_pdev_set_param(scn,
                         wmi_pdev_param_antenna_gain, value | ANTENNA_GAIN_2G_MASK);
            } else {
                retval = -EINVAL;
            }
            break;
        case OL_ATH_PARAM_ANTENNA_GAIN_5G:
            if (value >= 0 && value <= 30) {
                return ol_ath_pdev_set_param(scn,
                         wmi_pdev_param_antenna_gain, value | ANTENNA_GAIN_5G_MASK);
            } else {
                retval = -EINVAL;
            }
            break;
        case OL_ATH_PARAM_RX_FILTER:
            if (ic->ic_set_rxfilter)
                ic->ic_set_rxfilter(ic, value);
            else
                retval = -EINVAL;
            break;
       case OL_ATH_PARAM_SET_FW_HANG_ID:
            ol_ath_set_fw_hang(scn, value);
            break;
       case OL_ATH_PARAM_FW_RECOVERY_ID:
            if (value >= RECOVERY_DISABLE && value <= RECOVERY_ENABLE_WAIT)
                scn->soc->recovery_enable = value;
            else
                qdf_info("Please enter: 0 = Disable,  1 = Enable (auto recover), 2 = Enable (wait for user)");
            break;
       case OL_ATH_PARAM_FW_DUMP_NO_HOST_CRASH:
            if (value == 1){
                /* Do not crash host when target assert happened */
                /* By default, host will crash when target assert happened */
                scn->soc->sc_dump_opts |= FW_DUMP_NO_HOST_CRASH;
            }else{
                scn->soc->sc_dump_opts &= ~FW_DUMP_NO_HOST_CRASH;
            }
            break;
       case OL_ATH_PARAM_DISABLE_DFS:
            {
                if (!value)
                    scn->sc_is_blockdfs_set = false;
                else
                    scn->sc_is_blockdfs_set = true;
            }
            break;
        case OL_ATH_PARAM_QBOOST:
            {
		        if (!ic->ic_qboost_support)
                    return -EINVAL;
                /*
                 * Host and target should always contain the same value. So
                 * avoid talking to target if the values are same.
                 */
                if (value == scn->scn_qboost_enable) {
                    retval = EOK;
                    break;
                }

                    scn->scn_qboost_enable = value;
                    TAILQ_FOREACH(tmp_vap, &ic->ic_vaps, iv_next) {
                        qboost_config(tmp_vap, tmp_vap->iv_bss, scn->scn_qboost_enable);
                    }

                    qdf_info("QBOOST: %s qboost value %d\n", __func__, value);
            }
            break;
        case OL_ATH_PARAM_SIFS_FRMTYPE:
            {
		        if (!ic->ic_sifs_frame_support)
                    return -EINVAL;
                /*
                 * Host and target should always contain the same value. So
                 * avoid talking to target if the values are same.
                 */
                if (value == scn->scn_sifs_frmtype) {
                    retval = EOK;
                    break;
                }

                    scn->scn_sifs_frmtype = value;

                    qdf_info("SIFS RESP FRMTYPE: %s SIFS  value %d\n", __func__, value);
            }
            break;
        case OL_ATH_PARAM_SIFS_UAPSD:
            {
		        if (!ic->ic_sifs_frame_support)
                    return -EINVAL;
                /*
                 * Host and target should always contain the same value. So
                 * avoid talking to target if the values are same.
                 */
                if (value == scn->scn_sifs_uapsd) {
                    retval = EOK;
                    break;
                }

                    scn->scn_sifs_uapsd = value;

                    qdf_info("SIFS RESP UAPSD: %s SIFS  value %d\n", __func__, value);
            }
            break;
        case OL_ATH_PARAM_BLOCK_INTERBSS:
            {
		        if (!ic->ic_block_interbss_support)
                    return -EINVAL;

                if (value == scn->scn_block_interbss) {
                    retval = EOK;
                    break;
                }
		/* send the WMI command to enable and if that is success update the state */
                retval = ol_ath_pdev_set_param(scn,
                                wmi_pdev_param_block_interbss, value);

                /*
                 * we do not expect this to fail, if failed, eventually
                 * target and host may not be in agreement. Otherway is
                 * to keep it in same old state.
                 */
                if (EOK == retval) {
                    scn->scn_block_interbss = value;
                    qdf_info("set block_interbss: value %d wmi_status %d", value, retval );
                } else {
                    qdf_info("set block_interbss: wmi failed. retval = %d", retval );
                }
	    }
        break;
        case OL_ATH_PARAM_FW_DISABLE_RESET:
        {
		        if (!ic->ic_disable_reset_support)
                    return -EINVAL;
                /* value is set to either 1 (enable) or 0 (disable).
                 * if value passed is non-zero, convert it to 1 with
                 * double negation
                 */
                value = !!value;
                if (scn->fw_disable_reset != (u_int8_t)value) {
                    retval = ol_ath_pdev_set_param(scn,
                                 wmi_pdev_param_set_disable_reset_cmdid, value);
                    if (retval == EOK) {
                        scn->fw_disable_reset = (u_int8_t)value;
                    }
                }
        }
        break;
        case OL_ATH_PARAM_MSDU_TTL:
        {
            if (!ic->ic_msdu_ttl_support)
                return -EINVAL;
            /* value is set to 0 (disable) else set msdu_ttl in ms.
             */
            retval = ol_ath_pdev_set_param(scn,
                                 wmi_pdev_param_set_msdu_ttl_cmdid, value);
            if (retval == EOK) {
                qdf_info("set MSDU_TTL: value %d wmi_status %d", value, retval );
            } else {
                qdf_info("set MSDU_TTL wmi_failed: wmi_status %d", retval );
            }
#if PEER_FLOW_CONTROL
            /* update host msdu ttl */
            cdp_pflow_update_pdev_params(soc_txrx_handle,
                    (void *)pdev_txrx_handle, param, value, NULL);
#endif
        }
        break;
        case OL_ATH_PARAM_PPDU_DURATION:
        {
            if (!ic->ic_ppdu_duration_support)
                return -EINVAL;
            /* Set global PPDU duration in usecs.
             */

            /* In case Lithium based targets, value = 0 is allowed
             * for ppdu_duration, whereas for default case minimum value is 100,
             * should return error if value  = 0.
             */
            if((value < ic->ic_ppdu_min) || (value > 4000))
                return -EINVAL;

            retval = ol_ath_pdev_set_param(scn,
                                 wmi_pdev_param_set_ppdu_duration_cmdid, value);
            if (retval == EOK) {
                qdf_info("set PPDU_DURATION: value %d wmi_status %d", value, retval );
            } else {
                qdf_info("set PPDU_DURATION: wmi_failed: wmi_status %d", retval );
            }
        }
        break;

        case OL_ATH_PARAM_SET_TXBF_SND_PERIOD:
        {
            /* Set global TXBF sounding duration in usecs.
             */
            if(value < 10 || value > 10000)
                return -EINVAL;
            scn->txbf_sound_period = value;
            retval = ol_ath_pdev_set_param(scn,
                                 wmi_pdev_param_txbf_sound_period_cmdid, value);
            if (retval == EOK) {
                qdf_info("set TXBF_SND_PERIOD: value %d wmi_status %d", value, retval );
            } else {
                qdf_info("set TXBF_SND_PERIOD: wmi_failed: wmi_status %d", retval );
            }
        }
        break;

        case OL_ATH_PARAM_ALLOW_PROMISC:
        {
	    if (!ic->ic_promisc_support)
                return -EINVAL;
            /* Set or clear promisc mode.
             */
            if (promisc_is_active(&scn->sc_ic)) {
                qdf_info("Device have an active monitor vap");
                retval = -EINVAL;
            } else if (value == scn->scn_promisc) {
                retval = EOK;
            } else {
                retval = ol_ath_pdev_set_param(scn,
                                 wmi_pdev_param_set_promisc_mode_cmdid, value);
                if (retval == EOK) {
                    scn->scn_promisc = value;
                    qdf_info("set PROMISC_MODE: value %d wmi_status %d", value, retval );
                } else {
                    qdf_info("set PROMISC_MODE: wmi_failed: wmi_status %d", retval );
                }
            }
        }
        break;

        case OL_ATH_PARAM_BURST_MODE:
        {
		    if (!ic->ic_burst_mode_support)
                return -EINVAL;
            /* Set global Burst mode data-cts:0 data-ping-pong:1 data-cts-ping-pong:2.
             */
	    if(value < 0 || value >= 3) {
                qdf_info("Usage: burst_mode <0:data-cts 1:data-data 2:data-(data/cts)\n");
		return -EINVAL;
            }

            retval = ol_ath_pdev_set_param(scn,
                                 wmi_pdev_param_set_burst_mode_cmdid, value);
            if (retval == EOK) {
                qdf_info("set BURST_MODE: value %d wmi_status %d", value, retval );
            } else {
                qdf_info("set BURST_MODE: wmi_failed: wmi_status %d", retval );
            }
        }
        break;

#if ATH_SUPPORT_WRAP
         case OL_ATH_PARAM_MCAST_BCAST_ECHO:
        {
            /* Set global Burst mode data-cts:0 data-ping-pong:1 data-cts-ping-pong:2.
             */
            if(value < 0 || value > 1) {
                qdf_info("Usage: Mcast Bcast Echo mode usage  <0:disable 1:enable \n");
                return -EINVAL;
            }

            retval = ol_ath_pdev_set_param(scn,
                                 wmi_pdev_param_set_mcast_bcast_echo, value);
            if (retval == EOK) {
                qdf_info("set : Mcast Bcast Echo value %d wmi_status %d", value, retval );
                scn->mcast_bcast_echo = (u_int8_t)value;
            } else {
                qdf_info("set : Mcast Bcast Echo mode wmi_failed: wmi_status %d", retval );
            }
        }
        break;

        case OL_ATH_PARAM_ISOLATION:
            if(value < 0 || value > 1) {
                qdf_err("Usage: wrap_isolation mode usage  <0:disable 1:enable \n");
                return -EINVAL;
            }
            ic->ic_wrap_com->wc_isolation = value;
            qdf_info("Set: Qwrap isolation mode value %d", value);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
            mpsta_vap = ic->ic_mpsta_vap;
            if((value == 1) && mpsta_vap && ic->nss_vops && ic->nss_vops->ic_osif_nss_vdev_qwrap_isolation_enable) {
                osd = (osif_dev *)mpsta_vap->iv_ifp;
                ic->nss_vops->ic_osif_nss_vdev_qwrap_isolation_enable(osd);
                qdf_info("Set: NSS qwrap isolation mode value %d", value);
            }
#endif
	break;
#endif
         case OL_ATH_PARAM_OBSS_RSSI_THRESHOLD:
        {
            if (value >= OBSS_RSSI_MIN && value <= OBSS_RSSI_MAX) {
                ic->obss_rssi_threshold = value;
            } else {
                retval = -EINVAL;
            }
        }
        break;
         case OL_ATH_PARAM_OBSS_RX_RSSI_THRESHOLD:
        {
            if (value >= OBSS_RSSI_MIN && value <= OBSS_RSSI_MAX) {
                ic->obss_rx_rssi_threshold = value;
            } else {
                retval = -EINVAL;
            }
        }
        break;
        case OL_ATH_PARAM_ACS_TX_POWER_OPTION:
        {
            if(ic->ic_acs){
                ieee80211_acs_set_param(ic->ic_acs, IEEE80211_ACS_TX_POWER_OPTION, *(int *)buff);
            }
        }
        break;

        case OL_ATH_PARAM_ACS_2G_ALLCHAN:
        {
            if(ic->ic_acs){
                ieee80211_acs_set_param(ic->ic_acs, IEEE80211_ACS_2G_ALL_CHAN, *(int *)buff);
            }
        }
        break;
        case OL_ATH_PARAM_ANT_POLARIZATION:
        {
            retval = ol_ath_pdev_set_param(scn,
                          wmi_pdev_param_ant_plzn, value);
        }
        break;

         case OL_ATH_PARAM_ENABLE_AMSDU:
        {

            retval = ol_ath_pdev_set_param(scn,
                                  wmi_pdev_param_enable_per_tid_amsdu, value);
            if (retval == EOK) {
                qdf_info("enable AMSDU: value %d wmi_status %d", value, retval );
                scn->scn_amsdu_mask = value;
            } else {
                qdf_info("enable AMSDU: wmi_failed: wmi_status %d", retval );
            }
        }
        break;

        case OL_ATH_PARAM_MAX_CLIENTS_PER_RADIO:
        {
#ifdef QCA_LOWMEM_PLATFORM
            if (value > 0 && value <= IEEE80211_33_AID)
                ic->ic_num_clients = value;
            else {
                qdf_info("QCA_LOWMEM_PLATFORM: Range 1-33 clients");
                retval = -EINVAL;
                break;
            }
#else
            if (value > 0 && value <= IEEE80211_512_AID) {
                if (lmac_get_tgt_type(scn->soc->psoc_obj) == TARGET_TYPE_AR9888) {
                    if (value <= IEEE80211_128_AID)
                        ic->ic_num_clients = value;
                    else {
                        qdf_info("AR9888 PLATFORM: Range 1-128 clients");
                        retval = -EINVAL;
                        break;
                    }
                } else  /* As of now beeliner and lithium */
                    ic->ic_num_clients = value;
            } else {
                qdf_info("Range 1-512 clients");
                retval = -EINVAL;
                break;
            }
#endif
        }
        TAILQ_FOREACH(tmp_vap, &ic->ic_vaps, iv_next) {
            if ((tmp_vap->iv_opmode == IEEE80211_M_HOSTAP) &&
                (wlan_vdev_is_up(tmp_vap->vdev_obj) == QDF_STATUS_SUCCESS)) {
                wlan_iterate_station_list(tmp_vap, sta_disassoc, NULL);
            }
        }
        break;

        case OL_ATH_PARAM_ENABLE_AMPDU:
        {

            retval = ol_ath_pdev_set_param(scn,
                                   wmi_pdev_param_enable_per_tid_ampdu, value);
            if (retval == EOK) {
                qdf_info("enable AMPDU: value %d wmi_status %d", value, retval );
                scn->scn_ampdu_mask = value;
            } else {
                qdf_info("enable AMPDU: wmi_failed: wmi_status %d", retval );
            }
        }
        break;

#if WLAN_CFR_ENABLE
        case OL_ATH_PARAM_PERIODIC_CFR_CAPTURE:
            if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_CFR_ID) !=
                    QDF_STATUS_SUCCESS) {
                return -1;
            }
            if (value > 1) {
                qdf_info("Use 1/0 to enable/disable the timer\n");
                wlan_objmgr_pdev_release_ref(pdev, WLAN_CFR_ID);
                return -EINVAL;
            }

            retval = ucfg_cfr_set_timer(pdev, value);

            wlan_objmgr_pdev_release_ref(pdev, WLAN_CFR_ID);
        break;
#endif

       case OL_ATH_PARAM_PRINT_RATE_LIMIT:

        if (value <= 0) {
            retval = -EINVAL;
        } else {
            scn->soc->dbg.print_rate_limit = value;
            qdf_info("Changing rate limit to: %d \n", scn->soc->dbg.print_rate_limit);
        }
        break;

        case OL_ATH_PARAM_PDEV_RESET:
        {
                if ( (value > 0) && (value < 6)) {
                    return ol_ath_pdev_set_param(scn,
                             wmi_pdev_param_pdev_reset, value);
                } else {
                    qdf_info(" Invalid vaue : Use any one of the below values \n"
                        "    TX_FLUSH = 1 \n"
                        "    WARM_RESET = 2 \n"
                        "    COLD_RESET = 3 \n"
                        "    WARM_RESET_RESTORE_CAL = 4 \n"
                        "    COLD_RESET_RESTORE_CAL = 5 \n");
                    retval = -EINVAL;
                }
        }
        break;

        case OL_ATH_PARAM_CONSIDER_OBSS_NON_ERP_LONG_SLOT:
        {
            ic->ic_consider_obss_long_slot = !!value;
        }

        break;

        case OL_ATH_PARAM_TOTAL_Q_SIZE_RANGE0:
        case OL_ATH_PARAM_TOTAL_Q_SIZE_RANGE1:
        case OL_ATH_PARAM_TOTAL_Q_SIZE_RANGE2:
        case OL_ATH_PARAM_TOTAL_Q_SIZE_RANGE3:
        {
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
            if (scn->sc_ic.nss_radio_ops) {
                scn->sc_ic.nss_radio_ops->ic_nss_tx_queue_cfg(scn,
                        (param - OL_ATH_PARAM_TOTAL_Q_SIZE_RANGE0), value);
            }
#endif
        }
        break;

#if PEER_FLOW_CONTROL
         case OL_ATH_PARAM_VIDEO_DELAY_STATS_FC:
         case OL_ATH_PARAM_QFLUSHINTERVAL:
         case OL_ATH_PARAM_TOTAL_Q_SIZE:
         case OL_ATH_PARAM_MIN_THRESHOLD:
         case OL_ATH_PARAM_MAX_Q_LIMIT:
         case OL_ATH_PARAM_MIN_Q_LIMIT:
         case OL_ATH_PARAM_CONG_CTRL_TIMER_INTV:
         case OL_ATH_PARAM_STATS_TIMER_INTV:
         case OL_ATH_PARAM_ROTTING_TIMER_INTV:
         case OL_ATH_PARAM_LATENCY_PROFILE:
         case OL_ATH_PARAM_HOSTQ_DUMP:
         case OL_ATH_PARAM_TIDQ_MAP:
        {
            cdp_pflow_update_pdev_params(soc_txrx_handle,
                    (void *)pdev_txrx_handle, param, value, NULL);
        }
        break;
#endif
        case OL_ATH_PARAM_DBG_ARP_SRC_ADDR:
        {
            scn->sc_arp_dbg_srcaddr = value;
            retval = ol_ath_pdev_set_param(scn,
                         wmi_pdev_param_arp_srcaddr,
                         scn->sc_arp_dbg_srcaddr);
            if (retval != EOK) {
                qdf_info("Failed to set ARP DEBUG SRC addr in firmware \r\n");
            }
        }
        break;

        case OL_ATH_PARAM_DBG_ARP_DST_ADDR:
        {
            scn->sc_arp_dbg_dstaddr = value;
            retval = ol_ath_pdev_set_param(scn,
                         wmi_pdev_param_arp_dstaddr,
                         scn->sc_arp_dbg_dstaddr);
            if (retval != EOK) {
                qdf_info("Failed to set ARP DEBUG DEST addr in firmware \r\n");
            }
        }
        break;

        case OL_ATH_PARAM_ARP_DBG_CONF:
        {
#define ARP_RESET 0xff000000
            if (value & ARP_RESET) {
                /* Reset stats */
                scn->sc_tx_arp_req_count = 0;
                scn->sc_rx_arp_req_count = 0;
            } else {
                scn->sc_arp_dbg_conf = value;
                cdp_txrx_set_pdev_param(soc_txrx_handle, (struct cdp_pdev *)pdev_txrx_handle,
                        CDP_CONFIG_ARP_DBG_CONF, value);
            }
#undef ARP_RESET
        }
        break;
            /* Disable AMSDU for Station vap */
        case OL_ATH_PARAM_DISABLE_STA_VAP_AMSDU:
        {
            ic->ic_sta_vap_amsdu_disable = value;
        }
        break;

#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
        case OL_ATH_PARAM_STADFS_ENABLE:
            if(!value) {
                ieee80211com_clear_cap_ext(ic,IEEE80211_CEXT_STADFS);
            } else {
                ieee80211com_set_cap_ext(ic,IEEE80211_CEXT_STADFS);
            }

            if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                    QDF_STATUS_SUCCESS) {
                return -1;
            }

            if (dfs_rx_ops && dfs_rx_ops->dfs_enable_stadfs)
                dfs_rx_ops->dfs_enable_stadfs(pdev, !!value);

            wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
            break;
#endif
        case OL_ATH_PARAM_CHANSWITCH_OPTIONS:
            ic->ic_chanswitch_flags = (*(int *)buff);
            /* When TxCSA is set to 1, Repeater CAC(IEEE80211_CSH_OPT_CAC_APUP_BYSTA)
             * will be forced to 1. Because, the TXCSA is done by changing channel in
             * the beacon update function(ieee80211_beacon_update) and AP VAPs change
             * the channel, if the new channel is DFS then AP VAPs do CAC and STA VAP
             * has to synchronize with the AP VAPS' CAC.
             */
            if (IEEE80211_IS_CSH_CSA_APUP_BYSTA_ENABLED(ic)) {
                IEEE80211_CSH_CAC_APUP_BYSTA_ENABLE(ic);
                qdf_info("%s: When TXCSA is set to 1, Repeater CAC is forced to 1", __func__);
            }
            break;
        case OL_ATH_PARAM_BW_REDUCE:
            if (dfs_rx_ops && dfs_rx_ops->dfs_set_bw_reduction) {
                if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                        QDF_STATUS_SUCCESS) {
                    return -1;
                }
                dfs_rx_ops->dfs_set_bw_reduction(pdev, (bool)value);
                wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
            }
            break;
#if DBDC_REPEATER_SUPPORT
        case OL_ATH_PARAM_PRIMARY_RADIO:
            if(value != 1) {
                qdf_info("Value should be given as 1 to set primary radio");
                retval = -EINVAL;
                break;
            }

            if(ic->ic_primary_radio == value) {
                qdf_info("primary radio is set already for this radio");
                break;
            }

            cdp_txrx_set_pdev_param(soc_txrx_handle,
                                    (struct cdp_pdev *)pdev_txrx_handle,
                                    CDP_CONFIG_PRIMARY_RADIO, value);
            /*
             * For Lithium, because of HW AST issue, primary/secondary radio
             * configuration is needed for AP mode also
             */
            if(!ic->ic_sta_vap && !ol_target_lithium(scn->soc->psoc_obj)) {
                break;
            }

            for (i=0; i < MAX_RADIO_CNT; i++) {
                GLOBAL_IC_LOCK_BH(ic->ic_global_list);
                tmp_ic = ic->ic_global_list->global_ic[i];
                GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
                if (tmp_ic) {
                    spin_lock(&tmp_ic->ic_lock);
                    if (ic == tmp_ic) {
                        /* Setting current radio as primary radio*/
                        qdf_info("Setting primary radio for %s", ether_sprintf(ic->ic_myaddr));
                        tmp_ic->ic_primary_radio = 1;

                    } else {
                        tmp_ic->ic_primary_radio = 0;
                    }
                    dp_lag_pdev_set_primary_radio(tmp_ic->ic_pdev_obj,
                            tmp_ic->ic_primary_radio);
                    spin_unlock(&tmp_ic->ic_lock);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
                    tmp_scn = OL_ATH_SOFTC_NET80211(tmp_ic);
                    if (tmp_ic->nss_radio_ops) {
                        tmp_ic->nss_radio_ops->ic_nss_ol_set_primary_radio(tmp_scn, tmp_ic->ic_primary_radio);
                    }
#endif
                }
            }
            wlan_update_radio_priorities(ic);
#if ATH_SUPPORT_WRAP
            osif_set_primary_radio_event(ic);
#endif
            break;
        case OL_ATH_PARAM_DBDC_ENABLE:
            GLOBAL_IC_LOCK_BH(ic->ic_global_list);
            ic->ic_global_list->dbdc_process_enable = (value) ?1:0;

            if ((value == 0) || ic->ic_global_list->num_stavaps_up > 1) {
                dp_lag_soc_enable(ic->ic_pdev_obj,
                        ic->ic_global_list->dbdc_process_enable);

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
                if (ic->nss_radio_ops) {
                    ic->nss_radio_ops->ic_nss_ol_enable_dbdc_process(ic,
                            (uint32_t)ic->ic_global_list->dbdc_process_enable);
                }
#endif

            }

            GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
            for (i = 0; i < MAX_RADIO_CNT; i++) {
                tmp_ic = ic->ic_global_list->global_ic[i];
                if (tmp_ic) {
                    tmp_scn = OL_ATH_SOFTC_NET80211(tmp_ic);
                    if (tmp_scn) {
                        spin_lock(&tmp_ic->ic_lock);
                        if (value) {
                            if (tmp_ic->ic_global_list->num_stavaps_up > 1) {
                                if (tmp_ic->nss_radio_ops)
                                    tmp_ic->nss_radio_ops->ic_nss_ol_enable_dbdc_process(tmp_ic, value);
                            }
                        } else {
                            if (tmp_ic->nss_radio_ops)
                                tmp_ic->nss_radio_ops->ic_nss_ol_enable_dbdc_process(tmp_ic, 0);
                        }
                        spin_unlock(&tmp_ic->ic_lock);
                    }
                }
            }
#endif
            break;
        case OL_ATH_PARAM_CLIENT_MCAST:
            GLOBAL_IC_LOCK_BH(ic->ic_global_list);
            if(value) {
                ic->ic_global_list->force_client_mcast_traffic = 1;
                qdf_info("Enabling MCAST client traffic to go on corresponding STA VAP\n");
            } else {
                ic->ic_global_list->force_client_mcast_traffic = 0;
                qdf_info("Disabling MCAST client traffic to go on corresponding STA VAP\n");
            }

            dp_lag_soc_set_force_client_mcast_traffic(ic->ic_pdev_obj,
                    ic->ic_global_list->force_client_mcast_traffic);

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
            for (i = 0; i < MAX_RADIO_CNT; i++) {
                tmp_ic = ic->ic_global_list->global_ic[i];
                if (tmp_ic) {
                    tmp_scn = OL_ATH_SOFTC_NET80211(tmp_ic);
                    if (tmp_ic->nss_radio_ops) {
                        tmp_ic->nss_radio_ops->ic_nss_ol_set_force_client_mcast_traffic(tmp_ic, ic->ic_global_list->force_client_mcast_traffic);
                    }
                }
            }
#endif
            GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
            break;
#endif
        case OL_ATH_PARAM_TXPOWER_DBSCALE:
            {
                retval = ol_ath_pdev_set_param(scn,
                        wmi_pdev_param_txpower_decr_db, value);
            }
            break;
        case OL_ATH_PARAM_CTL_POWER_SCALE:
            {
                if((WMI_HOST_TP_SCALE_MAX <= value) && (value <= WMI_HOST_TP_SCALE_MIN))
                {
                    scn->powerscale = value;
                    return ol_ath_pdev_set_param(scn,
                            wmi_pdev_param_cust_txpower_scale, value);
                } else {
                    retval = -EINVAL;
                }
            }
            break;

#ifdef QCA_EMIWAR_80P80_CONFIG_SUPPORT
        case OL_ATH_PARAM_EMIWAR_80P80:
            {
                uint16_t cc;
                uint32_t target_type = lmac_get_tgt_type(scn->soc->psoc_obj);

                if (IS_EMIWAR_80P80_APPLICABLE(target_type)) {
                    if ((value >= EMIWAR_80P80_DISABLE) && (value < EMIWAR_80P80_MAX)) {
                        (scn->sc_ic).ic_emiwar_80p80 = value;
                        qdf_info("Re-applying current country code.\n");
                        cc = ieee80211_getCurrentCountry(ic);
                        retval = wlan_set_countrycode(&scn->sc_ic, NULL,
                                cc, CLIST_NEW_COUNTRY);
                        /*Using set country code for re-usability and non-duplication of INIT code */
                    }
                    else {
                        qdf_info(" Please enter 0:Disable, 1:BandEdge (FC1:5775, and FC2:5210), 2:All FC1>FC2\n");
                        retval = -EINVAL;
                    }
                }
                else {
                    qdf_info("emiwar80p80 not applicable for this chipset \n");

                }
            }
            break;
#endif /*QCA_EMIWAR_80P80_CONFIG_SUPPORT*/
        case OL_ATH_PARAM_BATCHMODE:
            return ol_ath_pdev_set_param(scn,
                         wmi_pdev_param_rx_batchmode, !!value);
            break;
        case OL_ATH_PARAM_PACK_AGGR_DELAY:
            return ol_ath_pdev_set_param(scn,
                         wmi_pdev_param_packet_aggr_delay, !!value);
            break;
#if UMAC_SUPPORT_ACFG
        case OL_ATH_PARAM_DIAG_ENABLE:
            if (value == 0 || value == 1) {
                if (value && !ic->ic_diag_enable) {
                    acfg_diag_pvt_t *diag = (acfg_diag_pvt_t *)ic->ic_diag_handle;
                    if (diag) {
                        ic->ic_diag_enable = value;
                        OS_SET_TIMER(&diag->diag_timer, 0);
                    }
                }else if (!value) {
                    ic->ic_diag_enable = value;
                }
            } else {
                qdf_info("Please enter 0 or 1.\n");
                retval = -EINVAL;
            }
            break;
#endif /* UMAC_SUPPORT_ACFG */

        case OL_ATH_PARAM_CHAN_STATS_TH:
            ic->ic_chan_stats_th = (value % 100);
            break;

        case OL_ATH_PARAM_PASSIVE_SCAN_ENABLE:
            ic->ic_strict_pscan_enable = !!value;
            break;

        case OL_ATH_MIN_RSSI_ENABLE:
            {
                if (value == 0 || value == 1) {
                    if (value)
                        ic->ic_min_rssi_enable = true;
                    else
                        ic->ic_min_rssi_enable = false;
               } else {
                   qdf_info("Please enter 0 or 1.\n");
                   retval = -EINVAL;
               }
            }
            break;
        case OL_ATH_MIN_RSSI:
            {
                int val = *(int *)buff;
                if (val <= 0) {
                    qdf_info("snr should be a positive value.\n");
                    retval = -EINVAL;
                } else if (ic->ic_min_rssi_enable)
                    ic->ic_min_rssi = val;
                else
                    qdf_info("Cannot set, feature not enabled.\n");
            }
            break;
#if DBDC_REPEATER_SUPPORT
        case OL_ATH_PARAM_DELAY_STAVAP_UP:
            GLOBAL_IC_LOCK_BH(ic->ic_global_list);
            if(value) {
                ic->ic_global_list->delay_stavap_connection = value;
                qdf_info("Enabling DELAY_STAVAP_UP:%d",value);
            } else {
                ic->ic_global_list->delay_stavap_connection = 0;
                qdf_info("Disabling DELAY_STAVAP_UP");
            }
            GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
            break;
#endif
        case OL_ATH_BTCOEX_ENABLE:
            {
                int val = !!(*((int *) buff));

                if(wlan_psoc_nif_feat_cap_get(scn->soc->psoc_obj,
					WLAN_SOC_F_BTCOEX_SUPPORT)) {
                    if (ol_ath_pdev_set_param(scn, wmi_pdev_param_enable_btcoex, val) == EOK) {
                        scn->soc->btcoex_enable = val;

                        if (scn->soc->btcoex_enable) {
                            if (scn->soc->btcoex_wl_priority == 0) {
                                scn->soc->btcoex_wl_priority  = WMI_HOST_PDEV_VI_PRIORITY_BIT |
                                    WMI_HOST_PDEV_BEACON_PRIORITY_BIT |
                                    WMI_HOST_PDEV_MGMT_PRIORITY_BIT;

                                if (ol_ath_btcoex_wlan_priority(scn->soc, scn->soc->btcoex_wl_priority) != EOK) {
                                    qdf_info("%s Assigning btcoex_wlan_priority:%d failed ",
                                            __func__,scn->soc->btcoex_wl_priority);
                                    return -ENOMEM;
                                } else {
                                    qdf_info("%s Set btcoex_wlan_priority:%d ",
                                            __func__,scn->soc->btcoex_wl_priority);
                                }
                            }
                            if (scn->soc->btcoex_duty_cycle && (scn->soc->btcoex_period <= 0)) {
                                if ( ol_ath_btcoex_duty_cycle(scn->soc,DEFAULT_PERIOD,DEFAULT_WLAN_DURATION) != EOK ) {
                                    qdf_info("%s Assigning btcoex_period:%d btcoex_duration:%d failed ",
                                            __func__,DEFAULT_PERIOD,DEFAULT_WLAN_DURATION);
                                    return -ENOMEM;
                                } else {
                                    scn->soc->btcoex_period = DEFAULT_PERIOD;
                                    scn->soc->btcoex_duration = DEFAULT_WLAN_DURATION;
                                    qdf_info("%s Set default val btcoex_period:%d btcoex_duration:%d ",
                                            __func__,scn->soc->btcoex_period,scn->soc->btcoex_duration);
                                }
                            }
                        }
                    } else {
                        qdf_info("%s Failed to send enable btcoex cmd:%d ",__func__,val);
                        return -ENOMEM;
                    }
                } else {
                    retval = -EPERM;
                }
            }
            break;
        case OL_ATH_BTCOEX_WL_PRIORITY:
            {
                int val = *((int *) buff);

                if(wlan_psoc_nif_feat_cap_get(scn->soc->psoc_obj,
					WLAN_SOC_F_BTCOEX_SUPPORT)) {
                    if (ol_ath_btcoex_wlan_priority(scn->soc,val) == EOK) {
                        scn->soc->btcoex_wl_priority = val;
                    } else {
                        return -ENOMEM;
                    }
                } else {
                    retval = -EPERM;
                }
            }
            break;
        case OL_ATH_PARAM_CAL_VER_CHECK:
            {
                if(wlan_psoc_nif_fw_ext_cap_get(scn->soc->psoc_obj,
                                              WLAN_SOC_CEXT_SW_CAL)) {
                    if(value == 0 || value == 1) {
                        ic->ic_cal_ver_check = value;
                        /* Setting to 0x0 as expected by FW */
                        retval = wmi_send_pdev_caldata_version_check_cmd(pdev_wmi_handle, 0);
                    } else {
                        qdf_info("Enter value 0 or 1. ");
                        retval = -EINVAL;
                    }
                } else {
                    qdf_info(" wmi service to check cal version not supported ");
                }
            }
            break;
        case OL_ATH_PARAM_TID_OVERRIDE_QUEUE_MAPPING:
           if (scn->soc->ol_if_ops->ol_pdev_set_tid_override_queue_mapping)
                   scn->soc->ol_if_ops->ol_pdev_set_tid_override_queue_mapping(
                                   pdev_txrx_handle, value);
            break;
        case OL_ATH_PARAM_NO_VLAN:
            ic->ic_no_vlan = !!value;
            break;
        case OL_ATH_PARAM_ATF_LOGGING:
            ic->ic_atf_logging = !!value;
            break;

        case OL_ATH_PARAM_STRICT_DOTH:
            ic->ic_strict_doth  = !!value;
            break;
        case OL_ATH_PARAM_CHANNEL_SWITCH_COUNT:
            ic->ic_chan_switch_cnt = value;
            break;
#if DBDC_REPEATER_SUPPORT
        case OL_ATH_PARAM_DISCONNECTION_TIMEOUT:
            GLOBAL_IC_LOCK_BH(ic->ic_global_list);
            ic->ic_global_list->disconnect_timeout = value;
            qdf_info("Disconnect_timeout value: %d",value);
            GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
            break;
        case OL_ATH_PARAM_RECONFIGURATION_TIMEOUT:
            GLOBAL_IC_LOCK_BH(ic->ic_global_list);
            ic->ic_global_list->reconfiguration_timeout = value;
            qdf_info("Reconfiguration_timeout value:%d",value);
            GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
            break;
        case OL_ATH_PARAM_ALWAYS_PRIMARY:
            GLOBAL_IC_LOCK_BH(ic->ic_global_list);
            ic->ic_global_list->always_primary = (value) ?1:0;
            dp_lag_soc_set_always_primary(ic->ic_pdev_obj,
                    ic->ic_global_list->always_primary);
            qdf_info("Setting always primary flag as %d ",value);
            GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
            for (i=0; i < MAX_RADIO_CNT - 1; i++) {
                GLOBAL_IC_LOCK_BH(ic->ic_global_list);
                tmp_ic = ic->ic_global_list->global_ic[i];
                if (tmp_ic) {
                    tmp_scn = OL_ATH_SOFTC_NET80211(tmp_ic);
                    if (tmp_ic->nss_radio_ops)
                        tmp_ic->nss_radio_ops->ic_nss_ol_set_always_primary(tmp_ic, ic->ic_global_list->always_primary);
                }
                GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
            }
#endif
            break;
        case OL_ATH_PARAM_FAST_LANE:
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
            if (scn->nss_radio.nss_rctx) {
                qdf_info( "fast lane not supported on nss offload ");
                break;
            }
#endif
            if (value && (ic->ic_global_list->num_fast_lane_ic > 1)) {
                /* fast lane support allowed only on 2 radios*/
                qdf_info("fast lane support allowed only on 2 radios ");
                retval = -EPERM;
                break;
            }
            if ((ic->fast_lane == 0) && value) {
                GLOBAL_IC_LOCK_BH(ic->ic_global_list);
                ic->ic_global_list->num_fast_lane_ic++;
                GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
            }
            if ((ic->fast_lane == 1) && !value) {
                GLOBAL_IC_LOCK_BH(ic->ic_global_list);
                ic->ic_global_list->num_fast_lane_ic--;
                GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
            }
            spin_lock(&ic->ic_lock);
            ic->fast_lane = value ?1:0;
            spin_unlock(&ic->ic_lock);
            qdf_info("Setting fast lane flag as %d for radio:%s",value,ether_sprintf(ic->ic_my_hwaddr));
            if (ic->fast_lane) {
                for (i=0; i < MAX_RADIO_CNT; i++) {
                    GLOBAL_IC_LOCK_BH(ic->ic_global_list);
                    tmp_ic = ic->ic_global_list->global_ic[i];
                    GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
                    if (tmp_ic && (tmp_ic != ic) && tmp_ic->fast_lane) {
                        spin_lock(&tmp_ic->ic_lock);
                        tmp_ic->fast_lane_ic = ic;
                        spin_unlock(&tmp_ic->ic_lock);
                        spin_lock(&ic->ic_lock);
                        ic->fast_lane_ic = tmp_ic;
                        qdf_info("fast lane ic mac:%s",ether_sprintf(ic->fast_lane_ic->ic_my_hwaddr));
                        spin_unlock(&ic->ic_lock);
                    }
                }
            } else {
                fast_lane_ic = ic->fast_lane_ic;
                if (fast_lane_ic) {
                    spin_lock(&fast_lane_ic->ic_lock);
                    fast_lane_ic->fast_lane_ic = NULL;
                    spin_unlock(&fast_lane_ic->ic_lock);
                }
                spin_lock(&ic->ic_lock);
                ic->fast_lane_ic = NULL;
                spin_unlock(&ic->ic_lock);
            }
            qdf_info("num fast lane ic count %d",ic->ic_global_list->num_fast_lane_ic);
            break;
        case OL_ATH_PARAM_PREFERRED_UPLINK:
            if(!ic->ic_sta_vap) {
                qdf_info("Radio not configured on repeater mode");
                retval = -EINVAL;
                break;
            }
            if(value != 1) {
                qdf_info("Value should be given as 1 to set as preferred uplink");
                retval = -EINVAL;
                break;
            }
            for (i=0; i < MAX_RADIO_CNT; i++) {
                GLOBAL_IC_LOCK_BH(ic->ic_global_list);
                tmp_ic = ic->ic_global_list->global_ic[i];
                GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
                if (tmp_ic) {
                    spin_lock(&tmp_ic->ic_lock);
                    if (ic == tmp_ic) {
                        /* Setting current radio as preferred uplink*/
                        tmp_ic->ic_preferredUplink = 1;
                    } else {
                        tmp_ic->ic_preferredUplink = 0;
                    }
                    spin_unlock(&tmp_ic->ic_lock);
                }
            }
            break;
#endif
        case OL_ATH_PARAM_SECONDARY_OFFSET_IE:
            ic->ic_sec_offsetie = !!value;
            break;
        case OL_ATH_PARAM_WIDE_BAND_SUB_ELEMENT:
            ic->ic_wb_subelem = !!value;
            break;
#if ATH_SUPPORT_ZERO_CAC_DFS
        case OL_ATH_PARAM_PRECAC_ENABLE:
            if (dfs_rx_ops && dfs_rx_ops->dfs_set_precac_enable) {
                if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                        QDF_STATUS_SUCCESS) {
                    return -1;
                }
                dfs_rx_ops->dfs_set_precac_enable(pdev, value);
                wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
            }
            break;
        case OL_ATH_PARAM_PRECAC_TIMEOUT:
            /* Call a function to update the PRECAC Timeout */
            if (dfs_rx_ops && dfs_rx_ops->dfs_override_precac_timeout) {
                if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                        QDF_STATUS_SUCCESS) {
                    return -1;
                }
                dfs_rx_ops->dfs_override_precac_timeout(pdev, value);
                wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
            }
            break;
#ifdef WLAN_DFS_PRECAC_AUTO_CHAN_SUPPORT
        case OL_ATH_PARAM_PRECAC_INTER_CHANNEL:
            /* Call a function to update the PRECAC intermediate channel */
            if (dfs_rx_ops && dfs_rx_ops->dfs_set_precac_intermediate_chan) {
                if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                        QDF_STATUS_SUCCESS) {
                    return -1;
                }
                retval = dfs_rx_ops->dfs_set_precac_intermediate_chan(pdev,
								      value);
                wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
            }
            break;
#endif

#endif
	case OL_ATH_PARAM_PDEV_TO_REO_DEST:
	    if ((value < cdp_host_reo_dest_ring_1) || (value > cdp_host_reo_dest_ring_4)) {
		qdf_info("reo ring destination value should be between 1 to 4");
		retval = -EINVAL;
		break;
	    }
	    cdp_set_pdev_reo_dest(soc_txrx_handle,
				  (void *)pdev_txrx_handle,
				  value);
	    break;
        case OL_ATH_PARAM_DUMP_OBJECTS:
            wlan_objmgr_print_ref_cnts(ic);
            break;

        case OL_ATH_PARAM_MGMT_RSSI_THRESHOLD:
            if (value <= RSSI_MIN || value > RSSI_MAX) {
                qdf_info("invalid value: %d, RSSI is between 1-127 ", value);
                return -EINVAL;
            }
            ic->mgmt_rx_rssi = value;
            break;

        case OL_ATH_PARAM_EXT_NSS_CAPABLE:
            if (!ic->ic_fw_ext_nss_capable) {
                qdf_info("FW is not Ext NSS Signaling capable");
                return -EINVAL;
            }
            if ((value != 0) && (value != 1)) {
                qdf_info("Valid values 1:Enable 0:Disable");
                return -EINVAL;
            }
            if (value != ic->ic_ext_nss_capable) {
                ic->ic_ext_nss_capable = value;

                osif_restart_for_config(ic, NULL, NULL);
            }
            break;
#if QCN_ESP_IE
        case OL_ATH_PARAM_ESP_PERIODICITY:
            if (value < 0 || value > 5000) {
                qdf_info("Invalid value! Periodicity value should be between 0 and 5000 ");
                retval = -EINVAL;
            } else {
                /* ESP indication period doesn't need service check to become compatible with legacy firmware. */
                if (ol_ath_pdev_set_param(scn, wmi_pdev_param_esp_indication_period, value) != EOK) {
                    QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
                    "%s: ERROR - Param setting failed. Periodicity value 0x%x.\n", __func__, value);
                } else {
                    ic->ic_esp_periodicity = value;
                    ic->ic_esp_flag = 1; /* This is required for updating the beacon packet */
                }
            }
            break;

        case OL_ATH_PARAM_ESP_AIRTIME:
            if (value < 0 || value > 255) {
                qdf_info("Invalid value! Airtime value should be between 0 and 255 ");
                retval = -EINVAL;
            } else {
                if (wmi_service_enabled(wmi_handle, wmi_service_esp_support)) {
                    if (ol_ath_pdev_set_param(scn, wmi_pdev_param_esp_airtime_fraction, value) != EOK) {
                        QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
                        "%s: ERROR - Param setting failed. Airtime value 0x%x.\n", __func__, value);
                        return -1;
                    }
                }
                ic->ic_esp_air_time_fraction = value;
                ic->ic_esp_flag = 1; /* This is required for updating the beacon packet */
            }
            break;

        case OL_ATH_PARAM_ESP_PPDU_DURATION:
            if (value < 0 || value > 255) {
                qdf_info("Invalid value! PPDU duration target should be between 0 and 255 ");
                retval = -EINVAL;
            } else {
                if (wmi_service_enabled(wmi_handle, wmi_service_esp_support)) {
                    if (ol_ath_pdev_set_param(scn, wmi_pdev_param_esp_ppdu_duration, value) != EOK) {
                        QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
                        "%s: ERROR - Param setting failed. PPDU duration value 0x%x.\n", __func__, value);
                        return -1;
                    }
                }
                ic->ic_esp_ppdu_duration = value;
                ic->ic_esp_flag = 1; /* This is required for updating the beacon packet */
            }
            break;

        case OL_ATH_PARAM_ESP_BA_WINDOW:
            if (value <= 0 || value > 7) {
                qdf_info("Invalid value! BA window size should be between 1 and 7 ");
                retval = -EINVAL;
            } else {
                if (wmi_service_enabled(wmi_handle, wmi_service_esp_support)) {
                    if (ol_ath_pdev_set_param(scn, wmi_pdev_param_esp_ba_window, value) != EOK) {
                        QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
                        "%s: ERROR - Param setting failed. BA window size value 0x%x.\n", __func__, value);
                        return -1;
                    }
                }
                ic->ic_esp_ba_window = value;
                ic->ic_esp_flag = 1; /* This is required for updating the beacon packet */
            }
            break;
#endif /* QCN_ESP_IE */

        case OL_ATH_PARAM_MGMT_PDEV_STATS_TIMER:
            if(value) {
                scn->pdev_stats_timer = value;
            } else {
                scn->is_scn_stats_timer_init = 0;
            }
            break ;

        case OL_ATH_PARAM_ICM_ACTIVE:
            ic->ic_extacs_obj.icm_active = value;
            break;

        case OL_ATH_PARAM_CHAN_INFO:
            qdf_mem_zero(ic->ic_extacs_obj.chan_info, sizeof(struct scan_chan_info)
                         * NUM_MAX_CHANNELS);
            break;

        case OL_ATH_PARAM_TXACKTIMEOUT:
            {
                if (wmi_service_enabled(wmi_handle,wmi_service_ack_timeout))
                {
                    if (value >= DEFAULT_TX_ACK_TIMEOUT && value <= MAX_TX_ACK_TIMEOUT)
                    {
                        if(ol_ath_pdev_set_param(scn,
                                wmi_pdev_param_tx_ack_timeout, value) == EOK) {
                            scn->tx_ack_timeout = value;
                        } else {
                            retval = -1;
                        }
                    }
                    else
                    {
                        qdf_info("TX ACK Time-out value should be between 0x40 and 0xFF ");
                        retval = -1;
                    }
                }
                else
                {
                    qdf_info("TX ACK Timeout Service is not supported");
                    retval = -1;
                }
            }
            break;

        case OL_ATH_PARAM_ACS_RANK:
            if (ic->ic_acs){
                ieee80211_acs_set_param(ic->ic_acs, IEEE80211_ACS_RANK , *(int *)buff);
            }
            break;

#ifdef OL_ATH_SMART_LOGGING
        case OL_ATH_PARAM_SMARTLOG_ENABLE:
            if (scn->soc->ol_if_ops->enable_smart_log)
                scn->soc->ol_if_ops->enable_smart_log(scn, value);
            break;

        case OL_ATH_PARAM_SMARTLOG_FATAL_EVENT:
            if (scn->soc->ol_if_ops->send_fatal_cmd)
                scn->soc->ol_if_ops->send_fatal_cmd(scn, value, 0);
            break;

        case OL_ATH_PARAM_SMARTLOG_SKB_SZ:
            ic->smart_log_skb_sz = value;
            break;

        case OL_ATH_PARAM_SMARTLOG_P1PINGFAIL:
            if ((value != 0) && (value != 1)) {
                qdf_info("Invalid value. Value for P1 ping failure smart "
                         "logging start/stop can be 1 (start) or 0 (stop). "
                         "Smart logging feature should be enabled in order to "
                         "start P1 ping failure smart logging.");
                retval = -EINVAL;
            } else if (value == 1) {
                if (!ic->smart_logging_p1pingfail_started) {
                    if (!ic->smart_logging) {
                        qdf_info("Smart logging feature not enabled. Cannot "
                                 "start P1 ping failure smart logging.");
                        retval = -EINVAL;
                    } else {
                        if (scn->soc->ol_if_ops->send_fatal_cmd) {
                            if ((scn->soc->ol_if_ops->send_fatal_cmd(scn,
                                    WMI_HOST_FATAL_CONDITION_CONNECTION_ISSUE,
                                    WMI_HOST_FATAL_SUBTYPE_P1_PING_FAILURE_START_DEBUG))
                                != QDF_STATUS_SUCCESS) {
                                qdf_err("Error: Could not successfully send "
                                        "command to start P1 ping failure "
                                        "logging. Smart logging or rest of "
                                        "system may no longer function as "
                                        "expected. Investigate!");
                                qdf_err("Preserving P1 ping failure status as "
                                        "stopped within host. This may or may "
                                        "not be consistent with FW state due "
                                        "to command send failure.");
                                retval = -EIO;
                            } else {
                                ic->smart_logging_p1pingfail_started = true;
                            }
                        } else {
                            qdf_info("No functionality available for sending "
                                     "fatal condition related command.");
                            retval = -EINVAL;
                        }
                    }
                } else {
                    qdf_info("P1 ping failure smart logging already started. "
                             "Ignoring.");
                }
            } else {
                 if (ic->smart_logging_p1pingfail_started) {
                    if (scn->soc->ol_if_ops->send_fatal_cmd) {
                        if ((scn->soc->ol_if_ops->send_fatal_cmd(scn,
                                WMI_HOST_FATAL_CONDITION_CONNECTION_ISSUE,
                                WMI_HOST_FATAL_SUBTYPE_P1_PING_FAILURE_STOP_DEBUG))
                             != QDF_STATUS_SUCCESS) {
                            qdf_err("Error: Could not successfully send "
                                    "command to stop P1 ping failure logging. "
                                    "Smart logging or rest of system may no "
                                    "longer function as expected. "
                                    "Investigate!");
                            qdf_err("Marking P1 ping failure as stopped "
                                    "within host. This may not be consistent "
                                    "with FW state due to command send "
                                    "failure.");
                            retval = -EIO;
                        }

                        ic->smart_logging_p1pingfail_started = false;
                    } else {
                        /*
                         * We should not have been able to start P1 ping failure
                         * logging if scn->soc->ol_if_ops->send_fatal_cmd were
                         * unavailable. This indicates a likely corruption. So
                         * we assert here.
                         */
                        qdf_err("No function registered for sending fatal "
                                "condition related command though P1 ping "
                                "failure logging is already enabled. "
                                "Asserting. Investigate!");
                        qdf_assert_always(0);
                    }
                } else {
                    qdf_info("P1 ping failure smart logging already stopped. "
                             "Ignoring.");
                }
            }
            break;
#endif /* OL_ATH_SMART_LOGGING */

        case OL_ATH_PARAM_TXCHAINSOFT:
            if (scn->soft_chain != value) {
                if (value > tgt_cfg->tx_chain_mask) {
                    qdf_info("ERROR - value 0x%x is greater than supported chainmask 0x%x",
                              value, tgt_cfg->tx_chain_mask);
                    retval = -EINVAL;
                } else {
                    if (ol_ath_pdev_set_param(scn, wmi_pdev_param_soft_tx_chain_mask, value) != EOK)
                        qdf_info("ERROR - soft chainmask value 0x%x couldn't be set", value);
                    else
                        scn->soft_chain = value;
                }
            }
            break;

        case OL_ATH_PARAM_WIDE_BAND_SCAN:
            if (ic->ic_widebw_scan == !!value) {
                qdf_info("same wide band scan config %d", value);
            } else {
                ic->ic_widebw_scan = !!value;
                wlan_scan_update_wide_band_scan_config(ic);
                /* update scan channel list */
                wlan_scan_update_channel_list(ic);
            }
            break;
#if ATH_PARAMETER_API
         case OL_ATH_PARAM_PAPI_ENABLE:
             if (value == 0 || value == 1) {
                 ic->ic_papi_enable = value;
             } else {
                 QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO,
                                "Value should be either 0 or 1\n");
                 return (-1);
             }
             break;
#endif
        case OL_ATH_PARAM_NF_THRESH:
            if (!ic->ic_acs) {
                qdf_info("Failed to set ACS NF Threshold");
                return -1;
            }

            retval = ieee80211_acs_set_param(ic->ic_acs,
                                             IEEE80211_ACS_NF_THRESH,
                                             *(int *)buff);
            break;

        case OL_ATH_PARAM_DUMP_TARGET:
            if(ol_target_lithium(scn->soc->psoc_obj)) {
                qdf_info("Feature not supported for this target!");
                retval = -EINVAL;
            } else {
		if (soc->ol_if_ops->dump_target)
			soc->ol_if_ops->dump_target(scn->soc);
            }
        break;

        case OL_ATH_PARAM_CCK_TX_ENABLE:
        if ((lmac_get_tgt_type(scn->soc->psoc_obj) == TARGET_TYPE_QCA8074) ||
                (lmac_get_tgt_type(scn->soc->psoc_obj) == TARGET_TYPE_QCA8074V2) ||
                (lmac_get_tgt_type(scn->soc->psoc_obj) == TARGET_TYPE_QCA6018)) {
                if (reg_cap == NULL)
                {
                    qdf_info("reg_cap is NULL, unable to process further. Investigate.");
                    retval = -1;
                } else {
                    if (reg_cap[pdev_idx].wireless_modes & WIRELESS_MODES_2G)
                    {
                        if (ic->cck_tx_enable != value) {
                            if (ol_ath_pdev_set_param(scn, wmi_pdev_param_cck_tx_enable, !!value) != EOK)
                                qdf_info("ERROR - CCK Tx enable value 0x%x couldn't be set", !!value);
                            else
                                ic->cck_tx_enable = value;
                        }
                    } else
                        qdf_info("CCK Tx is not supported for this band");
                }
            } else
                qdf_info("Setting the value of cck_tx_enable is not allowed for this chipset");
        break;

        case OL_ATH_PARAM_HE_UL_RU_ALLOCATION:

            if(!ic->ic_he_target) {
                qdf_err("Equal RU allocation setting not supported for this target\n");
                return -EINVAL;
            }

            if(value > 1) {
                qdf_err("Value should be either 0 or 1\n");
                return -EINVAL;
            }

            if(ic->ic_he_ru_allocation != value) {
                retval = ol_ath_pdev_set_param(scn,
                        wmi_pdev_param_equal_ru_allocation_enable, value);
                if(retval == EOK) {
                    ic->ic_he_ru_allocation = value;
                }
                else {
                    qdf_err("%s: WMI send for he_ru_alloc failed\n", __func__);
                    return retval;
                }
            }
            else {
                qdf_info("RU allocation enable already set with value %d\n",
                                                                        value);
            }
        break;
#if defined(WLAN_DFS_PARTIAL_OFFLOAD) && defined(HOST_DFS_SPOOF_TEST)
        case OL_ATH_PARAM_DFS_HOST_WAIT_TIMEOUT:
        /* Call a function to update the Host wait status Timeout */
        if (dfs_rx_ops && dfs_rx_ops->dfs_override_status_timeout) {
            if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                    QDF_STATUS_SUCCESS) {
                return -1;
            }
            dfs_rx_ops->dfs_override_status_timeout(pdev, value);
            wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
        }
        break;
#endif /* HOST_DFS_SPOOF_TEST */
        case OL_ATH_PARAM_TWICE_ANTENNA_GAIN:
        if ((value >= 0) && (value <= (MAX_ANTENNA_GAIN * 2))) {
            if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan)) {
                return ol_ath_pdev_set_param(scn,
                                             wmi_pdev_param_antenna_gain_half_db,
                                             value | ANTENNA_GAIN_2G_MASK);
            } else {
                return ol_ath_pdev_set_param(scn,
                                             wmi_pdev_param_antenna_gain_half_db,
                                             value | ANTENNA_GAIN_5G_MASK);
            }
        } else {
            retval = -EINVAL;
        }
        break;
        case OL_ATH_PARAM_ENABLE_PEER_RETRY_STATS:
            scn->retry_stats = value;
            return ol_ath_pdev_set_param(scn,
                                         wmi_pdev_param_enable_peer_retry_stats,
                                         !!value);
        break;
        case OL_ATH_PARAM_HE_UL_TRIG_INT:
            if(value > IEEE80211_HE_TRIG_INT_MAX) {
                qdf_err("%s:Trigger interval value should be less than %dms\n",
                        __func__, IEEE80211_HE_TRIG_INT_MAX);
                retval = -EINVAL;
            }

            if(ol_ath_pdev_set_param(scn, wmi_pdev_param_ul_trig_int, value) != EOK) {
                qdf_err("%s: Trigger interval value %d could not be set\n", __func__, value);
            } else {
                ic->ic_he_ul_trig_int = value;
            }
        break;
        case OL_ATH_PARAM_DFS_NOL_SUBCHANNEL_MARKING:
#if ATH_SUPPORT_DFS
            if (dfs_rx_ops && dfs_rx_ops->dfs_set_nol_subchannel_marking) {
                    if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                            QDF_STATUS_SUCCESS) {
                return -1;
            }
                    retval = dfs_rx_ops->dfs_set_nol_subchannel_marking(pdev,
                                        value);
                    wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
                }
                break;
#else
        retval = -EINVAL;
        break;
#endif
        case OL_ATH_PARAM_HE_SR:
            if(value > 1) {
                qdf_err("%s: Spatial Reuse value should be either 0 or 1\n",
                        __func__);
                retval = -EINVAL;
            }
#ifdef OBSS_PD
	    if(ic->ic_is_spatial_reuse_enabled &&
                ic->ic_is_spatial_reuse_enabled(ic) &&
                (ic->ic_he_sr_enable != value)) {
                TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
                    vap->iv_is_spatial_reuse_updated = true;
                    wlan_vdev_beacon_update(vap);
                }
            }
#endif /* OBSS_PD */
            ic->ic_he_sr_enable = value;
            osif_restart_for_config(ic, NULL, NULL);
            break;

        case OL_ATH_PARAM_HE_UL_PPDU_DURATION:

            if(!ic->ic_he_target) {
                qdf_err("UL PPDU Duration setting not supported for this target");
                return -EINVAL;
            }

            if(value > IEEE80211_UL_PPDU_DURATION_MAX) {
                qdf_err("UL PPDU Duration should be less than %d",
                        IEEE80211_UL_PPDU_DURATION_MAX);
                return -EINVAL;
            }

            if(ic->ic_he_ul_ppdu_dur != value) {
                retval = ol_ath_pdev_set_param(scn,
                        wmi_pdev_param_ul_ppdu_duration, value);
                if(retval == EOK) {
                    ic->ic_he_ul_ppdu_dur = value;
                }
                else {
                    qdf_err("%s: WMI send for ul_ppdu_dur failed", __func__);
                    return -EINVAL;
                }
            }
            else {
                qdf_info("UL PPDU duration already set with value %d", value);
            }
        break;
        case OL_ATH_PARAM_FLUSH_PEER_RATE_STATS:
             cdp_flush_rate_stats_request(soc_txrx_handle, (struct cdp_pdev *)pdev_txrx_handle);
            break;
#ifdef OBSS_PD
        case OL_ATH_PARAM_SET_CMD_OBSS_PD_THRESHOLD:
            OS_MEMZERO(&vap_opmode_count, sizeof(struct ieee80211_vap_opmode_count));
            ieee80211_get_vap_opmode_count(ic, &vap_opmode_count);

            /* Since spatial reuse works on a pdev level,
             * once both STA vap and AP vap present, or any monitor vap,
             * then we must disable spatial reuse params. This is because AP
             * and STA share the same HW register, so they will overwrite the
             * same fields in that register. Since Monitors do not transmit,
             * spatial reuse should be disabled for all monitor vaps.
             */
            if ((vap_opmode_count.ap_count && vap_opmode_count.sta_count) ||
                    (vap_opmode_count.monitor_count &&
                    !cfg_get(psoc, CFG_OL_ALLOW_MON_VAPS_IN_SR))) {
                retval = ol_ath_pdev_set_param(scn,
                            wmi_pdev_param_set_cmd_obss_pd_threshold,
                            !HE_SR_OBSS_PD_THRESH_ENABLE);
                if(retval) {
                    qdf_err("%s: Could not set obss pd thresh enable to 0", __func__);
                }
                else {
                    qdf_info("%s: Invalid vap combination for spatial reuse so"
                    " OBSS Thresh disabled. SR disallowed when both AP and STA vap"
                    " present or when MONITOR vap present", __func__);
                }
                return retval;
            }

            /* OBSS Packet Detect threshold bounds for Spatial Reuse feature.
             * The parameter value is programmed into the spatial reuse
             * register, to specify how low the background signal strength
             * from neighboring BSS cells must be, for this AP to
             * employ spatial reuse.
             */

            if (!GET_HE_OBSS_PD_THRESH_ENABLE(ic->ic_ap_obss_pd_thresh)
                                        || value > AP_OBSS_PD_UPPER_THRESH) {
                qdf_err("value %d invalid for this param", value);
                return -EINVAL;
            }

            /* Try to set threshold value by clearing out the bottom 8 bits which
             * is the current value and keep the currently set enable bit. Then,
             * OR the value with the enable bit to get the correct value and try
             * to wmi_send that value.
             */
            retval = ol_ath_pdev_set_param(scn,
                                           wmi_pdev_param_set_cmd_obss_pd_threshold,
                                           ((ic->ic_ap_obss_pd_thresh &
                                           ~HE_OBSS_PD_THRESH_MASK) | value));
            if(retval == EOK) {
                /* Since the value was successfully set, discard the old threshold
                 * value and overwrite with the specified value
                 */
                ic->ic_ap_obss_pd_thresh &= (~HE_OBSS_PD_THRESH_MASK);
                ic->ic_ap_obss_pd_thresh = (ic->ic_ap_obss_pd_thresh | value);
            } else {
                qdf_err("WMI send for set cmd obss pd threshold failed");
            }
        break;

        case OL_ATH_PARAM_SET_CMD_OBSS_PD_THRESHOLD_ENABLE:
            if(value > 1) {
                qdf_err("%s: OBSS_PD Threshold enable value should be either 0 or 1\n",
                        __func__);
                return -EINVAL;
            }

            /* Try to enable/disable obss thresh val */
            if(ol_ath_set_obss_thresh(ic, value, scn)) {
                qdf_err("Spatial reuse not allowed for AP/STA vap combination"
                " or for MONITOR vaps");
                return -EINVAL;
            }
            break;
#endif//OBSS_PD
#ifdef WLAN_RX_PKT_CAPTURE_ENH
        case OL_ATH_PARAM_RX_MON_LITE:

            if (QDF_STATUS_SUCCESS != cdp_txrx_set_pdev_param(soc_txrx_handle,
                   (void *) pdev_txrx_handle, CDP_CONFIG_ENH_RX_CAPTURE, value))
                return -EINVAL;

            ic->ic_rx_mon_lite = value;

            scn->soc->scn_rx_lite_monitor_mpdu_subscriber.callback
                = process_rx_mpdu;
            scn->soc->scn_rx_lite_monitor_mpdu_subscriber.context  = scn;
            cdp_wdi_event_sub(soc_txrx_handle, (void *) pdev_txrx_handle,
                &scn->soc->scn_rx_lite_monitor_mpdu_subscriber, WDI_EVENT_RX_MPDU);
        break;
#endif
        case OL_ATH_PARAM_TX_CAPTURE:
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
            nss_soc_cfg = cfg_get(soc->psoc_obj, CFG_NSS_WIFI_OL);

            if (nss_soc_cfg)
            {
                qdf_info("TX monitor mode not supported when NSS offload is enabled");
                break;
            }
#endif /* QCA_NSS_WIFI_OFFLOAD_SUPPORT */
            if (value > 2) {
               qdf_err("value %d is not allowed for this config "
                       "- supported enable/disable only", value);
               return -EINVAL;
            }
            ic->ic_tx_pkt_capture = value;
            if (ic->ic_tx_pkt_capture)
                ol_ath_set_debug_sniffer(scn, SNIFFER_TX_MONITOR_MODE);
            else
                ol_ath_set_debug_sniffer(scn, SNIFFER_DISABLE);

            cdp_txrx_set_pdev_param(soc_txrx_handle, (void *) pdev_txrx_handle,
                CDP_CONFIG_TX_CAPTURE, value);
        break;
        default:
            return (-1);
    }

#if QCN_ESP_IE
    if (ic->ic_esp_flag)
        wlan_pdev_beacon_update(ic);
#endif /* QCN_ESP_IE */

    return retval;
}

int
ol_ath_get_config_param(struct ol_ath_softc_net80211 *scn, enum _ol_ath_param_t param, void *buff)
{
    int retval = 0;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS || PEER_FLOW_CONTROL
    u_int32_t value = *(u_int32_t *)buff;
#endif
    struct ieee80211com *ic = &scn->sc_ic;
    struct wlan_objmgr_pdev *pdev;
    struct wlan_objmgr_psoc *psoc;
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_pdev_handle pdev_txrx_handle;
    bool bw_reduce = false;
#if QCA_AIRTIME_FAIRNESS
    int atf_sched = 0;
#endif
    struct wlan_lmac_if_dfs_rx_ops *dfs_rx_ops;
    struct wlan_psoc_host_hal_reg_capabilities_ext *reg_cap;
    uint8_t pdev_idx;
    struct common_wmi_handle *wmi_handle;
#ifdef DIRECT_BUF_RX_ENABLE
    struct wlan_lmac_if_direct_buf_rx_tx_ops *dbr_tx_ops = NULL;
#endif

    wmi_handle = lmac_get_wmi_hdl(scn->soc->psoc_obj);
    pdev = ic->ic_pdev_obj;
    if(pdev == NULL) {
        qdf_info("%s : pdev is null ", __func__);
        return -1;
    }

    psoc = wlan_pdev_get_psoc(pdev);

    if (psoc == NULL) {
        qdf_info("%s : psoc is null", __func__);
        return -1;
    }

    soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
    pdev_txrx_handle = wlan_pdev_get_dp_handle(pdev);
#if ATH_SUPPORT_DFS
    dfs_rx_ops = wlan_lmac_if_get_dfs_rx_ops(psoc);
#endif
    reg_cap = ucfg_reg_get_hal_reg_cap(psoc);
    pdev_idx = lmac_get_pdev_idx(pdev);

    switch(param)
    {
        case OL_ATH_PARAM_GET_IF_ID:
            *(int *)buff = IF_ID_OFFLOAD;
            break;

        case OL_ATH_PARAM_TXCHAINMASK:
            *(int *)buff = ieee80211com_get_tx_chainmask(ic);
            break;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS || ATH_SUPPORT_DSCP_OVERRIDE
        case OL_ATH_PARAM_HMMC_DSCP_TID_MAP:
            *(int *)buff = ol_ath_get_hmmc_tid(ic);
            break;

        case OL_ATH_PARAM_HMMC_DSCP_OVERRIDE:
            *(int *)buff = ol_ath_get_hmmc_dscp_override(ic);
            break;
#endif
        case OL_ATH_PARAM_RXCHAINMASK:
            *(int *)buff = ieee80211com_get_rx_chainmask(ic);
            break;
        case OL_ATH_PARAM_DYN_GROUPING:
            *(int *)buff = scn->dyngroup;
            break;
        case OL_ATH_PARAM_BCN_BURST:
            *(int *)buff = scn->bcn_mode;
            break;
        case OL_ATH_PARAM_DPD_ENABLE:
            *(int *)buff = scn->dpdenable;
            break;
        case OL_ATH_PARAM_ARPDHCP_AC_OVERRIDE:
            *(int *)buff = scn->arp_override;
            break;
        case OL_ATH_PARAM_IGMPMLD_OVERRIDE:
            *(int *)buff = scn->igmpmld_override;
            break;
        case OL_ATH_PARAM_IGMPMLD_TID:
            *(int *)buff = scn->igmpmld_tid;
            break;

        case OL_ATH_PARAM_TXPOWER_LIMIT2G:
            *(int *)buff = scn->txpowlimit2G;
            break;

        case OL_ATH_PARAM_TXPOWER_LIMIT5G:
            *(int *)buff = scn->txpowlimit5G;
            break;

        case OL_ATH_PARAM_TXPOWER_SCALE:
            *(int *)buff = scn->txpower_scale;
            break;
        case OL_ATH_PARAM_RTS_CTS_RATE:
            *(int *)buff =  scn->ol_rts_cts_rate;
            break;
        case OL_ATH_PARAM_DEAUTH_COUNT:
#if WDI_EVENT_ENABLE
            *(int *)buff =  scn->scn_user_peer_invalid_cnt;;
#endif
            break;
        case OL_ATH_PARAM_DYN_TX_CHAINMASK:
            *(int *)buff = scn->dtcs;
            break;
        case OL_ATH_PARAM_VOW_EXT_STATS:
            *(int *)buff = scn->vow_extstats;
            break;
        case OL_ATH_PARAM_DCS:
            /* do not need to talk to target */
            *(int *)buff = OL_IS_DCS_ENABLED(scn->scn_dcs.dcs_enable);
            break;
        case OL_ATH_PARAM_DCS_COCH_THR:
            *(int *)buff = scn->scn_dcs.coch_intr_thresh ;
            break;
        case OL_ATH_PARAM_DCS_TXERR_THR:
            *(int *)buff = scn->scn_dcs.tx_err_thresh;
            break;
        case OL_ATH_PARAM_DCS_PHYERR_THR:
            *(int *)buff = scn->scn_dcs.phy_err_threshold ;
            break;
        case OL_ATH_PARAM_DCS_PHYERR_PENALTY:
            *(int *)buff = scn->scn_dcs.phy_err_penalty ;
            break;
        case OL_ATH_PARAM_DCS_RADAR_ERR_THR:
            *(int *)buff = scn->scn_dcs.radar_err_threshold ;
            break;
        case OL_ATH_PARAM_DCS_USERMAX_CU_THR:
            *(int *)buff = scn->scn_dcs.user_max_cu ;
            break;
        case OL_ATH_PARAM_DCS_INTR_DETECT_THR:
            *(int *)buff = scn->scn_dcs.intr_detection_threshold ;
            break;
        case OL_ATH_PARAM_DCS_SAMPLE_WINDOW:
            *(int *)buff = scn->scn_dcs.intr_detection_window ;
            break;
        case OL_ATH_PARAM_DCS_DEBUG:
            *(int *)buff = scn->scn_dcs.dcs_debug ;
            break;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
        case OL_ATH_PARAM_BUFF_THRESH:
            *(int *)buff = scn->soc->buff_thresh.pool_size - scn->soc->buff_thresh.ald_free_buf_lvl;
            break;
        case OL_ATH_PARAM_BLK_REPORT_FLOOD:
            *(int *)buff = ic->ic_blkreportflood;
            break;
        case OL_ATH_PARAM_DROP_STA_QUERY:
            *(int *)buff = ic->ic_dropstaquery;
            break;
#endif
        case OL_ATH_PARAM_BURST_ENABLE:
            *(int *)buff = scn->burst_enable;
            break;
        case OL_ATH_PARAM_CCA_THRESHOLD:
            *(int *)buff = scn->cca_threshold;
            break;
        case OL_ATH_PARAM_BURST_DUR:
            *(int *)buff = scn->burst_dur;
            break;
        case OL_ATH_PARAM_ANI_ENABLE:
            *(int *)buff =  (scn->is_ani_enable == true);
            break;
        case OL_ATH_PARAM_ACS_CTRLFLAG:
            if(ic->ic_acs){
                *(int *)buff = ieee80211_acs_get_param(ic->ic_acs, IEEE80211_ACS_CTRLFLAG );
            }
            break;
        case OL_ATH_PARAM_ACS_ENABLE_BK_SCANTIMEREN:
            if(ic->ic_acs){
                *(int *)buff = ieee80211_acs_get_param(ic->ic_acs, IEEE80211_ACS_ENABLE_BK_SCANTIMER );
            }
            break;
        case OL_ATH_PARAM_ACS_SCANTIME:
            if(ic->ic_acs){
                *(int *)buff = ieee80211_acs_get_param(ic->ic_acs, IEEE80211_ACS_SCANTIME );
            }
            break;
        case OL_ATH_PARAM_ACS_RSSIVAR:
            if(ic->ic_acs){
                *(int *)buff = ieee80211_acs_get_param(ic->ic_acs, IEEE80211_ACS_RSSIVAR );
            }
            break;
        case OL_ATH_PARAM_ACS_CHLOADVAR:
            if(ic->ic_acs){
                *(int *)buff = ieee80211_acs_get_param(ic->ic_acs, IEEE80211_ACS_CHLOADVAR );
            }
            break;
        case OL_ATH_PARAM_ACS_SRLOADVAR:
            if(ic->ic_acs){
                *(int *)buff = ieee80211_acs_get_param(ic->ic_acs, IEEE80211_ACS_SRLOADVAR );
            }
            break;
        case OL_ATH_PARAM_ACS_LIMITEDOBSS:
            if(ic->ic_acs){
                *(int *)buff = ieee80211_acs_get_param(ic->ic_acs, IEEE80211_ACS_LIMITEDOBSS);
            }
            break;
        case OL_ATH_PARAM_ACS_DEBUGTRACE:
            if(ic->ic_acs){
                *(int *)buff = ieee80211_acs_get_param(ic->ic_acs, IEEE80211_ACS_DEBUGTRACE);
            }
            break;
#if ATH_CHANNEL_BLOCKING
        case OL_ATH_PARAM_ACS_BLOCK_MODE:
            if (ic->ic_acs) {
                *(int *)buff = ieee80211_acs_get_param(ic->ic_acs, IEEE80211_ACS_BLOCK_MODE);
            }
            break;
#endif
        case OL_ATH_PARAM_RESET_OL_STATS:
            ol_ath_reset_vap_stat(ic);
            break;
        case OL_ATH_PARAM_TOTAL_PER:
            *(int *)buff =
                ol_ath_net80211_get_total_per(ic);
            break;
#if ATH_RX_LOOPLIMIT_TIMER
        case OL_ATH_PARAM_LOOPLIMIT_NUM:
            *(int *)buff = scn->rx_looplimit_timeout;
            break;
#endif
        case OL_ATH_PARAM_RADIO_TYPE:
            *(int *)buff = ic->ic_is_mode_offload(ic);
            break;

        case OL_ATH_PARAM_FW_RECOVERY_ID:
            *(int *)buff = scn->soc->recovery_enable;
            break;
        case OL_ATH_PARAM_FW_DUMP_NO_HOST_CRASH:
            *(int *)buff = (scn->soc->sc_dump_opts & FW_DUMP_NO_HOST_CRASH ? 1: 0);
            break;
        case OL_ATH_PARAM_DISABLE_DFS:
            *(int *)buff =	(scn->sc_is_blockdfs_set == true);
            break;
        case OL_ATH_PARAM_PS_STATE_CHANGE:
            {
                *(int *) buff =  scn->ps_report ;
            }
            break;
        case OL_ATH_PARAM_BLOCK_INTERBSS:
            *(int*)buff = scn->scn_block_interbss;
            break;
        case OL_ATH_PARAM_SET_TXBF_SND_PERIOD:
            qdf_info("\n scn->txbf_sound_period hex %x %d\n", scn->txbf_sound_period, scn->txbf_sound_period);
            *(int*)buff = scn->txbf_sound_period;
            break;
#if ATH_SUPPORT_WRAP
        case OL_ATH_PARAM_MCAST_BCAST_ECHO:
            *(int*)buff = scn->mcast_bcast_echo;
            break;
        case OL_ATH_PARAM_ISOLATION:
            *(int *)buff = ic->ic_wrap_com->wc_isolation;
            break;
#endif
        case OL_ATH_PARAM_OBSS_RSSI_THRESHOLD:
            {
                *(int*)buff = ic->obss_rssi_threshold;
            }
            break;
        case OL_ATH_PARAM_OBSS_RX_RSSI_THRESHOLD:
            {
                *(int*)buff = ic->obss_rx_rssi_threshold;
            }
            break;
        case OL_ATH_PARAM_ALLOW_PROMISC:
            {
                *(int*)buff = (scn->scn_promisc || promisc_is_active(&scn->sc_ic)) ? 1 : 0;
            }
            break;
        case OL_ATH_PARAM_ACS_TX_POWER_OPTION:
            if(ic->ic_acs){
                *(int *)buff = ieee80211_acs_get_param(ic->ic_acs, IEEE80211_ACS_TX_POWER_OPTION);
            }
            break;
         case OL_ATH_PARAM_ACS_2G_ALLCHAN:
            if(ic->ic_acs){
                *(int *)buff = ieee80211_acs_get_param(ic->ic_acs, IEEE80211_ACS_2G_ALL_CHAN);
            }
            break;

        case OL_ATH_PARAM_MAX_CLIENTS_PER_RADIO:
             *(int*)buff = ic->ic_num_clients;
        break;

        case OL_ATH_PARAM_ENABLE_AMSDU:
             *(int*)buff = scn->scn_amsdu_mask;
        break;

#if WLAN_CFR_ENABLE
        case OL_ATH_PARAM_PERIODIC_CFR_CAPTURE:
            if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_CFR_ID) !=
                    QDF_STATUS_SUCCESS) {
                return -1;
            }

            *(int *)buff = ucfg_cfr_get_timer(pdev);

            wlan_objmgr_pdev_release_ref(pdev, WLAN_CFR_ID);
        break;
#endif

        case OL_ATH_PARAM_ENABLE_AMPDU:
             *(int*)buff = scn->scn_ampdu_mask;
        break;

       case OL_ATH_PARAM_PRINT_RATE_LIMIT:
             *(int*)buff = scn->soc->dbg.print_rate_limit;
       break;
        case OL_ATH_PARAM_CONSIDER_OBSS_NON_ERP_LONG_SLOT:
            *(int*)buff = ic->ic_consider_obss_long_slot;
        break;

#if PEER_FLOW_CONTROL
        case OL_ATH_PARAM_VIDEO_STATS_FC:
        case OL_ATH_PARAM_STATS_FC:
        case OL_ATH_PARAM_QFLUSHINTERVAL:
        case OL_ATH_PARAM_TOTAL_Q_SIZE:
        case OL_ATH_PARAM_MIN_THRESHOLD:
        case OL_ATH_PARAM_MAX_Q_LIMIT:
        case OL_ATH_PARAM_MIN_Q_LIMIT:
        case OL_ATH_PARAM_CONG_CTRL_TIMER_INTV:
        case OL_ATH_PARAM_STATS_TIMER_INTV:
        case OL_ATH_PARAM_ROTTING_TIMER_INTV:
        case OL_ATH_PARAM_LATENCY_PROFILE:
            {
                cdp_pflow_update_pdev_params(soc_txrx_handle,
                        (void *)pdev_txrx_handle, param, value, buff);
            }
            break;
#endif

        case OL_ATH_PARAM_DBG_ARP_SRC_ADDR:
        {
             /* arp dbg stats */
             qdf_info("---- ARP DBG STATS ---- \n");
             qdf_info("\n TX_ARP_REQ \t TX_ARP_RESP \t RX_ARP_REQ \t RX_ARP_RESP\n");
             qdf_info("\n %d \t\t %d \t %d \t %d \n", scn->sc_tx_arp_req_count, scn->sc_tx_arp_resp_count, scn->sc_rx_arp_req_count, scn->sc_rx_arp_resp_count);
        }
        break;

        case OL_ATH_PARAM_ARP_DBG_CONF:

             *(int*)buff = scn->sc_arp_dbg_conf;
        break;

        case OL_ATH_PARAM_DISABLE_STA_VAP_AMSDU:
            *(int*)buff = ic->ic_sta_vap_amsdu_disable;
        break;

#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
        case OL_ATH_PARAM_STADFS_ENABLE:
        if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                QDF_STATUS_SUCCESS) {
            return -1;
        }

        if (dfs_rx_ops && dfs_rx_ops->dfs_is_stadfs_enabled)
            *(int *)buff = dfs_rx_ops->dfs_is_stadfs_enabled(pdev);

        wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
        break;
#endif
        case OL_ATH_PARAM_CHANSWITCH_OPTIONS:
            (*(int *)buff) = ic->ic_chanswitch_flags;
            qdf_info(
                    "IEEE80211_CSH_OPT_NONDFS_RANDOM    0x00000001\n"
                    "IEEE80211_CSH_OPT_IGNORE_CSA_DFS   0x00000002\n"
                    "IEEE80211_CSH_OPT_CAC_APUP_BYSTA   0x00000004\n"
                    "IEEE80211_CSH_OPT_CSA_APUP_BYSTA   0x00000008\n"
                    "IEEE80211_CSH_OPT_RCSA_TO_UPLINK   0x00000010\n"
                    "IEEE80211_CSH_OPT_PROCESS_RCSA     0x00000020\n"
                    "IEEE80211_CSH_OPT_APRIORI_NEXT_CHANNEL 0x00000040\n"
                    "IEEE80211_CSH_OPT_AVOID_DUAL_CAC   0x00000080\n"
                    );
        break;
        case OL_ATH_PARAM_BW_REDUCE:
            if (dfs_rx_ops && dfs_rx_ops->dfs_is_bw_reduction_needed) {
                if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                        QDF_STATUS_SUCCESS) {
                    return -1;
                }

                dfs_rx_ops->dfs_is_bw_reduction_needed(pdev, &bw_reduce);
                *(int *) buff = bw_reduce;
                wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
            }
            break;
#if DBDC_REPEATER_SUPPORT
        case OL_ATH_PARAM_PRIMARY_RADIO:
            *(int *) buff =  ic->ic_primary_radio;
            break;
        case OL_ATH_PARAM_DBDC_ENABLE:
            *(int *) buff =  ic->ic_global_list->dbdc_process_enable;
            break;
        case OL_ATH_PARAM_CLIENT_MCAST:
            *(int *)buff = ic->ic_global_list->force_client_mcast_traffic;
            break;
#endif
        case OL_ATH_PARAM_CTL_POWER_SCALE:
            *(int *)buff = scn->powerscale;
            break;
#if QCA_AIRTIME_FAIRNESS
    case  OL_ATH_PARAM_ATF_STRICT_SCHED:
        atf_sched = target_if_atf_get_sched(psoc, pdev);
        *(int *)buff =!!(atf_sched & ATF_SCHED_STRICT);
        break;
    case  OL_ATH_PARAM_ATF_GROUP_POLICY:
        atf_sched = target_if_atf_get_sched(psoc, pdev);
        *(int *)buff =  !!(atf_sched & ATF_GROUP_SCHED_POLICY);
        break;
    case  OL_ATH_PARAM_ATF_OBSS_SCHED:
        atf_sched = target_if_atf_get_sched(psoc, pdev);
        *(int *)buff =!!(atf_sched & ATF_SCHED_OBSS);
        break;
    case  OL_ATH_PARAM_ATF_OBSS_SCALE:
        *(int *)buff = target_if_atf_get_obss_scale(psoc, pdev);
        break;
#endif
        case OL_ATH_PARAM_PHY_OFDM_ERR:
#ifdef QCA_SUPPORT_CP_STATS
            *(int *)buff = pdev_cp_stats_rx_phy_err_get(pdev);
#endif
            break;
        case OL_ATH_PARAM_PHY_CCK_ERR:
#ifdef QCA_SUPPORT_CP_STATS
            *(int *)buff = pdev_cp_stats_rx_phy_err_get(pdev);
#endif
            break;
        case OL_ATH_PARAM_FCS_ERR:
#ifdef QCA_SUPPORT_CP_STATS
            *(int *)buff = pdev_cp_stats_fcsbad_get(pdev);
#endif
            break;
        case OL_ATH_PARAM_CHAN_UTIL:
#ifdef QCA_SUPPORT_CP_STATS
            *(int *)buff = 100 - ucfg_pdev_chan_stats_free_medium_get(pdev);
#endif
            break;
        case OL_ATH_PARAM_EMIWAR_80P80:
            *(int *)buff = ic->ic_emiwar_80p80;
            break;
#if UMAC_SUPPORT_ACFG
        case OL_ATH_PARAM_DIAG_ENABLE:
            *(int *)buff = ic->ic_diag_enable;
        break;
#endif

        case OL_ATH_PARAM_CHAN_STATS_TH:
            *(int *)buff = ic->ic_chan_stats_th;
            break;

        case OL_ATH_PARAM_PASSIVE_SCAN_ENABLE:
            *(int *)buff = ic->ic_strict_pscan_enable;
            break;

        case OL_ATH_MIN_RSSI_ENABLE:
            *(int *)buff = ic->ic_min_rssi_enable;
            break;
        case OL_ATH_MIN_RSSI:
            *(int *)buff = ic->ic_min_rssi;
            break;
#if DBDC_REPEATER_SUPPORT
        case OL_ATH_PARAM_DELAY_STAVAP_UP:
            *(int *)buff = ic->ic_global_list->delay_stavap_connection;
            break;
#endif
        case OL_ATH_BTCOEX_ENABLE:
            *((int *) buff) = scn->soc->btcoex_enable;
            break;
        case OL_ATH_BTCOEX_WL_PRIORITY:
            *((int *) buff) = scn->soc->btcoex_wl_priority;
            break;
        case OL_ATH_GET_BTCOEX_DUTY_CYCLE:
            qdf_info("period: %d wlan_duration: %d ",
		      scn->soc->btcoex_period,scn->soc->btcoex_duration);
            *(int *)buff = scn->soc->btcoex_period;
            break;
        case OL_ATH_PARAM_CAL_VER_CHECK:
            *(int *)buff = ic->ic_cal_ver_check;
           break;
        case OL_ATH_PARAM_TID_OVERRIDE_QUEUE_MAPPING:
            if (scn->soc->ol_if_ops->ol_pdev_get_tid_override_queue_mapping)
               *(int *)buff =
                  scn->soc->ol_if_ops->ol_pdev_get_tid_override_queue_mapping(pdev_txrx_handle);
            break;
        case OL_ATH_PARAM_NO_VLAN:
            *(int *)buff = ic->ic_no_vlan;
            break;
        case OL_ATH_PARAM_ATF_LOGGING:
            *(int *)buff = ic->ic_atf_logging;
            break;
        case OL_ATH_PARAM_STRICT_DOTH:
            *(int *)buff = ic->ic_strict_doth;
            break;
        case OL_ATH_PARAM_CHANNEL_SWITCH_COUNT:
            *(int *)buff = ic->ic_chan_switch_cnt;
            break;
#if DBDC_REPEATER_SUPPORT
        case OL_ATH_PARAM_DISCONNECTION_TIMEOUT:
            *(int *)buff = ic->ic_global_list->disconnect_timeout;
            break;
        case OL_ATH_PARAM_RECONFIGURATION_TIMEOUT:
            *(int *)buff = ic->ic_global_list->reconfiguration_timeout;
            break;
        case OL_ATH_PARAM_ALWAYS_PRIMARY:
            *(int *)buff = ic->ic_global_list->always_primary;
            break;
        case OL_ATH_PARAM_FAST_LANE:
            *(int *)buff = ic->fast_lane;
            break;
        case OL_ATH_PARAM_PREFERRED_UPLINK:
            *(int *) buff =  ic->ic_preferredUplink;
            break;
#endif
        case OL_ATH_PARAM_SECONDARY_OFFSET_IE:
            *(int *)buff = ic->ic_sec_offsetie;
            break;
        case OL_ATH_PARAM_WIDE_BAND_SUB_ELEMENT:
            *(int *)buff = ic->ic_wb_subelem;
            break;
#if ATH_SUPPORT_ZERO_CAC_DFS
        case OL_ATH_PARAM_PRECAC_ENABLE:
            if (dfs_rx_ops && dfs_rx_ops->dfs_get_precac_enable) {
                if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                        QDF_STATUS_SUCCESS) {
                    return -1;
                }
                dfs_rx_ops->dfs_get_precac_enable(pdev, (int *)buff);
                wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
            }
            break;
        case OL_ATH_PARAM_PRECAC_TIMEOUT:
            {
                int tmp = 0;

                /* Call a function to get the precac timeout value */
                if (dfs_rx_ops && dfs_rx_ops->dfs_get_override_precac_timeout) {
                    if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                            QDF_STATUS_SUCCESS) {
                        return -1;
                    }
                    dfs_rx_ops->dfs_get_override_precac_timeout(pdev, &tmp);
                    wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
                }
                *(int *)buff = tmp;
            }
            break;
#ifdef WLAN_DFS_PRECAC_AUTO_CHAN_SUPPORT
        case OL_ATH_PARAM_PRECAC_INTER_CHANNEL:
            if (dfs_rx_ops && dfs_rx_ops->dfs_get_precac_intermediate_chan) {
                if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                        QDF_STATUS_SUCCESS) {
                    return -1;
                }
                dfs_rx_ops->dfs_get_precac_intermediate_chan(pdev,
							     (int *)buff);
                wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
            }
           break;
        case OL_ATH_PARAM_PRECAC_CHAN_STATE:
            if (dfs_rx_ops && dfs_rx_ops->dfs_get_precac_chan_state) {
                if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                        QDF_STATUS_SUCCESS) {
                    return -1;
                }
                retval = dfs_rx_ops->dfs_get_precac_chan_state(pdev,
                                                        *((int *)buff + 1));
                wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
                if (retval == PRECAC_ERR)
                    retval = -EINVAL;
                else {
                    *((int *)buff) = retval;
                    retval = EOK;
                }
            }
            break;
#endif
#endif
        case OL_ATH_PARAM_PDEV_TO_REO_DEST:
            *(int *)buff = cdp_get_pdev_reo_dest(soc_txrx_handle,
                                           (void *)pdev_txrx_handle);
            break;

        case OL_ATH_PARAM_DUMP_CHAINMASK_TABLES:
            ol_ath_dump_chainmaks_tables(scn);
            break;

        case OL_ATH_PARAM_MGMT_RSSI_THRESHOLD:
            *(int *)buff = ic->mgmt_rx_rssi;
            break;

        case OL_ATH_PARAM_EXT_NSS_CAPABLE:
             *(int *)buff = ic->ic_ext_nss_capable;
             break;

#if QCN_ESP_IE
        case OL_ATH_PARAM_ESP_PERIODICITY:
            *(int *)buff = ic->ic_esp_periodicity;
            break;
        case OL_ATH_PARAM_ESP_AIRTIME:
            *(int *)buff = ic->ic_esp_air_time_fraction;
            break;
        case OL_ATH_PARAM_ESP_PPDU_DURATION:
            *(int *)buff = ic->ic_esp_ppdu_duration;
            break;
        case OL_ATH_PARAM_ESP_BA_WINDOW:
            *(int *)buff = ic->ic_esp_ba_window;
            break;
#endif /* QCN_ESP_IE */

        case OL_ATH_PARAM_MGMT_PDEV_STATS_TIMER:
            *(int *)buff = scn->pdev_stats_timer;

        case OL_ATH_PARAM_ICM_ACTIVE:
            *(int *)buff = ic->ic_extacs_obj.icm_active;
            break;

#if ATH_SUPPORT_ICM
        case OL_ATH_PARAM_NOMINAL_NOISEFLOOR:
            *(int *)buff = ol_get_nominal_nf(ic);
            break;
#endif

        case OL_ATH_PARAM_TXACKTIMEOUT:
            if (wmi_service_enabled(wmi_handle,wmi_service_ack_timeout))
            {
                *(int *)buff = scn->tx_ack_timeout;
            }
            else
            {
                qdf_info("TX ACK Timeout Service is not supported");
                retval = -1;
            }
            break;

        case OL_ATH_PARAM_ACS_RANK:
            if (ic->ic_acs){
                *(int *)buff = ieee80211_acs_get_param(ic->ic_acs, IEEE80211_ACS_RANK);
            }
            break;

#ifdef OL_ATH_SMART_LOGGING
        case OL_ATH_PARAM_SMARTLOG_ENABLE:
            *(int *)buff = ic->smart_logging;
            break;

        case OL_ATH_PARAM_SMARTLOG_SKB_SZ:
            *(int *)buff = ic->smart_log_skb_sz;
            break;

        case OL_ATH_PARAM_SMARTLOG_P1PINGFAIL:
            *(int *)buff = ic->smart_logging_p1pingfail_started;
            break;
#endif /* OL_ATH_SMART_LOGGING */

        case OL_ATH_PARAM_CBS:
            if (ic->ic_cbs) {
                *(int *)buff = ieee80211_cbs_get_param(ic->ic_cbs, IEEE80211_CBS_ENABLE);
            }
            break;

        case OL_ATH_PARAM_CBS_DWELL_SPLIT_TIME:
            if (ic->ic_cbs) {
                *(int *)buff = ieee80211_cbs_get_param(ic->ic_cbs, IEEE80211_CBS_DWELL_SPLIT_TIME);
            }
            break;

        case OL_ATH_PARAM_CBS_DWELL_REST_TIME:
            if (ic->ic_cbs) {
                *(int *)buff = ieee80211_cbs_get_param(ic->ic_cbs, IEEE80211_CBS_DWELL_REST_TIME);
            }
            break;

        case OL_ATH_PARAM_CBS_WAIT_TIME:
            if (ic->ic_cbs) {
                *(int *)buff = ieee80211_cbs_get_param(ic->ic_cbs, IEEE80211_CBS_WAIT_TIME);
            }
            break;

        case OL_ATH_PARAM_CBS_REST_TIME:
            if (ic->ic_cbs) {
                *(int *)buff = ieee80211_cbs_get_param(ic->ic_cbs, IEEE80211_CBS_REST_TIME);
            }
            break;

        case OL_ATH_PARAM_CBS_CSA:
            if (ic->ic_cbs) {
                *(int *)buff = ieee80211_cbs_get_param(ic->ic_cbs, IEEE80211_CBS_CSA_ENABLE);
            }
            break;


        case OL_ATH_PARAM_TXCHAINSOFT:
            *(int *)buff = scn->soft_chain;
            break;

        case OL_ATH_PARAM_WIDE_BAND_SCAN:
            *(int *)buff = ic->ic_widebw_scan;
            break;

        case OL_ATH_PARAM_CCK_TX_ENABLE:
            if (reg_cap == NULL)
            {
                qdf_info("reg_cap is NULL, unable to process further. Investigate.");
                retval = -1;
            } else {
                if (reg_cap[pdev_idx].wireless_modes & WIRELESS_MODES_2G)
                {
                    *(int *)buff = ic->cck_tx_enable;
                }
                else
                {
                    qdf_info("CCK Tx is not supported for this band");
                    retval = -1;
                }
            }
            break;

#if ATH_PARAMETER_API
        case OL_ATH_PARAM_PAPI_ENABLE:
            *(int *)buff = ic->ic_papi_enable;
            break;
#endif
#if defined(WLAN_DFS_PARTIAL_OFFLOAD) && defined(HOST_DFS_SPOOF_TEST)
        case OL_ATH_PARAM_DFS_HOST_WAIT_TIMEOUT:
            {
                int tmp;

                /* Call a function to get the precac timeout value */
                if (dfs_rx_ops && dfs_rx_ops->dfs_get_override_status_timeout) {
                    if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                            QDF_STATUS_SUCCESS) {
                        return -1;
                    }
                    dfs_rx_ops->dfs_get_override_status_timeout(pdev, &tmp);
                    wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
                    *(int *)buff = tmp;
                } else {
                    qdf_info(" Host Wait Timeout is not supported");
                    retval = -1;
                }
            }
            break;
#endif /* HOST_DFS_SPOOF_TEST */
        case OL_ATH_PARAM_NF_THRESH:
            if (ic->ic_acs)
                *(int *)buff =  ieee80211_acs_get_param(ic->ic_acs, IEEE80211_ACS_NF_THRESH);
            else {
                qdf_info("Failed to get ACS NF Threshold");
                retval = -1;
            }
            break;
#ifdef DIRECT_BUF_RX_ENABLE
	case OL_ATH_PARAM_DBR_RING_STATUS:
	    dbr_tx_ops = &psoc->soc_cb.tx_ops.dbr_tx_ops;
            if (dbr_tx_ops->direct_buf_rx_print_ring_stat) {
                dbr_tx_ops->direct_buf_rx_print_ring_stat(pdev);
            }
            break;
#endif
        case OL_ATH_PARAM_ACTIVITY_FACTOR:
            {
                *(int *)buff =
                    (((scn->mib_cycle_cnts.rx_clear_count - scn->mib_cycle_cnts.rx_frame_count -
                       scn->mib_cycle_cnts.tx_frame_count) / scn->mib_cycle_cnts.cycle_count) * 100);
            }
            break;
        case OL_ATH_PARAM_ENABLE_PEER_RETRY_STATS:
            *(int *)buff = scn->retry_stats;
        break;
	case OL_ATH_PARAM_DFS_NOL_SUBCHANNEL_MARKING:
#if ATH_SUPPORT_DFS
	    {
		bool tmpval;
		if (dfs_rx_ops && dfs_rx_ops->dfs_get_nol_subchannel_marking) {
			if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
					QDF_STATUS_SUCCESS) {
				return -1;
			}
		retval = dfs_rx_ops->dfs_get_nol_subchannel_marking(pdev, &tmpval);
		wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
		if (retval == QDF_STATUS_SUCCESS)
			*(int *)buff = tmpval;
		}
	    }
	break;
#else
	retval = -EINVAL;
	break;
#endif
#ifdef QCA_SUPPORT_CP_STATS
        case OL_ATH_PARAM_CHAN_AP_RX_UTIL:
            *(int *)buff = pdev_chan_stats_ap_rx_util_get(pdev);
            break;
        case OL_ATH_PARAM_CHAN_FREE:
            *(int *)buff = pdev_chan_stats_free_medium_get(pdev);
            break;
        case OL_ATH_PARAM_CHAN_AP_TX_UTIL:
            *(int *)buff = pdev_chan_stats_ap_tx_util_get(pdev);
            break;
        case OL_ATH_PARAM_CHAN_OBSS_RX_UTIL:
            *(int *)buff = pdev_chan_stats_obss_rx_util_get(pdev);
            break;
        case OL_ATH_PARAM_CHAN_NON_WIFI:
            *(int *)buff = pdev_chan_stats_non_wifi_util_get(pdev);
            break;
        case OL_ATH_PARAM_HE_UL_TRIG_INT:
            *(int *)buff = ic->ic_he_ul_trig_int;
            break;
#endif
        case OL_ATH_PARAM_BAND_INFO:
            *(int *)buff = ol_ath_fill_umac_radio_band_info(pdev);
            break;
        case OL_ATH_PARAM_HE_SR:
            *(int *)buff = ic->ic_he_sr_enable;
            break;
        case OL_ATH_PARAM_HE_UL_PPDU_DURATION:
            *(int *)buff = ic->ic_he_ul_ppdu_dur;
            break;
        case OL_ATH_PARAM_HE_UL_RU_ALLOCATION:
            *(int *)buff = ic->ic_he_ru_allocation;
#ifdef WLAN_RX_PKT_CAPTURE_ENH
        case OL_ATH_PARAM_RX_MON_LITE:
            *(int *)buff = ic->ic_rx_mon_lite;
            break;
#endif
        case OL_ATH_PARAM_TX_CAPTURE:
            *(int *)buff = ic->ic_tx_pkt_capture;
            break;
        case OL_ATH_PARAM_SET_CMD_OBSS_PD_THRESHOLD:
            *(int *)buff = (HE_OBSS_PD_THRESH_MASK & ic->ic_ap_obss_pd_thresh);
            break;
        case OL_ATH_PARAM_SET_CMD_OBSS_PD_THRESHOLD_ENABLE:
            *(int *)buff = GET_HE_OBSS_PD_THRESH_ENABLE(ic->ic_ap_obss_pd_thresh);
            break;
        default:
            return (-1);
    }
    return retval;
}


int
ol_hal_set_config_param(struct ol_ath_softc_net80211 *scn, enum _ol_hal_param_t param, void *buff)
{
    return -1;
}

int
ol_hal_get_config_param(struct ol_ath_softc_net80211 *scn, enum _ol_hal_param_t param, void *address)
{
    return -1;
}

int
ol_net80211_set_mu_whtlist(wlan_if_t vap, u_int8_t *macaddr, u_int16_t tidmask)
{
    int retval = 0;
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);

    if((retval = ol_ath_node_set_param(scn, macaddr, WMI_HOST_PEER_SET_MU_WHITELIST,
            tidmask, avn->av_if_id))) {
        qdf_info("%s:Unable to set peer MU white list\n", __func__);
    }
    return retval;
}
#endif /* ATH_PERF_PWR_OFFLOAD */

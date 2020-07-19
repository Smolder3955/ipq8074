/*
 * Copyright (c) 2016-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
#include <linux/config.h>
#endif
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/utsname.h>
#include <linux/if_arp.h>       /* XXX for ARPHRD_ETHER */

#include <asm/uaccess.h>

#include <osif_private.h>
#include <wlan_opts.h>

#include "qdf_mem.h"
#include "ieee80211_var.h"
#include "ol_if_athvar.h"
#include "if_athioctl.h"
#include "init_deinit_lmac.h"
#include "fw_dbglog_api.h"
#include "ol_regdomain.h"
#if UNIFIED_SMARTANTENNA
#include <target_if_sa_api.h>
#endif /* UNIFIED_SMARTANTENNA */

#include "target_if.h"
#include "fw_dbglog_api.h"

#include <acfg_api_types.h>
#include "ol_txrx_stats.h"
#include "ol_if_stats.h"
#include "cdp_txrx_ctrl.h"
#include "ol_ath_ucfg.h"
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include <osif_nss_wifiol_if.h>
#endif
#include <wlan_lmac_if_api.h>
#if WLAN_SPECTRAL_ENABLE
#include <wlan_spectral_ucfg_api.h>
#endif

#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_utils_api.h>
#endif

#include "wlan_utility.h"
#include "cfg_ucfg_api.h"
#include <ieee80211_channel.h>

#define FLAG_PARTIAL_OL 1
#define FLAG_LITHIUM 2

extern void acfg_convert_to_acfgprofile (struct ieee80211_profile *profile,
                acfg_radio_vap_info_t *acfg_profile);
extern int wlan_get_vap_info(struct ieee80211vap *vap,
                struct ieee80211vap_profile *vap_profile,
                void *handle);
extern int ol_ath_target_start(ol_ath_soc_softc_t *soc);
extern int osif_ol_ll_vap_hardstart(struct sk_buff *skb, struct net_device *dev);
extern int osif_ol_vap_hardstart_wifi3(struct sk_buff *skb, struct net_device *dev);
extern int osif_ol_vap_send_exception_wifi3(struct sk_buff *skb, struct net_device *dev, void *mdata);

extern int wlan_cfg80211_ftm_testmode_cmd(struct wlan_objmgr_pdev *pdev,
        void *data, uint32_t len);
extern int wlan_ioctl_ftm_testmode_cmd(struct wlan_objmgr_pdev *pdev,
        int cmd, uint8_t *userdata, uint32_t length);

#if ATH_SUPPORT_ICM

#define SPECTRAL_PHY_CCA_NOM_VAL_AR9888_2GHZ    (-108)
#define SPECTRAL_PHY_CCA_NOM_VAL_AR9888_5GHZ    (-105)

/**
 * ol_get_nominal_nf() - Get nominal noise floor
 * @ic: Pointer to struct ieee80211com
 *
 * XXX Since we do not have a way to extract nominal noisefloor value from
 * firmware, we are exporting the values from the Host layers. These values are
 * taken from the file ar6000_reset.h. Can be replaced later.
 *
 * Return: Nominal noise floor
 */

int ol_get_nominal_nf(struct ieee80211com *ic)
{
    u_int32_t channel_freq;
    enum band_info band;
    int16_t nominal_nf = 0;

    channel_freq = ic->ic_curchan->ic_freq;
    band = (channel_freq > 4000)? BAND_5G : BAND_2G;

    if (band == BAND_5G) {
        nominal_nf = SPECTRAL_PHY_CCA_NOM_VAL_AR9888_5GHZ;
    } else {
        nominal_nf = SPECTRAL_PHY_CCA_NOM_VAL_AR9888_2GHZ;
    }

    return nominal_nf;
}
#else
int ol_get_nominal_nf(struct ieee80211com *ic)
{
	return 0;
}
#endif /* ATH_SUPPORT_ICM */

static void ol_ath_vap_iter_update_shpreamble(void *arg, wlan_if_t vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
    bool val = (*(u_int32_t *)arg)?1:0;

    ol_ath_wmi_send_vdev_param(scn, avn->av_if_id,
                    wmi_vdev_param_preamble,
                    (val) ? WMI_HOST_VDEV_PREAMBLE_SHORT : WMI_HOST_VDEV_PREAMBLE_LONG);
}

int ol_ath_ucfg_setparam(void *vscn, int param, int value)
{
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211*)vscn;
    struct ieee80211com *ic = &scn->sc_ic;
    bool restart_vaps = FALSE;
    int retval = 0;
    uint16_t orig_cc;
    struct wlan_lmac_if_reg_rx_ops *reg_rx_ops;
    struct wlan_objmgr_pdev *pdev;
    struct wlan_objmgr_psoc *psoc;
    struct common_wmi_handle *wmi_handle;
    struct common_wmi_handle *pdev_wmi_handle;
    void *dbglog_handle;
    struct target_psoc_info *tgt_psoc_info;
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_pdev_handle pdev_txrx_handle;

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    pdev_txrx_handle = wlan_pdev_get_dp_handle(scn->sc_pdev);
    if (atomic_read(&scn->soc->reset_in_progress)) {
        qdf_print("Reset in progress, return");
        return -1;
    }

    if (scn->soc->down_complete) {
        qdf_print("Starting the target before sending the command");
        if (ol_ath_target_start(scn->soc)) {
            qdf_print("failed to start the target");
            return -1;
        }
    }
    wmi_handle = lmac_get_wmi_hdl(scn->soc->psoc_obj);
    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);

    tgt_psoc_info = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
                                                    scn->soc->psoc_obj);
    if (tgt_psoc_info == NULL) {
        qdf_print("%s: target_psoc_info is null ", __func__);
        return -EINVAL;
    }

    if (!(dbglog_handle = target_psoc_get_dbglog_hdl(tgt_psoc_info))) {
        qdf_print("%s: dbglog_handle is null ", __func__);
        return -EINVAL;
    }

    /*
     ** Code Begins
     ** Since the parameter passed is the value of the parameter ID, we can call directly
     */
    if ( param & OL_ATH_PARAM_SHIFT )
    {
        /*
         ** It's an ATH value.  Call the  ATH configuration interface
         */

        param -= OL_ATH_PARAM_SHIFT;
        retval = ol_ath_set_config_param(scn, (enum _ol_ath_param_t)param,
                &value, &restart_vaps);
    }
    else if ( param & OL_SPECIAL_PARAM_SHIFT )
    {
        param -= OL_SPECIAL_PARAM_SHIFT;

        switch (param) {
            case OL_SPECIAL_PARAM_COUNTRY_ID:
                orig_cc = ieee80211_getCurrentCountry(ic);
                retval = wlan_set_countrycode(&scn->sc_ic, NULL, value, CLIST_NEW_COUNTRY);
                if (retval) {
                    qdf_print("%s: Unable to set country code ",__func__);
                    retval = wlan_set_countrycode(&scn->sc_ic, NULL,
                                    orig_cc, CLIST_NEW_COUNTRY);
                }
                break;
            case OL_SPECIAL_DBGLOG_REPORT_SIZE:
                fwdbg_set_report_size(dbglog_handle, scn, value);
                break;
            case OL_SPECIAL_DBGLOG_TSTAMP_RESOLUTION:
                fwdbg_set_timestamp_resolution(dbglog_handle, scn, value);
                break;
            case OL_SPECIAL_DBGLOG_REPORTING_ENABLED:
                fwdbg_reporting_enable(dbglog_handle, scn, value);
                break;
            case OL_SPECIAL_DBGLOG_LOG_LEVEL:
                fwdbg_set_log_lvl(dbglog_handle, scn, value);
                break;
            case OL_SPECIAL_DBGLOG_VAP_ENABLE:
                fwdbg_vap_log_enable(dbglog_handle, scn, value, TRUE);
                break;
            case OL_SPECIAL_DBGLOG_VAP_DISABLE:
                fwdbg_vap_log_enable(dbglog_handle, scn, value, FALSE);
                break;
            case OL_SPECIAL_DBGLOG_MODULE_ENABLE:
                fwdbg_module_log_enable(dbglog_handle, scn, value, TRUE);
                break;
            case OL_SPECIAL_DBGLOG_MODULE_DISABLE:
                fwdbg_module_log_enable(dbglog_handle, scn, value, FALSE);
                break;
            case OL_SPECIAL_PARAM_DISP_TPC:
                wmi_unified_pdev_get_tpc_config_cmd_send(pdev_wmi_handle, value);
                break;
            case OL_SPECIAL_PARAM_ENABLE_CH_144:
                pdev =  ic->ic_pdev_obj;
                if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_REGULATORY_SB_ID) !=
                        QDF_STATUS_SUCCESS) {
                    qdf_print("%s, %d unable to get reference", __func__, __LINE__);
                    return -EINVAL;
                }

                psoc = wlan_pdev_get_psoc(pdev);
                if (psoc == NULL) {
                    qdf_print("%s: psoc is NULL", __func__);
                    wlan_objmgr_pdev_release_ref(pdev, WLAN_REGULATORY_SB_ID);
                    return -EINVAL;
                }

                reg_rx_ops = wlan_lmac_if_get_reg_rx_ops(psoc);
                if (!(reg_rx_ops && reg_rx_ops->reg_set_chan_144)) {
                    qdf_print("%s : reg_rx_ops is NULL", __func__);
                    wlan_objmgr_pdev_release_ref(pdev, WLAN_REGULATORY_SB_ID);
                    return -EINVAL;
                }

                reg_rx_ops->reg_set_chan_144(pdev, value);
                wlan_objmgr_pdev_release_ref(pdev, WLAN_REGULATORY_SB_ID);

                if (!wmi_service_enabled(wmi_handle, wmi_service_regulatory_db)) {
                    ieee80211_reg_create_ieee_chan_list(&scn->sc_ic);
                }
                break;
            case OL_SPECIAL_PARAM_ENABLE_CH144_EPPR_OVRD:
                ol_regdmn_set_ch144_eppovrd(scn->ol_regdmn_handle, value);
                orig_cc = ieee80211_getCurrentCountry(ic);
                retval = wlan_set_countrycode(&scn->sc_ic, NULL, orig_cc, CLIST_NEW_COUNTRY);
                break;
            case OL_SPECIAL_PARAM_REGDOMAIN:
                {
                    struct ieee80211_ath_channel *chan = ieee80211_get_current_channel(&scn->sc_ic);
                    u_int16_t freq = ieee80211_chan2freq(&scn->sc_ic, chan);
                    u_int64_t flags = chan->ic_flags;
                    u_int8_t cfreq2 = chan->ic_vhtop_ch_freq_seg2;
                    u_int32_t mode = ieee80211_chan2mode(chan);
                    uint16_t orig_rd = ieee80211_get_regdomain(ic);
                    wlan_if_t tmpvap;

                    /*
                     * After this command, the channel list would change.
                     * must set ic_curchan properly.
                     */
                    retval = ol_ath_set_regdomain(&scn->sc_ic, value, true);
                    if (retval) {
                        qdf_print("%s: Unable to set regdomain, restore orig_rd = %d",__func__, orig_rd);
                        retval = ol_ath_set_regdomain(&scn->sc_ic, orig_rd, true);
                        if (retval) {
                            qdf_print("%s: Unable to set regdomain", __func__);
                            return -1;
                        }
                    }

                    chan = ieee80211_find_channel(&scn->sc_ic, freq, cfreq2, flags); /* cfreq2 arguement will be ignored for non VHT80+80 mode */
                    if (chan == NULL) {
                        printk("Current channel not supported in new RD. Configuring to a random channel\n");
                        chan = ieee80211_find_dot11_channel(&scn->sc_ic, 0, 0, mode);
                        if (chan == NULL) {
                            chan = ieee80211_find_dot11_channel(&scn->sc_ic, 0, 0, 0);
                            if(chan == NULL)
                                return -1;
                        }
                    }
                    scn->sc_ic.ic_curchan = chan;
                    if(chan) {
                            mode = ieee80211_chan2mode(chan);
                            TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
                                    tmpvap->iv_des_mode = mode;
                                    tmpvap->iv_des_hw_mode = mode;
                            }
                    }
                    /* After re-building the channel list, both curchan and
                     * prevchan may point to same base address. Hence force
                     * restart is required.
                     */
                    osif_restart_for_config(ic, &ieee80211_set_channel_for_cc_change, chan);
                }
                break;
            case OL_SPECIAL_PARAM_ENABLE_OL_STATS:
                if (scn->sc_ic.ic_ath_enable_ap_stats) {
                    retval = scn->sc_ic.ic_ath_enable_ap_stats(&scn->sc_ic, value);
#if defined(OL_ATH_SUPPORT_LED) && defined(OL_ATH_SUPPORT_LED_POLL)
                    if (scn->soc->led_blink_rate_table && value) {
                        OS_SET_TIMER(&scn->scn_led_poll_timer, LED_POLL_TIMER);
                    }
#endif
                }
                break;
            case OL_SPECIAL_PARAM_ENABLE_OL_STATSv2:
                scn->enable_statsv2 = value;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
                if (ic->nss_radio_ops) {
                    ic->nss_radio_ops->ic_nss_ol_enable_ol_statsv2(scn, value);
                }
#endif
                break;
            case OL_SPECIAL_PARAM_ENABLE_OL_STATSv3:
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
                if (ic->nss_radio_ops) {
                    ic->nss_radio_ops->ic_nss_ol_enable_v3_stats(scn, value);
                }
#endif
                break;
            case OL_SPECIAL_PARAM_ENABLE_MAC_REQ:
                /*
                 * The Mesh mode has a limitation in OL FW, where the VAP ID
                 * should be between 0-7. Since Static MAC request feature
                 * can send a VAP ID more than 7, we stop this by returning
                 * an error to the user
                 */
                if (scn->sc_ic.ic_mesh_vap_support) {
                    retval = -EINVAL;
                    break;
                }
                printk("%s: mac req feature %d \n", __func__, value);
                scn->macreq_enabled = value;
                break;
            case OL_SPECIAL_PARAM_WLAN_PROFILE_ID_ENABLE:
                {
                    struct wlan_profile_params param;
		    qdf_mem_set(&param, sizeof(param), 0);
                    param.profile_id = value;
                    param.enable = 1;
                    return wmi_unified_wlan_profile_enable_cmd_send(
                                                pdev_wmi_handle, &param);
                }
            case OL_SPECIAL_PARAM_WLAN_PROFILE_TRIGGER:
                {
                    struct wlan_profile_params param;
		    qdf_mem_set(&param, sizeof(param), 0);
                    param.enable = value;

                    return wmi_unified_wlan_profile_trigger_cmd_send(
                                               pdev_wmi_handle, &param);
                }

#if ATH_DATA_TX_INFO_EN
            case OL_SPECIAL_PARAM_ENABLE_PERPKT_TXSTATS:
                if (ic->ic_debug_sniffer) {
                    qdf_print("Please disable debug sniffer before enabling data_txstats");
                    retval = -EINVAL;
                    break;
                }
#ifdef QCA_SUPPORT_RDK_STATS
                if (scn->soc->wlanstats_enabled) {
                    qdf_err("Please disable peer rate stats before enabling data_txstats");
                    retval = -EINVAL;
                    break;
                }
#endif
                if(value == 1){
                    scn->enable_perpkt_txstats = 1;
                }else{
                    scn->enable_perpkt_txstats = 0;
                }
                cdp_txrx_set_pdev_param(soc_txrx_handle, (struct cdp_pdev *)pdev_txrx_handle,
                        CDP_CONFIG_ENABLE_PERPKT_TXSTATS, value);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
                if (ic->nss_radio_ops) {
                    ic->nss_radio_ops->ic_nss_ol_set_perpkt_txstats(scn);
                }
#endif
                break;
#endif
            case OL_SPECIAL_PARAM_ENABLE_SHPREAMBLE:
                if (value) {
                    IEEE80211_ENABLE_SHPREAMBLE(ic);
                } else {
                    IEEE80211_DISABLE_SHPREAMBLE(ic);
                }
                wlan_iterate_vap_list(ic, ol_ath_vap_iter_update_shpreamble, &value);
                restart_vaps = true;
                break;
            case OL_SPECIAL_PARAM_ENABLE_SHSLOT:
                if (value)
                    ieee80211_set_shortslottime(&scn->sc_ic, 1);
                else
                    ieee80211_set_shortslottime(&scn->sc_ic, 0);
                wlan_pdev_beacon_update(ic);
                break;
            case OL_SPECIAL_PARAM_RADIO_MGMT_RETRY_LIMIT:
                /* mgmt retry limit 1-15 */
                if( value < OL_MGMT_RETRY_LIMIT_MIN || value > OL_MGMT_RETRY_LIMIT_MAX ){
                    printk("mgmt retry limit invalid, should be in (1-15)\n");
                    retval = -EINVAL;
                }else{
                    retval = ic->ic_set_mgmt_retry_limit(&scn->sc_ic, value);
                }
                break;
            case OL_SPECIAL_PARAM_SENS_LEVEL:
                printk("%s[%d] PARAM_SENS_LEVEL \n", __func__,__LINE__);
                retval = ol_ath_pdev_set_param(scn,
                        wmi_pdev_param_sensitivity_level, value);
                break;
            case OL_SPECIAL_PARAM_TX_POWER_5G:
                printk("%s[%d] OL_SPECIAL_PARAM_TX_POWER_5G \n", __func__,__LINE__);
                retval = ol_ath_pdev_set_param(scn,
                        wmi_pdev_param_signed_txpower_5g, value);
                break;
            case OL_SPECIAL_PARAM_TX_POWER_2G:
                printk("%s[%d] OL_SPECIAL_PARAM_TX_POWER_2G \n", __func__,__LINE__);
                retval = ol_ath_pdev_set_param(scn,
                        wmi_pdev_param_signed_txpower_2g, value);
                break;
            case OL_SPECIAL_PARAM_CCA_THRESHOLD:
                printk("%s[%d] PARAM_CCA_THRESHOLD \n", __func__,__LINE__);
                retval = ol_ath_pdev_set_param(scn,
                        wmi_pdev_param_cca_threshold, value);
                break;
            case OL_SPECIAL_PARAM_BSTA_FIXED_IDMASK:
                scn->sc_bsta_fixed_idmask = value;
                break;
            default:
                retval = -EOPNOTSUPP;
                break;
        }
    }
    else
    {
        retval = (int) ol_hal_set_config_param(scn, (enum _ol_hal_param_t)param, &value);
    }

    if (restart_vaps == TRUE) {
        retval = osif_restart_vaps(&scn->sc_ic);
    }

    return retval;
}

int ol_ath_ucfg_getparam(void *vscn, int param, int *val)
{
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211*)vscn;
    struct ieee80211com *ic;
    int retval = 0;
    struct wlan_lmac_if_reg_rx_ops *reg_rx_ops;
    struct wlan_objmgr_pdev *pdev;
    struct wlan_objmgr_psoc *psoc;

    if (scn->soc->down_complete) {
        qdf_print("Starting the target before sending the command");
        if (ol_ath_target_start(scn->soc)) {
            qdf_print("failed to start the target");
            return -1;
        }
    }

    /*
     ** Code Begins
     ** Since the parameter passed is the value of the parameter ID, we can call directly
     */
    ic = &scn->sc_ic;

    if ( param & OL_ATH_PARAM_SHIFT )
    {
        /*
         ** It's an ATH value.  Call the  ATH configuration interface
         */

        param -= OL_ATH_PARAM_SHIFT;
        if (ol_ath_get_config_param(scn, (enum _ol_ath_param_t)param, (void *)val))
        {
            retval = -EOPNOTSUPP;
        }
    }
    else if ( param & OL_SPECIAL_PARAM_SHIFT )
    {
        if ( param == (OL_SPECIAL_PARAM_SHIFT | OL_SPECIAL_PARAM_COUNTRY_ID) ) {
            val[0] = ieee80211_getCurrentCountry(ic);
        } else if ( param == (OL_SPECIAL_PARAM_SHIFT | OL_SPECIAL_PARAM_ENABLE_CH_144) ) {
            pdev =  ic->ic_pdev_obj;
            if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_REGULATORY_SB_ID) !=
                    QDF_STATUS_SUCCESS) {
                qdf_print("%s, %d unable to get reference", __func__, __LINE__);
                return -EINVAL;
            }

            psoc = wlan_pdev_get_psoc(pdev);
            if (psoc == NULL) {
                qdf_print("%s: psoc is NULL", __func__);
                wlan_objmgr_pdev_release_ref(pdev, WLAN_REGULATORY_SB_ID);
                return -EINVAL;
            }

            reg_rx_ops = wlan_lmac_if_get_reg_rx_ops(psoc);
            if (!(reg_rx_ops && reg_rx_ops->reg_get_chan_144)) {
                qdf_print("%s : reg_rx_ops is NULL", __func__);
                wlan_objmgr_pdev_release_ref(pdev, WLAN_REGULATORY_SB_ID);
                return -EINVAL;
            }

            val[0] = reg_rx_ops->reg_get_chan_144(pdev);
            wlan_objmgr_pdev_release_ref(pdev, WLAN_REGULATORY_SB_ID);

        } else if ( param == (OL_SPECIAL_PARAM_SHIFT | OL_SPECIAL_PARAM_REGDOMAIN) ) {
            *val = ieee80211_get_regdomain(ic);
        } else if ( param == (OL_SPECIAL_PARAM_SHIFT | OL_SPECIAL_PARAM_ENABLE_SHPREAMBLE) ) {
            val[0] = (scn->sc_ic.ic_caps & IEEE80211_C_SHPREAMBLE) != 0;
        } else if ( param == (OL_SPECIAL_PARAM_SHIFT | OL_SPECIAL_PARAM_ENABLE_SHSLOT) ) {
            val[0] = IEEE80211_IS_SHSLOT_ENABLED(&scn->sc_ic) ? 1 : 0;
        } else if ( param == (OL_SPECIAL_PARAM_SHIFT | OL_SPECIAL_PARAM_RADIO_MGMT_RETRY_LIMIT) ) {
            val[0] = ic->ic_get_mgmt_retry_limit(ic);
        } else if ( param == (OL_SPECIAL_PARAM_SHIFT | OL_SPECIAL_PARAM_ENABLE_OL_STATS) ) {
#ifdef QCA_SUPPORT_CP_STATS
            *val = pdev_cp_stats_ap_stats_tx_cal_enable_get(ic->ic_pdev_obj);
#endif
        }
#if ATH_DATA_TX_INFO_EN
        else if ( param == (OL_SPECIAL_PARAM_SHIFT | OL_SPECIAL_PARAM_ENABLE_PERPKT_TXSTATS) ) {
            *val = scn->enable_perpkt_txstats;
        }
#endif
        else {
            retval = -EOPNOTSUPP;
        }
    }
    else
    {
        if ( ol_hal_get_config_param(scn, (enum _ol_hal_param_t)param, (void *)val))
        {
            retval = -EOPNOTSUPP;
        }
    }

    return retval;
}

/* Validate if AID exists */
static bool ol_ath_ucfg_validate_aid(wlan_if_t vaphandle, u_int32_t value)
{
    struct ieee80211_node *node = NULL;
    struct ieee80211_node_table *nt = &vaphandle->iv_ic->ic_sta;
    u_int16_t aid;

    TAILQ_FOREACH(node, &nt->nt_node, ni_list) {
        if (!ieee80211_try_ref_node(node))
            continue;

        aid = IEEE80211_AID(node->ni_associd);
        ieee80211_free_node(node);
        if (aid == value ) {
            return TRUE;
        }
    }
    return FALSE;
}

/*
 * Function to send WMI command to request for user position of a
 * peer in different MU-MIMO group
 */
int ol_ath_ucfg_get_user_postion(wlan_if_t vaphandle, u_int32_t value)
{
    int status = 0;
    struct ieee80211com *ic;
    struct ol_ath_softc_net80211 *scn;
    struct common_wmi_handle *pdev_wmi_handle;

    if (!vaphandle) {
        qdf_print("get_user_pos:vap not available");
        return A_ERROR;
    }

    ic = vaphandle->iv_ic;
    scn = OL_ATH_SOFTC_NET80211(ic);
    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    if (ol_ath_ucfg_validate_aid(vaphandle, value)){
    	status = wmi_send_get_user_position_cmd(pdev_wmi_handle, value);
    } else {
         qdf_print("Invalid AID value.");
    }

    return status;
}

/* Function to send WMI command to FW to reset MU-MIMO tx count for a peer */
int ol_ath_ucfg_reset_peer_mumimo_tx_count(wlan_if_t vaphandle, u_int32_t value)
{
    int status = 0;
    struct ieee80211com *ic;
    struct ol_ath_softc_net80211 *scn;
    struct common_wmi_handle *pdev_wmi_handle;

    if (!vaphandle) {
        qdf_print("reset_peer_mumimo_tx_count:vap not available");
        return A_ERROR;
    }

    ic = vaphandle->iv_ic;
    scn = OL_ATH_SOFTC_NET80211(ic);
    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    if (ol_ath_ucfg_validate_aid(vaphandle, value)){
    	status = wmi_send_reset_peer_mumimo_tx_count_cmd(pdev_wmi_handle, value);
    } else {
         qdf_print("Invalid AID value.");
    }

    return status;
}

/*
 * Function to send WMI command to FW to request for Mu-MIMO packets
 * transmitted for a peer
 */
int ol_ath_ucfg_get_peer_mumimo_tx_count(wlan_if_t vaphandle, u_int32_t value)
{
    int status = 0;
    struct ieee80211com *ic;
    struct ol_ath_softc_net80211 *scn;
    struct common_wmi_handle *pdev_wmi_handle;

    if (!vaphandle) {
        qdf_print("mumimo_tx_count:vap not available");
        return A_ERROR;
    }

    ic = vaphandle->iv_ic;
    scn = OL_ATH_SOFTC_NET80211(ic);
    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    if (ol_ath_ucfg_validate_aid(vaphandle, value)){
    	status = wmi_send_get_peer_mumimo_tx_count_cmd(pdev_wmi_handle, value);
    } else {
         qdf_print("Invalid AID value.");
    }
    return status;
}

int ol_ath_ucfg_set_country(void *vscn, char *cntry)
{
    int retval;
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211*)vscn;
    uint16_t orig_cc;

    if (ol_ath_target_start(scn->soc)) {
        qdf_print("failed to start the target");
        return -1;
    }

    if (&scn->sc_ic) {
        orig_cc = ieee80211_getCurrentCountry(&scn->sc_ic);
        retval=  wlan_set_countrycode(&scn->sc_ic, cntry, 0, CLIST_NEW_COUNTRY);
        if (retval) {
            qdf_print("%s: Unable to set country code", __func__);
            retval = wlan_set_countrycode(&scn->sc_ic, NULL, orig_cc, CLIST_NEW_COUNTRY);
        }
    } else {
        retval = -EOPNOTSUPP;
    }

    return retval;
}

int ol_ath_ucfg_get_country(void *vscn, char *str)
{
    int retval = 0;
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211*)vscn;
    struct ieee80211com *ic = &scn->sc_ic;

    ieee80211_getCurrentCountryISO(ic, str);

    return retval;
}

int ol_ath_ucfg_set_mac_address(void *vscn, char *addr)
{
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211*)vscn;
    struct net_device *dev = scn->netdev;
    struct ieee80211com *ic = &scn->sc_ic;
    struct sockaddr sa;
    int retval;

    if (!IEEE80211_ADDR_IS_VALID(addr)) {
        qdf_print("%s : Configured invalid mac address", __func__);
        return -1;
    }

    if (ol_ath_target_start(scn->soc)) {
        qdf_print("failed to start the target");
        return -1;
    }

    if ( !TAILQ_EMPTY(&ic->ic_vaps) ) {
        retval = -EBUSY; //We do not set the MAC address if there are VAPs present
    } else {
        IEEE80211_ADDR_COPY(&sa.sa_data, addr);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
        retval = dev->netdev_ops->ndo_set_mac_address(dev, &sa);
#else
        retval = dev->set_mac_address(dev, &sa);
#endif
    }

    return retval;
}

int ol_ath_ucfg_gpio_config(void *vscn,int gpionum,int input,int pulltype, int intrmode)
{
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)vscn;
    if (ol_ath_target_start(scn->soc)) {
        qdf_print("failed to start the target");
        return -1;
    }

    return ol_gpio_config(scn, gpionum, input, pulltype, intrmode);
}

int ol_ath_ucfg_gpio_output(void *vscn, int gpionum, int set)
{
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)vscn;
    if (ol_ath_target_start(scn->soc)) {
        qdf_print("failed to start the target");
        return -1;
    }

    return ol_ath_gpio_output(scn, gpionum, set);
}
#define BTCOEX_MAX_PERIOD   2000   /* 2000 msec */
int ol_ath_ucfg_btcoex_duty_cycle(void *vscn, u_int32_t bt_period, u_int32_t bt_duration)

{
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)vscn;
    int period,duration;
    uint8_t btcoex_support;
    if (ol_ath_target_start(scn->soc)) {
        qdf_print("failed to start the target");
        return -1;
    }
    btcoex_support = wlan_psoc_nif_feat_cap_get(scn->soc->psoc_obj,
					WLAN_SOC_F_BTCOEX_SUPPORT);
    period = (int)bt_period;
    duration = (int)bt_duration;
    if (btcoex_support && scn->soc->btcoex_duty_cycle) {
        if ( period  > BTCOEX_MAX_PERIOD ) {
            qdf_print("Invalid period : %d ",period);
            qdf_print("Allowed max period is 2000ms ");
            return -EINVAL;
        }
        if ( period < 0 || duration < 0) {
            qdf_print("Invalid values. Both period and must be +ve values ");
            return -EINVAL;
        }
        if( period < duration ) { /* period must be >= duration */
            qdf_print("Invalid values. period must be >= duration. period:%d duration:%d ",
                    period, duration);
            return -EINVAL;
        }

        if (period == 0 && duration == 0) {
            qdf_print("Both period and duration set to 0. Disabling this feature. ");
        }
        if (ol_ath_btcoex_duty_cycle(scn->soc, bt_period, bt_duration) == EOK ) {
            scn->soc->btcoex_duration = duration;
            scn->soc->btcoex_period = period;
        } else {
            qdf_print("BTCOEX Duty Cycle configuration is not success. ");
        }
    } else {
        qdf_print("btcoex_duty_cycle service not started. btcoex_support:%d btcoex_duty_cycle:%d ",
                btcoex_support, scn->soc->btcoex_duty_cycle);
        return -EPERM;
    }
    return 0;
}

#if UNIFIED_SMARTANTENNA
int ol_ath_ucfg_set_smart_antenna_param(void *vscn, char *val)
{
    struct ol_ath_softc_net80211 *scn = ( struct ol_ath_softc_net80211*) vscn;
    struct wlan_objmgr_pdev *pdev;
    QDF_STATUS status;
    int ret = -1;

    if (ol_ath_target_start(scn->soc)) {
        qdf_err("failed to start the target");
        return -1;
    }
    pdev = scn->sc_pdev;
    status = wlan_objmgr_pdev_try_get_ref(pdev, WLAN_SA_API_ID);
    if (QDF_IS_STATUS_ERROR(status)) {
        qdf_print("%s, %d unable to get pdev reference", __func__, __LINE__);
        return ret;
    }

    ret = target_if_sa_api_ucfg_set_param(pdev, val);
    wlan_objmgr_pdev_release_ref(pdev, WLAN_SA_API_ID);
    return ret;
}

int ol_ath_ucfg_get_smart_antenna_param(void *vscn, char *val)
{
    struct ol_ath_softc_net80211 *scn = ( struct ol_ath_softc_net80211*) vscn;
    struct wlan_objmgr_pdev *pdev;
    QDF_STATUS status;
    int ret = -1;

    pdev = scn->sc_pdev;
    status = wlan_objmgr_pdev_try_get_ref(pdev, WLAN_SA_API_ID);
    if (QDF_IS_STATUS_ERROR(status)) {
        qdf_print("%s, %d unable to get pdev reference", __func__, __LINE__);
        return ret;
    }

    ret = target_if_sa_api_ucfg_get_param(pdev, val);
    wlan_objmgr_pdev_release_ref(pdev, WLAN_SA_API_ID);
    return ret;
}
#endif

#if PEER_FLOW_CONTROL
void ol_ath_ucfg_txrx_peer_stats(void *vscn, char *addr)
{
    struct ol_ath_softc_net80211 *scn = ( struct ol_ath_softc_net80211*) vscn;
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_pdev_handle pdev_txrx_handle;

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    pdev_txrx_handle = wlan_pdev_get_dp_handle(scn->sc_pdev);

    cdp_per_peer_stats(soc_txrx_handle, (void *)pdev_txrx_handle, addr);
}
#endif

void ol_ath_get_dp_fw_peer_stats(void *vscn, char *addr, uint8_t caps)
{
    struct ol_ath_softc_net80211 *scn = ( struct ol_ath_softc_net80211*) vscn;
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_pdev_handle pdev_txrx_handle;

    if (ol_ath_target_start(scn->soc)) {
        qdf_err("failed to start the target");
        return;
    }

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    pdev_txrx_handle = wlan_pdev_get_dp_handle(scn->sc_pdev);

    cdp_get_dp_fw_peer_stats(soc_txrx_handle, (void *)pdev_txrx_handle, addr, caps, 0);
}

void ol_ath_set_ba_timeout(void *vscn, uint8_t ac, uint32_t value)
{
    struct ol_ath_softc_net80211 *scn = ( struct ol_ath_softc_net80211*) vscn;
    ol_txrx_soc_handle soc_txrx_handle;

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    cdp_set_ba_timeout(soc_txrx_handle, ac, value);
}

void ol_ath_get_ba_timeout(void *vscn, uint8_t ac, uint32_t *value)
{
    struct ol_ath_softc_net80211 *scn = ( struct ol_ath_softc_net80211*) vscn;
    ol_txrx_soc_handle soc_txrx_handle;

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    cdp_get_ba_timeout(soc_txrx_handle, ac, value);
}

void ol_ath_get_dp_htt_stats(void *vscn, void* data, uint32_t data_len)
{
    struct ol_ath_softc_net80211 *scn = ( struct ol_ath_softc_net80211*) vscn;
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_pdev_handle pdev_txrx_handle;

    if (ol_ath_target_start(scn->soc)) {
        qdf_err("failed to start the target");
        return;
    }

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    pdev_txrx_handle = wlan_pdev_get_dp_handle(scn->sc_pdev);

    if (!soc_txrx_handle || !pdev_txrx_handle)
        return;

    cdp_get_dp_htt_stats(soc_txrx_handle, (void *)pdev_txrx_handle,
                        data, data_len);
}

int ol_ath_ucfg_create_vap(struct ol_ath_softc_net80211 *scn, struct ieee80211_clone_params *cp, char *dev_name)
{
    struct net_device *dev = scn->netdev;
    struct ifreq ifr;
    int status;

    if (scn->soc->sc_in_delete) {
        return -ENODEV;
    }
    if (ol_ath_target_start(scn->soc)) {
        qdf_print("failed to start the target");
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_data = (void *) cp;

    /*
     * If the driver is compiled with FAST_PATH option
     * and the driver mode is offload, we override the
     * fast path entry point for Tx
     */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30)
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if(scn->nss_radio.nss_rctx) {
        scn->sc_osdev->vap_hardstart = osif_nss_ol_vap_hardstart;
    } else
#endif
    {
        if (ol_target_lithium(scn->soc->psoc_obj)) {
            scn->sc_osdev->vap_hardstart = osif_ol_vap_hardstart_wifi3;
        } else {
            scn->sc_osdev->vap_hardstart = osif_ol_ll_vap_hardstart;
        }
    }
#endif
    status = osif_ioctl_create_vap(dev, &ifr, cp, scn->sc_osdev);

    /* return final device name */
    strlcpy(dev_name, ifr.ifr_name, IFNAMSIZ);

    return status;
}

/*
 * Function to handle UTF commands from QCMBR and FTM daemon
 */
int ol_ath_ucfg_utf_unified_cmd(void *data, int cmd, char *userdata, unsigned int length)
{
    int error = -1;
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)data;
    struct ieee80211com *ic = NULL;
    struct wlan_objmgr_pdev *pdev;

    if (ol_ath_target_start(scn->soc)) {
        qdf_print("failed to start the target");
        return -1;
    }

    ic = &scn->sc_ic;
    pdev = ic->ic_pdev_obj;

    switch (cmd)
    {
#ifdef QCA_WIFI_FTM
#ifdef QCA_WIFI_FTM_IOCTL
        case ATH_XIOCTL_UNIFIED_UTF_CMD: /* UTF command from QCMBR */
        case ATH_XIOCTL_UNIFIED_UTF_RSP: /* UTF command to send response to QCMBR */
            {
                msleep(50);
                error = wlan_ioctl_ftm_testmode_cmd(pdev, cmd, (u_int8_t*)userdata, length);
                msleep(50);
            }
            break;
#endif
#ifdef QCA_WIFI_FTM_NL80211
        case ATH_FTM_UTF_CMD: /* UTF command from FTM daemon */
            {
                if (length > MAX_UTF_LENGTH) {
                    QDF_TRACE(QDF_MODULE_ID_CONFIG, QDF_TRACE_LEVEL_ERROR, "length: %d, max: %d \n",
                            length, MAX_UTF_LENGTH);
                    return -EFAULT;
                }

                error = wlan_cfg80211_ftm_testmode_cmd(pdev, (u_int8_t*)userdata, length);
            }
            break;
#endif
#endif
        default:
            qdf_print("FTM not supported\n");
    }

    return error;
}

int ol_ath_ucfg_get_ath_stats(void *vscn, void *vasc)
{
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)vscn;
    struct ath_stats_container *asc = (struct ath_stats_container *)vasc;
    struct net_device *dev = scn->netdev;
    struct ol_stats *stats;
    uint32_t size = MAX(sizeof(struct ol_stats), sizeof(struct cdp_pdev_stats));
    int error=0;
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_pdev_handle pdev_txrx_handle;
    ol_txrx_vdev_handle iv_txrx_handle;

    if(((dev->flags & IFF_UP) == 0)){
        return -ENXIO;
    }

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    pdev_txrx_handle = wlan_pdev_get_dp_handle(scn->sc_pdev);

    if (!soc_txrx_handle || !pdev_txrx_handle) {
        qdf_err("psoc dp handle %pK or pdev dp handle %pK is NULL",
                soc_txrx_handle, pdev_txrx_handle);
        return -EFAULT;
    }

    stats = OS_MALLOC(&scn->sc_osdev, size, GFP_KERNEL);
    if (stats == NULL)
        return -ENOMEM;

    if(asc->size == 0 || asc->address == NULL || asc->size < size) {
        error = -EFAULT;
    }else {
        if (ol_ath_target_start(scn->soc)) {
            qdf_err("failed to start the target");
            OS_FREE(stats);
            return -1;
        }
        if(ol_target_lithium(scn->soc->psoc_obj)) { /* lithium */

            stats->txrx_stats_level =
                cdp_stats_publish(soc_txrx_handle, (struct cdp_pdev *)pdev_txrx_handle,
                        (void *)stats);
            asc->offload_if = FLAG_LITHIUM;
            asc->size = sizeof(struct cdp_pdev_stats);
        }
        else {
            stats->txrx_stats_level =
                cdp_stats_publish(soc_txrx_handle, (struct cdp_pdev *)pdev_txrx_handle,
                    (void *)stats);
	        /*
             * TODO: This part of the code is specific to Legacy and should
             * should be moved to Legacy DP (ol_txrx) layer
	         */
            ol_get_radio_stats(scn,&stats->interface_stats);
            asc->offload_if = FLAG_PARTIAL_OL;
            asc->size = sizeof(struct ol_stats);

            if(asc->flag_ext & EXT_TXRX_FW_STATS) {  /* fw stats */
                struct ieee80211vap *vap = NULL;
                struct ol_txrx_stats_req req = {0};

                vap = ol_ath_vap_get(scn, 0);
                if (vap == NULL) {
                    printk("%s, vap not found!\n", __func__);
                    return -ENXIO;
                }

                req.wait.blocking = 1;
                req.stats_type_upload_mask = 1 << (TXRX_FW_STATS_TID_STATE - 2);
                req.copy.byte_limit = sizeof(struct wlan_dbg_tidq_stats);
                req.copy.buf = OS_MALLOC(&scn->sc_osdev,
                            sizeof(struct wlan_dbg_tidq_stats), GFP_KERNEL);
                if(req.copy.buf == NULL) {
                    printk("%s, no memory available!\n", __func__);
                    ol_ath_release_vap(vap);
                    return -ENOMEM;
                }

                iv_txrx_handle = wlan_vdev_get_dp_handle(vap->vdev_obj);
                if (cdp_fw_stats_get(soc_txrx_handle, (struct cdp_vdev *)iv_txrx_handle, &req, PER_RADIO_FW_STATS_REQUEST, 0) != 0) {
                    OS_FREE(req.copy.buf);
                    ol_ath_release_vap(vap);
                    return -EIO;
                }

                OS_MEMCPY(&stats->tidq_stats, req.copy.buf,
                        sizeof(struct wlan_dbg_tidq_stats));
                OS_FREE(req.copy.buf);
                ol_ath_release_vap(vap);
            } /* fw stats */
        } /* lithium */

        if (_copy_to_user(asc->address, stats, size))
            error = -EFAULT;
        else
            error = 0;
    }

    OS_FREE(stats);

    return error;
}

int ol_ath_ucfg_get_vap_info(struct ol_ath_softc_net80211 *scn,
                                struct ieee80211_profile *profile)
{
    struct net_device *dev = scn->netdev;
    struct ieee80211com *ic = &scn->sc_ic;
    struct ieee80211vap *vap = NULL;
    struct ieee80211vap_profile *vap_profile;
    wlan_chan_t chan;

    strlcpy(profile->radio_name, dev->name, IFNAMSIZ);
    wlan_get_device_mac_addr(ic, profile->radio_mac);
    profile->cc = (u_int16_t)wlan_get_device_param(ic,
                                IEEE80211_DEVICE_COUNTRYCODE);
    chan = wlan_get_dev_current_channel(ic);
    if (chan != NULL) {
        profile->channel = chan->ic_ieee;
        profile->freq = chan->ic_freq;
    }
    TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
        vap_profile = &profile->vap_profile[profile->num_vaps];
        wlan_get_vap_info(vap, vap_profile, (void *)scn->sc_osdev);
        profile->num_vaps++;
    }
    return 0;
}

int ol_ath_ucfg_get_nf_dbr_dbm_info(struct ol_ath_softc_net80211 *scn)
{
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    if(wmi_unified_nf_dbr_dbm_info_get_cmd_send(pdev_wmi_handle,
                                lmac_get_pdev_idx(scn->sc_pdev))) {
        printk("%s:Unable to send request to get NF dbr dbm info\n", __func__);
        return -1;
    }
    return 0;
}

int ol_ath_ucfg_get_packet_power_info(struct ol_ath_softc_net80211 *scn,
        struct packet_power_info_params *param)
{
    if(ol_ath_packet_power_info_get(scn, param)) {
        printk("%s:Unable to send request to get packet power info\n", __func__);
        return -1;
    }
    return 0;
}

#if defined(ATH_SUPPORT_DFS) || defined(WLAN_SPECTRAL_ENABLE)
int ol_ath_ucfg_phyerr(void *vscn, void *vad)
{
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)vscn;
    struct ath_diag *ad = (struct ath_diag *)vad;
    struct ieee80211com *ic = &scn->sc_ic;
    void *indata=NULL;
    void *outdata=NULL;
    int error = -EINVAL;
    u_int32_t insize = ad->ad_in_size;
    u_int32_t outsize = ad->ad_out_size;
    u_int id= ad->ad_id & ATH_DIAG_ID;
#if ATH_SUPPORT_DFS
    struct wlan_objmgr_pdev *pdev;
    struct wlan_objmgr_psoc *psoc;
    struct wlan_lmac_if_dfs_rx_ops *dfs_rx_ops;

    pdev = ic->ic_pdev_obj;
    if(pdev == NULL) {
        qdf_print("%s : pdev is NULL", __func__);
        return -EINVAL;
    }

    psoc = wlan_pdev_get_psoc(pdev);

    if (psoc == NULL) {
        qdf_print("%s : psoc is NULL", __func__);
        return -EINVAL;
    }

    dfs_rx_ops = wlan_lmac_if_get_dfs_rx_ops(psoc);
#endif

    if (ad->ad_id & ATH_DIAG_IN) {
        /*
         * Copy in data.
         */
        indata = OS_MALLOC(scn->sc_osdev,insize, GFP_KERNEL);
        if (indata == NULL) {
            error = -ENOMEM;
            goto bad;
        }
        if (__xcopy_from_user(indata, ad->ad_in_data, insize)) {
            error = -EFAULT;
            goto bad;
        }
        id = id & ~ATH_DIAG_IN;
    }
    if (ad->ad_id & ATH_DIAG_DYN) {
        /*
         * Allocate a buffer for the results (otherwise the HAL
         * returns a pointer to a buffer where we can read the
         * results).  Note that we depend on the HAL leaving this
         * pointer for us to use below in reclaiming the buffer;
         * may want to be more defensive.
         */
        outdata = OS_MALLOC(scn->sc_osdev, outsize, GFP_KERNEL);
        if (outdata == NULL) {
            error = -ENOMEM;
            goto bad;
        }
        id = id & ~ATH_DIAG_DYN;
    }

#if ATH_SUPPORT_DFS
    if (dfs_rx_ops && dfs_rx_ops->dfs_control) {
        if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
                QDF_STATUS_SUCCESS) {
            return -EINVAL;
        }

        dfs_rx_ops->dfs_control(pdev, id, indata, insize, outdata, &outsize, &error);

        wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
    }
#endif

#if WLAN_SPECTRAL_ENABLE
    if (error ==  -EINVAL ) {
        error = ucfg_spectral_control(ic->ic_pdev_obj, id, indata, insize, outdata, &outsize);
    }
#endif

    if (outsize < ad->ad_out_size)
        ad->ad_out_size = outsize;

    if (outdata &&
            _copy_to_user(ad->ad_out_data, outdata, ad->ad_out_size))
        error = -EFAULT;
bad:
    if ((ad->ad_id & ATH_DIAG_IN) && indata != NULL)
        OS_FREE(indata);
    if ((ad->ad_id & ATH_DIAG_DYN) && outdata != NULL)
        OS_FREE(outdata);

    return error;
}
#endif

int ol_ath_ucfg_ctl_set(struct ol_ath_softc_net80211 *scn, ath_ctl_table_t *ptr)
{
	struct ctl_table_params param;
	struct ieee80211com *ic = &scn->sc_ic;
	struct common_wmi_handle *pdev_wmi_handle;

	pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
	if (!ptr)
		return -1;

	qdf_mem_set(&param, sizeof(param), 0);
	qdf_print("%s[%d] Mode %d CTL table length %d", __func__,__LINE__,
			ptr->band, ptr->len);

	param.ctl_band = ptr->band;
	param.ctl_array = &ptr->ctl_tbl[0];
	param.ctl_cmd_len = ptr->len + sizeof(uint32_t);
	param.target_type = lmac_get_tgt_type(scn->soc->psoc_obj);
	if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan)) {
		param.is_2g = TRUE;
	} else {
		param.is_2g = FALSE;
	}

	if(QDF_STATUS_E_FAILURE ==
		wmi_unified_set_ctl_table_cmd_send(pdev_wmi_handle, &param)) {
		qdf_print("%s:Unable to set CTL table", __func__);
		return -1;
	}

	return 0;
}

/*
 * @brief set basic & supported rates in beacon,
 * and use lowest basic rate as mgmt mgmt/bcast/mcast rates by default.
 * target_rates: an array of supported rates with bit7 set for basic rates.
 */
static int
ol_ath_ucfg_set_vap_op_support_rates(wlan_if_t vap, struct ieee80211_rateset *target_rs)
{
    struct ieee80211_node *bss_ni = vap->iv_bss;
    enum ieee80211_phymode mode = wlan_get_desired_phymode(vap);
    struct ieee80211_rateset *bss_rs = &(bss_ni->ni_rates);
    struct ieee80211_rateset *op_rs = &(vap->iv_op_rates[mode]);
    struct ieee80211_rateset *ic_supported_rs = &(vap->iv_ic->ic_sup_rates[mode]);
    uint8_t num_of_rates, num_of_basic_rates, i, j, rate_found=0;
    uint8_t basic_rates[IEEE80211_RATE_MAXSIZE];
    int32_t retv=0, min_rate=0;

    if (vap->iv_disabled_legacy_rate_set) {
        qdf_print("%s: need to unset iv_disabled_legacy_rate_set!",__FUNCTION__);
        return -EINVAL;
    }

    num_of_rates = target_rs->rs_nrates;
    if(num_of_rates > ACFG_MAX_RATE_SIZE){
        num_of_rates = ACFG_MAX_RATE_SIZE;
    }

    /* Check if the new rates are supported by the IC */
    for (i=0; i < num_of_rates; i++) {
        rate_found = 0;
        for (j=0; j < (ic_supported_rs->rs_nrates); j++) {
            if((target_rs->rs_rates[i] & IEEE80211_RATE_VAL) ==
                    (ic_supported_rs->rs_rates[j] & IEEE80211_RATE_VAL)){
                rate_found  = 1;
                break;
            }
        }
        if(!rate_found){
            printk("Error: rate %d not supported in phymode %s !\n",
                    (target_rs->rs_rates[i]&IEEE80211_RATE_VAL)/2,ieee80211_phymode_name[mode]);
            return EINVAL;
        }
    }

    /* Update BSS rates and VAP supported rates with the new rates */
    for (i=0; i < num_of_rates; i++) {
        bss_rs->rs_rates[i] = target_rs->rs_rates[i];
        op_rs->rs_rates[i] = target_rs->rs_rates[i];
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG, "rate %d (%d kbps) added\n",
                target_rs->rs_rates[i], (target_rs->rs_rates[i]&IEEE80211_RATE_VAL)*1000/2);
    }
    bss_rs->rs_nrates = num_of_rates;
    op_rs->rs_nrates = num_of_rates;

    if (IEEE80211_VAP_IS_PUREG_ENABLED(vap)) {
        /*For pureg mode, all 11g rates are marked as Basic*/
        ieee80211_setpuregbasicrates(op_rs);
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG,
            "%s Mark flag so that new rates will be used in next beacon update\n",__func__);
    vap->iv_flags_ext2 |= IEEE80211_FEXT2_BR_UPDATE;

    /* Find all basic rates */
    num_of_basic_rates = 0;
    for (i=0; i < bss_rs->rs_nrates; i++) {
        if(bss_rs->rs_rates[i] & IEEE80211_RATE_BASIC){
            basic_rates[num_of_basic_rates] = bss_rs->rs_rates[i];
            num_of_basic_rates++;
        }
    }
    if(!num_of_basic_rates){
        printk("%s: Error, no basic rates set. \n",__FUNCTION__);
        return EINVAL;
    }

    /* Find lowest basic rate */
    min_rate = basic_rates[0];
    for (i=0; i < num_of_basic_rates; i++) {
        if ( min_rate > basic_rates[i] ) {
            min_rate = basic_rates[i];
        }
    }

    /*
     * wlan_set_param supports actual rate in unit of kbps
     * min: 1000 kbps max: 300000 kbps
     */
    min_rate = ((min_rate&IEEE80211_RATE_VAL)*1000)/2;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG,
            "%s Set default mgmt/bcast/mcast rates to %d Kbps\n",__func__,min_rate);

    /* Use lowest basic rate as mgmt mgmt/bcast/mcast rates by default */
    retv = wlan_set_param(vap, IEEE80211_MGMT_RATE, min_rate);
    if(retv){
        return retv;
    }

    retv = wlan_set_param(vap, IEEE80211_BCAST_RATE, min_rate);
    if(retv){
        return retv;
    }

    retv = wlan_set_param(vap, IEEE80211_MCAST_RATE, min_rate);
    if(retv){
        return retv;
    }

    retv = wlan_set_param(vap, IEEE80211_BEACON_RATE_FOR_VAP, min_rate);
    if (retv) {
        return retv;
    }

    /* Use the lowest basic rate as RTS and CTS rates by default */
    retv = wlan_set_param(vap, IEEE80211_RTSCTS_RATE, min_rate);

    return retv;
}

int ol_ath_ucfg_set_op_support_rates(struct ol_ath_softc_net80211 *scn, struct ieee80211_rateset *target_rs)
{
    struct ieee80211com *ic = &scn->sc_ic;
    wlan_if_t tmpvap=NULL;
    int32_t retv = -EINVAL;

    TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
        if(!tmpvap){
            return retv;
        }
        retv = ol_ath_ucfg_set_vap_op_support_rates(tmpvap, target_rs);
        if(retv){
            printk("%s: Set VAP basic rates failed, retv=%d\n",__FUNCTION__,retv);
            return retv;
        }
    }

    return 0;
}

int ol_ath_ucfg_get_radio_supported_rates(struct ol_ath_softc_net80211 *scn,
        enum ieee80211_phymode mode,
        struct ieee80211_rateset *target_rs)
{
    struct ieee80211com *ic = &scn->sc_ic;
    struct ieee80211_rateset *rs = NULL;
    uint8_t j=0;

    if (!IEEE80211_SUPPORT_PHY_MODE(ic, mode)) {
        printk("%s: The radio doesn't support this phymode: %d\n",__FUNCTION__,mode);
        return -EINVAL;
    }

    rs = &ic->ic_sup_rates[mode];
    if(!rs){
        return -EINVAL;
    }

    for (j=0; j<(rs->rs_nrates); j++) {
        target_rs->rs_rates[j] = rs->rs_rates[j];
    }
    target_rs->rs_nrates = rs->rs_nrates;

    return 0;
}

int ol_ath_ucfg_set_atf_sched_dur(void *vscn, uint32_t ac, uint32_t duration)
{
    int retval = 0;
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)vscn;

    if (ol_ath_target_start(scn->soc)) {
        qdf_err("failed to start the target");
        return -1;
    }

    if (ac < WME_AC_BE || ac > WME_AC_VO)
    {
        qdf_print("Input AC value range out between 0 and 3!! ");
        return -1;
    }

    if((duration < 0) || (duration > (1 << (30 - 1))) )
    {
        qdf_print("Input sched duration value range out of between 0 and  2^30-1!! ");
        return -1;
    }

    retval = ol_ath_pdev_set_param(scn, wmi_pdev_param_atf_sched_duration,
            (ac&0x03) << 30 | (0x3fffffff & duration));

    return retval;
}

int ol_ath_ucfg_set_aggr_burst(void *vscn, uint32_t ac, uint32_t duration)
{
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)vscn;
    struct ieee80211com *ic =  &scn->sc_ic;
    int retval = 0;

    if (!ic->ic_aggr_burst_support) {
        return -1;
    }

    if (ol_ath_target_start(scn->soc)) {
        qdf_err("failed to start the target");
        return -1;
    }

    if (ac < WME_AC_BE || ac > WME_AC_VO) {
        return -1;
    }

    retval = ol_ath_pdev_set_param(scn, wmi_pdev_param_aggr_burst,
                    (ac&0x0f) << 24 | (0x00ffffff & duration));

    if( EOK == retval) {
        scn->aggr_burst_dur[ac] = duration;
    }

    return retval;
}

int ol_ath_ucfg_set_pcp_tid_map(void *vscn, uint32_t pcp, uint32_t tid)
{
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)vscn;
    struct ieee80211com *ic =  &scn->sc_ic;
    int retval = 0;

    if ((pcp < 0 || pcp > 7) || (tid < 0 || tid > 7)) {
        qdf_err("Invalid input");
        return -EINVAL;
    }

    retval = ol_ath_set_default_pcp_tid_map(ic, pcp, tid);
    if (EOK == retval)
        ic->ic_pcp_tid_map[pcp] = tid;

    return retval;
}

int ol_ath_ucfg_get_pcp_tid_map(void *vscn, uint32_t pcp, uint32_t *value)
{
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)vscn;
    struct ieee80211com *ic =  &scn->sc_ic;
    int retval = 0;

    if (pcp < 0 || pcp > 7) {
        qdf_err("Invalid input");
        return -EINVAL;
    }

    *value = ic->ic_pcp_tid_map[pcp];

    return retval;
}

int ol_ath_ucfg_set_tidmap_prty(void *vscn, uint32_t value)
{
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)vscn;
    struct ieee80211com *ic =  &scn->sc_ic;
    int retval = 0;

    /*
     * The priority value needs to be set for allowing the Vlan-pcp
     * to be used for deciding the TID number. The DSCP-based TID
     * mapping is the default value and doesnt need to be configured
     * explicitly.
     */
    if (value < OL_TIDMAP_PRTY_SVLAN || value > OL_TIDMAP_PRTY_CVLAN) {
        qdf_err("Invalid input");
        return -EINVAL;
    }

    retval = ol_ath_set_default_tidmap_prty(ic, value);
    if (EOK == retval) {
        ic->ic_tidmap_prty = (uint8_t)value;
    }

    return retval;
}

int ol_ath_ucfg_get_tidmap_prty(void *vscn, uint32_t *value)
{
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)vscn;
    struct ieee80211com *ic =  &scn->sc_ic;
    int retval = 0;

    *value = (int)ic->ic_tidmap_prty;

    return retval;
}

int
ol_ath_ucfg_set_he_bss_color(void *vscn,
                        uint8_t value, uint8_t ovrride)
{
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)vscn;
    struct ieee80211com *ic = &scn->sc_ic;
    struct ieee80211_bsscolor_handle *bsscolor_hdl = &ic->ic_bsscolor_hdl;

    if(ovrride > 1) {
        qdf_err("Override argument should be either 0 or 1");
        return -EINVAL;
    }

    if (!ieee80211_is_bcca_ongoing_for_any_vap(ic)) {
        /* Set the user set BSS color and the override
         * flag values, update the heop params fields.
         */
        if((value >= IEEE80211_HE_BSS_COLOR_MIN) &&
                (value <= IEEE80211_HE_BSS_COLOR_MAX)) {
            if(ic->ic_he_bsscolor != value) {
                ic->ic_he_bsscolor          = value;
                ic->ic_he_bsscolor_override = ovrride;
                if(value) {
                    /* By design, color selection algorithm exits
                     * with failure if it is already in user-disabled
                     * state. Change state to disabled if it is
                     * in user-disabled state so that select-API
                     * call executes and user can set a new color
                     * even if it disabled it in previous step.
                     */
                    if (bsscolor_hdl->state ==
                            IEEE80211_BSS_COLOR_CTX_STATE_COLOR_USER_DISABLED) {
                        bsscolor_hdl->state =
                            IEEE80211_BSS_COLOR_CTX_STATE_COLOR_DISABLED;
                    }
                    ieee80211_select_new_bsscolor(ic);
                } else {
                    bsscolor_hdl->state =
                            IEEE80211_BSS_COLOR_CTX_STATE_COLOR_USER_DISABLED;
                    ieee80211_set_ic_heop_bsscolor_disabled_bit(ic, true);
                }
            } else {
                QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_ERROR,
                    "HE BSS Color already set with this value=%d", value);
                return EOK;
            }
        } else {
                QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_ERROR,
                    "HE BSS color should be less then 63");
            return -EINVAL;
        }
    } else {
        QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_ERROR,
            "User update for BSS Color is prohibited"
             "during BSS Color Change Announcement");
    }

    return 0;
}

int
ol_ath_ucfg_get_he_bss_color(void *vscn, int *value)
{
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)vscn;
    struct ieee80211com *ic           = &scn->sc_ic;
    bool staonlymode                  = true;
    struct ieee80211vap *vap;

    TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
        if ((vap->iv_opmode == IEEE80211_M_HOSTAP)) {
            staonlymode = false;
            break;
        }
    }

    if (staonlymode) {
        /* cfg80211tool wifix get_he_bsscolor reflects the bss
         * color from an ic level variable. The bsscolor design
         * is such that for a particular radio all the AP VAPs
         * will have same bss color but a STA vap will assume
         * the bsscolor as advertised by the BSS it is associated to.
         */
        qdf_err("This cmd is not applicable in STA only mode");
        return -EINVAL;
    }

    value[0] = ic->ic_he_bsscolor;
    value[1] = ic->ic_he_bsscolor_override;

    return 0;
}

#ifdef WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG
int ol_ath_ucfg_set_rx_pkt_protocol_tagging(void *vscn,
                          struct ieee80211_rx_pkt_protocol_tag *rx_pkt_protocol_tag_info)
{
    struct wmi_rx_pkt_protocol_routing_info param;
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)vscn;
    struct ieee80211com     *ic = &scn->sc_ic;
    ol_txrx_soc_handle soc_handle;
    ol_txrx_pdev_handle pdev_handle;

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    uint32_t nss_soc_cfg = cfg_get(scn->soc->psoc_obj, CFG_NSS_WIFI_OL);

    if (nss_soc_cfg)
    {
      qdf_info("RX Protocol Tag not supported when NSS offload is enabled");
      return 0;
    }
#endif /* QCA_NSS_WIFI_OFFLOAD_SUPPORT */

    soc_handle = (ol_txrx_soc_handle) wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    pdev_handle = (ol_txrx_pdev_handle) wlan_pdev_get_dp_handle(scn->sc_pdev);
    if (!soc_handle || !pdev_handle) {
        qdf_err("psoc handle %pK or pdev handle %pK is NULL",
                soc_handle, pdev_handle);
        return -EFAULT;
    }

    if (QDF_STATUS_SUCCESS == wlan_util_is_vdev_active(ic->ic_pdev_obj,
                                                    WLAN_RX_PKT_TAG_ID))
    {
      qdf_err("Protocol Tags should be added before VDEVs are UP");
      qdf_err("Protocol Tags: Total VDEV Cnt(Active + Inactive): %d",
                                                  scn->vdev_count);
      qdf_err("Protocol Tags: Total Monitor VDEV Cnt(Active + Inactive): %d",
                                                      scn->mon_vdev_count);
      return -1;
    }

    if (rx_pkt_protocol_tag_info->op_code == RX_PKT_TAG_OPCODE_ADD)
    {
      param.op_code = ADD_PKT_ROUTING;
      /* Add a constant offset to protocol type before passing it as metadata */
      /*
      * The reason for passing on the packet_type instead of actual tag as metadata is
      * to increment the corresponding protocol type tag counter in stats structure. The
      * packet type to metadata lookup is faster and easier than metadata to packet type
      * lookup.
      */
      param.meta_data = rx_pkt_protocol_tag_info->pkt_type + RX_PROTOCOL_TAG_START_OFFSET;

      /* Update the driver protocol tag mask. This maintains bitmask of protocols
       * for which protocol tagging is enabled */
      ic->rx_pkt_protocol_tag_mask |= (1 << rx_pkt_protocol_tag_info->pkt_type);
    }
    else
    {
      if (!(ic->rx_pkt_protocol_tag_mask & (1 << rx_pkt_protocol_tag_info->pkt_type)))
      {
        qdf_err("Unable to delete RX packet type TAG: %d", rx_pkt_protocol_tag_info->pkt_type);
        return 0;
      }

      param.op_code = DEL_PKT_ROUTING;
      /* Disable the protocol in bitmask */
      ic->rx_pkt_protocol_tag_mask &= ~(1 << rx_pkt_protocol_tag_info->pkt_type);
      rx_pkt_protocol_tag_info->pkt_type_metadata = 0;
    }

    /* Save the user provided metadata in CDP layer. This value will be looked up by
     * CDP layer for tagging the same onto QDF packet. */
    cdp_update_pdev_rx_protocol_tag(soc_handle, (struct cdp_pdev *)pdev_handle,
              ic->rx_pkt_protocol_tag_mask, rx_pkt_protocol_tag_info->pkt_type,
              rx_pkt_protocol_tag_info->pkt_type_metadata);

    /* Generate the protocol bitmap based on the packet type provided */
    switch (rx_pkt_protocol_tag_info->pkt_type)
    {
      case RECV_PKT_TYPE_ARP:
        param.routing_type_bitmap = (1 << PDEV_PKT_TYPE_ARP_IPV4);
        break;
      case RECV_PKT_TYPE_NS:
        param.routing_type_bitmap = (1 << PDEV_PKT_TYPE_NS_IPV6);
        break;
      case RECV_PKT_TYPE_IGMP_V4:
        param.routing_type_bitmap = (1 << PDEV_PKT_TYPE_IGMP_IPV4);
        break;
      case RECV_PKT_TYPE_MLD_V6:
        param.routing_type_bitmap = (1 << PDEV_PKT_TYPE_MLD_IPV6);
        break;
      case RECV_PKT_TYPE_DHCP_V4:
        param.routing_type_bitmap = (1 << PDEV_PKT_TYPE_DHCP_IPV4);
        break;
      case RECV_PKT_TYPE_DHCP_V6:
        param.routing_type_bitmap = (1 << PDEV_PKT_TYPE_DHCP_IPV6);
        break;
      case RECV_PKT_TYPE_DNS_TCP_V4:
        param.routing_type_bitmap = (1 << PDEV_PKT_TYPE_DNS_TCP_IPV4);
        break;
      case RECV_PKT_TYPE_DNS_TCP_V6:
        param.routing_type_bitmap = (1 << PDEV_PKT_TYPE_DNS_TCP_IPV6);
        break;
      case RECV_PKT_TYPE_DNS_UDP_V4:
        param.routing_type_bitmap = (1 << PDEV_PKT_TYPE_DNS_UDP_IPV4);
        break;
      case RECV_PKT_TYPE_DNS_UDP_V6:
        param.routing_type_bitmap = (1 << PDEV_PKT_TYPE_DNS_UDP_IPV6);
        break;
      case RECV_PKT_TYPE_ICMP_V4:
        param.routing_type_bitmap = (1 << PDEV_PKT_TYPE_ICMP_IPV4);
        break;
      case RECV_PKT_TYPE_ICMP_V6:
        param.routing_type_bitmap = (1 << PDEV_PKT_TYPE_ICMP_IPV6);
        break;
      case RECV_PKT_TYPE_TCP_V4:
        param.routing_type_bitmap = (1 << PDEV_PKT_TYPE_TCP_IPV4);
        break;
      case RECV_PKT_TYPE_TCP_V6:
        param.routing_type_bitmap = (1 << PDEV_PKT_TYPE_TCP_IPV6);
        break;
      case RECV_PKT_TYPE_UDP_V4:
        param.routing_type_bitmap = (1 << PDEV_PKT_TYPE_UDP_IPV4);
        break;
      case RECV_PKT_TYPE_UDP_V6:
        param.routing_type_bitmap = (1 << PDEV_PKT_TYPE_UDP_IPV6);
        break;
      case RECV_PKT_TYPE_IPV4:
        param.routing_type_bitmap = (1 << PDEV_PKT_TYPE_IPV4);
        break;
      case RECV_PKT_TYPE_IPV6:
        param.routing_type_bitmap = (1 << PDEV_PKT_TYPE_IPV6);
        break;
      case RECV_PKT_TYPE_EAP:
        param.routing_type_bitmap = (1 << PDEV_PKT_TYPE_EAP);
        break;
      default:
        qdf_err("Invalid packet type: %u", rx_pkt_protocol_tag_info->pkt_type);
        return -1;
    }

    /* Get the PDEV ID and REO destination ring for this PDEV */
    param.pdev_id = wlan_objmgr_pdev_get_pdev_id(ic->ic_pdev_obj);
    param.dest_ring = cdp_get_pdev_reo_dest(soc_handle, (struct cdp_pdev *)pdev_handle);
    param.dest_ring_handler = PDEV_WIFIRXCCE_USE_FT_E;

    qdf_info ("Set RX packet type TAG, opcode : %d, pkt_type : %d, metadata : 0x%x,"
                  "pdev_id = %u, REO dest ring = %d\n",
                  rx_pkt_protocol_tag_info->op_code, rx_pkt_protocol_tag_info->pkt_type,
                  rx_pkt_protocol_tag_info->pkt_type_metadata, param.pdev_id, param.dest_ring);

    return wmi_unified_set_rx_pkt_type_routing_tag(lmac_get_pdev_wmi_handle(scn->sc_pdev), &param);
}

#ifdef WLAN_SUPPORT_RX_TAG_STATISTICS
int ol_ath_ucfg_dump_rx_pkt_protocol_tag_stats(void *vscn, uint32_t protocol_type)
{
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)vscn;
    ol_txrx_soc_handle soc_handle;
    ol_txrx_pdev_handle pdev_handle;

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    uint32_t nss_soc_cfg = cfg_get(scn->soc->psoc_obj, CFG_NSS_WIFI_OL);

    if (nss_soc_cfg)
    {
      qdf_info("RX Protocol Tag not supported when NSS offload is enabled");
      return 0;
    }
#endif /* QCA_NSS_WIFI_OFFLOAD_SUPPORT */

    soc_handle = (ol_txrx_soc_handle) wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    pdev_handle = (ol_txrx_pdev_handle) wlan_pdev_get_dp_handle(scn->sc_pdev);
    if (!soc_handle || !pdev_handle) {
        qdf_err("psoc handle %pK or pdev handle %pK is NULL",
                soc_handle, pdev_handle);
        return -EFAULT;
    }

    cdp_dump_pdev_rx_protocol_tag_stats(soc_handle,
                                        (struct cdp_pdev *)pdev_handle,
                                        protocol_type);

    return 0;
}
#endif //WLAN_SUPPORT_RX_TAG_STATISTICS
#endif //WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG




int ol_ath_ucfg_set_he_sr_config(void *vscn,
                                 uint8_t param, uint8_t value, void *data)
{
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)vscn;
    struct ieee80211com *ic           = &scn->sc_ic;

    switch (param) {

        case HE_SR_SRP_ENABLE:
            if (value > 1) {
                qdf_err("%s: SR ctrl SRP_disallowed field can either be 0 or 1\n",
                        __func__);
                return -EINVAL;
            }

            ic->ic_he_srctrl_srp_disallowed = !value;
            osif_restart_for_config(ic, NULL, NULL);
        break;

        case HE_SR_NON_SRG_OBSSPD_ENABLE:
            if (value > 1) {
                qdf_err("%s: SR ctrl non_srg_obss_pd_disallowed field can"
                        "  either be 0 or 1\n",
                        __func__);
                return -EINVAL;
            }

            ic->ic_he_srctrl_non_srg_obsspd_disallowed = !value;

            if (value) {
                if(*((uint8_t *)data) >
                        HE_SR_NON_SRG_OBSS_PD_MAX_THRESH_OFFSET_VAL) {
                    qdf_err("%s: Max OBSS PD Threshold Offset value must be"
                    " <= %u", __func__, HE_SR_NON_SRG_OBSS_PD_MAX_THRESH_OFFSET_VAL);
                    return -EINVAL;
                }
                ic->ic_he_non_srg_obsspd_max_offset = *((uint8_t *)data);
            }
            osif_restart_for_config(ic, NULL, NULL);
        break;

        case HE_SR_SR15_ENABLE:
            if (value > 1) {
                qdf_err("%s: SR ctrl SR15_allowed field can either be 0 or 1\n",
                        __func__);
                return -EINVAL;
            }

            ic->ic_he_srctrl_sr15_allowed = value;
            osif_restart_for_config(ic, NULL, NULL);
        break;
        default:
            qdf_info("Unhandled SR config command");
            return -EINVAL;
    }

    return 0;
}

int ol_ath_ucfg_get_he_sr_config(void *vscn, uint8_t param, uint32_t *value)
{
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)vscn;
    struct ieee80211com *ic           = &scn->sc_ic;

    switch (param) {

        case HE_SR_SRP_ENABLE:
            value[0] = !ic->ic_he_srctrl_srp_disallowed;
        break;

        case HE_SR_NON_SRG_OBSSPD_ENABLE:
            value[0] = !ic->ic_he_srctrl_non_srg_obsspd_disallowed;
            if (!ic->ic_he_srctrl_non_srg_obsspd_disallowed)
                value[1] = ic->ic_he_non_srg_obsspd_max_offset;
            else
                value[1] = 0;
        break;

        case HE_SR_SR15_ENABLE:
            value[0] = ic->ic_he_srctrl_sr15_allowed;
        break;

        default:
            qdf_info("Unhandled SR config command");
            return -EINVAL;
    }

    return 0;
}

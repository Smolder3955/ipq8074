/*
 * Copyright (c) 2011-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * copyright (c) 2011 Atheros Communications Inc.
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
#include "wlan_mlme_vdev_mgmt_ops.h"
#include <cdp_txrx_cmn.h>
#include <cdp_txrx_ctrl.h>
#include <cdp_txrx_wds.h>
#include <dp_txrx.h>
#include <wlan_osif_priv.h>

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include <osif_nss_wifiol_if.h>
#include <osif_nss_wifiol_vdev_if.h>
#endif
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <cdp_txrx_mon.h>
#include <ieee80211_objmgr_priv.h>

#if MESH_MODE_SUPPORT
#include <if_meta_hdr.h>
#endif

#if QCA_SUPPORT_GPR
#include "ieee80211_ioctl_acfg.h"
#endif

#include <wlan_vdev_mgr_tgt_if_tx_api.h>
#include <wlan_vdev_mgr_tgt_if_rx_defs.h>
#include "vdev_mgr/core/src/vdev_mgr_ops.h"
#include "include/wlan_vdev_mlme.h"
#include <wlan_vdev_mgr_ucfg_api.h>
#include <wlan_vdev_mgr_utils_api.h>
#include <wlan_mlme_dbg.h>
#include <wlan_mlme_dispatcher.h>
#include <osif_private.h>
#include <ieee80211_channel.h>
#include <wlan_dfs_tgt_api.h>

#define DEFAULT_WLAN_VDEV_AP_KEEPALIVE_MAX_UNRESPONSIVE_TIME_SECS  (IEEE80211_INACT_RUN * IEEE80211_INACT_WAIT)
#define DEFAULT_WLAN_VDEV_AP_KEEPALIVE_MAX_IDLE_TIME_SECS          (DEFAULT_WLAN_VDEV_AP_KEEPALIVE_MAX_UNRESPONSIVE_TIME_SECS - 5)
#define DEFAULT_WLAN_VDEV_AP_KEEPALIVE_MIN_IDLE_TIME_SECS          (DEFAULT_WLAN_VDEV_AP_KEEPALIVE_MAX_IDLE_TIME_SECS/2)

/* WAR: this declaration to be made in cmn code*/
QDF_STATUS vdev_mgr_multiple_restart_send(struct wlan_objmgr_pdev *pdev,
                                          struct mlme_channel_param *chan,
                                          uint32_t disable_hw_ack,
                                          uint32_t *vdev_ids,
                                          uint32_t num_vdevs);

uint32_t num_chain_from_chain_mask(uint32_t mask)
{
    int num_rf_chain = 0;

    while (mask) {
        if (mask & 0x1)
            num_rf_chain++;

        mask >>= 1;
    }

    return num_rf_chain;
}

#if ATH_NON_BEACON_AP
bool static inline is_beacon_tx_suspended(struct ieee80211vap *vap)
{
    return (IEEE80211_VAP_IS_NON_BEACON_ENABLED(vap) ||
                  ieee80211_mlme_beacon_suspend_state(vap) ||
                  ieee80211_nawds_disable_beacon(vap));
}
#else
bool static inline is_beacon_tx_suspended(struct ieee80211vap *vap)
{
     return (ieee80211_mlme_beacon_suspend_state(vap) ||
                  ieee80211_nawds_disable_beacon(vap));
}
#endif

#if ATH_SUPPORT_WRAP
static inline void
wrap_disable_da_war_on_all_radios(struct ieee80211com *ic,
                                  struct cdp_soc_t *soc_txrx_handle,
                                  struct cdp_vdev *vdev_txrx_handle)
{
    int i;
    struct ieee80211vap *tmpvap = NULL;

    /* set wrap_disable_da_war to true so that none of the vaps in the ic
     * will have da_war enabled */
    ic->wrap_disable_da_war = true;

    /* Disable DA_WAR for current vap */
    cdp_txrx_set_vdev_param(soc_txrx_handle,
        (struct cdp_vdev *)vdev_txrx_handle, CDP_ENABLE_WDS, 0);

    /* Disable DA_WAR for all vaps in current current ic */
    TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
        struct cdp_vdev *tmp_vdev_txrx_handle =
            (struct cdp_vdev *)wlan_vdev_get_dp_handle(tmpvap->vdev_obj);
        cdp_txrx_set_vdev_param(soc_txrx_handle,
            (struct cdp_vdev *)tmp_vdev_txrx_handle, CDP_ENABLE_WDS, 0);
    }

    /* Disable DA_WAR for all vaps in other ics */
    for (i = 0; i < MAX_RADIO_CNT-1; i++) {
        struct ieee80211com *other_ic = NULL;
        spin_lock(&ic->ic_lock);
        if (ic->other_ic[i] == NULL) {
            spin_unlock(&ic->ic_lock);
            continue;
        }
        other_ic = ic->other_ic[i];
        other_ic->wrap_disable_da_war = true;
        spin_unlock(&ic->ic_lock);

        TAILQ_FOREACH(tmpvap, &other_ic->ic_vaps, iv_next) {
            struct cdp_vdev *tmp_vdev_txrx_handle =
                (struct cdp_vdev *)wlan_vdev_get_dp_handle(tmpvap->vdev_obj);
            cdp_txrx_set_vdev_param(soc_txrx_handle,
                (struct cdp_vdev *)tmp_vdev_txrx_handle, CDP_ENABLE_WDS, 0);
        }
    }
}
#endif

static QDF_STATUS mlme_ext_vap_setup(struct vdev_mlme_obj *vdev_mlme,
                                     int flags,
                                     const u_int8_t *mataddr,
                                     const u_int8_t *bssid)
{
    enum QDF_OPMODE opmode;
    uint32_t mbssid_flags = 0;
    uint8_t vdevid_trans = 0;
    uint16_t type = 0;
    uint16_t sub_type = 0;
    struct wlan_objmgr_vdev *vdev;
    struct ieee80211com *ic;
    struct ieee80211vap *vap;
#if ATH_SUPPORT_WRAP
    u_int8_t bssid_var[IEEE80211_ADDR_LEN];
    u_int8_t mataddr_var[IEEE80211_ADDR_LEN];
#endif

    vdev = vdev_mlme->vdev;
    vap = vdev_mlme->ext_vdev_ptr;
    ic = vap->iv_ic;

    opmode = wlan_vdev_mlme_get_opmode(vdev);
    switch (opmode) {
    case QDF_STA_MODE:
        type = WLAN_VDEV_MLME_TYPE_STA;
        break;
    case QDF_IBSS_MODE:
        type = WLAN_VDEV_MLME_TYPE_IBSS;
        break;
    case QDF_MONITOR_MODE:
        type = WLAN_VDEV_MLME_TYPE_MONITOR;
        break;
    case QDF_SAP_MODE:
    case QDF_WDS_MODE:
    case QDF_BTAMP_MODE:
        type = WLAN_VDEV_MLME_TYPE_AP;
        break;
    default:
        return QDF_STATUS_E_FAILURE;
    }

    if (flags & IEEE80211_P2PDEV_VAP) {
        sub_type = WLAN_VDEV_MLME_SUBTYPE_P2P_DEVICE;
    }else if (flags & IEEE80211_P2PCLI_VAP) {
        sub_type = WLAN_VDEV_MLME_SUBTYPE_P2P_CLIENT;
    }else if (flags & IEEE80211_P2PGO_VAP) {
        sub_type = WLAN_VDEV_MLME_SUBTYPE_P2P_GO;
    }

    if (flags & IEEE80211_SPECIAL_VAP) {
        vap->iv_special_vap_mode = 1;
    }
#if ATH_SUPPORT_NAC
    if (flags & IEEE80211_SMART_MONITOR_VAP)
        vap->iv_smart_monitor_vap =1;
#endif

    vap->mhdr_len = 0;
#if MESH_MODE_SUPPORT
    if (flags & IEEE80211_MESH_VAP) {
        if (!ic->ic_mesh_vap_support) {
            mlme_err("Mesh vap not supported by this radio!!");
            return QDF_STATUS_E_CANCELED;
        }
        vap->iv_mesh_vap_mode =1;
        sub_type = WLAN_VDEV_MLME_SUBTYPE_MESH;
        vap->mhdr_len = sizeof(struct meta_hdr_s);
        vap->mhdr = 0;
    }
#endif

#if ATH_SUPPORT_WRAP
    OS_MEMCPY(mataddr_var, mataddr, IEEE80211_ADDR_LEN);
    OS_MEMCPY(bssid_var, bssid, IEEE80211_ADDR_LEN);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    vap->iv_nss_qwrap_en = 1;
#endif
    if ((opmode == QDF_SAP_MODE) && (flags & IEEE80211_WRAP_VAP)) {
        vap->iv_wrap = 1;
        ic->ic_nwrapvaps++;
    } else if ((opmode == QDF_STA_MODE) && (flags & IEEE80211_CLONE_MACADDR)) {
        if (!(flags & IEEE80211_WRAP_NON_MAIN_STA))
        {
            /*
             * Main ProxySTA VAP for uplink WPS PBC and
             * downlink multicast receive.
             */
            vap->iv_mpsta = 1;
        } else {
            /*
             * Generally, non-Main ProxySTA VAP's don't need to
             * register umac event handlers. We can save some memory
             * space by doing so. This is required to be done before
             * ieee80211_vap_setup. However we still give the scan
             * capability to the first ATH_NSCAN_PSTA_VAPS non-Main
             * PSTA VAP's. This optimizes the association speed for
             * the first several PSTA VAP's (common case).
             */
#define ATH_NSCAN_PSTA_VAPS 0
            if (ic->ic_nscanpsta >= ATH_NSCAN_PSTA_VAPS)
                vap->iv_no_event_handler = 1;
            else
                ic->ic_nscanpsta++;
        }
        vap->iv_psta = 1;
        ic->ic_npstavaps++;
    }

    if (flags & IEEE80211_CLONE_MATADDR) {
        vap->iv_mat = 1;
        OS_MEMCPY(vap->iv_mat_addr, mataddr_var, IEEE80211_ADDR_LEN);
    }

    if (flags & IEEE80211_WRAP_WIRED_STA) {
        vap->iv_wired_pvap = 1;
    }
    if (vap->iv_psta) {
        if (!vap->iv_mpsta) {
            sub_type = WLAN_VDEV_MLME_SUBTYPE_PROXY_STA;
        }
    }

#endif

    if (wlan_pdev_nif_feat_cap_get(ic->ic_pdev_obj, WLAN_PDEV_F_MBSS_IE_ENABLE)) {
        if (opmode == QDF_SAP_MODE) {
            /* Set up this VDEV as transmitting or non-transmitting */
           if (ieee80211_mbssid_setup(vap)) {
               mlme_err("MBSSID setup failed for vap! \n");
               return QDF_STATUS_E_FAILURE;
            }
        }

        if (IEEE80211_VAP_IS_MBSS_NON_TRANSMIT_ENABLED(vap)) {
            mbssid_flags = WLAN_VDEV_MLME_FLAGS_NON_TRANSMIT_AP;
            vdevid_trans = ic->ic_mbss.transmit_vap->iv_unit;
        } else {
            mbssid_flags = WLAN_VDEV_MLME_FLAGS_TRANSMIT_AP;
	}
    } else {
        mbssid_flags = WLAN_VDEV_MLME_FLAGS_NON_MBSSID_AP;
    }


    vdev_mlme->mgmt.mbss_11ax.mbssid_flags = mbssid_flags;
    vdev_mlme->mgmt.mbss_11ax.vdevid_trans = vdevid_trans;
    vdev_mlme->mgmt.generic.type = type;
    vdev_mlme->mgmt.generic.subtype = sub_type;
    vdev_mlme->mgmt.inactivity_params.keepalive_max_unresponsive_time_secs =
                    DEFAULT_WLAN_VDEV_AP_KEEPALIVE_MAX_UNRESPONSIVE_TIME_SECS;
    vdev_mlme->mgmt.inactivity_params.keepalive_max_idle_inactive_time_secs =
                    DEFAULT_WLAN_VDEV_AP_KEEPALIVE_MAX_IDLE_TIME_SECS;
    vdev_mlme->mgmt.inactivity_params.keepalive_min_idle_inactive_time_secs =
                    DEFAULT_WLAN_VDEV_AP_KEEPALIVE_MIN_IDLE_TIME_SECS;
    vdev_mlme->proto.generic.nss_2g = ic->ic_rx_chainmask;
    vdev_mlme->proto.generic.nss_5g = ic->ic_tx_chainmask;

    return QDF_STATUS_SUCCESS;
}

static struct ieee80211vap
*mlme_ext_vap_create_pre_init(struct ieee80211com *ic,
                              int                 opmode,
                              int                 flags,
                              const u_int8_t      *mataddr,
                              const u_int8_t      *bssid,
                              void                *vdev_handle,
                              void                *osifp_handle)
{
    struct ieee80211vap *vap = NULL;
    struct vdev_mlme_obj *vdev_mlme = (struct vdev_mlme_obj *)vdev_handle;
    struct wlan_objmgr_vdev *vdev = vdev_mlme->vdev;

#if ATH_SUPPORT_WRAP
    if ((opmode == IEEE80211_M_STA) && (ic->ic_nstavaps > 0)) {
       if (!(flags & IEEE80211_WRAP_WIRED_STA) &&
           !(flags & IEEE80211_CLONE_MATADDR )) {
           return NULL;
       }
    }
#endif

    /* OL initialization
     * This also takes care of vap allocation through avn
     * vap allocation will be moved here after removing
     * avn and scn dependencies
     */
    vap = ic->ic_vap_create_pre_init(vdev_mlme, flags);
    if (!vap) {
        mlme_err("Pre init validation failed for creating vap");
        return NULL;
    }

    vap->vdev_obj = vdev;
    vap->iv_ic = ic;
    vap->iv_opmode = opmode;
    vdev_mlme->ext_vdev_ptr = vap;
    vap->vdev_mlme = vdev_mlme;
    if (opmode == IEEE80211_M_STA) {
        ieee80211_vap_reset_ap_vaps_set(vap);
    }

    if (mlme_ext_vap_setup(vdev_mlme, flags,
                           mataddr, bssid) != QDF_STATUS_SUCCESS) {
        mlme_err("Unable to setup vap params");
        goto mlme_ext_vap_create_pre_init_alloc_end;
    }

    return vap;

mlme_ext_vap_create_pre_init_alloc_end:
    ic->ic_vap_free(vap);
    return NULL;
}

static QDF_STATUS mlme_ext_vap_create_complete(
                                    struct vdev_mlme_obj *vdev_mlme,
                                    int                 opmode,
                                    int                 scan_priority_base,
                                    int                 flags,
                                    const u_int8_t      *bssid,
                                    const u_int8_t      *mataddr)
{
    struct cdp_soc_t *soc_txrx_handle;
    struct cdp_vdev *vdev_txrx_handle;
    struct ieee80211vap *vap = vdev_mlme->ext_vdev_ptr;
    struct ieee80211com *ic = vap->iv_ic;
    struct wlan_objmgr_vdev *vdev = vdev_mlme->vdev;
    struct wlan_objmgr_psoc *psoc = wlan_vdev_get_psoc(vdev);
    uint8_t vdev_id;
    int nactivevaps = 0;
#if DBDC_REPEATER_SUPPORT
    dp_pdev_link_aggr_t *pdev_lag;
#endif
    struct wlan_vdev_mgr_cfg mlme_cfg;

    soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
    vdev_txrx_handle = wlan_vdev_get_dp_handle(vdev);

    vdev_id = wlan_vdev_get_id(vdev);
    vap->iv_is_up = false;
    vap->iv_is_started = false;
    vap->iv_unit = vdev_id;

    ieee80211_vap_setup(ic, vap, vdev_mlme, opmode, scan_priority_base,
                        flags, bssid);

    /* FIMPLE: update to core mlme structures */
    vdev_mlme->mgmt.generic.ampdu = IEEE80211_AMPDU_SUBFRAME_MAX;
    vdev_mlme->mgmt.generic.amsdu = ic->ic_vht_amsdu;

    /*  Enable MU-BFER & SU-BFER if the Tx chain number
     *  is 0x2, 0x3 and 0x4 and not otherwise.   */
    if(ieee80211_get_txstreams(ic, vap) < 2) {
        vdev_mlme->proto.vht_info.subfer = 0;
        vdev_mlme->proto.vht_info.mubfer = 0;
    }
#if ATH_SUPPORT_WRAP
    if (vap->iv_mpsta) {
        vap->iv_ic->ic_mpsta_vap = vap;
        wlan_pdev_nif_feat_cap_set(vap->iv_ic->ic_pdev_obj,
                                   WLAN_PDEV_F_WRAP_EN);
    }
    if (vap->iv_wrap) {
         vap->iv_ic->ic_wrap_vap = vap;
    }
#if ATH_PROXY_NOACK_WAR
    if (vap->iv_mpsta || vap->iv_wrap) {
        ic->proxy_ast_reserve_wait.blocking = 1;
        qdf_semaphore_init(&(ic->proxy_ast_reserve_wait.sem_ptr));
        qdf_semaphore_acquire(&(ic->proxy_ast_reserve_wait.sem_ptr));
    }
#endif
#endif
    ieee80211vap_set_macaddr(vap, bssid);

#if ATH_SUPPORT_WRAP
    if (vap->iv_wrap || vap->iv_psta) {
        if (ic->ic_nwrapvaps) {
            ieee80211_ic_enh_ind_rpt_set(vap->iv_ic);
        }
    }
#endif

    /* Enabling the advertisement of STA's maximum capabilities instead of
     * the negotiated channel width capabilties (HT and VHT) with the AP */
    if (vap->iv_opmode == IEEE80211_M_STA)
        vap->iv_sta_max_ch_cap = 1;

    /* set user selected channel width to an invalid value by default */
    vap->iv_chwidth = IEEE80211_CWM_WIDTHINVALID;
    vap->iv_he_ul_ppdu_bw = IEEE80211_CWM_WIDTHINVALID;

    /* Enable 256 QAM by default */
    vap->iv_256qam = 1;

    vap->iv_no_cac = 0;

     /*
      * init IEEE80211_DPRINTF control object
      * Register with asf.
      */
    ieee80211_dprintf_init(vap);

#if DBG_LVL_MAC_FILTERING
    vap->iv_print.dbgLVLmac_on = 0; /*initialize dbgLVLmac flag*/
#endif

    nactivevaps = ieee80211_vaps_active(ic);
    if (nactivevaps==0) {
        ic->ic_opmode = opmode;
    }

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (ic->ic_nss_vap_create(vdev_mlme) != QDF_STATUS_SUCCESS) {
#if DBDC_REPEATER_SUPPORT
        if (opmode == IEEE80211_M_STA) {
#if ATH_SUPPORT_WRAP
            /*
             * set sta_vdev to NULL only if its mpsta or a normal sta but not for
             * psta
             */
            if (vap->iv_mpsta || (!vap->iv_mpsta && !vap->iv_psta))
#endif
                dp_lag_pdev_set_sta_vdev(ic->ic_pdev_obj, NULL);
        }

        if (opmode == IEEE80211_M_HOSTAP)
            dp_lag_pdev_set_ap_vdev(ic->ic_pdev_obj, NULL);
#endif
        goto mlme_ext_vap_create_complete_end;
    }
#endif

    /*Setting default value of security type to cdp_sec_type_none*/
    cdp_txrx_set_vdev_param(soc_txrx_handle,
            (struct cdp_vdev *)vdev_txrx_handle, CDP_ENABLE_CIPHER,
            cdp_sec_type_none);

    /* Setting default value to be retrieved
     * when iwpriv get_inact command is used */

#if ATH_SUPPORT_WRAP
    if (vap->iv_psta) {
        cdp_txrx_set_vdev_param(soc_txrx_handle,
                (struct cdp_vdev *)vdev_txrx_handle, CDP_ENABLE_PROXYSTA,
                1);
    }
    if (vap->iv_psta && vap->iv_ic->ic_wrap_com->wc_isolation) {
        struct ieee80211vap *mpsta_vap = vap->iv_ic->ic_mpsta_vap;
        struct cdp_vdev *mpsta_iv_txrx_handle;
        mpsta_iv_txrx_handle = wlan_vdev_get_dp_handle(mpsta_vap->vdev_obj);
        cdp_txrx_set_vdev_param(soc_txrx_handle,
                    (struct cdp_vdev *)vdev_txrx_handle,
                     CDP_ENABLE_QWRAP_ISOLATION , 1);
        cdp_txrx_set_vdev_param(soc_txrx_handle,
                (struct cdp_vdev *)mpsta_iv_txrx_handle,
                     CDP_ENABLE_QWRAP_ISOLATION , 1);
    }

#endif

#if UMAC_SUPPORT_WNM
    /* configure wnm default settings */
    ieee80211_vap_wnm_set(vap);
#endif

    if  ((opmode == IEEE80211_M_HOSTAP) && (flags & IEEE80211_LP_IOT_VAP)) {
        if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan))
            mlme_cfg.value = MLME_BCN_TX_RATE_CODE_2_M;
        else
            mlme_cfg.value = MLME_BCN_TX_RATE_CODE_6_M;

        wlan_util_vdev_mlme_set_param(vdev_mlme, WLAN_MLME_CFG_BCN_TX_RATE_CODE,
                mlme_cfg);
    }
#if MESH_MODE_SUPPORT
    if (vap->iv_mesh_vap_mode) {
        cdp_set_mesh_mode(soc_txrx_handle,
                          (struct cdp_vdev *)vdev_txrx_handle,1);
    }
#endif
#ifndef ATH_WIN_NWF
    vap->iv_tx_encap_type = htt_cmn_pkt_type_ethernet;
    vap->iv_rx_decap_type = htt_cmn_pkt_type_ethernet;
#else
    vap->iv_tx_encap_type = htt_cmn_pkt_type_native_wifi;
    vap->iv_rx_decap_type = htt_cmn_pkt_type_native_wifi;
#endif
    /* disable RTT by default. WFA requirement */
    vap->rtt_enable=0;

    /* Enable EXT NSS support on vap by default if FW provides support */
    if (ic->ic_ext_nss_capable) {
        vap->iv_ext_nss_support = 1;
    }

    if (opmode == IEEE80211_M_HOSTAP)
        vap->iv_rev_sig_160w = DEFAULT_REV_SIG_160_STATUS;

#if ATH_SUPPORT_WRAP
    if (opmode == IEEE80211_M_STA)
        ic->ic_nstavaps++;
#endif

    /* Enable WDS by default in AP mode, except for QWRAP mode */
    if (opmode == IEEE80211_M_HOSTAP) {
#if ATH_SUPPORT_WRAP
        if (!vap->iv_wrap && !ic->wrap_disable_da_war) {
            cdp_txrx_set_vdev_param(soc_txrx_handle,
                (struct cdp_vdev *)vdev_txrx_handle, CDP_ENABLE_WDS, 1);
        } else {
            wrap_disable_da_war_on_all_radios(ic, soc_txrx_handle,
                                              vdev_txrx_handle);
        }
#else
        cdp_txrx_set_vdev_param(soc_txrx_handle,
            (struct cdp_vdev *)vdev_txrx_handle, CDP_ENABLE_WDS, 1);
#endif
    }

#if ATH_SUPPORT_WRAP
    if (opmode == IEEE80211_M_STA && vap->iv_psta) {
        wrap_disable_da_war_on_all_radios(ic, soc_txrx_handle,
                                          vdev_txrx_handle);
    }
#endif



#if DBDC_REPEATER_SUPPORT
    pdev_lag = dp_get_lag_handle(vdev);

    if (pdev_lag) {
        if (opmode == IEEE80211_M_HOSTAP) {
            pdev_lag->ap_vdev = vdev;
        } else {
#if ATH_SUPPORT_WRAP
            /*
             * set sta_vdev to vdev only if its mpsta or a normal sta but not
             * for psta
             */
            if (vap->iv_mpsta || (!vap->iv_mpsta && !vap->iv_psta))
#endif
                pdev_lag->sta_vdev = vdev;
        }
    }
#endif

    return QDF_STATUS_SUCCESS;

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
mlme_ext_vap_create_complete_end:
    return QDF_STATUS_E_FAILURE;
#endif
}

struct ieee80211vap
*mlme_ext_vap_create(struct ieee80211com *ic,
                     int                 opmode,
                     int                 scan_priority_base,
                     int                 flags,
                     const u_int8_t      bssid[IEEE80211_ADDR_LEN],
                     const u_int8_t      mataddr[IEEE80211_ADDR_LEN],
                     void                *vdev_handle)
{
    QDF_STATUS status;
    struct ieee80211vap *vap = NULL;
    struct vdev_mlme_obj *vdev_mlme = (struct vdev_mlme_obj *)vdev_handle;
    struct wlan_objmgr_vdev *vdev = vdev_mlme->vdev;
    struct wlan_objmgr_psoc *psoc = wlan_vdev_get_psoc(vdev);
    void *osifp_handle;
    struct vdev_osif_priv *vdev_osifp = NULL;
    struct cdp_soc_t *soc_txrx_handle;
    struct cdp_vdev *vdev_txrx_handle;

    vdev_osifp = wlan_vdev_get_ospriv(vdev);
    if (!vdev_osifp) {
        QDF_ASSERT(0);
        return NULL;
    }
    osifp_handle = vdev_osifp->legacy_osif_priv;

    /* vap allocation */
    vap = mlme_ext_vap_create_pre_init(ic, opmode, flags, mataddr, bssid,
                                       vdev_mlme, osifp_handle);
    if (vap == NULL) {
        mlme_err("failed to create a vap object");
        return NULL;
    }

    /* NSS allocation */
    status = ic->ic_vap_create_init(vdev_mlme);
    if (status != QDF_STATUS_SUCCESS) {
        mlme_err("Pre init failed for creating vap");
        goto mlme_ext_vap_create_init_end;
    }

    /* send create command */
    status = vdev_mgr_create_send(vdev_mlme);
    if (status != QDF_STATUS_SUCCESS) {
        mlme_err("Failed to send create cmd");
        goto mlme_ext_vap_create_send_end;
    }

    /* send WMI cfg */
    if (tgt_vdev_mgr_create_complete(vdev_mlme) != QDF_STATUS_SUCCESS) {
        mlme_err("Failed to init after create cmd");
        goto mlme_ext_vap_create_complete_end;
    }

    /* feature flag setup and NSS vap creation */
    if (mlme_ext_vap_create_complete(vdev_mlme, opmode, scan_priority_base,
                                     flags, bssid,
                                     mataddr) != QDF_STATUS_SUCCESS) {
        mlme_err("Failed to init after create cmd");
        goto mlme_ext_vap_create_complete_end;
    }

    /* OL specific WMI cfg */
    if (ic->ic_vap_create_post_init(vdev_mlme, flags)
                                                 != QDF_STATUS_SUCCESS) {
        mlme_err("Failed to init after create cmd");
        goto mlme_ext_vap_create_complete_end;
    }

    return vap;

mlme_ext_vap_create_complete_end:

    /* WAR: compoent attach and detach is done since delete request
       and response will expect vdev_mlme object */
    wlan_objmgr_vdev_component_obj_attach((struct wlan_objmgr_vdev *)vdev,
                                          WLAN_UMAC_COMP_MLME,
                                          (void *)vdev_mlme,
                                          QDF_STATUS_SUCCESS);

     ieee80211_dprintf_deregister(vap);
     soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
     vdev_txrx_handle = wlan_vdev_get_dp_handle(vdev);
     wlan_vdev_set_dp_handle(vdev, NULL);
     cdp_vdev_detach(soc_txrx_handle, (struct cdp_vdev *)vdev_txrx_handle,
                     NULL, NULL);
     vdev_mgr_delete_send(vdev_mlme);
     wlan_objmgr_vdev_component_obj_detach((struct wlan_objmgr_vdev *)vdev,
                                           WLAN_UMAC_COMP_MLME,
                                           (void *)vdev_mlme);

mlme_ext_vap_create_send_end:
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    ic->ic_nss_vap_destroy(vap->vdev_obj);
#endif
mlme_ext_vap_create_init_end:
    vdev_mlme->ext_vdev_ptr = NULL;
    vap->vdev_mlme = NULL;
    ic->ic_vap_free(vap);

    return NULL;
}

static QDF_STATUS mlme_ext_vap_post_delete_setup(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
#if QCA_SUPPORT_GPR
    acfg_netlink_pvt_t *acfg_nl;
#endif

    if (!ic)
        return QDF_STATUS_E_FAILURE;

#if ATH_SUPPORT_WRAP
    if (ieee80211vap_get_opmode(vap) == IEEE80211_M_STA) {
        ic->ic_nstavaps--;
    }
#endif

#if QCA_SUPPORT_GPR
    ic = vap->iv_ic;
    acfg_nl = (acfg_netlink_pvt_t *)ic->ic_acfg_handle;
    if (qdf_semaphore_acquire_intr(acfg_nl->sem_lock)){
        /*failed to acquire mutex*/
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                 "%s(): failed to acquire mutex!\n", __func__);
    }
    if (ic->ic_gpr_enable) {
        ic->ic_gpr_enable_count--;
        if (ic->ic_gpr_enable_count == 0) {
            qdf_hrtimer_kill(&ic->ic_gpr_timer);
            qdf_mem_free(ic->acfg_frame);
            ic->acfg_frame = NULL;
            ic->ic_gpr_enable = 0;
            qdf_err("\nStopping GPR timer as this is last vap with gpr \n");
        }
    }
    qdf_semaphore_release(acfg_nl->sem_lock);
#endif
    /* deregister IEEE80211_DPRINTF control object */
    ieee80211_dprintf_deregister(vap);

    ic->ic_vap_post_delete(vap);

    /*
     * Should a callback be provided for notification once the
     * txrx vdev object has actually been deleted?
     */
#if DBDC_REPEATER_SUPPORT
    if (ieee80211vap_get_opmode(vap) == IEEE80211_M_STA) {
#if ATH_SUPPORT_WRAP
        /*
         * set sta_vdev to NULL only if its mpsta or a normal sta but not for
         * psta
         */
        if (vap->iv_mpsta || (!vap->iv_mpsta && !vap->iv_psta))
#endif
            dp_lag_pdev_set_sta_vdev(vap->iv_ic->ic_pdev_obj, NULL);
    }

    if (ieee80211vap_get_opmode(vap) == IEEE80211_M_HOSTAP)
        dp_lag_pdev_set_ap_vdev(vap->iv_ic->ic_pdev_obj, NULL);
#endif

#if ATH_SUPPORT_WRAP
    if (vap->iv_mpsta) {
        vap->iv_ic->ic_mpsta_vap = NULL;
        wlan_pdev_nif_feat_cap_clear(vap->iv_ic->ic_pdev_obj,
                                     WLAN_PDEV_F_WRAP_EN);
    }
    if (vap->iv_wrap) {
         vap->iv_ic->ic_wrap_vap = NULL;
    }
#endif

    if (ic && wlan_pdev_nif_feat_cap_get(ic->ic_pdev_obj, WLAN_PDEV_F_MBSS_IE_ENABLE)) {
        if (!IEEE80211_VAP_IS_MBSS_NON_TRANSMIT_ENABLED(vap)) {
            ic->ic_mbss.transmit_vap = NULL;
        } else {
            qdf_clear_bit(vap->vdev_mlme->mgmt.mbss_11ax.profile_idx - 1,
                          &ic->ic_mbss.bssid_index_bmap[0]);
            IEEE80211_VAP_MBSS_NON_TRANS_DISABLE(vap);
            vap->iv_mbss.mbssid_add_del_profile = 0;
        }
    }

    return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_ext_vap_delete_wait(struct ieee80211vap *vap)
{
    QDF_STATUS status;
    struct vdev_response_timer *vdev_rsp;
    int waitcnt = 0;

    status = mlme_ext_vap_delete(vap);
    if (QDF_IS_STATUS_ERROR(status))
        return status;

    vdev_rsp = &vap->vdev_mlme->vdev_rt;

    waitcnt = 0;
    while(qdf_atomic_test_bit(DELETE_RESPONSE_BIT, &vdev_rsp->rsp_status) &&
          waitcnt < OSIF_MAX_DELETE_VAP_TIMEOUT) {
          schedule_timeout_interruptible(HZ);
          waitcnt++;
    }

    return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_ext_vap_delete(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct wlan_objmgr_psoc *psoc = wlan_vdev_get_psoc(vap->vdev_obj);
    struct cdp_soc_t *soc_txrx_handle;
    struct cdp_vdev *vdev_txrx_handle;
#if ATH_SUPPORT_NAC_RSSI
    char nullmac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
#endif

    /* delete key before vdev delete */
    delete_default_vap_keys(vap);
#if ATH_SUPPORT_WRAP
    /*
     * Both WRAP and ProxySTA VAP's populate keycache slot with
     * vap->iv_myaddr even when security is not used.
     */
    if (vap->iv_wrap) {
        ic->ic_nwrapvaps--;
    } else if (vap->iv_psta) {
        if (!vap->iv_mpsta) {
            if (vap->iv_no_event_handler == 0)
                ic->ic_nscanpsta--;
        }
    ic->ic_npstavaps--;
    }
#endif
    ic->ic_vap_delete(vap->vdev_obj);

#if ATH_SUPPORT_NAC
    /* For HKv2, Nac entries are delted in AST table by FW
     * so host doesn't have to send nac delete cmd.
     * nac entries in dp_pdev are deleted by host
     * in vdev detach. Nac deletes are sent for non
     * HKv2 case only.
     */
    if (vap->iv_smart_monitor_vap) {
        struct ieee80211_nac *nac = &vap->iv_nac;
        int i = 0;
        vap->iv_smart_monitor_vap = 0;
        if (!vap->iv_ic->ic_hw_nac_monitor_support) {
           for (i = 0; i < NAC_MAX_CLIENT; i++) {
                vap->iv_neighbour_rx(vap , 0, IEEE80211_NAC_PARAM_DEL,
                                     IEEE80211_NAC_MACTYPE_CLIENT,
                                     nac->client[i].macaddr);
           }
        }
    }
#endif
    soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
    vdev_txrx_handle = wlan_vdev_get_dp_handle(vap->vdev_obj);

#if ATH_SUPPORT_NAC_RSSI
    if (vap->iv_scan_nac_rssi &&
       !IEEE80211_ADDR_EQ((vap->iv_nac_rssi.client_mac), nullmac)) {
       cdp_update_filter_neighbour_peers(soc_txrx_handle,
                                         (struct cdp_vdev *)vdev_txrx_handle,
                                         IEEE80211_NAC_PARAM_DEL,
                                         vap->iv_nac_rssi.client_mac);
    }
#endif

    if (vdev_mgr_delete_send(vap->vdev_mlme) != QDF_STATUS_SUCCESS)
        mlme_err("Unable to remove an interface for ath_dev.");

    return mlme_ext_vap_post_delete_setup(vap);
}

QDF_STATUS mlme_ext_vap_down(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    QDF_STATUS status;

    ic->ic_opmode = ieee80211_new_opmode(vap, false);

    if (vap->iv_down(vap->vdev_obj) != QDF_STATUS_SUCCESS) {
        mlme_err("Unable to bring down the interface for ath_dev.");
        return QDF_STATUS_E_FAILURE;
    }

    /* bring down vdev in target */
    if (vdev_mgr_down_send(vap->vdev_mlme)) {
        mlme_err("Unable to bring down the interface for ath_dev.");
	status = QDF_STATUS_E_FAILURE;
    } else {
        vap->iv_is_up = false;
	status = QDF_STATUS_SUCCESS;
    }

    wlan_vdev_mlme_sm_deliver_evt_sync(vap->vdev_obj,
                                       WLAN_VDEV_SM_EV_DOWN_COMPLETE,
                                       0, NULL);
    return status;
}

void mlme_ext_vap_flush_bss_peer_tids(struct ieee80211vap *vap)
{
    u_int32_t peer_tid_bitmap = 0xffffffff;
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_node *ni;

    if(ic && !ic->ic_is_mode_offload(ic)) {
        return;
    }

    ni = ieee80211_ref_bss_node(vap);
    if (ni != NULL) {
        if (vdev_mgr_peer_flush_tids_send(vap->vdev_mlme, ni->ni_macaddr,
                                          peer_tid_bitmap) !=
                                                        QDF_STATUS_SUCCESS)
            mlme_err("Unable to Flush tids peer in Target");
        ieee80211_free_node(ni);
    }

    return;
}

int mlme_ext_vap_stop(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    enum ieee80211_opmode opmode = ieee80211vap_get_opmode(vap);
    struct cdp_soc_t *soc_txrx_handle;
    struct cdp_pdev *pdev_txrx_handle;
    struct cdp_vdev *vdev_txrx_handle;

    soc_txrx_handle =
           wlan_psoc_get_dp_handle(wlan_pdev_get_psoc(ic->ic_pdev_obj));
    pdev_txrx_handle = wlan_pdev_get_dp_handle(ic->ic_pdev_obj);
    vdev_txrx_handle = wlan_vdev_get_dp_handle(vap->vdev_obj);

    switch (opmode) {
    case IEEE80211_M_MONITOR:
        cdp_reset_monitor_mode(soc_txrx_handle,
                               (struct cdp_pdev *)pdev_txrx_handle);
        break;
   case IEEE80211_M_STA:
        OS_CANCEL_TIMER(&vap->iv_cswitch_timer);
        /* channel_switch_state is set to true when AP announces
         * channel switch and is reset when chanel switch completes.
         * As STA mode channel switch timer is cancelled here,
         * channel_switch_state will endup telling CSA in progress
         * which is wrong.
         * reset channel_switch_state here to reflect correct state
         */
        vap->channel_switch_state = 0;
        OS_CANCEL_TIMER(&vap->iv_disconnect_sta_timer);
        break;
    default:
        break;
    }

    spin_lock_dpc(&vap->init_lock);

    /* Flush all TIDs for bss node - to cleanup
     * pending traffic in bssnode
     */
    mlme_ext_vap_flush_bss_peer_tids(vap);

    if (vap->iv_stop_pre_init(vap->vdev_obj) == QDF_STATUS_E_CANCELED) {
        spin_unlock_dpc(&vap->init_lock);
        vap->iv_is_started = false;
        return EOK;
    }

    /* bring down vdev in target */
    if (vdev_mgr_stop_send(vap->vdev_mlme)) {
        mlme_err("Unable to send stop to target.");
        spin_unlock_dpc(&vap->init_lock);
        wlan_vdev_mlme_sm_deliver_evt_sync(vap->vdev_obj,
                                           WLAN_VDEV_SM_EV_STOP_FAIL,
                                           0, NULL);
        return -EFAULT;
    } else {
        vap->iv_is_started = false;
    }

    spin_unlock_dpc(&vap->init_lock);

    return EOK;
}

QDF_STATUS mlme_ext_vap_up(struct ieee80211vap *vap, bool restart)
{
    QDF_STATUS status;
    struct ieee80211com *ic           = vap->iv_ic;
    enum ieee80211_opmode opmode      = ieee80211vap_get_opmode(vap);
    struct ieee80211_node *ni         = vap->iv_bss;
    uint32_t aid         = 0;
    uint32_t value       = 0;
    struct cdp_soc_t *soc_txrx_handle;
    struct cdp_vdev *vdev_txrx_handle;
    enum htt_cmn_pkt_type pkt_type;
    uint8_t bssid_null[] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
    struct vdev_mlme_obj *vdev_mlme = vap->vdev_mlme;

    soc_txrx_handle =
              wlan_psoc_get_dp_handle(wlan_pdev_get_psoc(ic->ic_pdev_obj));
    vdev_txrx_handle = wlan_vdev_get_dp_handle(vap->vdev_obj);

    if (!soc_txrx_handle || !vdev_txrx_handle) {
        mlme_err("Failed to get DP handles");
        return QDF_STATUS_E_FAILURE;
    }

    switch (opmode) {
        case IEEE80211_M_STA:
            /* Set assoc id */
            aid = IEEE80211_AID(ni->ni_associd);
            vdev_mlme->proto.sta.assoc_id = aid;

            /* Set the beacon interval of the bss */
            vdev_mlme->proto.generic.beacon_interval = ni->ni_intval;

            /* set uapsd configuration */
            if (ieee80211_vap_wme_is_set(vap) &&
                    (ni->ni_ext_caps & IEEE80211_NODE_C_UAPSD)) {
                value = 0;
                if (vap->iv_uapsd & WME_CAPINFO_UAPSD_VO) {
                    value |= WLAN_MLME_HOST_STA_PS_UAPSD_AC3_DELIVERY_EN |
                        WLAN_MLME_HOST_STA_PS_UAPSD_AC3_TRIGGER_EN;
                }
                if (vap->iv_uapsd & WME_CAPINFO_UAPSD_VI) {
                    value |= WLAN_MLME_HOST_STA_PS_UAPSD_AC2_DELIVERY_EN |
                        WLAN_MLME_HOST_STA_PS_UAPSD_AC2_TRIGGER_EN;
                }
                if (vap->iv_uapsd & WME_CAPINFO_UAPSD_BK) {
                    value |= WLAN_MLME_HOST_STA_PS_UAPSD_AC1_DELIVERY_EN |
                        WLAN_MLME_HOST_STA_PS_UAPSD_AC1_TRIGGER_EN;
                }
                if (vap->iv_uapsd & WME_CAPINFO_UAPSD_BE) {
                    value |= WLAN_MLME_HOST_STA_PS_UAPSD_AC0_DELIVERY_EN |
                        WLAN_MLME_HOST_STA_PS_UAPSD_AC0_TRIGGER_EN;
                }
            }

            vdev_mlme->proto.sta.uapsd_cfg = value;
            break;
        case IEEE80211_M_HOSTAP:
        case IEEE80211_M_IBSS:
            if (vap->iv_special_vap_mode) {
                if (cdp_set_monitor_mode(
                            soc_txrx_handle,
                            (struct cdp_vdev *)vdev_txrx_handle,
                            vap->iv_smart_monitor_vap)) {
                    mlme_err("Unable to bring up in special vap interface");
                    return QDF_STATUS_E_FAILURE;
                }

                break;
            }
            /*currently ratemask has to be set before vap is up*/
            if (!vap->iv_ratemask_default) {
               /*
		* ratemask higher 32 bit is reserved for beeliner,
		* use 0x0 for peregrine
		*/
               if (vap->iv_legacy_ratemasklower32 != 0) {
                   vdev_mlme->mgmt.rate_info.type = 0;
                   vdev_mlme->mgmt.rate_info.lower32 = vap->iv_legacy_ratemasklower32;
                   vdev_mlme->mgmt.rate_info.higher32 = 0x0;
                   vdev_mlme->mgmt.rate_info.lower32_2 = 0x0;
                   wlan_util_vdev_mlme_set_ratemask_config(vdev_mlme);
               }
               if (vap->iv_ht_ratemasklower32 != 0) {
                   vdev_mlme->mgmt.rate_info.type = 1;
                   vdev_mlme->mgmt.rate_info.lower32 = vap->iv_ht_ratemasklower32;
                   vdev_mlme->mgmt.rate_info.higher32 = 0x0;
                   vdev_mlme->mgmt.rate_info.lower32_2 = 0x0;
                   wlan_util_vdev_mlme_set_ratemask_config(vdev_mlme);
               }
               if (vap->iv_vht_ratemasklower32 != 0 ||
                   vap->iv_vht_ratemaskhigher32 != 0 ||
                   vap->iv_vht_ratemasklower32_2 != 0) {
                   vdev_mlme->mgmt.rate_info.type = 2;
                   vdev_mlme->mgmt.rate_info.lower32 = vap->iv_vht_ratemasklower32;
                   vdev_mlme->mgmt.rate_info.higher32 = vap->iv_vht_ratemaskhigher32;
                   vdev_mlme->mgmt.rate_info.lower32_2 = vap->iv_vht_ratemasklower32_2;
                   wlan_util_vdev_mlme_set_ratemask_config(vdev_mlme);
               }
               if (vap->iv_he_ratemasklower32 != 0 ||
                   vap->iv_he_ratemaskhigher32 != 0 ||
                   vap->iv_he_ratemasklower32_2 != 0) {
                   vdev_mlme->mgmt.rate_info.type = 3;
                   vdev_mlme->mgmt.rate_info.lower32 = vap->iv_he_ratemasklower32;
                   vdev_mlme->mgmt.rate_info.higher32 = vap->iv_he_ratemaskhigher32;
                   vdev_mlme->mgmt.rate_info.lower32_2 = vap->iv_he_ratemasklower32_2;
                   wlan_util_vdev_mlme_set_ratemask_config(vdev_mlme);
               }
            }

        break;
    case IEEE80211_M_MONITOR:
        IEEE80211_ADDR_COPY(&vdev_mlme->mgmt.generic.bssid, bssid_null);
        break;
    default:
        break;
    }

    status = vap->iv_up_pre_init(vap->vdev_obj, restart);
    if (QDF_IS_STATUS_ERROR(status)) {
        if (status == QDF_STATUS_E_CANCELED)
            return QDF_STATUS_SUCCESS;
        mlme_err("OL Up pre init failed");
        goto mlme_ext_vap_up_fail;
    }

    ic->ic_opmode = ieee80211_new_opmode(vap,true);

    /* Send beacon template for regular or MBSS Tx VAP */
    if (!IEEE80211_VAP_IS_MBSS_NON_TRANSMIT_ENABLED(vap)) {

        if (vap->iv_bcn_offload_enable) {
            ic->ic_bcn_tmpl_send(vap->vdev_obj);
        }

#if DYNAMIC_BEACON_SUPPORT
        if (vap->iv_dbeacon) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG, "ic_ieee:%u  freq:%u \n",
                ic->ic_curchan->ic_ieee,ic->ic_curchan->ic_freq);
            /* Do not suspend beacon for DFS channels or hidden ssid not enabled */
            if (IEEE80211_IS_CHAN_DFS(ic->ic_curchan) ||
                !IEEE80211_VAP_IS_HIDESSID_ENABLED(vap)) {
                ieee80211_mlme_set_dynamic_beacon_suspend(vap,false);
            } else {
                ieee80211_mlme_set_dynamic_beacon_suspend(vap,true);
            }
        }
#endif

        if (vap->iv_bcn_offload_enable) {
            if (is_beacon_tx_suspended(vap)) {
                /*for non-beaconing VAP, don't send beacon*/
                ic->ic_beacon_offload_control(vap, IEEE80211_BCN_OFFLD_TX_DISABLE);
            } else {
                if (ieee80211_wnm_tim_is_set(vap->wnm))
                    ic->ic_beacon_offload_control(vap,
                                              IEEE80211_BCN_OFFLD_SWBA_ENABLE);
               else
                    ic->ic_beacon_offload_control(vap,
                                              IEEE80211_BCN_OFFLD_SWBA_DISABLE);
            }
        } else {
            if (is_beacon_tx_suspended(vap)) {
                mlme_debug("suspend the beacon: vap-%d(%s) \n",
                            vap->iv_unit,vap->iv_netdev_name);
                if (ic->ic_beacon_offload_control) {
                    /*for non-beaconing VAP, don't send beacon*/
                    ic->ic_beacon_offload_control(vap,
                                              IEEE80211_BCN_OFFLD_TX_DISABLE);
               }
            }
        }
    } /* IEEE80211_VAP_IS_MBSS_NON_TRANSMIT_ENABLED */

    mlme_debug("Setting Rx Decap type %d, Tx Encap type: %d\n",
            vap->iv_rx_decap_type, vap->iv_tx_encap_type);

    if (vap->iv_rx_decap_type == 0) {
        pkt_type = htt_cmn_pkt_type_raw;
    } else if (vap->iv_rx_decap_type == 1) {
        pkt_type = htt_cmn_pkt_type_native_wifi;
    } else {
        pkt_type = htt_cmn_pkt_type_ethernet;
    }

    vdev_mlme->mgmt.generic.rx_decap_type = pkt_type;

    if (vap->iv_tx_encap_type == 0) {
        pkt_type = htt_cmn_pkt_type_raw;
    } else if (vap->iv_tx_encap_type == 1) {
        pkt_type = htt_cmn_pkt_type_native_wifi;
    } else {
        pkt_type = htt_cmn_pkt_type_ethernet;
    }

    vdev_mlme->mgmt.generic.tx_decap_type = pkt_type;

    if (IEEE80211_VAP_IS_MBSS_NON_TRANSMIT_ENABLED(vap)) {
        IEEE80211_ADDR_COPY(vdev_mlme->mgmt.mbss_11ax.trans_bssid,
                            ic->ic_mbss.transmit_vap->iv_myaddr);
        vdev_mlme->mgmt.mbss_11ax.profile_num =
                            ic->ic_mbss.num_non_transmit_vaps;
    } else {
        vdev_mlme->mgmt.mbss_11ax.profile_num = 0;
    }

    /* Add a check to see if mpsta is up before bringing up psta interface.
     * This check is made to prevent a corner case during wifi down when mpsta
     * is in bringdown phase and then psta connection happens */
#if ATH_SUPPORT_WRAP
    if (vap->iv_psta && !vap->iv_mpsta) {
        wlan_if_t mpsta_vap = vap->iv_ic->ic_mpsta_vap;
        if (!mpsta_vap || !mpsta_vap->iv_is_started || !mpsta_vap->iv_is_up) {
            mlme_err("mpsta is down, stopped or deleted");
            goto mlme_ext_vap_up_fail;
        }
    }
#endif

    if (opmode != IEEE80211_M_MONITOR)
        IEEE80211_ADDR_COPY(&vdev_mlme->mgmt.generic.bssid, ni->ni_bssid);

    if (vdev_mgr_up_send(vdev_mlme) != QDF_STATUS_SUCCESS) {
        mlme_err("Unable to bring up the interface for ath_dev.");
        goto mlme_ext_vap_up_fail;
    }

    vap->iv_is_up = true;
    return vap->iv_up_complete(vap->vdev_obj);

mlme_ext_vap_up_fail:
    wlan_vdev_mlme_sm_deliver_evt_sync(vap->vdev_obj,
                                       WLAN_VDEV_SM_EV_UP_FAIL,
                                       0, NULL);
    return QDF_STATUS_E_FAILURE;
}

static void mlme_ext_vap_start_param_reset(struct vdev_mlme_obj *vdev_mlme)
{
    vdev_mlme->mgmt.generic.minpower     = 0;
    vdev_mlme->mgmt.generic.maxpower     = 0;
    vdev_mlme->mgmt.generic.maxregpower  = 0;
    vdev_mlme->mgmt.generic.antennamax   = 0;
    vdev_mlme->mgmt.chainmask_info.num_rx_chain = 0;
    vdev_mlme->mgmt.chainmask_info.num_tx_chain = 0;
    vdev_mlme->proto.he_ops_info.he_ops = 0;
    vdev_mlme->mgmt.chainmask_info.num_rx_chain = 0;
    vdev_mlme->mgmt.chainmask_info.num_tx_chain = 0;
    vdev_mlme->mgmt.rate_info.half_rate = FALSE;
    vdev_mlme->mgmt.rate_info.quarter_rate = FALSE;
    vdev_mlme->mgmt.generic.reg_class_id = 0;
}

static QDF_STATUS mlme_ext_vap_start_setup(struct ieee80211vap *vap,
                                           struct ieee80211_ath_channel *chan,
                                           struct wlan_channel *des_chan,
                                           struct vdev_mlme_obj *vdev_mlme)
{
    u_int32_t chan_mode;
    struct ieee80211com *ic = vap->iv_ic;
    struct wlan_objmgr_pdev *pdev;
    struct wlan_objmgr_psoc *psoc;
    uint32_t cfreq1 = 0, cfreq2 = 0;
    uint32_t freq = 0, he_ops = 0;

    freq = ieee80211_chan2freq(ic, chan);
    if (!freq) {
        mlme_err("Invalid frequency");
        return QDF_STATUS_E_INVAL;
    }

    pdev = ic->ic_pdev_obj;
    if (pdev == NULL) {
        QDF_ASSERT(0);
        return QDF_STATUS_E_FAILURE;
    }

    psoc = wlan_pdev_get_psoc(pdev);

    if (psoc == NULL) {
        QDF_ASSERT(0);
        return QDF_STATUS_E_FAILURE;
    }

    mlme_ext_vap_start_param_reset(vdev_mlme);
    chan_mode = ieee80211_chan2mode(chan);

    if ((chan_mode == IEEE80211_MODE_11AC_VHT80) ||
           (chan_mode == IEEE80211_MODE_11AC_VHT160) ||
           (chan_mode == IEEE80211_MODE_11AC_VHT80_80) ||
           (chan_mode == IEEE80211_MODE_11AXA_HE80) ||
           (chan_mode == IEEE80211_MODE_11AXA_HE160) ||
           (chan_mode == IEEE80211_MODE_11AXA_HE80_80)) {
       if (chan->ic_ieee < 20)
           cfreq1 = ieee80211_ieee2mhz(ic, chan->ic_vhtop_ch_freq_seg1,
                                       IEEE80211_CHAN_2GHZ);
       else
           cfreq1 = ieee80211_ieee2mhz(ic, chan->ic_vhtop_ch_freq_seg1,
                                       IEEE80211_CHAN_5GHZ);

       if ((chan_mode == IEEE80211_MODE_11AC_VHT80_80) ||
              (chan_mode == IEEE80211_MODE_11AC_VHT160) ||
              (chan_mode == IEEE80211_MODE_11AXA_HE80_80) ||
              (chan_mode == IEEE80211_MODE_11AXA_HE160))
           cfreq2 = ieee80211_ieee2mhz(ic, chan->ic_vhtop_ch_freq_seg2,
                                       IEEE80211_CHAN_5GHZ);
    } else if ((chan_mode == IEEE80211_MODE_11NA_HT40PLUS) ||
                   (chan_mode == IEEE80211_MODE_11NG_HT40PLUS) ||
                   (chan_mode == IEEE80211_MODE_11AC_VHT40PLUS) ||
                   (chan_mode == IEEE80211_MODE_11AXA_HE40PLUS) ||
                   (chan_mode == IEEE80211_MODE_11AXG_HE40PLUS)) {
           cfreq1 = freq + 10;
    } else if ((chan_mode == IEEE80211_MODE_11NA_HT40MINUS) ||
                   (chan_mode == IEEE80211_MODE_11NG_HT40MINUS) ||
                   (chan_mode == IEEE80211_MODE_11AC_VHT40MINUS) ||
                   (chan_mode == IEEE80211_MODE_11AXA_HE40MINUS) ||
                   (chan_mode == IEEE80211_MODE_11AXG_HE40MINUS)) {
           cfreq1 = freq - 10;
    } else {
           cfreq1 = freq;
    }

    des_chan->ch_cfreq1 = cfreq1;
    des_chan->ch_cfreq2 = cfreq2;
    if (IEEE80211_IS_CHAN_HALF(chan))
        vdev_mlme->mgmt.rate_info.half_rate = TRUE;

    if (IEEE80211_IS_CHAN_QUARTER(chan))
        vdev_mlme->mgmt.rate_info.quarter_rate = TRUE;

    ieee80211com_set_num_rx_chain(ic,
                              num_chain_from_chain_mask(ic->ic_rx_chainmask));
    ieee80211com_set_num_tx_chain(ic,
                              num_chain_from_chain_mask(ic->ic_tx_chainmask));
    ieee80211com_set_spatialstreams(ic,
                              num_chain_from_chain_mask(ic->ic_rx_chainmask));
    vdev_mlme->mgmt.generic.minpower     = chan->ic_minpower;
    vdev_mlme->mgmt.generic.maxpower     = chan->ic_maxpower;
    vdev_mlme->mgmt.generic.maxregpower  = chan->ic_maxregpower;
    vdev_mlme->mgmt.generic.antennamax   = chan->ic_antennamax;
    vdev_mlme->mgmt.generic.reg_class_id = chan->ic_regClassId;
    vdev_mlme->mgmt.chainmask_info.num_rx_chain = ic->ic_num_rx_chain;
    vdev_mlme->mgmt.chainmask_info.num_tx_chain = ic->ic_num_tx_chain;
#if SUPPORT_11AX_D3
    he_ops               = (ic->ic_he.heop_param |
                            (ic->ic_he.heop_bsscolor_info << HEOP_PARAM_S));
    /* override only bsscolor at this time */
    he_ops              &= ~(IEEE80211_HEOP_BSS_COLOR_MASK <<
                                        HEOP_PARAM_S);
    he_ops              |= (ic->ic_bsscolor_hdl.selected_bsscolor <<
                                        HEOP_PARAM_S);
#else
    he_ops               = ic->ic_he.heop_param;
    /* override only bsscolor at this time */
    he_ops              &= ~IEEE80211_HEOP_BSS_COLOR_MASK;
    he_ops              |= ic->ic_bsscolor_hdl.selected_bsscolor;
#endif
    vdev_mlme->proto.he_ops_info.he_ops = he_ops;

    if (!IEEE80211_IS_CHAN_HE(chan) && !IEEE80211_IS_CHAN_VHT(chan))
    {
        /* IC might support higher than IEEE80211_MAX_PRE11AC_STREAMS,
         * but if we aren't using 11ac or higher, we limit the streams to
         * IEEE80211_MAX_PRE11AC_STREAMS.
         *
         * Note: It would have been preferable to look at the exact PHY mode
         * older than 11ac, and determine max value for streams accordingly.
         * E.g. for pre-11n modes like 11a, we could limit to 1. However, prior
         * to adding 8 stream support, we have been configuring the max value
         * per IC capability (e.g. 4) even for pre-11n modes. To maintain
         * compatility with older FW and preserve any proprietary interactions
         * that might have been built around assumptions based on this
         * behaviour, we only cap to IEEE80211_MAX_PRE11AC_STREAMS (which is 4).
         * XXX: Modify this in the future once confirmed that there are no
         * issues in altering behaviour.
         */
        if (vdev_mlme->mgmt.chainmask_info.num_rx_chain >
                                               IEEE80211_MAX_PRE11AC_STREAMS)
        {
            vdev_mlme->mgmt.chainmask_info.num_rx_chain =
                                               IEEE80211_MAX_PRE11AC_STREAMS;
        }

        if (vdev_mlme->mgmt.chainmask_info.num_tx_chain >
                                                IEEE80211_MAX_PRE11AC_STREAMS)
        {
            vdev_mlme->mgmt.chainmask_info.num_tx_chain =
                                                IEEE80211_MAX_PRE11AC_STREAMS;
        }
    }

    return QDF_STATUS_SUCCESS;
}

int mlme_ext_vap_start(struct ieee80211vap *vap,
                       struct ieee80211_ath_channel *chan,
                       u_int16_t reqid,
                       u_int16_t max_start_time,
                       u_int8_t restart)
{
    struct ieee80211com *ic = vap->iv_ic;
    u_int32_t freq;
    bool disable_hw_ack= false;
    bool dfs_channel = false;
    struct vdev_mlme_obj *vdev_mlme;
    struct wlan_channel *des_chan = NULL;

    if (!chan) {
        mlme_err("Invalid channel input");
        return EINVAL;
    }

    freq = ieee80211_chan2freq(ic, chan);
    if (!freq) {
        mlme_err("ERROR : INVALID Freq \n");
        return EINVAL;
    }

    vdev_mlme = vap->vdev_mlme;
    if (!vdev_mlme) {
        mlme_err("vdev mlme component not found");
        return EINVAL;
    }

    des_chan = wlan_vdev_mlme_get_des_chan(vap->vdev_obj);
    if (!des_chan) {
        mlme_err("desired channel not found");
        return EINVAL;
    }

    dfs_channel = (IEEE80211_IS_CHAN_DFS(chan) ||
        ((IEEE80211_IS_CHAN_11AC_VHT80_80(chan) ||
          IEEE80211_IS_CHAN_11AC_VHT160(chan) ||
          IEEE80211_IS_CHAN_11AXA_HE80_80(chan) ||
          IEEE80211_IS_CHAN_11AXA_HE160(chan)) &&
          IEEE80211_IS_CHAN_DFS_CFREQ2(chan)));

    spin_lock_dpc(&vap->init_lock);
#if ATH_SUPPORT_DFS
    if (vap->iv_opmode == IEEE80211_M_HOSTAP &&
        dfs_channel &&
        !ieee80211_vap_is_any_running(ic) && !vap->iv_no_cac) {
        /*
         * Do not set HW no ack bit, if STA vap is present and
         * if it is not in Repeater-ind mode
	 */
#if ATH_SUPPORT_WRAP
        if (ic->ic_nstavaps == 0 ||
            (ic->ic_nstavaps >=1 &&
             ieee80211_ic_enh_ind_rpt_is_set(vap->iv_ic))) {
           disable_hw_ack = true;
        }
#endif
    }
#endif
    if (!ic->ic_ext_nss_capable) {
        vap->iv_ext_nss_support = 0;
    }

    if (vap->iv_start_pre_init(vap->vdev_obj, chan, restart)
                                               != QDF_STATUS_SUCCESS) {
       spin_unlock_dpc(&vap->init_lock);
       return EBUSY;
    }

    qdf_atomic_set(&(vap->iv_is_start_sent), 1);
    qdf_atomic_set(&(vap->iv_is_start_resp_received), 0);

    vdev_mlme->mgmt.generic.disable_hw_ack = disable_hw_ack;
    mlme_ext_vap_start_setup(vap, chan, des_chan, vdev_mlme);

    if (vdev_mgr_start_send(vdev_mlme, restart) != QDF_STATUS_SUCCESS) {
        mlme_err("Unable to bring up the interface for ath_dev.");
        if (!restart)
            qdf_atomic_set(&(vap->iv_is_start_sent), 0);
        else
            wlan_vdev_mlme_sm_deliver_evt_sync(vap->vdev_obj,
                                               WLAN_VDEV_SM_EV_RESTART_REQ_FAIL,
                                               0, NULL);
    } else {
        vap->iv_is_started = true;
    }

    if (vap->iv_start_post_init(vap->vdev_obj, chan, restart) !=
                                                   QDF_STATUS_SUCCESS) {
        qdf_atomic_set(&(vap->iv_is_start_sent), 0);
        spin_unlock_dpc(&vap->init_lock);
        if (!restart)
            wlan_vdev_mlme_sm_deliver_evt_sync(vap->vdev_obj,
                                               WLAN_VDEV_SM_EV_START_REQ_FAIL,
                                               0, NULL);
        return EBUSY;
    }

    spin_unlock_dpc(&vap->init_lock);
    if (ic->ic_cbs && ic->ic_cbs->cbs_enable) {
        wlan_bk_scan(ic);
    }

    return EBUSY;
}

int
mlme_ext_vap_start_response_event_handler(struct vdev_start_response *rsp,
                                          struct vdev_mlme_obj *vdev_mlme)
{
    QDF_STATUS status;
    struct ieee80211com  *ic;
    ieee80211_resmgr_t resmgr;
    wlan_if_t vaphandle;
    struct wlan_objmgr_pdev *pdev;
    struct wlan_objmgr_vdev *vdev;
    struct wlan_objmgr_psoc *psoc;
    uint8_t vdev_id;
    int restart_resp = 0;

    vdev = vdev_mlme->vdev;
    vaphandle = wlan_vdev_get_mlme_ext_obj(vdev);
    if (NULL == vaphandle) {
        mlme_err("Event received for Invalid/Deleted vap handle");
        return 0;
    }

    qdf_atomic_set(&(vaphandle->iv_is_start_resp_received), 1);
    qdf_atomic_set(&(vaphandle->iv_is_start_sent), 0);

    ic = vaphandle->iv_ic;
    resmgr = ic->ic_resmgr;

    vdev_id = wlan_vdev_get_id(vdev);
    pdev = ic->ic_pdev_obj;
    psoc = wlan_pdev_get_psoc(pdev);

    spin_lock_dpc(&vaphandle->init_lock);

    switch (vaphandle->iv_opmode) {

        case IEEE80211_M_MONITOR:
               /* Handle same as HOSTAP */
        case IEEE80211_M_HOSTAP:
            if (rsp->status == WLAN_MLME_HOST_VDEV_START_CHAN_INVALID) {
               spin_unlock_dpc(&vaphandle->init_lock);
               wlan_vdev_mlme_sm_deliver_evt(vdev,
                                             WLAN_VDEV_SM_EV_START_REQ_FAIL,
                                             0, NULL);

                return 0;
            }
            break;
        default:
            break;
    }

    status = vaphandle->iv_vap_start_rsp_handler(rsp, vdev_mlme);
    spin_unlock_dpc(&vaphandle->init_lock);
    if (status == QDF_STATUS_E_AGAIN) {
        /* Notify START Response to do RESTART req with new channel */
        wlan_vdev_mlme_sm_deliver_evt(vdev, WLAN_VDEV_SM_EV_START_RESP,
                                      0, NULL);
        return 0;
    } else if (status == QDF_STATUS_E_CANCELED) {
        wlan_vdev_mlme_sm_deliver_evt(vdev, WLAN_VDEV_SM_EV_START_REQ_FAIL,
                                      0, NULL);
        return 0;
    } else if (status != QDF_STATUS_SUCCESS) {
        return 0;
    }

    if (rsp->resp_type == WLAN_MLME_HOST_VDEV_START_RESP_EVENT) {
        wlan_vdev_mlme_sm_deliver_evt(vdev, WLAN_VDEV_SM_EV_START_RESP, 1,
                                      &restart_resp);
    } else {
        restart_resp = 1;
        wlan_vdev_mlme_sm_deliver_evt(vdev, WLAN_VDEV_SM_EV_RESTART_RESP, 1,
                                      &restart_resp);
    }

    return 0;
}

QDF_STATUS mlme_ext_vap_recover(struct ieee80211vap *vap)
{
    struct vdev_response_timer *vdev_rsp;

    if (!vap)
	    return QDF_STATUS_E_FAILURE;

    vdev_rsp = &vap->vdev_mlme->vdev_rt;
    if (!vdev_rsp)
	    return QDF_STATUS_E_FAILURE;

    mlme_debug("Vap recovery: %lu", vdev_rsp->rsp_status);
    if (qdf_atomic_test_and_clear_bit(START_RESPONSE_BIT,
                                      &vdev_rsp->rsp_status)) {
        qdf_timer_stop(&vdev_rsp->rsp_timer);
        wlan_objmgr_vdev_release_ref(vap->vdev_obj,
                                     WLAN_VDEV_TARGET_IF_ID);
    }

    if (qdf_atomic_test_and_clear_bit(RESTART_RESPONSE_BIT,
                                      &vdev_rsp->rsp_status)) {
        qdf_timer_stop(&vdev_rsp->rsp_timer);
        wlan_objmgr_vdev_release_ref(vap->vdev_obj,
                                     WLAN_VDEV_TARGET_IF_ID);
    }

    if (qdf_atomic_test_and_clear_bit(STOP_RESPONSE_BIT,
                                      &vdev_rsp->rsp_status)) {
        qdf_timer_stop(&vdev_rsp->rsp_timer);
        wlan_objmgr_vdev_release_ref(vap->vdev_obj,
                                     WLAN_VDEV_TARGET_IF_ID);
    }

    if (qdf_atomic_test_and_clear_bit(DELETE_RESPONSE_BIT,
                                      &vdev_rsp->rsp_status)) {
        qdf_timer_stop(&vdev_rsp->rsp_timer);
        wlan_objmgr_vdev_release_ref(vap->vdev_obj,
                                     WLAN_VDEV_TARGET_IF_ID);
    }

    return QDF_STATUS_SUCCESS;
}

EXPORT_SYMBOL(mlme_ext_vap_recover);

static void mlme_ext_vap_reset_monitor_mode(struct wlan_objmgr_pdev *pdev,
                                            void *obj, void *arg)
{
   struct wlan_objmgr_vdev *vdev = obj;
   struct wlan_objmgr_psoc *psoc;
   enum QDF_OPMODE opmode;
   struct cdp_soc_t *soc_txrx_handle;
   struct cdp_pdev *pdev_txrx_handle;
   struct ieee80211vap *vap = NULL;

   psoc = wlan_pdev_get_psoc(pdev);
   if (psoc == NULL) {
       mlme_err("psoc is null");
       return;
   }

   vap = wlan_vdev_get_mlme_ext_obj(vdev);
   if (vap == NULL) {
       mlme_err("Legacy vap is null");
       return;
   }

   soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
   pdev_txrx_handle = wlan_pdev_get_dp_handle(pdev);
   if (!soc_txrx_handle || !pdev_txrx_handle) {
       mlme_err("DP handle null");
       return;
   }

   opmode = wlan_vdev_mlme_get_opmode(vdev);
   if (opmode == QDF_SAP_MODE) {
       if (vap->iv_special_vap_mode && !vap->iv_special_vap_is_monitor) {
           if (soc_txrx_handle && pdev_txrx_handle)
               cdp_reset_monitor_mode(soc_txrx_handle,
                                      (struct cdp_pdev*)pdev_txrx_handle);
       }
   } else if (opmode == QDF_MONITOR_MODE) {
       if (soc_txrx_handle && pdev_txrx_handle)
           cdp_reset_monitor_mode(soc_txrx_handle,
                                  (struct cdp_pdev*)pdev_txrx_handle);
   }
}

QDF_STATUS mlme_ext_multi_vdev_restart(
                                    struct ieee80211com *ic,
                                    uint32_t *vdev_ids, uint32_t num_vdevs)
{
   uint32_t disable_hw_ack;
   struct mlme_channel_param chan;
   struct wlan_objmgr_pdev *pdev;
   struct ieee80211_ath_channel *curchan;

   pdev = ic->ic_pdev_obj;
   if (pdev == NULL) {
       mlme_err("pdev is null");
       return QDF_STATUS_E_FAILURE;
   }

   curchan = ic->ic_curchan;
   if (curchan == NULL) {
       mlme_err("channel struct is null");
       return QDF_STATUS_E_FAILURE;
   }

   /* Update vdev restart related param before issuing restart command */
   mlme_ext_update_multi_vdev_restart_param(ic, vdev_ids, num_vdevs,
                                            FALSE, FALSE);

   /* Issue multiple vdev restart command after beacon update for all VAPs
    * through multiple vdev restart request WMI command to FW when CSA
    * switch count has reached 0.
    */
   if (wlan_pdev_nif_feat_cap_get(pdev, WLAN_PDEV_F_MULTIVDEV_RESTART)) {
       disable_hw_ack = (IEEE80211_IS_CHAN_DFS(curchan) ||
                        ((IEEE80211_IS_CHAN_11AC_VHT80_80(curchan) ||
                          IEEE80211_IS_CHAN_11AC_VHT160(curchan) ||
                          IEEE80211_IS_CHAN_11AXA_HE80_80(curchan) ||
                          IEEE80211_IS_CHAN_11AXA_HE160(curchan)) &&
                          IEEE80211_IS_CHAN_DFS_CFREQ2(curchan)));

       wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_VDEV_OP,
                                         mlme_ext_vap_reset_monitor_mode,
                                         NULL, 0, WLAN_MLME_SB_ID);
       mlme_ext_update_channel_param(&chan, ic);
       tgt_dfs_set_current_channel(pdev,
                                   curchan->ic_freq,
                                   curchan->ic_flags,
                                   curchan->ic_flagext,
                                   curchan->ic_ieee,
                                   curchan->ic_vhtop_ch_freq_seg1,
                                   curchan->ic_vhtop_ch_freq_seg2);

       if (vdev_mgr_multiple_restart_send(pdev, &chan, disable_hw_ack,
                                         vdev_ids, num_vdevs)) {
           /* Update vdev restart related param in case of failure */
           mlme_ext_update_multi_vdev_restart_param(ic, vdev_ids, num_vdevs,
                                                   TRUE, FALSE);
           return QDF_STATUS_E_FAILURE;
       }
   }

   /* The channel configured in target is not same always with the
    * vap desired channel due to 20/40 coexistence scenarios, so,
    * channel is saved to configure on VDEV START RESP
    */
   mlme_ext_update_multi_vdev_restart_param(ic, vdev_ids, num_vdevs,
                                            FALSE, TRUE);
   return QDF_STATUS_SUCCESS;
}

void mlme_ext_update_multi_vdev_restart_param(struct ieee80211com *ic,
                                              uint32_t *vdev_ids,
                                              uint32_t num_vdevs,
                                              bool reset,
                                              bool restart_success)
{
    int i = 0;
    struct wlan_objmgr_vdev *vdev;
    struct wlan_objmgr_pdev *pdev;
    struct ieee80211vap *vap = NULL;

    pdev = ic->ic_pdev_obj;
    for (i = 0; i < num_vdevs; i++) {
         vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_ids[i],
                                                     WLAN_MLME_SB_ID);
         if (vdev == NULL)
             continue;

         vap = wlan_vdev_get_mlme_ext_obj(vdev);
         if (vap == NULL) {
             wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
             continue;
         }

         if (!reset) {
             qdf_atomic_set(&(vap->iv_is_start_sent), 1);
         } else {
             qdf_atomic_set(&(vap->iv_is_start_sent), 0);
         }

         ic->ic_update_restart_param(vap, reset, restart_success);
         wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
    }
}

int mlme_ext_update_channel_param(struct mlme_channel_param *ch_param,
                                  struct ieee80211com *ic)
{
    struct ieee80211_ath_channel *c = NULL;

    c = ic->ic_curchan;
    if (!c)
        return -1;

    ch_param->mhz = c->ic_freq;
    ch_param->cfreq1 = c->ic_freq;
    ch_param->cfreq2 = 0;

    if (IEEE80211_IS_CHAN_PASSIVE(c)) {
        ch_param->is_chan_passive = TRUE;
    }
    if (IEEE80211_IS_CHAN_DFS(c))
        ch_param->dfs_set = TRUE;

    if (IEEE80211_IS_CHAN_11AXA_HE80_80(c)) {
        ch_param->allow_vht = TRUE;
        ch_param->allow_ht = TRUE;
        ch_param->cfreq1 = ieee80211_ieee2mhz(ic, c->ic_vhtop_ch_freq_seg1,c->ic_flags);
        ch_param->cfreq2 = ieee80211_ieee2mhz(ic, c->ic_vhtop_ch_freq_seg2,c->ic_flags);
    } else if (IEEE80211_IS_CHAN_11AXA_HE160(c)) {
        ch_param->allow_vht = TRUE;
        ch_param->allow_ht = TRUE;
        ch_param->cfreq1 = ieee80211_ieee2mhz(ic, c->ic_vhtop_ch_freq_seg1,c->ic_flags);
        ch_param->cfreq2 = ieee80211_ieee2mhz(ic, c->ic_vhtop_ch_freq_seg2,c->ic_flags);
    } else if (IEEE80211_IS_CHAN_11AXA_HE80(c)) {
        ch_param->allow_vht = TRUE;
        ch_param->allow_ht = TRUE;
        ch_param->cfreq1 = ieee80211_ieee2mhz(ic, c->ic_vhtop_ch_freq_seg1,c->ic_flags);
        ch_param->cfreq2 = 0;
    } else if (IEEE80211_IS_CHAN_11AXA_HE40PLUS(c) || IEEE80211_IS_CHAN_11AXA_HE40MINUS(c)) {
        ch_param->allow_vht = TRUE;
        ch_param->allow_ht = TRUE;
        ch_param->cfreq1 = ieee80211_ieee2mhz(ic, c->ic_vhtop_ch_freq_seg1,c->ic_flags);
        ch_param->cfreq2 = 0;
    } else if (IEEE80211_IS_CHAN_11AXA_HE20(c)) {
        ch_param->allow_vht = TRUE;
        ch_param->allow_ht = TRUE;
        ch_param->cfreq1 = ieee80211_ieee2mhz(ic, c->ic_vhtop_ch_freq_seg1,c->ic_flags);
        ch_param->cfreq2 = 0;
    } else if (IEEE80211_IS_CHAN_11AC_VHT80_80(c)) {
        ch_param->allow_vht = TRUE;
        ch_param->allow_ht = TRUE;
        ch_param->cfreq1 = ieee80211_ieee2mhz(ic, c->ic_vhtop_ch_freq_seg1,c->ic_flags);
        ch_param->cfreq2 = ieee80211_ieee2mhz(ic, c->ic_vhtop_ch_freq_seg2,c->ic_flags);
    } else  if (IEEE80211_IS_CHAN_11AC_VHT160(c)) {
        ch_param->allow_vht = TRUE;
        ch_param->allow_ht = TRUE;
        ch_param->cfreq1 = ieee80211_ieee2mhz(ic, c->ic_vhtop_ch_freq_seg1,c->ic_flags);
        ch_param->cfreq2 = ieee80211_ieee2mhz(ic, c->ic_vhtop_ch_freq_seg2,c->ic_flags);
    } else if (IEEE80211_IS_CHAN_11AC_VHT80(c)) {
        ch_param->allow_vht = TRUE;
        ch_param->allow_ht = TRUE;
        ch_param->cfreq1 = ieee80211_get_chan_centre_freq(ic,c);
        ch_param->cfreq2 = 0;
    } else if (IEEE80211_IS_CHAN_11AC_VHT40PLUS(c) || IEEE80211_IS_CHAN_11AC_VHT40MINUS(c)) {
        ch_param->allow_vht = TRUE;
        ch_param->allow_ht = TRUE;
        ch_param->cfreq1 = ieee80211_get_chan_centre_freq(ic,c);
        ch_param->cfreq2 = 0;
    } else if (IEEE80211_IS_CHAN_11AC_VHT20(c)) {
        ch_param->allow_vht = TRUE;
        ch_param->allow_ht = TRUE;
        ch_param->cfreq1 = ieee80211_get_chan_centre_freq(ic,c);
        ch_param->cfreq2 = 0;
    } else if (IEEE80211_IS_CHAN_11NA_HT40PLUS(c) || IEEE80211_IS_CHAN_11NA_HT40MINUS(c)) {
        ch_param->allow_ht = TRUE;
        ch_param->cfreq1 = ieee80211_get_chan_centre_freq(ic,c);
        ch_param->cfreq2 = 0;
    } else if (IEEE80211_IS_CHAN_11NA_HT20(c)) {
        ch_param->allow_ht = TRUE;
        ch_param->cfreq1 = ieee80211_get_chan_centre_freq(ic,c);
        ch_param->cfreq2 = 0;
    } else if (IEEE80211_IS_CHAN_11AXG_HE40PLUS(c) || IEEE80211_IS_CHAN_11AXG_HE40MINUS(c)) {
        ch_param->allow_vht = TRUE;
        ch_param->allow_ht = TRUE;
        ch_param->cfreq1 = ieee80211_get_chan_centre_freq(ic,c);
        ch_param->cfreq2 = 0;
    } else if (IEEE80211_IS_CHAN_11AXG_HE20(c)) {
        ch_param->allow_vht = TRUE;
        ch_param->allow_ht = TRUE;
        ch_param->cfreq1 = ieee80211_get_chan_centre_freq(ic,c);
        ch_param->cfreq2 = 0;
    } else if (IEEE80211_IS_CHAN_11NG_HT40PLUS(c) || IEEE80211_IS_CHAN_11NG_HT40MINUS(c)) {
        ch_param->allow_ht = TRUE;
        ch_param->cfreq1 = ieee80211_get_chan_centre_freq(ic,c);
        ch_param->cfreq2 = 0;
    } else if (IEEE80211_IS_CHAN_11NG_HT20(c)) {
        ch_param->allow_ht = TRUE;
        ch_param->cfreq1 = ieee80211_get_chan_centre_freq(ic,c);
        ch_param->cfreq2 = 0;
    }

    ic->ic_update_phy_mode(ch_param, ic);

    if (IEEE80211_IS_CHAN_HALF(c))
        ch_param->half_rate = TRUE;
    if (IEEE80211_IS_CHAN_QUARTER(c))
        ch_param->quarter_rate = TRUE;

    /* Also fill in power information */
    ch_param->minpower = c->ic_minpower;
    ch_param->maxpower = c->ic_maxpower;
    ch_param->maxregpower = c->ic_maxregpower;
    ch_param->antennamax = c->ic_antennamax;
    ch_param->reg_class_id = c->ic_regClassId;

    return 0;
}

QDF_STATUS mlme_ext_vap_custom_aggr_size_send(
                                        struct vdev_mlme_obj *vdev_mlme,
                                        bool is_amsdu)
{
    return vdev_mgr_set_custom_aggr_size_send(vdev_mlme, is_amsdu);
}

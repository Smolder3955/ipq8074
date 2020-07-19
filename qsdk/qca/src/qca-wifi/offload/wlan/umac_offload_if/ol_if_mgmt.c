/*
 * Copyright (c) 2015, 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2015 Qualcomm Atheros, Inc.
 */

/*
 * Copyright (c) 2011 Atheros Communications Inc.
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
 * UMAC management specific offload interface functions - for power and performance offload model
 */
#include "ol_if_athvar.h"
#include "ol_if_ath_api.h"
#include "qdf_mem.h"
#include <init_deinit_lmac.h>
#include <cdp_txrx_ctrl.h>
#include <wdi_event_api.h>
#include <enet.h>
#include "ol_helper.h"
#include "ol_if_txrx_handles.h"
#include "a_debug.h"

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include <osif_private.h>
#include <osif_nss_wifiol_if.h>
#include <osif_nss_wifiol_vdev_if.h>
#endif
#include <wlan_lmac_if_api.h>
#include <wlan_osif_priv.h>
#include <ieee80211_cfg80211.h>

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
#include "wlan_crypto_global_def.h"
#include "wlan_crypto_global_api.h"
#endif
#include <wlan_offchan_txrx_api.h>
#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_utils_api.h>
#endif

#ifdef QCA_SUPPORT_SON
#include <wlan_son_pub.h>
#endif

#include <wlan_vdev_mgr_ucfg_api.h>
#include <wlan_vdev_mgr_utils_api.h>

#if ATH_PERF_PWR_OFFLOAD
#define NDIS_SOFTAP_SSID  "NDISTEST_SOFTAP"
#define NDIS_SOFTAP_SSID_LEN  15
#define OFFCHAN_EXT_TID_NONPAUSE    19
#define MGMT_TARGET_SUPPORTED_MAX_WMI   64
#define MGMT_TARGET_SUPPORTED_MAX_HTT   32
#define WMI_MGMT_DESC_POOL_SIZE 512
#define OL_TXRX_MGMT_TYPE_BASE htt_cmn_pkt_num_types
#define OL_TXRX_MGMT_NUM_TYPES 8

static u_int32_t
ol_ath_net80211_rate_node_update(struct ieee80211com *ic,
                                 struct ieee80211_node *ni,
                                 int isnew);
int wmi_mgmt_desc_pool_init(struct ieee80211com *ic, uint32_t pool_size);
void wmi_mgmt_desc_pool_deinit(struct ieee80211com *ic);
struct wmi_mgmt_desc_t *wmi_mgmt_desc_get(struct ieee80211com *ic);
void wmi_mgmt_desc_put(struct ieee80211com *ic, struct wmi_mgmt_desc_t *wmi_mgmt_desc);
void ol_ath_update_dp_stats(void *soc, enum WDI_EVENT event, void *stats, uint16_t id, uint32_t type);
void ieee80211_csa_interop_update(void *pdev, enum WDI_EVENT event, void *stats, uint16_t id, uint32_t type);

/*
 *  WMI API for 802.11 management frame processing
 */
static uint32_t
ol_map_phymode_to_wmimode(struct ieee80211_node *ni)
{
    uint32_t phymode;
    struct ol_ath_softc_net80211 *scn = NULL;
    bool is_2gvht_en;
    enum wmi_target_type wmi_tgt_type;


    is_2gvht_en = ieee80211_vap_256qam_is_set(ni->ni_vap) &&
                  (ni->ni_flags & IEEE80211_NODE_VHT);
    phymode = ni->ni_phymode;

    qdf_assert_always(NULL != ni->ni_ic);
    scn = OL_ATH_SOFTC_NET80211(ni->ni_ic);
    qdf_assert_always(NULL != scn);

    if (ol_ath_get_wmi_target_type(scn->soc, &wmi_tgt_type) != QDF_STATUS_SUCCESS) {
        qdf_info("Not able to get wmi target type");
        return WMI_HOST_MODE_UNKNOWN;
    }

    /* For non tlv chipsets there is a difference in behaviour b/w
     * phymode send on vdev start and peer assoc. Following table
     * captures this info.
     *  ____________________________________________________________
     * |AP(vht_11ng) |STA (vht_11ng) | Mode to FW(peer assoc)       |
     * |_____________|_______________|______________________________|
     * |1            |   1           | WMI_HOST_MODE_11AC_VHT20     |
     * |1            |   0           | WMI_HOST_MODE_11NG_HT20      |
     * |0            |   0           | WMI_HOST_MODE_11NG_HT20      |
     * |0            |   1           | WMI_HOST_MODE_11NG_HT20      |
     * |_____________|_______________|______________________________|
     *
     *  ____________________________________________________________
     * |AP(vht_11ng) |STA (vht_11ng) | Mode to FW(vdev start)       |
     * |_____________|_______________|______________________________|
     * |1            |   1           | WMI_HOST_MODE_11NG_HT20      |
     * |1            |   0           | WMI_HOST_MODE_11NG_HT20      |
     * |0            |   0           | WMI_HOST_MODE_11NG_HT20      |
     * |0            |   1           | WMI_HOST_MODE_11NG_HT20      |
     * |_____________|_______________|______________________________|
     */
     if (wmi_tgt_type == WMI_NON_TLV_TARGET && is_2gvht_en) {
        switch(phymode) {
            case IEEE80211_MODE_11NG_HT20:
                return WMI_HOST_MODE_11AC_VHT20;
            break;
            case IEEE80211_MODE_11NG_HT40PLUS:
            case IEEE80211_MODE_11NG_HT40MINUS:
            case IEEE80211_MODE_11NG_HT40:
                return WMI_HOST_MODE_11AC_VHT40;
            break;
            default:
            break;
        }
    }

    return(ol_get_phymode_info(scn, phymode, is_2gvht_en));
}

/*
 *  WMI API for 802.11 management frame processing
 */
int
ol_ath_send_peer_assoc(struct ol_ath_softc_net80211 *scn, struct ieee80211com *ic,
                            struct ieee80211_node *ni, int isnew )
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct peer_assoc_params param;
    u_int8_t rx_nss160   = 0;
    u_int8_t rx_nss80p80 = 0;
    bool same_nss_for_all_bw = false;
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_peer_handle an_txrx_handle;
    struct common_wmi_handle *pdev_wmi_handle;
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
    u_int32_t authmode;
#endif
    struct wlan_objmgr_peer *peer = ni->peer_obj;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);

    soc_txrx_handle = wlan_psoc_get_dp_handle(wlan_pdev_get_psoc(ic->ic_pdev_obj));
    an_txrx_handle = wlan_peer_get_dp_handle(ni->peer_obj);

    IEEE80211_ADDR_COPY(param.peer_mac, ni->ni_macaddr);
    param.vdev_id = (OL_ATH_VAP_NET80211(vap))->av_if_id;
    param.peer_new_assoc = isnew;
    param.peer_associd = IEEE80211_AID(ni->ni_associd);
    param.peer_bw_rxnss_override = 0;
    vap->iv_sta_negotiated_ch_width = ni->ni_chwidth;

    if(ieee80211_is_pmf_enabled(vap, ni)) {
        param.is_pmf_enabled = TRUE;
    }

    /*
     * Do not enable HT/VHT if WMM/wme is disabled for vap.
     */
    if (ieee80211_vap_wme_is_set(vap)) {
        param.is_wme_set = TRUE;

        if ((ni->ni_flags & IEEE80211_NODE_QOS) || (ni->ni_flags & IEEE80211_NODE_HT) ||
                (ni->ni_flags & IEEE80211_NODE_VHT) || (ni->ni_ext_flags & IEEE80211_NODE_HE)) {
            param.qos_flag = TRUE;
        }
        if (ni->ni_flags & IEEE80211_NODE_UAPSD ) {
            param.apsd_flag = TRUE;
        }
        if (ni->ni_flags & IEEE80211_NODE_HT) {
            param.ht_flag = TRUE;
        }
        if ((ni->ni_chwidth == IEEE80211_CWM_WIDTH40) || (ni->ni_chwidth == IEEE80211_CWM_WIDTH80) ||
                (ni->ni_chwidth == IEEE80211_CWM_WIDTH160)){
            param.bw_40 = TRUE;
        }
        if ((ni->ni_chwidth == IEEE80211_CWM_WIDTH80) || (ni->ni_chwidth == IEEE80211_CWM_WIDTH160)) {
            param.bw_80 = TRUE;
        }
        if ((ni->ni_chwidth == IEEE80211_CWM_WIDTH160)) {
            param.bw_160 = TRUE;
        }

        /* Typically if STBC is enabled for VHT it should be enabled for HT as well */
        if ((ni->ni_htcap & IEEE80211_HTCAP_C_RXSTBC) && (ni->ni_vhtcap & IEEE80211_VHTCAP_RX_STBC)) {
            param.stbc_flag = TRUE;
        }

        /* Typically if LDPC is enabled for VHT it should be enabled for HT as well */
        if ((ni->ni_htcap & IEEE80211_HTCAP_C_ADVCODING) && (ni->ni_vhtcap & IEEE80211_VHTCAP_RX_LDPC)) {
            param.ldpc_flag = TRUE;
        }

        if ((ni->ni_htcap & IEEE80211_HTCAP_C_SM_MASK) == IEEE80211_HTCAP_C_SMPOWERSAVE_STATIC) {
            param.static_mimops_flag = TRUE;
        }
        if ((ni->ni_htcap & IEEE80211_HTCAP_C_SM_MASK) == IEEE80211_HTCAP_C_SMPOWERSAVE_DYNAMIC) {
            param.dynamic_mimops_flag = TRUE;
        }
        if ((ni->ni_htcap & IEEE80211_HTCAP_C_SM_MASK) == IEEE80211_HTCAP_C_SMPOWERSAVE_DISABLED) {
            param.spatial_mux_flag = TRUE;
        }
        if (ni->ni_flags & IEEE80211_NODE_VHT) {
            param.vht_flag = TRUE;
        }
        if ((ni->ni_flags & IEEE80211_NODE_VHT) &&
             (ieee80211_vap_256qam_is_set(ni->ni_vap)) &&
              IEEE80211_IS_CHAN_11NG(ic->ic_curchan)) {
            param.vht_ng_flag = TRUE;
        }
        if (ni->ni_ext_flags & IEEE80211_NODE_HE) {
            param.he_flag = TRUE;
        }
    }
    if (ni->ni_ext_flags & IEEE80211_NODE_TWT_REQUESTER)
        param.twt_requester = true;

    if (ni->ni_ext_flags & IEEE80211_NODE_TWT_RESPONDER)
        param.twt_responder = true;

    /*
     * Suppress authorization for all AUTH modes that need 4-way handshake (during re-association).
     * Authorization will be done for these modes on key installation.
     */
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
        authmode = wlan_crypto_get_peer_param(ni->peer_obj, WLAN_CRYPTO_PARAM_AUTH_MODE);

        if((ni->ni_authmode != IEEE80211_AUTH_8021X) &&
	   (wlan_crypto_vdev_has_auth_mode(vap->vdev_obj, authmode)) &&
           (!(authmode & ((uint32_t)((1 << WLAN_CRYPTO_AUTH_WPA) | (1 << WLAN_CRYPTO_AUTH_RSNA)
                                     | (1 << WLAN_CRYPTO_AUTH_8021X) | (1 << WLAN_CRYPTO_AUTH_WAPI)))) ||
            IEEE80211_VAP_IS_SAFEMODE_ENABLED(ni->ni_vap))) {
#else
    if ((ni->ni_flags & IEEE80211_NODE_AUTH) &&
        (RSN_AUTH_MATCH(&ni->ni_rsn, &vap->iv_rsn)) &&
        ((!(RSN_AUTH_IS_WPA(&ni->ni_rsn) ||
            RSN_AUTH_IS_WPA2(&ni->ni_rsn) ||
            RSN_AUTH_IS_8021X(&ni->ni_rsn)||
            RSN_AUTH_IS_WAI(&ni->ni_rsn))) ||
            IEEE80211_VAP_IS_SAFEMODE_ENABLED(ni->ni_vap))) {
#endif
        param.auth_flag = TRUE;
        cdp_peer_authorize(soc_txrx_handle, (void *)an_txrx_handle, 1);
    } else {
        cdp_peer_authorize(soc_txrx_handle, (void *)an_txrx_handle, 0);
    }

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
    if(authmode & ((uint32_t)((1 << WLAN_CRYPTO_AUTH_WPA)
                             | (1 << WLAN_CRYPTO_AUTH_RSNA)
                             | (1 << WLAN_CRYPTO_AUTH_WAPI)))) {
#else
    if (RSN_AUTH_IS_WPA(&ni->ni_rsn) ||
            RSN_AUTH_IS_WPA2(&ni->ni_rsn) ||
            RSN_AUTH_IS_WAI(&ni->ni_rsn)) {
#endif
		/*
		 *  In WHCK NDIS Test, 4-way handshake is not mandatory in WPA TKIP/CCMP mode.
		 *  Check the SSID, if the SSID is NDIS softAP ssid, the function will unset WMI_PEER_NEED_PTK_4_WAY flag.
		 *  This will bypass the check and set the ALLOW_DATA in fw to let the data packet sent out.
		 */
		if (OS_MEMCMP(ni->ni_essid, NDIS_SOFTAP_SSID, NDIS_SOFTAP_SSID_LEN) == 0) {
            param.need_ptk_4_way = FALSE;
		} else {
            param.need_ptk_4_way = TRUE;
		}
    }
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
    if(authmode & ((uint32_t)(1 << WLAN_CRYPTO_AUTH_WPA))){
#else
    if (RSN_AUTH_IS_WPA(&ni->ni_rsn)) {
#endif
        param.need_gtk_2_way = TRUE;
    }
    /* safe mode bypass the 4-way handshake */
    if (IEEE80211_VAP_IS_SAFEMODE_ENABLED(ni->ni_vap)) {
        param.safe_mode_enabled = TRUE;
    }
      /* Disable AMSDU for station transmit, if user configures it */
    if ((vap->iv_opmode == IEEE80211_M_STA) && (ic->ic_sta_vap_amsdu_disable) &&
        !(ni->ni_flags & IEEE80211_NODE_VHT)) {
        param.amsdu_disable = TRUE;
    }
      /* Disable AMSDU for AP transmit to 11n Stations, if user configures it */
    if ((vap->iv_opmode == IEEE80211_M_HOSTAP) && (vap->iv_disable_ht_tx_amsdu) &&
        (ni->ni_flags & IEEE80211_NODE_HT) && (!(IEEE80211_NODE_USE_VHT(ni))) ) {
        param.amsdu_disable = TRUE;
    }
    param.peer_caps = ni->ni_capinfo;
    param.peer_listen_intval = ni->ni_lintval;
    param.peer_ht_caps = ni->ni_htcap;
    param.peer_max_mpdu = ni->ni_maxampdu;
    param.peer_mpdu_density = ni->ni_mpdudensity;
    param.peer_vht_caps = ni->ni_vhtcap;
#if WAR_DISABLE_MU_2x2_STA
    /* WAR to disable MU-MIMO for 2x2 STAs */
    if (((ni->ni_vhtcap & IEEE80211_VHTCAP_SOUND_DIM) >> IEEE80211_VHTCAP_SOUND_DIM_S) == 1) {
        param.peer_vht_caps &= ~IEEE80211_VHTCAP_SOUND_DIM;
        param.peer_vht_caps &= ~IEEE80211_VHTCAP_MU_BFORMEE;
    }
#endif

    /* Update peer rate information */
    param.peer_rate_caps = ol_ath_net80211_rate_node_update(ic, ni, isnew);
    param.peer_legacy_rates.num_rates = ni->ni_rates.rs_nrates;
    /* NOTE: cmd->peer_legacy_rates.rates is of type A_UINT32 */
    /* ni->ni_rates.rs_rates is of type u_int8_t */
    /**
     * for cmd->peer_legacy_rates.rates:
     * rates (each 8bit value) packed into a 32 bit word.
     * the rates are filled from least significant byte to most
     * significant byte.
     */
    OS_MEMCPY( param.peer_legacy_rates.rates, ni->ni_rates.rs_rates, ni->ni_rates.rs_nrates);

    param.peer_ht_rates.num_rates = ni->ni_htrates.rs_nrates;
    OS_MEMCPY( param.peer_ht_rates.rates, ni->ni_htrates.rs_rates, ni->ni_htrates.rs_nrates);

    param.peer_nss = (ni->ni_streams==0)?1:ni->ni_streams;

    /* set the default vht max rate info */
    if (ni->ni_vhtcap) {
        if((ni->ni_maxrate_vht != 0xff) && (!isnew)) {
	        u_int8_t user_max_nss = (ni->ni_maxrate_vht >> VHT_MAXRATE_IDX_SHIFT) & 0XF;
            param.peer_nss = (param.peer_nss > user_max_nss) ? user_max_nss : param.peer_nss;
        } else {
            ni->ni_maxrate_vht = 0xff;
        }
    } else {
        ni->ni_maxrate_vht = 0;
    }

    if (ni->ni_vhtcap) {
        param.vht_capable = TRUE;
        param.rx_max_rate = ni->ni_rx_max_rate;
        param.rx_mcs_set = ni->ni_rx_vhtrates;
        param.tx_max_rate = ni->ni_tx_max_rate;
        param.tx_mcs_set = ni->ni_tx_vhtrates;
        /* Update Peer support for VHT MCS10/11 */
        if(ic->ic_he_target) {
            param.tx_mcs_set |= IEEE80211_VHT_MCS10_11_SUPP;
            if(ni->ni_higher_vhtmcs_supp) {
                param.tx_mcs_set |= ((ni->ni_higher_vhtmcs_supp <<
                                        IEEE80211_VHT_HIGHER_MCS_S) &
                                        IEEE80211_VHT_HIGHER_MCS_MAP);
            }
            else if(ni->ni_vap->iv_vht_mcs10_11_nq2q_peer_supp &&
                                ni->ni_vap->iv_vht_mcs10_11_supp) {
                param.tx_mcs_set |= ((ic->ic_vhtcap_max_mcs.tx_mcs_set.higher_mcs_supp <<
                                        IEEE80211_VHT_HIGHER_MCS_S) &
                                        IEEE80211_VHT_HIGHER_MCS_MAP);
            }
        }
        param.tx_max_mcs_nss = ni->ni_maxrate_vht;
    }

    /* In very exceptional  conditions it is observed  that
     * firmware was receiving phymode as 0 for peer from host, and resulting in Target Assert
     * Changing the phymode to desired mode
     */
    if(ni->ni_phymode == 0) {
        ni->ni_phymode = vap->iv_des_mode;
    }
#if SUPPORT_11AX_D3
    param.peer_he_ops = (ni->ni_he.heop_param |
             (ni->ni_he.heop_bsscolor_info << HEOP_PARAM_S));
#else
    param.peer_he_ops = ni->ni_he.heop_param;
#endif
    qdf_mem_copy(&param.peer_he_cap_macinfo, &(ni->ni_he.hecap_macinfo),
                             qdf_min(sizeof(param.peer_he_cap_macinfo),
                                 sizeof(ni->ni_he.hecap_macinfo)));
    qdf_mem_copy(&param.peer_he_cap_phyinfo, &(ni->ni_he.hecap_phyinfo),
                             sizeof(param.peer_he_cap_phyinfo));
    qdf_mem_copy(&param.peer_ppet, &(ni->ni_he.hecap_ppet),
                             sizeof(param.peer_ppet));

    /* If peer is NAWDS client, do not advertize dl mu cap */
    if (ni->ni_flags & IEEE80211_NODE_NAWDS) {
        ni->ni_he.hecap_info_internal |=
        IEEE80211_HE_DL_MU_SUPPORT_DISABLE << IEEE80211_HE_DL_OFDMA_SUPP_S;
        ni->ni_he.hecap_info_internal |=
        IEEE80211_HE_DL_MU_SUPPORT_DISABLE << IEEE80211_HE_DL_MUMIMO_SUPP_S;
    }
    param.peer_he_cap_info_internal = ni->ni_he.hecap_info_internal;

    if (IEEE80211_IS_HECAP_MACINFO(ni->ni_he.hecap_macinfo)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
            "%s HE MAC Capabilities info: All bits are 0\n", __func__);
    }

    /*11AX TODO (Phase II) - Add further checks for population NSS & MCS */
    if ((ni->ni_phymode >= IEEE80211_MODE_11AXA_HE20) &&
           (ni->ni_phymode <= IEEE80211_MODE_11AXA_HE80_80)) {
        uint32_t *peer_he_rx_mcs_set = param.peer_he_rx_mcs_set;
        uint32_t *peer_he_tx_mcs_set = param.peer_he_tx_mcs_set;
        uint16_t *ni_he_rx_mcs_set   = ni->ni_he.hecap_rxmcsnssmap;
        uint16_t *ni_he_tx_mcs_set   = ni->ni_he.hecap_txmcsnssmap;

        param.peer_he_mcs_count = 0;

        switch(ni->ni_phymode) {
            case IEEE80211_MODE_11AXA_HE80_80:

                peer_he_rx_mcs_set[HECAP_TXRX_MCS_NSS_IDX_80_80] =
                        ni_he_rx_mcs_set[HECAP_TXRX_MCS_NSS_IDX_80_80];
                peer_he_tx_mcs_set[HECAP_TXRX_MCS_NSS_IDX_80_80] =
                        ni_he_tx_mcs_set[HECAP_TXRX_MCS_NSS_IDX_80_80];
                param.peer_he_mcs_count++;
                /* fall through */

            case IEEE80211_MODE_11AXA_HE160:

                peer_he_rx_mcs_set[HECAP_TXRX_MCS_NSS_IDX_160] =
                        ni_he_rx_mcs_set[HECAP_TXRX_MCS_NSS_IDX_160];
                peer_he_tx_mcs_set[HECAP_TXRX_MCS_NSS_IDX_160] =
                        ni_he_tx_mcs_set[HECAP_TXRX_MCS_NSS_IDX_160];
                param.peer_he_mcs_count++;
                /* fall through */

            default:

                peer_he_rx_mcs_set[HECAP_TXRX_MCS_NSS_IDX_80] =
                        ni_he_rx_mcs_set[HECAP_TXRX_MCS_NSS_IDX_80];
                peer_he_tx_mcs_set[HECAP_TXRX_MCS_NSS_IDX_80] =
                        ni_he_tx_mcs_set[HECAP_TXRX_MCS_NSS_IDX_80];
                param.peer_he_mcs_count++;
                /* fall through */

                break;
        }
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
            "%s VHT Caps=%x HT Caps=%x RX MCS=%x TX MCS=%x \n",
            __func__, param.peer_vht_caps, param.peer_ht_caps,
            param.rx_mcs_set ,param.tx_mcs_set);

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
            "%s 11ax Param OPS=%x  MACInfo= %x MCS_cnt=%x"
            " TX_MCS[80]=%x TX_MCS[160]=%x TX_MCS[80_80]=%x"
            " RX_MCS[80]=%x RX_MCS[160]=%x RX_MCS[80_80]=%x"
            " Phyinfo[0]=%x Phyinfo[1]=%x Phyinfo[2]=%x"
            " Q2Q Feature Support=%x \n",
            " Peer NSS = %d\n",
            __func__, param.peer_he_ops,
            param.peer_he_cap_macinfo, param.peer_he_mcs_count,
            param.peer_he_tx_mcs_set[HECAP_TXRX_MCS_NSS_IDX_80],
            param.peer_he_tx_mcs_set[HECAP_TXRX_MCS_NSS_IDX_160],
            param.peer_he_tx_mcs_set[HECAP_TXRX_MCS_NSS_IDX_80_80],
            param.peer_he_rx_mcs_set[HECAP_TXRX_MCS_NSS_IDX_80],
            param.peer_he_rx_mcs_set[HECAP_TXRX_MCS_NSS_IDX_160],
            param.peer_he_rx_mcs_set[HECAP_TXRX_MCS_NSS_IDX_80_80],
            param.peer_he_cap_phyinfo[IC_HECAP_PHYDWORD_IDX0],
            param.peer_he_cap_phyinfo[IC_HECAP_PHYDWORD_IDX1],
            param.peer_he_cap_phyinfo[IC_HECAP_PHYDWORD_IDX2],
            param.peer_he_cap_info_internal, param.peer_nss);

    param.peer_phymode = ol_map_phymode_to_wmimode(ni);

    if (peer->obj_state == WLAN_OBJ_STATE_LOGICALLY_DELETED) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
                "%s: Error, STA %s is in logically deleted state, return\n",
                __func__, ether_sprintf(ni->ni_macaddr));
        return -1;
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
            "%s ni phymode=%d peer phymode=%d \n",
            __func__, ni->ni_phymode, param.peer_phymode);

    /* Send bandwidth-NSS mapping to FW if we are 160/80+80 MHz capable.
     *
     * Note: At this point, the exact advertisement mechanisms and the degree
     * of flexibility applicable for 802.11ax BW-NSS mapping have not settled.
     * For now, we provision for HE160/80+80 capable chipsets to be able to
     * use host-FW exchange for BW-NSS mapping if required.
     * 11AX TODO: Modify below provision and above comment based on updates in
     * the standard.
     */

    if (ic->ic_modecaps &
            ((1 << IEEE80211_MODE_11AC_VHT160) |
             (1 << IEEE80211_MODE_11AC_VHT80_80) |
             (1 << IEEE80211_MODE_11AXA_HE160) |
             (1ULL << IEEE80211_MODE_11AXA_HE80_80))) {
        if (ni->ni_prop_ie_used || ni->ni_ext_nss_support) {
            rx_nss160 = ni->ni_bw160_nss;
            rx_nss80p80 = ni->ni_bw80p80_nss;
        } else if ((param.peer_phymode == WMI_HOST_MODE_11AC_VHT160) || (param.peer_phymode == WMI_HOST_MODE_11AC_VHT80_80)) {
            if (param.peer_phymode == WMI_HOST_MODE_11AC_VHT80_80) {
                rx_nss80p80 = param.peer_nss;
            }
            rx_nss160 = param.peer_nss;
            same_nss_for_all_bw = true;
        }

        if (!ic->ic_fw_ext_nss_capable) {
            if (rx_nss160) {
                param.peer_bw_rxnss_override = IEEE80211_BW_NSS_FWCONF_160(rx_nss160);
            }
        } else {
            /* Irrespective of whether vap supports EXT NSS or not populate into FW in same manner
             * since rx_nss values are to be populated appropriately into thge local variables by
             * this point
             */
            if (rx_nss160) {
                param.peer_bw_rxnss_override = IEEE80211_BW_NSS_FWCONF_160(rx_nss160);
            }
            if (rx_nss80p80) {
                param.peer_bw_rxnss_override |= IEEE80211_BW_NSS_FWCONF_80_80(rx_nss80p80);
            }
        }

        /* In very exceptional  conditions it is observed  that
         * firmware was receiving bw_rxnss_override as 0 for peer from host, and resulting in Target Assert.
         * Changing the rxnss_override to minimum nss. This is a temporary WAR. Needs to be fixed
         * properly.
         */
        if (((param.peer_phymode == WMI_HOST_MODE_11AC_VHT160) || (param.peer_phymode == WMI_HOST_MODE_11AC_VHT80_80))
               && (!param.peer_bw_rxnss_override) && !(same_nss_for_all_bw)) {
            param.peer_bw_rxnss_override = IEEE80211_BW_NSS_FWCONF_160(1);
        }
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
            "%s wmi_unified_peer_assoc_send for macaddr:%s isnew: %d\n",
            __func__,ether_sprintf(ni->ni_macaddr), isnew);

    return wmi_unified_peer_assoc_send(pdev_wmi_handle, &param);
}

int
ol_ath_send_addba_clearresponse(struct ol_ath_softc_net80211 *scn, struct ieee80211_node *ni)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct addba_clearresponse_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    param.vdev_id = (OL_ATH_VAP_NET80211(vap))->av_if_id;

    /* Send the management frame buffer to the target */
    return wmi_unified_addba_clearresponse_cmd_send(pdev_wmi_handle, ni->ni_macaddr, &param);
}

int
ol_ath_send_addba_send(struct ol_ath_softc_net80211 *scn,
                            struct ieee80211_node *ni,
                            u_int8_t tidno,
                            u_int16_t buffersize)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct addba_send_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    param.vdev_id = (OL_ATH_VAP_NET80211(vap))->av_if_id;
    param.tidno = tidno;
    param.buffersize = buffersize;

    /* Send the management frame buffer to the target */
    return wmi_unified_addba_send_cmd_send(pdev_wmi_handle, ni->ni_macaddr, &param);

}

void
ol_ath_delba_send(struct ol_ath_softc_net80211 *scn,
                    struct ieee80211_node *ni,
                    u_int8_t tidno,
                    u_int8_t initiator,
                    u_int16_t reasoncode)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct delba_send_params param;
    struct common_wmi_handle *pdev_wmi_handle;
    struct ieee80211_action_mgt_args actionargs;
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211_delba_parameterset delbaparams;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    param.vdev_id = (OL_ATH_VAP_NET80211(vap))->av_if_id;
    param.tidno = tidno;
    param.initiator = initiator;
    param.reasoncode = reasoncode;

    if (ic->ic_he_target && initiator == 0) {
        /* In case the BA Session was established by
         * remote,
         *
         * 1. send a delba frame to the remote
         * 2. clear the data structures updated
         *    during the receipt of  add_ba_req
         */
        delbaparams.tid       = tidno;
        delbaparams.initiator = 0;
        delbaparams.reserved0 = 0;

        spin_lock(&ic->ic_addba_lock);
        if (ic->ic_delba_process)
                ic->ic_delba_process(ni, &delbaparams, reasoncode);
        spin_unlock(&ic->ic_addba_lock);

        /* send DELBA */
        actionargs.category     = IEEE80211_ACTION_CAT_BA;
        actionargs.action       = IEEE80211_ACTION_BA_DELBA;
        actionargs.arg1         = tidno;
        actionargs.arg2         = initiator;
        actionargs.arg3         = reasoncode;

        ieee80211_send_action(ni, &actionargs, NULL);

    } else {
        /* send the management frame buffer to the target */
        wmi_unified_delba_send_cmd_send(pdev_wmi_handle, ni->ni_macaddr, &param);
    }
}

void
ol_ath_addba_setresponse(struct ol_ath_softc_net80211 *scn,
                        struct ieee80211_node *ni,
                        u_int8_t tidno,
                        u_int16_t statuscode)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct addba_setresponse_params param;
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_peer_handle an_txrx_handle;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    soc_txrx_handle = wlan_psoc_get_dp_handle(wlan_pdev_get_psoc(scn->sc_pdev));
    an_txrx_handle = wlan_peer_get_dp_handle(ni->peer_obj);
    if (!an_txrx_handle)
        return;

    param.vdev_id = (OL_ATH_VAP_NET80211(vap))->av_if_id;
    param.tidno = tidno;
    param.statuscode = statuscode;

    cdp_set_addbaresponse(soc_txrx_handle, an_txrx_handle,
                                   tidno, statuscode);
    /* send the management frame buffer to the target */
    wmi_unified_addba_setresponse_cmd_send(pdev_wmi_handle, ni->ni_macaddr, &param);
}

void
ol_ath_send_singleamsdu(struct ol_ath_softc_net80211 *scn,
                        struct ieee80211_node *ni,
                        u_int8_t tidno)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct singleamsdu_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    param.vdev_id = (OL_ATH_VAP_NET80211(vap))->av_if_id;
    param.tidno = tidno;

    /* send the management frame buffer to the target */
    wmi_unified_singleamsdu_cmd_send(pdev_wmi_handle, ni->ni_macaddr, &param);
}

/*
 * Clean ADDBA response status
 */
static void
ol_ath_net80211_addba_clearresponse(struct ieee80211_node *ni)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    /* Notify target of the association/reassociation */
    ol_ath_send_addba_clearresponse(scn, ni);
}
/*
 * Send out ADDBA request
 */
static int
ol_ath_net80211_addba_send(struct ieee80211_node *ni,
                        u_int8_t tidno,
                        u_int16_t buffersize)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    return ol_ath_send_addba_send(scn,ni,tidno,buffersize);
}

/*
 * Send out DELBA request
 */
static void
ol_ath_net80211_delba_send(struct ieee80211_node *ni,
                        u_int8_t tidno,
                        u_int8_t initiator,
                        u_int16_t reasoncode)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    ol_ath_delba_send(scn,ni,tidno,initiator,reasoncode);
}

/*
 * Set ADDBA response
 */
static void
ol_ath_net80211_addba_setresponse(struct ieee80211_node *ni,
                        u_int8_t tidno,
                        u_int16_t statuscode)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    ol_ath_addba_setresponse(scn,ni,tidno,statuscode);
}

/*
 * Send single VHT MPDU AMSDUs
 */
static void
ol_ath_net80211_send_singleamsdu(struct ieee80211_node *ni,
                        u_int8_t tidno)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    ol_ath_send_singleamsdu(scn,ni,tidno);
}

/*
 * Determine the capabilities of the peer for use by the rate control module
 * residing in the target
 */
static u_int32_t
ol_ath_set_ratecap(struct ol_ath_softc_net80211 *scn, struct ieee80211_node *ni,
        struct ieee80211vap *vap)
{
    u_int32_t ratecap = 0;

    /* peer can support 3 streams */
    if (ni->ni_streams == 3)
    {
        ratecap |= WMI_HOST_RC_TS_FLAG;
    }

    /* peer can support 2 streams */
    if (ni->ni_streams >= 2)
    {
        ratecap |= WMI_HOST_RC_DS_FLAG;
    }

    /*
     * With SM power save, only singe stream rates can be used for static MIMOPS.
     * In dynamic SM power save mode, a STA enables its multiple receive chains
     * when it receives the start of a frame sequence addressed to it.
     * The receiver switches to the multiple receive chain mode when it receives the
     * RTS addressed to it and switches back immediately when the frame sequence ends.
     */
    if((ni->ni_htcap & IEEE80211_HTCAP_C_SM_MASK) == IEEE80211_HTCAP_C_SMPOWERSAVE_STATIC) {
        ratecap &= ~(WMI_HOST_RC_TS_FLAG|WMI_HOST_RC_DS_FLAG);
    }

    return ratecap;
}

static u_int32_t
ol_ath_net80211_rate_node_update(struct ieee80211com *ic,
                                 struct ieee80211_node *ni, int isnew)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ieee80211vap *vap = ieee80211_node_get_vap(ni);
    enum ieee80211_cwm_width ic_cw_width = ic->ic_cwm_get_width(ic);
    u_int32_t capflag = 0;

    if (ni->ni_flags & IEEE80211_NODE_HT) {
        capflag |=  WMI_HOST_RC_HT_FLAG;
        if ((ni->ni_chwidth == IEEE80211_CWM_WIDTH40) &&
            (ic_cw_width == IEEE80211_CWM_WIDTH40))
        {
            capflag |=  WMI_HOST_RC_CW40_FLAG;
        }
        if (((ni->ni_htcap & IEEE80211_HTCAP_C_SHORTGI40) &&
             (ic_cw_width == IEEE80211_CWM_WIDTH40)) ||
            ((ni->ni_htcap & IEEE80211_HTCAP_C_SHORTGI20) &&
             (ic_cw_width == IEEE80211_CWM_WIDTH20))) {
            capflag |= WMI_HOST_RC_SGI_FLAG;
        }

        /* Rx STBC is a 2-bit mask. Needs to convert from ieee definition to ath definition. */
        capflag |= (((ni->ni_htcap & IEEE80211_HTCAP_C_RXSTBC) >> IEEE80211_HTCAP_C_RXSTBC_S)
                    << WMI_HOST_RC_RX_STBC_FLAG_S);
        capflag |= ol_ath_set_ratecap(scn, ni, vap);

#if 0
        /* TODO: Security mode related adjustments */

        /* TODO: TxBF related adjustments */
#endif
    }

    if (ni->ni_flags & IEEE80211_NODE_UAPSD) {
        capflag |= WMI_HOST_RC_UAPSD_FLAG;
    }

    return capflag;
}


/*
 *  WMI API for 802.11 management frame processing
 */
static void
ol_ath_send_peer_update(struct ol_ath_softc_net80211 *scn, struct ieee80211com *ic,
                            struct ieee80211_node *ni, uint32_t val)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
    const u_int32_t min_idle_inactive_time_secs = 256;
    const u_int32_t max_idle_inactive_time_secs = 256 * 2;
    const u_int32_t max_unresponsive_time_secs  = (256 * 2) + 5;
    struct wlan_vdev_mgr_cfg mlme_cfg;
    struct vdev_mlme_obj *vdev_mlme = vap->vdev_mlme;

    if(ol_ath_node_set_param(scn, ni->ni_macaddr,
                WMI_HOST_PEER_USE_4ADDR, val,
                (OL_ATH_VAP_NET80211(vap))->av_if_id)) {
        printk("%s:Unable to change peer Next Hop setting\n", __func__);
    }

    if(ol_ath_wmi_send_vdev_param(scn,
                avn->av_if_id, wmi_vdev_param_ap_enable_nawds,
                min_idle_inactive_time_secs)) {
        printk("%s: Enable NAWDS Failed\n", __func__);
    }

    mlme_cfg.value = min_idle_inactive_time_secs;
    if (wlan_util_vdev_mlme_set_param(vdev_mlme,
            WLAN_MLME_CFG_MIN_IDLE_INACTIVE_TIME,
            mlme_cfg))
        qdf_err("setting MIN inactive time failed");

    mlme_cfg.value = max_idle_inactive_time_secs;
    if (wlan_util_vdev_mlme_set_param(vdev_mlme,
            WLAN_MLME_CFG_MAX_IDLE_INACTIVE_TIME,
            mlme_cfg))
        qdf_err("setting MAX inactive time failed");

    mlme_cfg.value = max_unresponsive_time_secs;
    if (wlan_util_vdev_mlme_set_param(vdev_mlme,
            WLAN_MLME_CFG_MAX_UNRESPONSIVE_INACTIVE_TIME,
            mlme_cfg))
        qdf_err("setting MAX unresponsive time failed");
}

int
wmi_unified_set_qboost_param(
		struct ol_ath_vap_net80211 *avn,
		struct ol_ath_node_net80211 *anode,
		A_UINT32 value)
{
    struct ol_ath_softc_net80211 *svn = avn->av_sc;
    struct set_qboost_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    qdf_mem_set(&param, sizeof(param), 0);
    param.vdev_id = avn->av_if_id;
    param.value = value;
    pdev_wmi_handle = lmac_get_pdev_wmi_handle(svn->sc_pdev);
    return wmi_unified_set_qboost_param_cmd_send(pdev_wmi_handle,
	anode->an_node.ni_macaddr, &param);
}

void
qboost_config(struct ieee80211vap *vap, struct ieee80211_node *ni, bool qboost_cfg)
{
#define QBOOST_ENABLE  1
#define QBOOST_DISABLE 0
	if(qboost_cfg) {
		(void)wmi_unified_set_qboost_param(OL_ATH_VAP_NET80211(vap),
				OL_ATH_NODE_NET80211(ni), QBOOST_ENABLE);
	}else
		(void)wmi_unified_set_qboost_param(OL_ATH_VAP_NET80211(vap),
				OL_ATH_NODE_NET80211(ni), QBOOST_DISABLE);
}

void
ol_ath_net80211_newassoc(struct ieee80211_node *ni, int isnew)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211vap *vap = ni->ni_vap;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    A_UINT32 uapsd, max_sp, trigger_tid = 0;
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_peer_handle an_txrx_handle;
    struct ol_txrx_vdev_t *vdev;
    struct wlan_objmgr_psoc *psoc;
    ol_ath_soc_softc_t *soc;

    psoc = wlan_pdev_get_psoc(ic->ic_pdev_obj);
    soc = (ol_ath_soc_softc_t *)lmac_get_psoc_feature_ptr(psoc);
    soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
    an_txrx_handle = wlan_peer_get_dp_handle(ni->peer_obj);
    if (!an_txrx_handle)
        return;

    vdev = (struct ol_txrx_vdev_t *)wlan_vdev_get_dp_handle(vap->vdev_obj);


#define USE_4ADDR 1
#if WDS_VENDOR_EXTENSION
    if ((ni->ni_flags & IEEE80211_NODE_NAWDS) && (ni->ni_flags & IEEE80211_NODE_WDS)) {
	    int wds_tx_policy_ucast = 0, wds_tx_policy_mcast = 0;

	    if (ni->ni_wds_tx_policy) {
		    wds_tx_policy_ucast = (ni->ni_wds_tx_policy & WDS_POLICY_TX_UCAST_4ADDR) ? 1: 0;
		    wds_tx_policy_mcast = (ni->ni_wds_tx_policy & WDS_POLICY_TX_MCAST_4ADDR) ? 1: 0;
		    cdp_set_wds_tx_policy_update(soc_txrx_handle, an_txrx_handle,
				    wds_tx_policy_ucast, wds_tx_policy_mcast);
	    }
	    else {
		    /* if tx_policy is not set, and node is WDS, ucast/mcast frames will be sent as 4ADDR */
		    wds_tx_policy_ucast = wds_tx_policy_mcast = 1;
		    cdp_set_wds_tx_policy_update(soc_txrx_handle, an_txrx_handle,
				    wds_tx_policy_ucast, wds_tx_policy_mcast);
	    }
	    if (ieee80211vap_get_opmode(vap) == IEEE80211_M_HOSTAP) {
		    if (wds_tx_policy_mcast) {
			    struct ol_ath_vap_net80211 *avn = OL_ATH_VAP_NET80211(vap);
			    /* turn on MCAST_INDICATE so that multicast/broadcast frames
			     * can be cloned and sent as 4-addr directed frames to clients
			     * that want them in 4-addr format
			     */
			    ol_ath_wmi_send_vdev_param(scn,avn->av_if_id,
					    wmi_vdev_param_mcast_indicate, 1);
			    /* turn on UNKNOWN_DEST_INDICATE so that unciast frames to
			     * unknown destinations are indicated to host which can then
			     * send to all connected WDS clients
			     */
			    ol_ath_wmi_send_vdev_param(scn,avn->av_if_id,
					    wmi_vdev_param_unknown_dest_indicate, 1);
		    }
		    if (wds_tx_policy_ucast || wds_tx_policy_mcast) {
			    /* turn on 4-addr framing for this node */
			    ol_ath_node_set_param(scn, ni->ni_macaddr,
					    WMI_HOST_PEER_USE_4ADDR, USE_4ADDR,
					    (OL_ATH_VAP_NET80211(vap))->av_if_id);
		    }
	    }
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
        if (scn->nss_radio.nss_rctx) {
            /*
             * Sets WDS Vendor Extension flag and pushes configuration to NSS FW
             */
            if (ic->nss_vops) {
                ic->nss_vops->ic_osif_nss_vdev_set_cfg(vap->iv_ifp, OSIF_NSS_WIFI_VDEV_WDS_EXT_ENABLE);
            }
            if (ic->nss_radio_ops) {
                ic->nss_radio_ops->ic_nss_ol_wds_extn_peer_cfg_send(scn, an_txrx_handle);
            }
        }
#endif //QCA_NSS_WIFI_OFFLOAD_SUPPORT
    }
#endif //WDS_VENDOR_EXTENSION

    /*
     * 1. Check NAWDS or not
     * 2. Do not Pass PHY MODE 0 as association not allowed in NAWDS.
     *     rc_mask will be NULL and Tgt will assert
     */

    if (ni->ni_flags & IEEE80211_NODE_NAWDS ) {
        IEEE80211_DPRINTF( vap, IEEE80211_MSG_ASSOC, "\n NODE %pK is NAWDS ENABLED\n",ni);

        if(ni->ni_phymode == 0){
            ni->ni_phymode = vap->iv_des_mode;
        }
            /* Update Host Peer Table */
        cdp_peer_set_nawds(soc_txrx_handle, (void *)an_txrx_handle, 1);
            /* Vdev nawds enabling is required only
             * to differentiate Normal/Nawds path
             * But NAWDS is per peer not per vdev
             */
        cdp_txrx_set_vdev_param(soc_txrx_handle, (void *)vdev, CDP_ENABLE_NAWDS, 1);

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
        if (ic->nss_vops)
            ic->nss_vops->ic_osif_nss_vdev_set_cfg(vap->iv_ifp, OSIF_NSS_WIFI_VDEV_NAWDS_MODE);

        /*
         * Send nawds enable on this peer.
         */
        if (soc && soc->nss_soc.ops) {
            soc->nss_soc.ops->nss_soc_nawds_enable(soc, (void *)an_txrx_handle, 1);
        }
#endif

	ol_ath_send_peer_update(scn, ic, ni, USE_4ADDR);
    }
    /* TODO: Fill in security params */

    /* Notify target of the association/reassociation */
    if ( ol_ath_send_peer_assoc(scn, ic, ni, isnew) != QDF_STATUS_SUCCESS ) {
        qdf_print("%s: Failed to send peer assoc to FW",__LINE__);
    }

#ifdef ATH_SUPPORT_HYFI_ENHANCEMENTS
    ol_ath_node_ext_stats_enable(ni, 1); // Enable Peer Ext Stats Collection
#endif

    /* XXX must be sent _after_ new assoc */
    switch (vap->iv_opmode) {
    case IEEE80211_M_HOSTAP:
        if (ni->ni_flags & IEEE80211_NODE_UAPSD) {
            if(isnew){
                uapsd = 0;
                if (WME_UAPSD_AC_ENABLED(0, ni->ni_uapsd)) {
                    uapsd |= WMI_HOST_AP_PS_UAPSD_AC0_DELIVERY_EN |
                        WMI_HOST_AP_PS_UAPSD_AC0_TRIGGER_EN;
                }
                if (WME_UAPSD_AC_ENABLED(1, ni->ni_uapsd)) {
                    uapsd |= WMI_HOST_AP_PS_UAPSD_AC1_DELIVERY_EN |
                        WMI_HOST_AP_PS_UAPSD_AC1_TRIGGER_EN;
                }
                if (WME_UAPSD_AC_ENABLED(2, ni->ni_uapsd)) {
                    uapsd |= WMI_HOST_AP_PS_UAPSD_AC2_DELIVERY_EN |
                        WMI_HOST_AP_PS_UAPSD_AC2_TRIGGER_EN;
                }
                if (WME_UAPSD_AC_ENABLED(3, ni->ni_uapsd)) {
                    uapsd |= WMI_HOST_AP_PS_UAPSD_AC3_DELIVERY_EN |
                        WMI_HOST_AP_PS_UAPSD_AC3_TRIGGER_EN;
                }
                (void)wmi_unified_set_ap_ps_param(OL_ATH_VAP_NET80211(vap),
                     OL_ATH_NODE_NET80211(ni), WMI_HOST_AP_PS_PEER_PARAM_UAPSD, uapsd);
            } else {
#if UMAC_SUPPORT_ADMCTL
                ic->ic_node_update_dyn_uapsd(ni,0,WME_UAPSD_AC_INVAL,WME_UAPSD_AC_INVAL);
#endif
            }
            switch (ni->ni_uapsd_maxsp) {
            case 2:
                max_sp = WMI_HOST_AP_PS_PEER_PARAM_MAX_SP_2;
                break;
            case 4:
                max_sp = WMI_HOST_AP_PS_PEER_PARAM_MAX_SP_4;
                break;
            case 6:
                max_sp = WMI_HOST_AP_PS_PEER_PARAM_MAX_SP_6;
                break;
            default:
                max_sp = WMI_HOST_AP_PS_PEER_PARAM_MAX_SP_UNLIMITED;
                break;
            }
        } else {
            uapsd = 0;
            max_sp = 0;
            (void)wmi_unified_set_ap_ps_param(OL_ATH_VAP_NET80211(vap),
                 OL_ATH_NODE_NET80211(ni), WMI_HOST_AP_PS_PEER_PARAM_UAPSD, uapsd);
        }
        (void)wmi_unified_set_ap_ps_param(OL_ATH_VAP_NET80211(vap),
                OL_ATH_NODE_NET80211(ni), WMI_HOST_AP_PS_PEER_PARAM_MAX_SP, max_sp);

	qboost_config(vap, ni, scn->scn_qboost_enable);

	if(scn->scn_sifs_frmtype) {
		(void)wmi_unified_set_ap_ps_param(OL_ATH_VAP_NET80211(vap),
				OL_ATH_NODE_NET80211(ni), WMI_HOST_AP_PS_PEER_PARAM_SIFS_RESP_FRMTYPE, scn->scn_sifs_frmtype);

#define BE_AC_MASK 0x1
#define BK_AC_MASK 0x2
#define VI_AC_MASK 0x4
#define VO_AC_MASK 0x8

#define TRIGGER_BE_TIDS 0x9
#define TRIGGER_BK_TIDS 0x6
#define TRIGGER_VI_TIDS 0x30
#define TRIGGER_VO_TIDS 0xC0
		if(scn->scn_sifs_uapsd & BE_AC_MASK)
			trigger_tid = TRIGGER_BE_TIDS;
		if(scn->scn_sifs_uapsd & BK_AC_MASK)
			trigger_tid |= TRIGGER_BK_TIDS;
		if(scn->scn_sifs_uapsd & VI_AC_MASK)
			trigger_tid |= TRIGGER_VI_TIDS;
		if(scn->scn_sifs_uapsd & VO_AC_MASK)
			trigger_tid |= TRIGGER_VO_TIDS;

		(void)wmi_unified_set_ap_ps_param(OL_ATH_VAP_NET80211(vap),
				OL_ATH_NODE_NET80211(ni), WMI_HOST_AP_PS_PEER_PARAM_SIFS_RESP_UAPSD, trigger_tid);
	} else {
		/*
		 * No meaning to enable trigger too. Disable this too
		 */
		AR_DEBUG_PRINTF(ATH_DEBUG_INFO,("\n Disabling Both SIFS RESP and UAPSD TRIGGER\n"));
#define DISABLE_SIFS_RESP_TRIGGER 0x0
		(void)wmi_unified_set_ap_ps_param(OL_ATH_VAP_NET80211(vap),
				OL_ATH_NODE_NET80211(ni), WMI_HOST_AP_PS_PEER_PARAM_SIFS_RESP_FRMTYPE, DISABLE_SIFS_RESP_TRIGGER);
		(void)wmi_unified_set_ap_ps_param(OL_ATH_VAP_NET80211(vap),
				OL_ATH_NODE_NET80211(ni), WMI_HOST_AP_PS_PEER_PARAM_SIFS_RESP_UAPSD, DISABLE_SIFS_RESP_TRIGGER);
	}
        break;
    case IEEE80211_M_STA:
	qboost_config(vap, ni, scn->scn_qboost_enable);
	break;
    default:
        break;
    }
}

static INLINE void
ol_ath_rxstat2ieee(struct ieee80211com *ic,
                ol_ath_ieee80211_rx_status_t *rx_status,
                struct ieee80211_rx_status *rs)
{
    uint32_t phy_mode = (uint32_t) rx_status->phy_mode;
    /* TBD: More fields to be updated later */
    rs->rs_rssi     = rx_status->rssi;
    rs->rs_datarate = rx_status->rateKbps;
    rs->rs_channel  = rx_status->channel;
    rs->rs_flags  = 0;
    if (rx_status->status & WMI_HOST_RXERR_CRC)
        rs->rs_flags  |= IEEE80211_RX_FCS_ERROR;
    if (rx_status->status & WMI_HOST_RXERR_DECRYPT)
        rs->rs_flags  |= IEEE80211_RX_DECRYPT_ERROR;
    if (rx_status->status & WMI_HOST_RXERR_MIC)
        rs->rs_flags  |= IEEE80211_RX_MIC_ERROR;
    if (rx_status->status & WMI_HOST_RXERR_KEY_CACHE_MISS)
        rs->rs_flags  |= IEEE80211_RX_KEYMISS;
    /* TBD: whalGetNf in firmware is fixed to-96. Maybe firmware should calculate it based on whalGetChanNf? */
    rs->rs_abs_rssi = rx_status->rssi - 96;
    switch (phy_mode)
    {
    case WMI_HOST_MODE_11A:
        rs->rs_phymode = IEEE80211_MODE_11A;
        break;
    case WMI_HOST_MODE_11B:
        rs->rs_phymode = IEEE80211_MODE_11B;
        break;
    case WMI_HOST_MODE_11G:
        rs->rs_phymode = IEEE80211_MODE_11G;
        break;
    case WMI_HOST_MODE_11NA_HT20:
        rs->rs_phymode = IEEE80211_MODE_11NA_HT20;
        break;
    case WMI_HOST_MODE_11NA_HT40:
        if (ic->ic_cwm_get_extoffset(ic) == 1)
            rs->rs_phymode = IEEE80211_MODE_11NA_HT40PLUS;
        else
            rs->rs_phymode = IEEE80211_MODE_11NA_HT40MINUS;
        break;
    case WMI_HOST_MODE_11NG_HT20:
        rs->rs_phymode = IEEE80211_MODE_11NG_HT20;
        break;
    case WMI_HOST_MODE_11NG_HT40:
        if (ic->ic_cwm_get_extoffset(ic) == 1)
            rs->rs_phymode = IEEE80211_MODE_11NG_HT40PLUS;
        else
            rs->rs_phymode = IEEE80211_MODE_11NG_HT40MINUS;
        break;
    case WMI_HOST_MODE_11AC_VHT20:
        rs->rs_phymode = IEEE80211_MODE_11AC_VHT20;
        break;
    case WMI_HOST_MODE_11AC_VHT40:
        if (ic->ic_cwm_get_extoffset(ic) == 1)
            rs->rs_phymode = IEEE80211_MODE_11AC_VHT40PLUS;
        else
            rs->rs_phymode = IEEE80211_MODE_11AC_VHT40MINUS;
        break;
    case WMI_HOST_MODE_11AC_VHT80:
        rs->rs_phymode = IEEE80211_MODE_11AC_VHT80;
        break;
    case WMI_HOST_MODE_11AC_VHT160:
        rs->rs_phymode = IEEE80211_MODE_11AC_VHT160;
        break;
    case WMI_HOST_MODE_11AC_VHT80_80:
        rs->rs_phymode = IEEE80211_MODE_11AC_VHT80_80;
        break;
    /* 11AX TODO (Phase II) - Add HE mode related entries here after confirming
     * applicability.
     */
    default:
        break;
    }
    rs->rs_freq = ieee80211_ieee2mhz(ic, rs->rs_channel, 0);
#ifdef HOST_SUPPORT_BEELINER_MPHYR
#define DEFAULT_MPHYR_FREQ 5200
        rs->rs_freq = DEFAULT_MPHYR_FREQ;
#endif
    rs->rs_full_chan = ol_ath_find_full_channel(ic, rs->rs_freq);
    rs->rs_isvalidrssi = 0;
}

#ifndef REMOVE_PKT_LOG
void
chk_vht_groupid_action_frame(struct ol_ath_softc_net80211 * scn,struct ieee80211_node *ni, wbuf_t wbuf)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211_frame *wh;
    struct ieee80211_action_vht_gid_mgmt  *gid_mgmt_frm;
	ol_txrx_pdev_handle pdev_txrx_handle = wlan_pdev_get_dp_handle(scn->sc_pdev);
	ol_txrx_soc_handle soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    int type, subtype;
    u_int8_t action_category;
    u_int8_t vht_action;
    u_int8_t *mem_status;
    u_int8_t *user_position;

    wlan_wbuf_set_peer_node(wbuf, ni);

    if (wbuf_get_pktlen(wbuf) < ic->ic_minframesize) {
        goto endtortn;
    }
    wh = (struct ieee80211_frame *) wbuf_header(wbuf);
    if ((wh->i_fc[0] & IEEE80211_FC0_VERSION_MASK) != IEEE80211_FC0_VERSION_0) {
        goto endtortn;
    }
    type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
    subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
    if((type == IEEE80211_FC0_TYPE_MGT)&&(subtype == IEEE80211_FC0_SUBTYPE_ACTION))
    {
        action_category = ((struct ieee80211_action *)(&wh[1]))->ia_category;
        vht_action = ((struct ieee80211_action *)(&wh[1]))->ia_action;
        if((action_category == IEEE80211_ACTION_CAT_VHT)&&(vht_action == IEEE80211_ACTION_VHT_GROUP_ID))
        {
           /* Save the group ID managemnt information so dump themn into log file in rx_info_remote of pktlog*/
           gid_mgmt_frm = (struct ieee80211_action_vht_gid_mgmt *)(&wh[1]);
	   mem_status = &((gid_mgmt_frm->member_status[0]));
	   user_position = &((gid_mgmt_frm->user_position[0]));

	   cdp_set_gid_flag(soc_txrx_handle, (void *) pdev_txrx_handle,
			   mem_status, user_position);

        }
    }

endtortn:
    return;

}
#endif


void ol_ath_mgmt_handler(struct wlan_objmgr_pdev *pdev, struct ol_ath_softc_net80211 *scn, wbuf_t wbuf, struct ieee80211_frame * wh, ol_ath_ieee80211_rx_status_t rx_status, struct mgmt_rx_event_params rx_event, bool null_data_handler);

/*
 * WMI RX event handler for management frames
 */
static int
ol_ath_mgmt_rx_event_handler(ol_scn_t sc, u_int8_t *data, u_int32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    struct ieee80211com *ic;
    ol_ath_ieee80211_rx_status_t rx_status;
    struct ieee80211_frame *wh;
    uint8_t *bufp;
    uint32_t len;
    wbuf_t wbuf;
    struct mgmt_rx_event_params rx_event = {0};
    struct ol_ath_softc_net80211 *scn;
    struct common_wmi_handle *wmi_handle;
    struct wlan_objmgr_pdev *pdev;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    if(wmi_extract_mgmt_rx_params(wmi_handle, data, &rx_event, &bufp)) {
        qdf_print("Failed to extract mgmt frame");
        return 0;
    }
    /* Use received frames SNR as RSSI as our algorithms
     * rely on +ve SNR values and not on -ve RSSI values.
     */
    rx_event.rssi = rx_event.snr;
    pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, PDEV_UNIT(
                                        rx_event.pdev_id), WLAN_MLME_SB_ID);
    if (pdev == NULL) {
         qdf_print("%s: pdev object (id: %d) is NULL", __func__,
                  PDEV_UNIT(rx_event.pdev_id));
         return -1;
    }

    scn = lmac_get_pdev_feature_ptr(pdev);
    ic = &scn->sc_ic;
    /* Update RX status */
    rx_status.channel = rx_event.channel;
    rx_status.rssi = rx_event.snr;
    rx_status.rateKbps = rx_event.rate;
    rx_status.phy_mode = rx_event.phy_mode;
    rx_status.status = rx_event.status;

    if (!ic) {
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
        qdf_print("ic is NULL");
        return 0;
    }

#if ATH_ACS_DEBUG_SUPPORT
    /*
     * Ignoring external beacons when the ACS debug framework
     * is enabled so as to display only the custom beacons sent from the
     * ACS debug tool
     */
    if (ic->ic_acs_debug_support) {
        return 0;
    }
#endif

    len = roundup(rx_event.buf_len, sizeof(u_int32_t));
    wbuf =  wbuf_alloc(ic->ic_osdev, WBUF_RX_INTERNAL,
                       len);

    if (wbuf == NULL) {
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
        qdf_print("wbuf alloc failed");
        return 0;
    }

#ifdef DEBUG_RX_FRAME
   {
    int i;
    printk("%s wbuf 0x%x frame length %d  \n ",
                 __func__,(unsigned int) wbuf, rx_event.buf_len);
    for (i=0;i<rx_event.buf_len; ++i ) {
      printk("%x ", bufp[i]);
      if (i%16 == 0) printk("\n");
    }
   }
   printk("%s rx frame type 0x%x frame length %d  \n ",
                 __func__,bufp[0], rx_event.buf_len);
#endif

    wbuf_init(wbuf, rx_event.buf_len);
    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
#ifdef BIG_ENDIAN_HOST
    {
        /* for big endian host, copy engine byte_swap is enabled
         * But the rx mgmt frame buffer content is in network byte order
         * Need to byte swap the mgmt frame buffer content - so when copy engine
         * does byte_swap - host gets buffer content in the correct byte order
         */
        int i;
        u_int32_t *destp, *srcp;
        destp = (u_int32_t *)wh;
        srcp =  (u_int32_t *)bufp;
        for(i=0; i < (len/4); i++) {
            *destp = cpu_to_le32(*srcp);
            destp++; srcp++;
        }
    }
#else
    OS_MEMCPY(wh, bufp, rx_event.buf_len);
#endif

    ol_ath_mgmt_handler(pdev, scn, wbuf, wh, rx_status, rx_event, false);
    wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
    return 0;
}

void
ol_ath_mgmt_handler(struct wlan_objmgr_pdev *pdev, struct ol_ath_softc_net80211 *scn, wbuf_t wbuf, struct ieee80211_frame * wh, ol_ath_ieee80211_rx_status_t rx_status, struct mgmt_rx_event_params rx_event, bool null_data_handler)
{

    struct ieee80211com *ic = &scn->sc_ic;
    struct ieee80211_rx_status rs;
    struct ieee80211_node *ni;
    struct wlan_objmgr_psoc *psoc;

#if ATH_SUPPORT_IWSPY
    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    ieee80211_input_iwspy_update_rssi(ic, wh->i_addr2, rx_status.rssi);
#endif
    /*
     * From this point on we assume the frame is at least
     * as large as ieee80211_frame_min; verify that.
     */
    if (wbuf_get_pktlen(wbuf) < ic->ic_minframesize) {
        printk("%s: short packet %d\n", __func__, wbuf_get_pktlen(wbuf));
        wbuf_free(wbuf);
        return;
    }

    qdf_mem_zero(&rs, sizeof(rs));

    /*
     * Locate the node for sender, track state, and then
     * pass the (referenced) node up to the 802.11 layer
     * for its use.  If the sender is unknown spam the
     * frame; it'll be dropped where it's not wanted.
     */
    ni = ieee80211_find_rxnode(ic, (struct ieee80211_frame_min *)
                               wbuf_header(wbuf));
    psoc = wlan_pdev_get_psoc(scn->sc_pdev);
    rs.rs_ic = (void *)ic;
    rs.rs_ni = (void *)ni;
    /*
     * rs to be made available in the placeholder
     * in rx_event structure used by mgmt_txrx
     */
    rx_event.rx_params = (void *)&rs;

    /* If we receive a probereq with broadcast bssid which is usually
     * sent by sender to discover new networks, all the vaps should send reply
     */
    if (ni == NULL || (IEEE80211_IS_PROBEREQ(wh) && IEEE80211_IS_BROADCAST(wh->i_addr3))) {
        ol_ath_rxstat2ieee(ic, &rx_status, &rs);
        mgmt_txrx_rx_handler(psoc, wbuf, &rx_event);
        if (ni) {
            ieee80211_free_node(ni);
        }
    } else {
        ol_ath_rxstat2ieee(ni->ni_ic, &rx_status, &rs);
#ifdef QCA_SUPPORT_CP_STATS
        WLAN_PEER_CP_STAT_SET(ni, rx_mgmt_rate, rx_status.rateKbps);
        WLAN_PEER_CP_STAT_SET(ni, rx_mgmt_rssi, rx_status.rssi);
#endif

#ifndef REMOVE_PKT_LOG
        if(scn->pl_dev && !null_data_handler)
            chk_vht_groupid_action_frame(scn, ni, wbuf);
#endif
        /*
         * hand over the wbuf to the mgmt_txrx layer
         * for further processing.
         */
        mgmt_txrx_rx_handler(psoc, wbuf, &rx_event);
        ieee80211_free_node(ni);
    }
    return ;
}

qdf_export_symbol(ol_ath_mgmt_handler);

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
static int
mgmt_crypto_encap(wbuf_t wbuf, struct ieee80211_node *ni)
{
    struct ieee80211_frame *wh;
    wh = (struct ieee80211_frame *)wbuf_header(wbuf);

    /* encap only incase of WEP bit set */
    if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
        struct wlan_objmgr_pdev *pdev;
        struct wlan_objmgr_psoc *psoc;
        struct wlan_lmac_if_crypto_rx_ops *crypto_rx_ops;
        struct ieee80211com *ic = ni->ni_ic;
        struct ieee80211vap *vap = ni->ni_vap;
        struct ieee80211vap *tvap = NULL, *pvap =NULL;
        int tid;

        pdev = ic->ic_pdev_obj;

        if(pdev == NULL) {
            qdf_print("%s[%d]pdev is NULL", __func__, __LINE__);
            return -1;
        }
        psoc = wlan_pdev_get_psoc(pdev);

        if(psoc == NULL) {
            qdf_print("%s[%d]psoc is NULL", __func__, __LINE__);
            return -1;
        }

        crypto_rx_ops = wlan_crypto_get_crypto_rx_ops(psoc);
        tid = wbuf_get_tid(wbuf);
        if (tid == OFFCHAN_EXT_TID_NONPAUSE) {
            if (!TAILQ_EMPTY(&ic->ic_vaps)) {
                TAILQ_FOREACH_SAFE(vap, &ic->ic_vaps, iv_next, tvap) {
                    if (vap->iv_opmode == IEEE80211_M_MONITOR) {
                         pvap = vap;
                         break;
                    }
                }
            }
            if(pvap != NULL) {
                if (IEEE80211_ADDR_EQ(wh->i_addr1,vap->mcast_encrypt_addr)) {
                    if (crypto_rx_ops && WLAN_CRYPTO_RX_OPS_ENCAP(crypto_rx_ops)) {
                        if (WLAN_CRYPTO_RX_OPS_ENCAP(crypto_rx_ops)(pvap->vdev_obj,
                            wbuf, pvap->mcast_encrypt_addr, 0) != QDF_STATUS_SUCCESS) {
                            printk("Mgmt encap Failed \n");
                            return -1;
                        }
                    }
                }
            }
        }
        else {
            if (crypto_rx_ops && WLAN_CRYPTO_RX_OPS_ENCAP(crypto_rx_ops)) {
              if (WLAN_CRYPTO_RX_OPS_ENCAP(crypto_rx_ops)(vap->vdev_obj,
                            wbuf, ni->ni_macaddr, 0) != QDF_STATUS_SUCCESS) {
                  struct ieee80211_frame *wh;
                  wh = (struct ieee80211_frame *)wbuf_header(wbuf);
                  wh->i_fc[1] &= ~IEEE80211_FC1_WEP;
              }
            }
       }
    }

    return 0;
}
#else
static int
mgmt_crypto_encap(wbuf_t wbuf, struct ieee80211_node *ni)
{
    struct ieee80211_frame *wh;
    int tid;
    wh = (struct ieee80211_frame *)wbuf_header(wbuf);

    /* encap only incase of WEP bit set */
    if (wh->i_fc[1] & IEEE80211_FC1_WEP) {

        struct ieee80211_key *k;

        tid = wbuf_get_tid(wbuf);
        /* construct iv header */
        if (tid == OFFCHAN_EXT_TID_NONPAUSE) {
            k = ieee80211_crypto_encap_mon(ni, wbuf);
        }
        else {
            k = ieee80211_crypto_encap(ni, wbuf);
        }

        if (k == NULL) {
            printk("Mgmt encap Failed \n");
            return -1;
        }
        if (tid != OFFCHAN_EXT_TID_NONPAUSE) {
            /* OFFCHAN_EXT_TID_NONPAUSE tid is used for offchan raw mode TX,
              encrypt trailer already added in e.g. __wep_encrypt()*/
             wbuf_append(wbuf, k->wk_cipher->ic_trailer);
        }

    }

    return 0;
}
#endif

static inline bool ol_mgmt_fill_tx_params(struct ieee80211com *ic,
        uint32_t vdev_id, wbuf_t wbuf, struct tx_send_params *params,
        bool offchan)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ieee80211_frame *wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    bool data_frame = false;
    A_UINT8 retry, power, subtype_index, tid;
    A_UINT8 nss, preamble;
    uint32_t mcs = 0, rate;
    struct ieee80211vap *vap;
    bool fill_tx_params = false;


    vap = ol_ath_vap_get(scn, vdev_id);
    if (vap == NULL) {
        qdf_print("vap NULL");
        return false;
    }

    qdf_mem_zero(params, sizeof(*params));
    subtype_index = ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) >> IEEE80211_FC0_SUBTYPE_SHIFT);

    if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_DATA)
        data_frame = true;

    params->frame_type = data_frame;

    wbuf_get_tx_ctrl(wbuf, &retry, &power, NULL);
    wbuf_get_tx_rate(wbuf, &rate);
    tid = wbuf_get_tid(wbuf);

    if (offchan || (tid == EXT_TID_NONPAUSE)) {
        {
            if(ic->ic_he_target) {
                mcs = rate & RATECODE_V1_RIX_MASK;
                preamble = (rate >> PREAMBLE_OFFSET_IN_V1_RC) &
                            RATECODE_V1_PREAMBLE_MASK;
                nss = (rate >> RATECODE_V1_NSS_OFFSET) & RATECODE_V1_NSS_MASK;
            }
            else {
                mcs = rate & RATECODE_LEGACY_RIX_MASK;
                preamble = (rate >> PREAMBLE_OFFSET_IN_LEGACY_RC) &
                            RATECODE_LEGACY_PREAMBLE_MASK;
                nss = (rate >> RATECODE_LEGACY_NSS_OFFSET) & NSS_MASK_IN_LEGACY_RC;
            }

            if ((preamble == WMI_HOST_RATE_PREAMBLE_OFDM) ||
                (preamble == WMI_HOST_RATE_PREAMBLE_CCK)) {
                nss = 1;
            }

#define MGMT_MAX_CCK_RATES 4
#define MGMT_MAX_OFDM_RATES 8
            if (preamble == WMI_HOST_RATE_PREAMBLE_CCK) {
                uint8_t cck_bit_pos[MGMT_MAX_CCK_RATES] = {3, 2, 1, 0};

                if (mcs < MGMT_MAX_CCK_RATES)
                    mcs = (0x1 << cck_bit_pos[mcs]);
                else
                    mcs = 0;
            }

            if (preamble == WMI_HOST_RATE_PREAMBLE_OFDM) {
                uint8_t ofdm_bit_pos[MGMT_MAX_OFDM_RATES] =
                                            {10, 8, 6, 4, 11, 9, 7, 5};

                if (mcs < MGMT_MAX_OFDM_RATES)
                    mcs = (0x1 << ofdm_bit_pos[mcs]);
                else
                    mcs = 0;
            }

            params->mcs_mask = (mcs & 0xFFF);
            if (nss)
                params->nss_mask = (0x1 << (nss - 1));
            params->preamble_type = (0x1 << preamble);
        }

        if (power) {
            params->pwr = power & 0xFF;
        }

        if (retry == 0)
            retry = 1;

        params->retry_limit = retry & 0xF;
        fill_tx_params = true;
        offchan_debug("power - %d mcs - %d nss - %d retry - %d preamble - 0x%x frame - %d chain_mask - 0x%x bw_mask - 0x%x",
            params->pwr, params->mcs_mask, params->nss_mask,
            params->retry_limit, params->preamble_type, params->frame_type,
            params->chain_mask, params->bw_mask);
    } else {
        /* Only power populated */
        if (subtype_index <= (IEEE80211_FC0_SUBTYPE_DEAUTH >> IEEE80211_FC0_SUBTYPE_SHIFT))
            if(vap->iv_txpow_mgt_frm[subtype_index] != 0xff) {
                params->pwr = vap->iv_txpow_mgt_frm[subtype_index];
                fill_tx_params = true;
            }
    }
    params->chain_mask = 0;
    params->bw_mask = 0;

    ol_ath_release_vap(vap);

    return fill_tx_params;
}

int32_t ol_mgmt_offchan_tx(struct ieee80211com *ic, uint32_t vdev_id, wbuf_t wbuf)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct wmi_offchan_data_tx_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_zero(&param, sizeof(param));

    param.tx_params_valid = ol_mgmt_fill_tx_params(ic, vdev_id, wbuf, &param.tx_param, true);

    param.qdf_ctx = scn->soc->qdf_dev;
    param.vdev_id = vdev_id;
    param.desc_id = wbuf_get_txrx_desc_id(wbuf);
    param.chanfreq = 0;
    param.tx_frame = wbuf;
    param.frm_len = qdf_nbuf_len(wbuf);
    param.pdata = qdf_nbuf_data(wbuf);

    offchan_debug("Send offchan frame");
    if (wmi_offchan_data_tx_cmd_send(pdev_wmi_handle, &param)) {
        offchan_err("Failed to send offchan Tx");
        return -1;
    }
#ifdef QCA_SUPPORT_CP_STATS
    pdev_cp_stats_wmi_tx_mgmt_inc(ic->ic_pdev_obj, 1);
#endif

    return 0;
}

int32_t ol_mgmt_send(struct ieee80211com *ic, uint32_t vdev_id, wbuf_t wbuf)
{
	struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
	struct wmi_mgmt_params param;
	struct common_wmi_handle *pdev_wmi_handle;

	if (!qdf_nbuf_len(wbuf)) {
		qdf_err("%s: frame len is zero", __func__);
		return -1;
	}

	pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
	qdf_mem_zero(&param, sizeof(param));

	param.tx_params_valid = ol_mgmt_fill_tx_params(ic, vdev_id, wbuf, &param.tx_param, false);

	param.qdf_ctx = scn->soc->qdf_dev;
	param.vdev_id = vdev_id;
	param.desc_id = wbuf_get_txrx_desc_id(wbuf);
	param.chanfreq = 0;
	param.tx_frame = wbuf;
	param.frm_len = qdf_nbuf_len(wbuf);
	param.pdata = qdf_nbuf_data(wbuf);

	if (wmi_mgmt_unified_cmd_send(pdev_wmi_handle, &param)) {
		qdf_print("%s: Failed to send mgmt Tx", __func__);
		return -1;
	}
#ifdef QCA_SUPPORT_CP_STATS
        pdev_cp_stats_wmi_tx_mgmt_inc(ic->ic_pdev_obj, 1);
#endif
	return 0;
}

static bool mgmt_send_check_probe_rsp_for_throttling(struct ieee80211com *ic,
        wbuf_t wbuf, struct ieee80211_node *ni)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ieee80211_frame *wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    struct ieee80211vap *vap = ni->ni_vap;

    switch (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) {
    case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
        {
            u_int64_t adjusted_tsf_le;
            uint64_t tsf_adj;

            if(qdf_atomic_read(&scn->mgmt_ctx.mgmt_pending_completions) >
                        scn->mgmt_ctx.mgmt_pending_probe_resp_threshold) {
                return true;
            }
            ucfg_wlan_vdev_mgr_get_tsf_adjust(vap->vdev_obj,
                    &tsf_adj);
            /*
             * Make the TSF offset negative to match TSF in beacons
             */
            adjusted_tsf_le = cpu_to_le64(0ULL -tsf_adj);

            OS_MEMCPY(&wh[1], &adjusted_tsf_le, sizeof(adjusted_tsf_le));
        }
        break;
    }

    return false;
}
/*
 * Send Mgmt frames via WMI
 */
int
ol_ath_tx_mgmt_wmi_send(struct ieee80211com *ic, wbuf_t wbuf,
                        struct ieee80211_node *ni, void *mgmt_tx_params)
{

    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_frame *wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    int ret = 0;
    uint32_t offchan_tx = 0;
    int subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

    if (mgmt_tx_params)
        offchan_tx = *((uint32_t*)mgmt_tx_params);

    if ((qdf_atomic_read(&scn->mgmt_ctx.mgmt_pending_completions) >=
                scn->mgmt_ctx.mgmt_pending_max) ||
            mgmt_send_check_probe_rsp_for_throttling(ic, wbuf, ni)) {
        qdf_spin_lock_bh(&scn->mgmt_ctx.mgmt_backlog_queue_lock);
        /* Limit probe response not to fill more than 3/4 of mgmt
         * backlog queue. This will help prioritize other mgmt
         * in max multi vap scenario.
         */
        if (subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP ) {
            if (qdf_nbuf_queue_len(&scn->mgmt_ctx.mgmt_backlog_queue) >
               ((MGMT_DESC_POOL_MAX * 3) / 4)) {
                qdf_spin_unlock_bh(&scn->mgmt_ctx.mgmt_backlog_queue_lock);
                return -ENOMEM;
            }
        }
        else if (qdf_nbuf_queue_len(&scn->mgmt_ctx.mgmt_backlog_queue) >
                MGMT_DESC_POOL_MAX) {
            qdf_spin_unlock_bh(&scn->mgmt_ctx.mgmt_backlog_queue_lock);
            return -ENOMEM;
        }
        qdf_nbuf_queue_add(&scn->mgmt_ctx.mgmt_backlog_queue, wbuf);
        qdf_spin_unlock_bh(&scn->mgmt_ctx.mgmt_backlog_queue_lock);

        return 0;
    }

    /*Encap crypto header and trailer
     *Needed in case of shared WEP authentication
     */
    if (mgmt_crypto_encap(wbuf, ni) < 0)
        return -1;

    mgmt_txrx_debug("frame subtype: %d, ni: 0x%pK, mac: %s, refcnt: %d\n",
            wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK,
            ni, ether_sprintf(ni->ni_macaddr), wlan_objmgr_node_refcnt(ni));

    if (!wlan_psoc_nif_fw_ext_cap_get(scn->soc->psoc_obj,
                                  WLAN_SOC_CEXT_WMI_MGMT_REF)) {
          ret = cdp_mgmt_send(wlan_psoc_get_dp_handle(wlan_pdev_get_psoc(ic->ic_pdev_obj)),
                                (void *)wlan_vdev_get_dp_handle(vap->vdev_obj),
                 wbuf, OL_TXRX_MGMT_TYPE_BASE);
    } else {
        if (offchan_tx) {
            if(qdf_atomic_read(&scn->mgmt_ctx.mgmt_pending_completions) <
                    scn->mgmt_ctx.mgmt_pending_max)
                ret = ol_mgmt_offchan_tx(ic, (OL_ATH_VAP_NET80211(vap))->av_if_id, wbuf);
            else
                return -ENOMEM;
        } else
            ret = ol_mgmt_send(ic, (OL_ATH_VAP_NET80211(vap))->av_if_id, wbuf);

        if (!ret) {
            vap->wmi_tx_mgmt++;
            if (ni != vap->iv_bss)
                vap->wmi_tx_mgmt_sta++;
        }

    }

    if (!ret)
        qdf_atomic_inc(&scn->mgmt_ctx.mgmt_pending_completions);

    return ret;
}

/*
 * This API gets invoked from the mgmt_txrx layer
 * to hand over the Tx frames to lower layer
 */
QDF_STATUS ol_if_mgmt_send (struct wlan_objmgr_vdev *vdev,
                            qdf_nbuf_t nbuf, u_int32_t desc_id,
                            void *mgmt_tx_params)
{
    struct wlan_objmgr_pdev *pdev;
    struct pdev_osif_priv *osif_priv;
    struct ol_ath_softc_net80211 *scn;
    struct ieee80211com *ic;
    struct ieee80211_node *ni;
    QDF_STATUS status;

    if (vdev == NULL) {
        return QDF_STATUS_E_FAILURE;
    }

    pdev = wlan_vdev_get_pdev(vdev);

    if (pdev == NULL) {
        return QDF_STATUS_E_FAILURE;
    }

    osif_priv = wlan_pdev_get_ospriv(pdev);

    scn = (struct ol_ath_softc_net80211 *)(osif_priv->legacy_osif_priv);
    ic = &scn->sc_ic;
    ni = wlan_wbuf_get_peer_node(nbuf);
    if (ni == NULL) {
        return QDF_STATUS_E_FAILURE;
    }

    /*
     * Store mgmt_txrx_desc_id in cb
     */
    wbuf_set_txrx_desc_id(nbuf, desc_id);
    status = ol_ath_tx_mgmt_wmi_send(ic, nbuf, ni, mgmt_tx_params);
    if (status) {
        /* restore back peer in cb->peer_desc.peer before
         * returning back to umac layer.
         */
        wlan_wbuf_set_peer_node(nbuf, ni);
    }

    return status;
}

static inline void ol_if_process_rx_invalid(struct ol_ath_softc_net80211 *scn, enum WDI_EVENT event,
        void *data, u_int16_t peer_id)
{
    #define MIN_DEAUTH_INTERVAL 10 /* in msec */
    struct ieee80211com *ic = &scn->sc_ic;
    struct wdi_event_rx_peer_invalid_msg *msg = (struct wdi_event_rx_peer_invalid_msg *)data;
    qdf_nbuf_t msdu = msg->msdu;
    struct ieee80211_frame *wh = msg->wh;
    u_int8_t vdev_id = msg->vdev_id;
    struct ieee80211vap *vap;
    ol_ath_ieee80211_rx_status_t rx_status;
    struct ieee80211_rx_status rs = { 0 };
    wbuf_t wbuf;
    int wbuf_len;
    qdf_time_t now = 0,elasped_time = 0,max;
    struct ieee80211_node *ni;

    vap = ol_ath_vap_get(scn, vdev_id);
    if (vap == NULL) {
        /* No active vap */
        return;
    }
    max = (qdf_time_t)(-1); /*Max value 0xffffffffffffffff*/

    now = qdf_system_ticks_to_msecs(qdf_system_ticks());
    /* Wrap around condition */
    if(now < scn->scn_last_peer_invalid_time) {
        elasped_time = qdf_system_ticks_to_msecs(max) - scn->scn_last_peer_invalid_time + now + 1;
    } else
        elasped_time = now - scn->scn_last_peer_invalid_time;

    if(((elasped_time < MIN_DEAUTH_INTERVAL) &&
                scn->scn_last_peer_invalid_time) &&
            scn->scn_peer_invalid_cnt >= scn->scn_user_peer_invalid_cnt) {
        ol_ath_release_vap(vap);
        return;
    }
    if(elasped_time >= MIN_DEAUTH_INTERVAL)
        scn->scn_peer_invalid_cnt = 0;

    scn->scn_last_peer_invalid_time = qdf_system_ticks_to_msecs(qdf_system_ticks());

    scn->scn_peer_invalid_cnt++;

    /* Some times host gets peer_invalid frames
	 * for already associated node
	 * Not sending such frames to UMAC if the node
	 * is already associated
	 * to prevent UMAC sending Deauth to such associated
	 * nodes.
	 */
    ni = ieee80211_find_rxnode(ic, (struct ieee80211_frame_min *)wh);
    if (ni != NULL) {
       ieee80211_free_node(ni);
       ol_ath_release_vap(vap);
       return;
    }

    ni = ieee80211_try_ref_bss_node(vap);
    if (ni == NULL) {
       /* BSS node is already got deleted */
       ol_ath_release_vap(vap);
       return;
    }

    /* the msdu is already encapped with eth hdr */
    wbuf_len = qdf_nbuf_len(msdu) + sizeof(struct ieee80211_frame) - sizeof(struct ethernet_hdr_t);

    wbuf =  wbuf_alloc(ic->ic_osdev, WBUF_RX_INTERNAL, wbuf_len);
    if (wbuf == NULL) {
        qdf_print("%s: wbuf alloc failed", __func__);
        goto done; /* to free bss node ref */
    }
    wbuf_init(wbuf, wbuf_len);
    OS_MEMCPY(wbuf_header(wbuf), wh, sizeof(struct ieee80211_frame));
    OS_MEMCPY(wbuf_header(wbuf) + sizeof(struct ieee80211_frame),
              qdf_nbuf_data(msdu) + sizeof(struct ethernet_hdr_t),
              wbuf_len - sizeof(struct ieee80211_frame));
    OS_MEMZERO(&rx_status, sizeof(rx_status));
    /* we received this message because there is no entry for the peer in the key table */
    rx_status.status |= WMI_HOST_RXERR_KEY_CACHE_MISS;
    ol_ath_rxstat2ieee(ic, &rx_status, &rs);
    ieee80211_input(ni, wbuf, &rs);

done:
    ieee80211_free_node(ni);
    ol_ath_release_vap(vap);
#undef MIN_DEAUTH_INTERVAL
}

#if WDI_EVENT_ENABLE
void rx_peer_invalid(void *pdev, enum WDI_EVENT event, void *data, u_int16_t peer_id, enum htt_cmn_rx_status status)
{
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)pdev;

    ol_if_process_rx_invalid(scn, event, data, peer_id);
}

void htt_stats_callback(void *pdev, enum WDI_EVENT event, void *data, u_int16_t data_len, enum htt_cmn_rx_status status)
{
    struct ol_ath_softc_net80211 *scn;
    struct ieee80211com *ic;

    scn = (struct ol_ath_softc_net80211 *) pdev;

    ic = &scn->sc_ic;

    wlan_cfg80211_httstats_event(ic, data, data_len);
}
#endif

void rx_dp_peer_invalid(void *scn_handle, enum WDI_EVENT event, void *data, uint16_t peer_id)
{
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)scn_handle;

    ol_if_process_rx_invalid(scn, event, data, peer_id);
}

void mgmt_send_queued_frames(struct ol_ath_softc_net80211 *scn)
{
    qdf_nbuf_t wbuf = NULL;
    struct ieee80211_node *ni;
    struct ieee80211com *ic = &scn->sc_ic;
    u_int32_t mgmt_txrx_desc_id = 0;
    struct wlan_objmgr_peer *peer;
    struct ieee80211vap *vap;

    qdf_spin_lock_bh(&scn->mgmt_ctx.mgmt_backlog_queue_lock);
    if (!qdf_nbuf_is_queue_empty(&scn->mgmt_ctx.mgmt_backlog_queue)) {
        wbuf = qdf_nbuf_queue_remove(&scn->mgmt_ctx.mgmt_backlog_queue);
    }
    qdf_spin_unlock_bh(&scn->mgmt_ctx.mgmt_backlog_queue_lock);

    if (wbuf == NULL)
        return;

    mgmt_txrx_desc_id = wbuf_get_txrx_desc_id(wbuf);
    peer = mgmt_txrx_get_peer(scn->sc_pdev, mgmt_txrx_desc_id);
    QDF_ASSERT(peer != NULL);
    ni = wlan_peer_get_mlme_ext_obj(peer);
    if (ni == NULL) {
        qdf_print("%s:%d: ni NULL. Drop mgmt", __func__, __LINE__);
        goto drop;
    }
    vap = ni->ni_vap;

    if (mgmt_send_check_probe_rsp_for_throttling(ic, wbuf, ni)) {
        /* This is probe response and still needs throttling */
        if(qdf_nbuf_queue_len(&scn->mgmt_ctx.mgmt_backlog_queue) > MGMT_DESC_POOL_MAX/2) {
            goto drop;
        }

    }
    if (mgmt_crypto_encap(wbuf, ni) < 0)
        goto drop;

    if (wlan_psoc_nif_fw_ext_cap_get(scn->soc->psoc_obj,WLAN_SOC_CEXT_WMI_MGMT_REF)) {
        if (ol_mgmt_send(ic, (OL_ATH_VAP_NET80211(vap))->av_if_id, wbuf) == 0) {
            qdf_atomic_inc(&scn->mgmt_ctx.mgmt_pending_completions);
            vap->wmi_tx_mgmt++;
            if (ni != vap->iv_bss)
                vap->wmi_tx_mgmt_sta++;
            return;
        }
    } else {
        if (cdp_mgmt_send(wlan_psoc_get_dp_handle(wlan_pdev_get_psoc(ic->ic_pdev_obj)),
                                                  (void *)wlan_vdev_get_dp_handle(vap->vdev_obj),
                                                  wbuf, OL_TXRX_MGMT_TYPE_BASE) == 0) {
            qdf_atomic_inc(&scn->mgmt_ctx.mgmt_pending_completions);
            return;
        }
    }

drop:
    {
        struct ieee80211_tx_status ts;

        wbuf_set_peer(wbuf, peer);
        ts.ts_flags = IEEE80211_TX_ERROR;
        ts.ts_retries = 0;
        mgmt_txrx_tx_completion_handler(scn->sc_pdev,
                mgmt_txrx_desc_id,
                IEEE80211_TX_ERROR,
                (void *)&ts);
    }

}

static void
ol_mgmt_tx_completion_update_vdev_mgmt_stats(struct ol_ath_softc_net80211 *scn, wbuf_t wbuf)
{
    struct wlan_objmgr_peer *peer;
    struct ieee80211_node *ni;
    uint32_t mgmt_txrx_desc_id = 0;
    struct ieee80211vap *vap;

    mgmt_txrx_desc_id = wbuf_get_txrx_desc_id(wbuf);
    peer = mgmt_txrx_get_peer(scn->sc_pdev, mgmt_txrx_desc_id);
    if (!peer) {
        qdf_print("%s: null peer for desc id %d", __func__, mgmt_txrx_desc_id);
        qdf_assert_always(0);
        return;
    }
    ni = wlan_peer_get_mlme_ext_obj(peer);
    if (!ni) {
        qdf_print("%s: null ni for peer: 0x%pK", __func__, peer);
        qdf_assert_always(0);
        return;
    }

    vap = ni->ni_vap;
    if (vap) {
        vap->wmi_tx_mgmt_completions++;
        if (ni != vap->iv_bss)
            vap->wmi_tx_mgmt_completions_sta++;
    } else
        qdf_print("%s: null vap for ni: 0x%pK peer: 0x%pK mac:%s",
            __func__, ni, peer, ether_sprintf(ni->ni_macaddr));
}

/*
 * Management related attach functions for offload solutions
 */
void
ol_ath_mgmt_tx_complete(void *ctxt, wbuf_t wbuf, int err)
{
    struct wlan_objmgr_pdev *pdev;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct wlan_objmgr_peer *peer;
    struct ieee80211_tx_status ts;
    u_int32_t mgmt_txrx_desc_id = 0;
    uint32_t cleanup_flag = 0;
    struct ieee80211_frame *wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    int subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
    struct ieee80211_node *ni;

    pdev = ctxt;
    scn = lmac_get_pdev_feature_ptr(pdev);
    if (scn == NULL) {
        wbuf_complete_any(wbuf);
        return;
    }

    mgmt_txrx_desc_id = wbuf_get_txrx_desc_id(wbuf);

    if (err & IEEE80211_SKB_FREE_ONLY) {
        wbuf_complete_any(wbuf);
        return;
    } else if (err & IEEE80211_TX_ERROR_NO_SKB_FREE) {
        cleanup_flag = IEEE80211_TX_ERROR_NO_SKB_FREE;
    }

    if (!err) {
        ts.ts_flags = 0;
    } else {
        if (err == 3) { /* 1 = drop due to wal resoure, 3 = retry  */
            ts.ts_flags = IEEE80211_TX_XRETRY;
        } else {
            ts.ts_flags = IEEE80211_TX_ERROR;
        }
    }
    ts.ts_flags |= cleanup_flag;
    ts.ts_retries=0;

    peer = mgmt_txrx_get_peer(pdev, mgmt_txrx_desc_id);
    QDF_ASSERT(peer != NULL);
    wbuf_set_peer(wbuf, peer);

    KASSERT((wlan_wbuf_get_peer_node(wbuf) != NULL),("ni can not be null"));

    ni = wlan_wbuf_get_peer_node(wbuf);
    if (!ni) {
        qdf_print("%s: ni can not be null", __func__);
        qdf_assert_always(ni);
        return;
    }
    KASSERT((ni->ni_vap != NULL),("vap can not be null"));

    if (subtype == IEEE80211_FC0_SUBTYPE_ASSOC_RESP) {
        ni->ni_last_assoc_rx_time = 0;
    }

    ni->ni_vap->wmi_tx_mgmt_completions++;
    mgmt_txrx_tx_completion_handler(pdev, mgmt_txrx_desc_id, err, (void *)&ts);
    /* Check for frames in backlog queue and send it */
    mgmt_send_queued_frames(scn);

}

static int
ol_ath_mgmt_tx_completion_event_handler(ol_scn_t sc, uint8_t *data,
				uint32_t datalen)
{
	ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
	wmi_host_mgmt_tx_compl_event cmpl_params;
	struct ol_ath_softc_net80211 *scn;
	qdf_nbuf_t nbuf;
	struct ieee80211com *ic;
	uint32_t *skb_ptr;
	struct common_wmi_handle *wmi_handle;
	struct wlan_objmgr_pdev *pdev;

	wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
	if (wmi_extract_mgmt_tx_compl_param(wmi_handle, data,
				&cmpl_params) < 0) {
		qdf_print("Failed wmi extract tx comp");
		return -1;
	}
        pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, PDEV_UNIT(
                                     cmpl_params.pdev_id), WLAN_MLME_SB_ID);
        if (pdev == NULL) {
             qdf_print("%s: pdev object (id: %d) is NULL", __func__,
                       PDEV_UNIT(cmpl_params.pdev_id));
             return -1;
        }

        scn = lmac_get_pdev_feature_ptr(pdev);
        if (scn == NULL) {
             qdf_print("%s: scn(id: %d) is NULL", __func__,
                       PDEV_UNIT(cmpl_params.pdev_id));
             wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
             return -1;
        }

	nbuf = mgmt_txrx_get_nbuf(scn->sc_pdev, cmpl_params.desc_id);
	ic = &scn->sc_ic;
	if (!nbuf) {
		qdf_atomic_dec(&scn->mgmt_ctx.mgmt_pending_completions);
#ifdef QCA_SUPPORT_CP_STATS
                pdev_cp_stats_wmi_tx_mgmt_completion_err_inc(ic->ic_pdev_obj, 1);
#endif
		qdf_print("Wbuf not found for Desc_id");
                wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
		qdf_assert_always(0);
		return -1;
	}

	qdf_nbuf_unmap_single(scn->soc->qdf_dev, nbuf,
					QDF_DMA_TO_DEVICE);

        /* Clone mgmt packet to provide to upper layer */
        if ((ic->ic_debug_sniffer) && ic->ic_process_nondata_tx_frames) {
            qdf_nbuf_t mgmt_frm_cpy = NULL;
	    mgmt_frm_cpy = qdf_nbuf_copy((qdf_nbuf_t)nbuf);
	    if (mgmt_frm_cpy) {
                skb_ptr = (uint32_t *)qdf_nbuf_push_head(
                                 mgmt_frm_cpy, sizeof(cmpl_params.ppdu_id));
                *skb_ptr = cmpl_params.ppdu_id;
		ic->ic_process_nondata_tx_frames(ic, mgmt_frm_cpy);
	    }
        }

	mgmt_txrx_debug("desc_id: %d, wbuf: 0x%pK\n",
		cmpl_params.desc_id, nbuf);

#ifdef QCA_SUPPORT_CP_STATS
        pdev_cp_stats_wmi_tx_mgmt_completions_inc(ic->ic_pdev_obj, 1);
#endif
	ol_mgmt_tx_completion_update_vdev_mgmt_stats(scn, nbuf);

        qdf_atomic_dec(&scn->mgmt_ctx.mgmt_pending_completions);
	ol_ath_mgmt_tx_complete(scn->sc_pdev, nbuf,
			cmpl_params.status);
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);

	return 0;
}

static int
ol_ath_mgmt_offchan_tx_completion_event_handler(ol_scn_t sc, uint8_t *data,
				uint32_t datalen)
{
	ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
	struct wmi_host_offchan_data_tx_compl_event cmpl_params;
	struct ol_ath_softc_net80211 *scn;
	qdf_nbuf_t nbuf;
	struct common_wmi_handle *wmi_handle;
	struct ieee80211com *ic;
	struct wlan_objmgr_pdev *pdev;

	wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);

	if (wmi_extract_offchan_data_tx_compl_param(wmi_handle, data,
				&cmpl_params) < 0) {
		offchan_err("Failed tp extract tx comp");
		return -1;
	}
	offchan_debug("Desc_id = %d pdev_id = %d",
				cmpl_params.desc_id, cmpl_params.pdev_id);
        pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj,
                         PDEV_UNIT(cmpl_params.pdev_id), WLAN_MLME_SB_ID);
        if (pdev == NULL) {
             qdf_print("%s: pdev object (id: %d) is NULL ", __func__, PDEV_UNIT(cmpl_params.pdev_id));
             return -1;
        }

        scn = lmac_get_pdev_feature_ptr(pdev);
        if (scn == NULL) {
             qdf_print("%s: scn (id: %d) is NULL ", __func__, PDEV_UNIT(cmpl_params.pdev_id));
             wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
             return -1;
        }

	nbuf = mgmt_txrx_get_nbuf(scn->sc_pdev, cmpl_params.desc_id);
	ic = &scn->sc_ic;
	if (!nbuf) {
		qdf_atomic_dec(&scn->mgmt_ctx.mgmt_pending_completions);
#ifdef QCA_SUPPORT_CP_STATS
                pdev_cp_stats_wmi_tx_mgmt_completion_err_inc(ic->ic_pdev_obj, 1);
#endif
		qdf_print("Wbuf not found for Desc_id");
                wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
		qdf_assert_always(0);
		return -1;
	}
	qdf_nbuf_unmap_single(scn->soc->qdf_dev, nbuf,
				QDF_DMA_TO_DEVICE);

#ifdef QCA_SUPPORT_CP_STATS
        pdev_cp_stats_wmi_tx_mgmt_completions_inc(ic->ic_pdev_obj, 1);
#endif
	ol_mgmt_tx_completion_update_vdev_mgmt_stats(scn, nbuf);

        qdf_atomic_dec(&scn->mgmt_ctx.mgmt_pending_completions);
	ol_ath_mgmt_tx_complete(scn->sc_pdev, nbuf,
			cmpl_params.status);
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);

	return 0;
}

void ol_ath_wmi_over_htt_comp_handler(void *ctxt, wbuf_t wbuf, int err)
{
    struct wlan_objmgr_pdev *pdev;
    struct ol_ath_softc_net80211 *scn = NULL;

    pdev = ctxt;
    scn = lmac_get_pdev_feature_ptr(pdev);
    if (scn == NULL) {
        wbuf_complete_any(wbuf);
        return;
    }
    qdf_atomic_dec(&scn->mgmt_ctx.mgmt_pending_completions);
    ol_ath_mgmt_tx_complete(ctxt, wbuf, err);
}

void
ol_ath_mgmt_register_offload_beacon_tx_status_event(struct ieee80211com *ic,
                                            bool unregister) {
    struct wlan_objmgr_psoc *psoc;
    wmi_unified_t wmi_handle;

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s>>", __func__);

    psoc = wlan_pdev_get_psoc(ic->ic_pdev_obj);

    wmi_handle = lmac_get_wmi_unified_hdl(psoc);

    if (wmi_handle) {
        if (!unregister) {
            wmi_unified_register_event_handler(wmi_handle,
                    wmi_offload_bcn_tx_status_event_id,
                    ol_ath_offload_bcn_tx_status_event_handler,
                    WMI_RX_UMAC_CTX);
        } else {
            wmi_unified_unregister_event_handler(wmi_handle,
                    wmi_offload_bcn_tx_status_event_id);
        }
    } else {
        QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_ERROR,
                            "%s: wmi_handle is null.", __func__);
    }

    QDF_TRACE(QDF_MODULE_ID_BSSCOLOR, QDF_TRACE_LEVEL_DEBUG,
                                            "%s<<", __func__);
}

void
ol_ath_mgmt_soc_attach(ol_ath_soc_softc_t *soc)
{
    wmi_unified_t wmi_handle;

    wmi_handle = lmac_get_wmi_unified_hdl(soc->psoc_obj);
    /* Register WMI event handlers */
    wmi_unified_register_event_handler(wmi_handle, wmi_mgmt_rx_event_id,
            ol_ath_mgmt_rx_event_handler, WMI_RX_UMAC_CTX);

    if (wlan_psoc_nif_fw_ext_cap_get(soc->psoc_obj,
                                  WLAN_SOC_CEXT_WMI_MGMT_REF)) {
        wmi_unified_register_event_handler(wmi_handle,
                wmi_mgmt_tx_completion_event_id,
                ol_ath_mgmt_tx_completion_event_handler, WMI_RX_UMAC_CTX);
        wmi_unified_register_event_handler(wmi_handle,
                wmi_offchan_data_tx_completion_event,
                ol_ath_mgmt_offchan_tx_completion_event_handler, WMI_RX_UMAC_CTX);
    }
}

static int ol_ath_subscribe_csa_interop_phy(struct ieee80211com *ic, bool subscribe)
{
    struct ol_ath_softc_net80211 *scn = NULL;
    ol_txrx_pdev_handle pdev_txrx_handle = NULL;
    ol_txrx_soc_handle soc_txrx_handle = NULL;

    if (!ic) {
        return -1;
    }

    scn = OL_ATH_SOFTC_NET80211(ic);
    pdev_txrx_handle = wlan_pdev_get_dp_handle(scn->sc_pdev);
    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);

    scn->csa_phy_update_subscriber.callback = ieee80211_csa_interop_update;
    scn->csa_phy_update_subscriber.context = ic->ic_pdev_obj;

#if WDI_EVENT_ENABLE
    if (subscribe) {
        return cdp_wdi_event_sub(soc_txrx_handle, (void *) pdev_txrx_handle,
                &scn->csa_phy_update_subscriber, WDI_EVENT_RX_PPDU_DESC);
    } else {
        return cdp_wdi_event_unsub(soc_txrx_handle, (void *) pdev_txrx_handle,
                &scn->csa_phy_update_subscriber, WDI_EVENT_RX_PPDU_DESC);
    }
#endif
    return -1;
}

void
ol_ath_mgmt_attach(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    ol_txrx_pdev_handle pdev_txrx_handle = wlan_pdev_get_dp_handle(scn->sc_pdev);
    ol_txrx_soc_handle soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);

    /* TODO:
     * Disable this WMI xmit logic once we have the transport ready
     * for management frames
     */
    ic->ic_newassoc = ol_ath_net80211_newassoc;
    ic->ic_addba_clearresponse = ol_ath_net80211_addba_clearresponse;
    ic->ic_addba_send = ol_ath_net80211_addba_send;
    ic->ic_delba_send = ol_ath_net80211_delba_send;
    ic->ic_addba_setresponse = ol_ath_net80211_addba_setresponse;
    ic->ic_send_singleamsdu = ol_ath_net80211_send_singleamsdu;
    ic->ic_subscribe_csa_interop_phy = ol_ath_subscribe_csa_interop_phy;


#if WDI_EVENT_ENABLE
    scn->soc->scn_rx_peer_invalid_subscriber.callback      = rx_peer_invalid;
    scn->soc->scn_rx_peer_invalid_subscriber.context       = scn;
    cdp_wdi_event_sub(soc_txrx_handle, (void *) pdev_txrx_handle, &scn->soc->scn_rx_peer_invalid_subscriber, WDI_EVENT_RX_PEER_INVALID);
    scn->scn_last_peer_invalid_time = 0;
    scn->scn_peer_invalid_cnt = 0;
    scn->scn_user_peer_invalid_cnt = 1 ;/* By default we will send one deauth in 10 msec in response to rx_peer_invalid */

    scn->htt_stats_subscriber.callback = htt_stats_callback;
    scn->htt_stats_subscriber.context = scn;
    cdp_wdi_event_sub(soc_txrx_handle, (void *) pdev_txrx_handle,
                      &scn->htt_stats_subscriber,
                      WDI_EVENT_HTT_STATS);

#if QCA_SUPPORT_SON
    scn->peer_stats_subscriber.callback = son_bs_stats_update_cb;
#endif
    scn->peer_stats_subscriber.context = pdev_txrx_handle;
    cdp_wdi_event_sub(soc_txrx_handle, (void *) pdev_txrx_handle,
        &scn->peer_stats_subscriber, WDI_EVENT_PEER_STATS);

    scn->dp_stats_subscriber.callback = ol_ath_update_dp_stats;
    scn->dp_stats_subscriber.context = ic->ic_pdev_obj;
    cdp_wdi_event_sub(soc_txrx_handle, (void *) pdev_txrx_handle,
        &scn->dp_stats_subscriber, WDI_EVENT_UPDATE_DP_STATS);
#endif
    /* should always be equal to define DEFAULT_LOWEST_RATE_IN_5GHZ 0x03  6 Mbps  in firmware */
    scn->ol_rts_cts_rate = 0x03;
    qdf_nbuf_queue_init(&scn->mgmt_ctx.mgmt_backlog_queue);
    qdf_spinlock_create(&scn->mgmt_ctx.mgmt_backlog_queue_lock);
    qdf_atomic_init(&scn->mgmt_ctx.mgmt_pending_completions);
   /* register txmgmt completion call back */
    if (!wlan_psoc_nif_fw_ext_cap_get(scn->soc->psoc_obj,
                                  WLAN_SOC_CEXT_WMI_MGMT_REF)) {
        cdp_mgmt_tx_cb_set(soc_txrx_handle, (struct cdp_pdev *)pdev_txrx_handle,
			    (OL_TXRX_MGMT_NUM_TYPES-1),
			    NULL,
			    ol_ath_wmi_over_htt_comp_handler,
			    scn->sc_pdev);
        scn->mgmt_ctx.mgmt_pending_max = MGMT_TARGET_SUPPORTED_MAX_HTT;
	scn->mgmt_ctx.mgmt_pending_probe_resp_threshold = (MGMT_TARGET_SUPPORTED_MAX_HTT * 3) / 4;
    } else {
	scn->mgmt_ctx.mgmt_pending_max = MGMT_TARGET_SUPPORTED_MAX_WMI;
        scn->mgmt_ctx.mgmt_pending_probe_resp_threshold = (MGMT_TARGET_SUPPORTED_MAX_WMI * 3) / 4;
    }
}

void ol_ath_mgmt_detach(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    if (wlan_psoc_nif_fw_ext_cap_get(scn->soc->psoc_obj,
                                  WLAN_SOC_CEXT_WMI_MGMT_REF)) {
        if (!qdf_nbuf_is_queue_empty(&scn->mgmt_ctx.mgmt_backlog_queue)) {
            qdf_print("Mgmt frames still pending in queue");
            QDF_BUG(0);
        }
        qdf_spinlock_destroy(&scn->mgmt_ctx.mgmt_backlog_queue_lock);
    }
}
#endif

/*
 * Copyright (c) 2011,2018-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 * Qualcomm Innovation Center,Inc. has chosen to take madwifi subject to the BSD license and terms.
 *
 * 2011 Qualcomm Atheros, Inc.
 * Qualcomm Atheros, Inc. has chosen to take madwifi subject to the BSD license and terms.
 *
 * Copyright (c) 2008, Atheros Communications Inc.
 */

#include "ieee80211_mlme_priv.h"
#include "ieee80211_bssload.h"
#include "ieee80211_quiet_priv.h"
#include "osif_private.h"

#include "ol_if_athvar.h"
#include "cfg_ucfg_api.h"
#include <wlan_utility.h>

#include <wlan_son_pub.h>
/* This macro is copied from FW headers*/
#define HTT_TX_EXT_TID_NONPAUSE_PRIVATE 19
/*
 * Send a probe response frame.
 * NB: for probe response, the node may not represent the peer STA.
 * We could use BSS node to reduce the memory usage from temporary node.
 */
int
ieee80211_send_proberesp(struct ieee80211_node *ni, u_int8_t *macaddr,
                         const void *optie, const size_t  optielen,
                         struct ieee80211_framing_extractx *extractx)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    wbuf_t wbuf;
    struct ieee80211_frame *wh;
    u_int8_t *frm;
    u_int16_t capinfo;
    int enable_htrates;
    bool add_wpa_ie = true;
    struct ieee80211_bwnss_map nssmap;
    u_int8_t rx_chainmask = ieee80211com_get_rx_chainmask(ic);
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    struct ieee80211_rsnparms *rsn = &vap->iv_rsn;
#endif
#if QCN_IE
    u_int16_t ie_len;
#endif
#if DBDC_REPEATER_SUPPORT
    struct global_ic_list *ic_list = ic->ic_global_list;
#endif

#if QCN_ESP_IE
    u_int16_t esp_ie_len;
#endif
    struct ieee80211vap *tmpvap = NULL;
    uint8_t num_non_trans_profiles;
    uint8_t *new = NULL;
    uint8_t *orig_frm = NULL;

    qdf_mem_zero(&nssmap, sizeof(nssmap));

    ASSERT(vap->iv_opmode == IEEE80211_M_HOSTAP || vap->iv_opmode == IEEE80211_M_IBSS ||
           vap->iv_opmode == IEEE80211_M_BTAMP);

    /*
     * XXX : This section needs more testing with P2P
     */
    if (!vap->iv_bss) {
        return 0;
    }

    wbuf = wbuf_alloc(ic->ic_osdev, WBUF_TX_MGMT, MAX_TX_RX_PACKET_SIZE);
    if (wbuf == NULL) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,
                          "%s: Error: unable to alloc wbuf of type WBUF_TX_MGMT.\n",
                          __func__);
        return -ENOMEM;
    }

    /* setup the wireless header */
    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    ieee80211_send_setup(vap, ni, wh,
                         IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_PROBE_RESP,
                         vap->iv_myaddr, macaddr,
                         ieee80211_node_get_bssid(ni));
    frm = (u_int8_t *)&wh[1];

    /*
     * probe response frame format
     *  [8] time stamp
     *  [2] beacon interval
     *  [2] cabability information
     *  [tlv] ssid
     *  [tlv] supported rates
     *  [tlv] parameter set (FH/DS)
     *  [tlv] parameter set (QUIET)
     *  [tlv] parameter set (IBSS)
     *  [tlv] extended rate phy (ERP)
     *  [tlv] extended supported rates
     *  [tlv] country (if present)
     *  [3] power constraint
     *  [tlv] WPA
     *  [tlv] WME
     *  [tlv] HT Capabilities
     *  [tlv] HT Information
     *      [tlv] Atheros Advanced Capabilities
     */
    OS_MEMZERO(frm, 8);  /* timestamp should be filled later */
    frm += 8;
    *(u_int16_t *)frm = htole16(vap->iv_bss->ni_intval);
    frm += 2;
    if (vap->iv_opmode == IEEE80211_M_IBSS){
        if(ic->ic_softap_enable)
            capinfo = IEEE80211_CAPINFO_ESS;
        else
            capinfo = IEEE80211_CAPINFO_IBSS;
	}
    else
        capinfo = IEEE80211_CAPINFO_ESS;
    if (IEEE80211_VAP_IS_PRIVACY_ENABLED(vap))
        capinfo |= IEEE80211_CAPINFO_PRIVACY;
    if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
        IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan))
        capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
    if (ic->ic_flags & IEEE80211_F_SHSLOT)
        capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
    if (ieee80211_ic_doth_is_set(ic) && ieee80211_vap_doth_is_set(vap))
        capinfo |= IEEE80211_CAPINFO_SPECTRUM_MGMT;
    if (ieee80211_vap_rrm_is_set(vap)) {
        capinfo |= IEEE80211_CAPINFO_RADIOMEAS;
    }
    *(u_int16_t *)frm = htole16(capinfo);
    frm += 2;

    frm = ieee80211_add_ssid(frm, vap->iv_bss->ni_essid,
                             vap->iv_bss->ni_esslen);
    frm = ieee80211_add_rates(frm, &vap->iv_bss->ni_rates);

    if (!IEEE80211_IS_CHAN_FHSS(vap->iv_bsschan)) {
        *frm++ = IEEE80211_ELEMID_DSPARMS;
        *frm++ = 1;
        *frm++ = ieee80211_chan2ieee(ic, ic->ic_curchan);
    }

    if (vap->iv_opmode == IEEE80211_M_IBSS) {
        *frm++ = IEEE80211_ELEMID_IBSSPARMS;
        *frm++ = 2;
        *frm++ = 0; *frm++ = 0;     /* TODO: ATIM window */
    }

    if (IEEE80211_IS_COUNTRYIE_ENABLED(ic) && ieee80211_vap_country_ie_is_set(vap)) {
        frm = ieee80211_add_country(frm, vap);
    }

    if (ieee80211_ic_doth_is_set(ic) && ieee80211_vap_doth_is_set(vap)) {
        *frm++ = IEEE80211_ELEMID_PWRCNSTR;
        *frm++ = 1;
        *frm++ = IEEE80211_PWRCONSTRAINT_VAL(vap);
    }

    frm = ieee80211_add_quiet(vap, ic, frm);

#if ATH_SUPPORT_IBSS_DFS
    if (vap->iv_opmode == IEEE80211_M_IBSS) {
        frm =  ieee80211_add_ibss_dfs(frm,vap);
    }
#endif

    if (IEEE80211_IS_CHAN_ANYG(ic->ic_curchan) ||
        IEEE80211_IS_CHAN_11NG(ic->ic_curchan) ||
        IEEE80211_IS_CHAN_11AXG(ic->ic_curchan)) {
        frm = ieee80211_add_erp(frm, ic);
    }

    /*
     * check if os shim has setup RSN IE it self.
     */
    if (!vap->iv_rsn_override) {
    IEEE80211_VAP_LOCK(vap);
    if (vap->iv_app_ie[IEEE80211_FRAME_TYPE_PROBERESP].length) {
        add_wpa_ie = ieee80211_check_wpaie(vap, vap->iv_app_ie[IEEE80211_FRAME_TYPE_PROBERESP].ie,
                                           vap->iv_app_ie[IEEE80211_FRAME_TYPE_PROBERESP].length);
    }
    /*
     * Check if RSN ie is present in probe response frame. Traverse the linked list
     * and check for RSN IE.
     * If not present, add it in the driver
     */
    add_wpa_ie=ieee80211_mlme_app_ie_check_wpaie(vap,IEEE80211_FRAME_TYPE_PROBERESP);
    if (vap->iv_opt_ie.length) {
        add_wpa_ie = ieee80211_check_wpaie(vap, vap->iv_opt_ie.ie,
                                           vap->iv_opt_ie.length);
    }
    IEEE80211_VAP_UNLOCK(vap);
    }
#if ATH_SUPPORT_HS20
    if (add_wpa_ie && !vap->iv_osen && !vap->iv_rsn_override) {
#else
    if (add_wpa_ie && !vap->iv_rsn_override) {
#endif
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
        if (wlan_crypto_vdev_has_auth_mode(vap->vdev_obj, (1 << WLAN_CRYPTO_AUTH_RSNA))) {
            frm = wlan_crypto_build_rsnie(vap->vdev_obj, frm, NULL);
            if (!frm) {
                wbuf_release(ic->ic_osdev, wbuf);
                return -EINVAL;
            }
        }
#else
        if (RSN_AUTH_IS_RSNA(rsn))
            frm = ieee80211_setup_rsn_ie(vap, frm);
#endif
    }

#if ATH_SUPPORT_WAPI
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
    if (wlan_crypto_vdev_has_auth_mode(vap->vdev_obj, (1 << WLAN_CRYPTO_AUTH_WAPI)))
#else
    if (RSN_AUTH_IS_WAI(rsn))
#endif
    {
        frm = ieee80211_setup_wapi_ie(vap, frm);
        if (!frm) {
            wbuf_release(ic->ic_osdev, wbuf);
            return -EINVAL;
        }
    }
#endif

    frm = ieee80211_add_xrates(frm, &vap->iv_bss->ni_rates);

    frm = ieee80211_add_qbssload(frm, ni);

    /* Add MBSS IE */
    if (wlan_pdev_nif_feat_cap_get(ic->ic_pdev_obj, WLAN_PDEV_F_MBSS_IE_ENABLE)) {
        orig_frm = frm;
        num_non_trans_profiles = 0;
        TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
            if (IEEE80211_VAP_IS_MBSS_NON_TRANSMIT_ENABLED(tmpvap) &&
                    tmpvap->iv_mbss.mbssid_send_bcast_probe_resp) {
                ni = tmpvap->iv_bss;
                new = ieee80211_add_update_mbssid_ie(frm,
                       ni, &num_non_trans_profiles,
                       IEEE80211_FRAME_TYPE_PROBERESP);
                if (!new) {
                    wbuf_release(ic->ic_osdev, wbuf);
                    return -EINVAL;
                }
                frm = orig_frm;
            }
        } /* TAILQ_FOREACH */
        if (num_non_trans_profiles != 0)
            frm = new;
    }

    /* Add rrm capbabilities, if supported */
    frm = ieee80211_add_rrm_cap_ie(frm, ni);

    enable_htrates = ieee80211vap_htallowed(vap);

    if (ieee80211_vap_wme_is_set(vap) &&
        (IEEE80211_IS_CHAN_11AX(ic->ic_curchan) ||
         IEEE80211_IS_CHAN_11AC(ic->ic_curchan) ||
         IEEE80211_IS_CHAN_11N(ic->ic_curchan)) &&
        enable_htrates) {
        frm = ieee80211_add_htcap(frm, ni, IEEE80211_FC0_SUBTYPE_PROBE_RESP);

        frm = ieee80211_add_htinfo(frm, ni);

        if (!(ic->ic_flags & IEEE80211_F_COEXT_DISABLE)) {
            frm = ieee80211_add_obss_scan(frm, ni);
        }
    }

    /* Add extended capbabilities, if applicable */
    frm = ieee80211_add_extcap(frm, ni, IEEE80211_FC0_SUBTYPE_PROBE_RESP);

    /*  VHT capable  */
      /* Add vht cap for 2.4G mode, if 256QAM is enabled */
    if (ieee80211_vap_wme_is_set(vap) &&
        (IEEE80211_IS_CHAN_11AX(ic->ic_curchan) ||
         IEEE80211_IS_CHAN_11AC(ic->ic_curchan) ||
         IEEE80211_IS_CHAN_11NG(ic->ic_curchan)) &&
        ieee80211vap_vhtallowed(vap)) {

        /* Add VHT capabilities IE */
        if (ASSOCWAR160_IS_VHT_CAP_CHANGE(vap->iv_cfg_assoc_war_160w))
            frm = ieee80211_add_vhtcap(frm, ni, ic,
                    IEEE80211_FC0_SUBTYPE_PROBE_RESP, extractx, macaddr);
        else
            frm = ieee80211_add_vhtcap(frm, ni, ic,
                    IEEE80211_FC0_SUBTYPE_PROBE_RESP, NULL, macaddr);

        /* Add VHT Operation IE */
        frm = ieee80211_add_vhtop(frm, ni, ic, IEEE80211_FC0_SUBTYPE_PROBE_RESP, extractx);

        /* Add Extended BSS Load IE */
        frm = ieee80211_add_ext_bssload(frm, ni);

        /* Add VHT TX power Envelope IE */
        if (ieee80211_ic_doth_is_set(ic) && ieee80211_vap_doth_is_set(vap)) {
            frm = ieee80211_add_vht_txpwr_envlp(frm, ni, ic, IEEE80211_FC0_SUBTYPE_PROBE_RESP,
                                                         !IEEE80211_VHT_TXPWR_IS_SUB_ELEMENT);
            /* If iv_chanchange_count is non zero and ic_chanchange_channel is not null,
               it means vap is in chan/ch_width switch period */
            if(vap->iv_chanchange_count && (ic->ic_chanchange_channel != NULL)) {
                uint8_t chanchange_tbtt = ic->ic_chanchange_tbtt - vap->iv_chanchange_count;

                frm = ieee80211_mgmt_add_chan_switch_ie(frm, ni,
                              IEEE80211_FC0_SUBTYPE_PROBE_RESP, chanchange_tbtt);
            }
        }
    }

    if (ieee80211_vap_wme_is_set(vap) &&  IEEE80211_IS_CHAN_11AX(ic->ic_curchan)
         && ieee80211vap_heallowed(vap)) {
        /* Add HE Capabilities IE */
        frm = ieee80211_add_hecap(frm, ni, ic, IEEE80211_FC0_SUBTYPE_PROBE_RESP);

        /* Add HE Operation IE */
        frm = ieee80211_add_heop(frm, ni, ic, IEEE80211_FC0_SUBTYPE_PROBE_RESP, extractx);
    }


    if (add_wpa_ie && !vap->iv_rsn_override) {
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
        if (wlan_crypto_vdev_has_auth_mode(vap->vdev_obj, (1 << WLAN_CRYPTO_AUTH_WPA))) {
            frm = wlan_crypto_build_wpaie(vap->vdev_obj, frm);
            if (!frm) {
                wbuf_release(ic->ic_osdev, wbuf);
                return -EINVAL;
            }
        }
#else
        if (RSN_AUTH_IS_WPA(rsn))
            frm = ieee80211_setup_wpa_ie(vap, frm);
#endif
    }

#ifdef OBSS_PD
    if (ic->ic_he_sr_enable &&
        IEEE80211_IS_CHAN_11AX(ic->ic_curchan) && ieee80211vap_heallowed(vap)) {
        frm = ieee80211_add_srp_ie(ic, frm);
    }
#endif

    /* Add MU EDCA parameter element */
    if(ieee80211_vap_wme_is_set(vap) &&
            ieee80211vap_heallowed(vap) &&
            ieee80211vap_muedca_is_enabled(vap)) {
        frm = ieee80211_add_muedca_param(frm, &vap->iv_muedcastate);
    }

    if (ieee80211_vap_wme_is_set(vap) &&
        (vap->iv_opmode == IEEE80211_M_HOSTAP || vap->iv_opmode == IEEE80211_M_BTAMP)) /* don't support WMM in ad-hoc for now */
        frm = ieee80211_add_wme_param(frm, &vap->iv_wmestate, IEEE80211_VAP_IS_UAPSD_ENABLED(vap));

    if (ieee80211_vap_wme_is_set(vap) &&
        (IEEE80211_IS_CHAN_11AX(ic->ic_curchan) ||
         IEEE80211_IS_CHAN_11AC(ic->ic_curchan) ||
         IEEE80211_IS_CHAN_11N(ic->ic_curchan)) &&
        (IEEE80211_IS_HTVIE_ENABLED(ic)) && enable_htrates) {
        frm = ieee80211_add_htcap_vendor_specific(frm, ni, IEEE80211_FC0_SUBTYPE_PROBE_RESP);
        frm = ieee80211_add_htinfo_vendor_specific(frm, ni);
    }
    if (vap->iv_ena_vendor_ie == 1) {
	    if (vap->iv_bss->ni_ath_flags) {
		    frm = ieee80211_add_athAdvCap(frm, vap->iv_bss->ni_ath_flags,
				    vap->iv_bss->ni_ath_defkeyindex);
	    } else {
		    frm = ieee80211_add_athAdvCap(frm, 0, IEEE80211_INVAL_DEFKEY);
	    }
    }
    /* Insert ieee80211_ie_ath_extcap IE to beacon */
    if (ic->ic_ath_extcap)
        frm = ieee80211_add_athextcap(frm, ic->ic_ath_extcap, ic->ic_weptkipaggr_rxdelim);

    if (!(vap->iv_ext_nss_support) && !(ic->ic_disable_bwnss_adv) && !ieee80211_get_bw_nss_mapping(vap, &nssmap, rx_chainmask))  {
        frm = ieee80211_add_bw_nss_maping(frm, &nssmap);
    }


    IEEE80211_VAP_LOCK(vap);
    if (vap->iv_opt_ie.length) {
        OS_MEMCPY(frm, vap->iv_opt_ie.ie,
                  vap->iv_opt_ie.length);
        frm += vap->iv_opt_ie.length;
    }

    if (vap->iv_app_ie[IEEE80211_FRAME_TYPE_PROBERESP].length) {
        OS_MEMCPY(frm, vap->iv_app_ie[IEEE80211_FRAME_TYPE_PROBERESP].ie,
                  vap->iv_app_ie[IEEE80211_FRAME_TYPE_PROBERESP].length);
        frm += vap->iv_app_ie[IEEE80211_FRAME_TYPE_PROBERESP].length;
    }
    /* Add hardware & software version in probe response */
    frm = ieee80211_add_sw_version_ie(frm, ic);

    /* Add the Application IE's */
    frm = ieee80211_mlme_app_ie_append(vap, IEEE80211_FRAME_TYPE_PROBERESP, frm);
    IEEE80211_VAP_UNLOCK(vap);

    if (IEEE80211_VAP_IS_WDS_ENABLED(vap) &&
            !son_vdev_map_capability_get(vap->vdev_obj, SON_MAP_CAPABILITY)) {
        u_int16_t whcCaps = QCA_OUI_WHC_AP_INFO_CAP_WDS;
        u_int16_t ie_len;

        /* SON mode requires WDS as a prereq */
        if (son_vdev_feat_capablity(vap->vdev_obj,
                                    SON_CAP_GET,
                                    WLAN_VDEV_F_SON)) {
            whcCaps |= QCA_OUI_WHC_AP_INFO_CAP_SON;
        }

        frm = son_add_ap_info_ie(frm, whcCaps, vap->vdev_obj, &ie_len);
    }
#if QCN_IE
    /*Add QCN IE for the feature set*/
    frm = ieee80211_add_qcn_info_ie(frm, vap, &ie_len, NULL);
    frm = ieee80211_add_qcn_ie_mac_phy_params(frm, vap);
#endif
#if QCN_ESP_IE
    if (ic->ic_esp_periodicity){
        frm = ieee80211_add_esp_info_ie(frm, ic, &esp_ie_len);
    }
#endif

    if (vap->ap_chan_rpt_enable) {
        frm = ieee80211_add_ap_chan_rpt_ie (frm, vap);
    }

    if (vap->rnr_enable) {
        if (IEEE80211_IS_BROADCAST(macaddr) || (extractx->oce_sta) || (extractx->fils_sta)) {
            frm = ieee80211_add_rnr_ie(frm, vap, extractx->ssid, extractx->ssid_len);
        }
    }

    if (ieee80211_vap_mbo_check(vap) || ieee80211_vap_oce_check(vap)) {
        frm = ieee80211_setup_mbo_ie(IEEE80211_FC0_SUBTYPE_PROBE_RESP, vap, frm, ni);
    }

    if (optie != NULL && optielen != 0) {
        OS_MEMCPY(frm, optie, optielen);
        frm += optielen;
    }
#if DBDC_REPEATER_SUPPORT
    if (ic_list->same_ssid_support) {
        /* Add the Extender IE */
        frm = ieee80211_add_extender_ie(vap, IEEE80211_FRAME_TYPE_PROBERESP, frm);
    }
#endif
    wbuf_set_pktlen(wbuf, (frm - (u_int8_t *)wbuf_header(wbuf)));
    if (extractx->datarate) {
        if (extractx->datarate == 6000)       /* 6 Mbps */
            wbuf_set_tx_rate(wbuf, 0, RATECODE_PREAM_OFDM, 3, ic->ic_he_target);
        else if (extractx->datarate == 5500)  /* 5.5 Mbps */
            wbuf_set_tx_rate(wbuf, 0, RATECODE_PREAM_CCK, 1, ic->ic_he_target);
        else if (extractx->datarate == 2000)  /* 2 Mbps */
            wbuf_set_tx_rate(wbuf, 0, RATECODE_PREAM_CCK, 2, ic->ic_he_target);
        else                                  /* 1 Mbps */
            wbuf_set_tx_rate(wbuf, 0, RATECODE_PREAM_CCK, 3, ic->ic_he_target);

		/* tid should be set to HTT_TX_EXT_TID_NONPAUSE to apply tx_rate */
        wbuf_set_tid(wbuf, HTT_TX_EXT_TID_NONPAUSE_PRIVATE);
    }

    return ieee80211_send_mgmt(vap,ni, wbuf,true);
}

/* Determine whether probe response needs modification towards 160 MHz width
   association WAR.
 */
static bool
is_assocwar160_reqd_proberesp(struct ieee80211vap *vap,
        struct ieee80211_ie_ssid *probereq_ssid_ie,
        struct ieee80211_ie_vhtcap *sta_vhtcap)
{
    int is_sta_any160cap = 0;

    qdf_assert_always(vap != NULL);
    qdf_assert_always(probereq_ssid_ie != NULL);

    /* Since this WAR is deprecated, it will not be made available for 11ax. */

    if (((vap->iv_cur_mode != IEEE80211_MODE_11AC_VHT160) &&
                (vap->iv_cur_mode != IEEE80211_MODE_11AC_VHT80_80)) ||
        !vap->iv_cfg_assoc_war_160w) {
        return false;
    }

    /* The WAR is required only for STAs not having any 160/80+80 MHz
     * capability. */
    if (sta_vhtcap == NULL) {
        return true;
    }

    is_sta_any160cap =
        ((sta_vhtcap->vht_cap_info &
            (IEEE80211_VHTCAP_SUP_CHAN_WIDTH_160 |
             IEEE80211_VHTCAP_SUP_CHAN_WIDTH_80_160 |
             IEEE80211_VHTCAP_SHORTGI_160)) != 0);

    if (is_sta_any160cap) {
        return false;
    }

    return true;
}

int
ieee80211_recv_probereq(struct ieee80211_node *ni, wbuf_t wbuf, int subtype,
                        struct ieee80211_rx_status *rs)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_frame *wh;
    unsigned int found_vap  = 0;
    unsigned int found_null_bssid = 0;
    int ret = -EINVAL;
    u_int8_t *frm, *efrm;
    u_int8_t *ssid, *rates, *ven;
#if ATH_SUPPORT_HS20 || QCN_IE
    u_int8_t *iw = NULL, *xcaps = NULL;
    uint8_t empty[IEEE80211_ADDR_LEN] = {0x00,0x00,0x00,0x00,0x00,0x00};
#endif
#if QCN_IE
    u_int8_t *qcn = NULL;

    /*
     * Max-ChannelTime parameter represented in units of TUs
     * 255 used to indicate any duration of more than 254 TUs, or an
     * unspecified or unknown duration.
     */
    u_int8_t channel_time = 0;
    /* Index 0 has version and index 1 has subversion of QCN IE*/
    u_int8_t data[2] = {0};
    qdf_ktime_t eff_chan_time, bpr_delay;
    qdf_hrtimer_data_t *bpr_gen_timer = &vap->bpr_timer;
#endif
    u_int8_t *mbo = NULL;
    bool suppress_resp = false;
    u_int8_t nullbssid[IEEE80211_ADDR_LEN] = {0x00,0x00,0x00,0x00,0x00,0x00};
    int snd_prb_resp = 0;
    struct ieee80211_ie_vhtcap *vhtcap = NULL;
    struct ieee80211_ie_hecap  *hecap  = NULL;
    struct ieee80211_ie_heop   *heop   = NULL;
    struct ieee80211_framing_extractx extractx;
    u_int8_t dedicated_oui_present = 0;

    OS_MEMZERO(&extractx, sizeof(extractx));
#if ATH_SUPPORT_AP_WDS_COMBO
    if (vap->iv_opmode == IEEE80211_M_STA ||
        (wlan_vdev_is_up(vap->vdev_obj) != QDF_STATUS_SUCCESS) ||
        vap->iv_no_beacon) {
#else
    if (vap->iv_opmode == IEEE80211_M_STA ||
        (wlan_vdev_is_up(vap->vdev_obj) != QDF_STATUS_SUCCESS)) {
#endif
#ifdef QCA_SUPPORT_CP_STATS
        vdev_cp_stats_rx_mgmt_discard_inc(vap->vdev_obj, 1);
#endif
        return -EINVAL;
    }

    wh = (struct ieee80211_frame *) wbuf_header(wbuf);
    frm = (u_int8_t *)&wh[1];
    efrm = wbuf_header(wbuf) + wbuf_get_pktlen(wbuf);

    if (IEEE80211_IS_MULTICAST(wh->i_addr2)) {
        /* frame must be directed */
#ifdef QCA_SUPPORT_CP_STATS
        vdev_cp_stats_rx_mgmt_discard_inc(vap->vdev_obj, 1);
#endif
        return -EINVAL;
    }

    /* Drop mcast Probe requests if Tx buffers availability goes low */
    if ((IEEE80211_IS_MULTICAST(wh->i_addr1)) &&
            (!ic->ic_is_mode_offload(ic)) &&
            (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan)) &&
            (vap->iv_opmode == IEEE80211_M_HOSTAP))
    {
        u_int32_t txbuf_free_count = 0, nvaps = 0, drop_threshold = 0;

        nvaps = ieee80211_get_num_active_vaps(ic);
        txbuf_free_count = ic->ic_get_txbuf_free(ic);
        if (nvaps > 8)
            drop_threshold = ATH_TXBUF/3;
        else
            drop_threshold = ATH_TXBUF/4;

        if (txbuf_free_count <= drop_threshold)
            goto exit;
    }

#if UMAC_SUPPORT_NAWDS
    /* Skip probe request if configured as NAWDS bridge */
    if(vap->iv_nawds.mode == IEEE80211_NAWDS_STATIC_BRIDGE
		  || vap->iv_nawds.mode == IEEE80211_NAWDS_LEARNING_BRIDGE) {
        return -EINVAL;
    }
#endif
    /*Update node if ni->bssid is NULL*/
    if(!OS_MEMCMP(ni->ni_bssid,nullbssid,IEEE80211_ADDR_LEN))
    {
        ni = ieee80211_try_ref_bss_node(vap);
        if(ni == NULL) {
            return -EINVAL;
        }

        found_null_bssid = 1;
    }
#if QCA_SUPPORT_SON
    son_send_probereq_event(vap->vdev_obj, wh->i_addr2, rs->rs_rssi);

    if (IEEE80211_IS_CHAN_2GHZ(vap->iv_bsschan) &&
        son_is_probe_resp_wh_2G(vap->vdev_obj, wh->i_addr2,
                                rs->rs_rssi)) {
            return 0;
    }

    /* If band steering is withholding probes (due to steering being in
     * progress), return here so that the response is not sent.
     */
    if (son_is_probe_resp_wh(vap->vdev_obj, wh->i_addr2)) {
        return 0;
    }
#else
    // To silence compiler warning about unused variable.
    (void) rs;
#endif
#if ATH_PARAMETER_API
    ieee80211_papi_send_probe_req_event(vap, ni, wbuf, rs);
#endif
    /*
     * prreq frame format
     *  [tlv] ssid
     *  [tlv] supported rates
     *  [tlv] extended supported rates
     *  [tlv] Atheros Advanced Capabilities
     */
    ssid = rates = NULL;
    while (((frm+1) < efrm) && (frm + frm[1] + 1 < efrm)) {
        switch (*frm) {
        case IEEE80211_ELEMID_SSID:
            ssid = frm;
            break;
        case IEEE80211_ELEMID_RATES:
            rates = frm;
            break;
#if ATH_SUPPORT_HS20
        case IEEE80211_ELEMID_XCAPS:
            xcaps = frm;
            break;
        case IEEE80211_ELEMID_INTERWORKING:
            iw = frm;
            break;
#endif
        case IEEE80211_ELEMID_VENDOR:
            if (vap->iv_venie && vap->iv_venie->ven_oui_set) {
                ven = frm;
                if (ven[2] == vap->iv_venie->ven_oui[0] &&
                    ven[3] == vap->iv_venie->ven_oui[1] &&
                    ven[4] == vap->iv_venie->ven_oui[2]) {
                    vap->iv_venie->ven_ie_len = MIN(ven[1] + 2, IEEE80211_MAX_IE_LEN);
                    OS_MEMCPY(vap->iv_venie->ven_ie, ven, vap->iv_venie->ven_ie_len);
                }
            }
            if (isdedicated_cap_oui(frm)) {
                dedicated_oui_present = 1;
            }
            else if ((vhtcap == NULL) &&
                    /*
                     * Standalone-VHT CAP IE outside
                     * of Interop IE
                     * will obviously supercede
                     * VHT CAP inside interop IE
                     */
                    ieee80211vap_11ng_vht_interopallowed(vap) &&
                    isinterop_vht(frm)) {
                /* frm+7 is the location , where 2.4G Interop VHT IE starts */
                vhtcap = (struct ieee80211_ie_vhtcap *) (frm + 7);
            }
#if QCN_IE
            else if(isqcn_oui(frm)) {
                qcn = frm;
            }
#endif
            else if (ismbooui(frm)) {
                mbo = frm;
            }

            if ( snd_prb_resp == 0 ) {
                snd_prb_resp = isorbi_ie(vap, frm);
              }
            break;
        case IEEE80211_ELEMID_VHTCAP:
            vhtcap = (struct ieee80211_ie_vhtcap *)frm;
            break;

        case WLAN_ELEMID_EXTN_ELEM:
            if (((frm + IEEE80211_HE_IE_HDR_OFFSET_TO_ID_EXT) < efrm) &&
                (*(frm + IEEE80211_HE_IE_HDR_OFFSET_TO_ID_EXT)
                     == IEEE80211_ELEMID_EXT_HECAP)) {
                hecap = (struct ieee80211_ie_hecap *)frm;
            } else if (((frm + IEEE80211_HE_IE_HDR_OFFSET_TO_ID_EXT) < efrm) &&
                    (*(frm + IEEE80211_HE_IE_HDR_OFFSET_TO_ID_EXT)
                         == IEEE80211_ELEMID_EXT_HEOP)) {
                heop = (struct ieee80211_ie_heop *)frm;
            }
#if QCN_IE
            else if(isfils_req_parm(frm)) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,"FILS STA found mac[%s] \n",ether_sprintf(wh->i_addr2));
                /* Get the Channel time |IE|LEN|EXT|BITMAP|CHANNEL TIME|..| skip Parameter Control Bitmap */
                if(frm[4]) {
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,"FILS Max channel time : %uTU\n",frm[4]);
                    channel_time = frm[4];
                    eff_chan_time = qdf_ns_to_ktime(QDF_NSEC_PER_MSEC *
                                EFF_CHAN_TIME((channel_time * 1024)/1000, ic->ic_bpr_latency_comp));
                }
                else {
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,"FILS STA with invalid IE Ignoring \n");
                }
            }
#endif
            break;
        }

        /* elem id + len = 2 bytes */
        frm += frm[1] + 2;
    }

    if (frm > efrm) {
        ret = -EINVAL;
        goto exit;
    }

    if (dedicated_oui_present &&
        (vhtcap != NULL) &&
        (le32toh(vhtcap->vht_cap_info) & IEEE80211_VHTCAP_MU_BFORMEE)) {

        ni->dedicated_client = 1;
    }

    IEEE80211_VERIFY_ELEMENT(rates, IEEE80211_RATE_MAXSIZE);
    IEEE80211_VERIFY_ELEMENT(ssid, IEEE80211_NWID_LEN);

    /* update rate and rssi information */
#ifdef QCA_SUPPORT_CP_STATS
    WLAN_PEER_CP_STAT_SET(ni, rx_mgmt_rate, rs->rs_datarate);
    WLAN_PEER_CP_STAT_SET(ni, rx_mgmt_rssi, rs->rs_rssi);
#endif

    IEEE80211_DELIVER_EVENT_RECV_PROBE_REQ(vap, wh->i_addr2, ssid);
    /*
     * XXX bug fix 107944: STA Entry exists in the node table,
     * But the STA want to associate with the other vap,  vap should
     * send the correct proble response to Station.
     *
     */

    if(IEEE80211_MATCH_SSID(vap->iv_bss, ssid)  == 1)  //ssid not match
    {
        struct ieee80211vap *tmpvap = NULL;
        if(ni != vap->iv_bss)
        {
            TAILQ_FOREACH(tmpvap, &(ic)->ic_vaps, iv_next)
            {
                if((tmpvap->iv_opmode == IEEE80211_M_HOSTAP) && (!IEEE80211_MATCH_SSID(tmpvap->iv_bss, ssid)))
                {
                        found_vap = 1;
                        break;
                }
            }
        }
        if(found_vap  == 1)
        {
            ni = ieee80211_ref_bss_node(tmpvap);
            if ( ni ) {
                vap = ni->ni_vap;
            }
        }
        else
        {
#ifdef QCA_SUPPORT_CP_STATS
            vdev_cp_stats_rx_ssid_mismatch_inc(vap->vdev_obj, 1);
#endif
            goto exit;
        }

    }

#if ATH_ACL_SOFTBLOCKING
    if (ssid[1] != 0) { // directed probe request.
        if (!wlan_acl_check_softblocking(vap, wh->i_addr2)) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACL,
                    "Directed Probe Req Frames from %s are softblocked\n",
                    ether_sprintf(wh->i_addr2));
#ifdef QCA_SUPPORT_CP_STATS
            vdev_cp_stats_rx_mgmt_discard_inc(vap->vdev_obj, 1);
#endif
            goto exit;
        }
    }
#endif

    if (IEEE80211_VAP_IS_HIDESSID_ENABLED(vap) && (ssid[1] == 0) && !(IEEE80211_VAP_IS_BACKHAUL_ENABLED(vap))) {
        IEEE80211_DISCARD(vap, IEEE80211_MSG_INPUT,
                          wh, ieee80211_mgt_subtype_name[
                              subtype >> IEEE80211_FC0_SUBTYPE_SHIFT],
                          "%s", "no ssid with ssid suppression enabled");
#ifdef QCA_SUPPORT_CP_STATS
        vdev_cp_stats_rx_ssid_mismatch_inc(vap->vdev_obj, 1);
#endif
        goto exit;
    }

#if DYNAMIC_BEACON_SUPPORT
    /*
     * If probe req received from non associated STA,
     * check the rssi and send probe resp.
     */
    if (vap->iv_dbeacon == 1 && vap->iv_dbeacon_runtime == 1) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_DEBUG, "node(%s): rs_rssi %d, iv_dbeacon_rssi_thr: %d \n",
                ether_sprintf(wh->i_addr2),rs->rs_rssi, vap->iv_dbeacon_rssi_thr);
        if (rs->rs_rssi < vap->iv_dbeacon_rssi_thr) {
            /* don't send probe resp if rssi is low. */
#ifdef QCA_SUPPORT_CP_STATS
            vdev_cp_stats_rx_mgmt_discard_inc(vap->vdev_obj, 1);
#endif
            goto exit;
        }
    }
#endif

#if ATH_SUPPORT_HS20
    if (!IEEE80211_ADDR_EQ(vap->iv_hessid, &empty)) {
        if (iw && !xcaps)
            goto exit;
        if (iw && (xcaps[5] & 0x80)) {
            /* hessid match ? */
            if (iw[1] == 9 && !IEEE80211_ADDR_EQ(iw+5, vap->iv_hessid) && !IEEE80211_ADDR_EQ(iw+5, IEEE80211_GET_BCAST_ADDR(ic)))
                goto exit;
            if (iw[1] == 7 && !IEEE80211_ADDR_EQ(iw+3, vap->iv_hessid) && !IEEE80211_ADDR_EQ(iw+3, IEEE80211_GET_BCAST_ADDR(ic)))
                goto exit;
            /* access_network_type match ? */
            if ((iw[2] & 0xF) != vap->iv_access_network_type && (iw[2] & 0xF) != 0xF)
                goto exit;
        }
    }
#endif

#if QCN_IE
    if (xcaps) {
        struct ieee80211_ie_ext_cap *extcaps = (struct ieee80211_ie_ext_cap *) xcaps;

        if ((extcaps->elem_len > 9) && (extcaps->ext_capflags4 & IEEE80211_EXTCAPIE_FILS)) {
            extractx.fils_sta = true;
        }
    }

    if (ssid) {
        extractx.ssid_len = *(ssid + 1);
        OS_MEMCPY(extractx.ssid, ssid + 2, extractx.ssid_len);
    }

    if (qcn && ni) {
        /*
         * Record qcn parameters for station, mark
         * node as using qcn and record information element
         * for applications that require it.
         */
          ieee80211_parse_qcnie(qcn, wh, ni,data);
    }
#endif
    if (mbo && ieee80211_vap_oce_check(vap)) {
        extractx.oce_sta = ieee80211_oce_capable(mbo);
        suppress_resp = ieee80211_oce_suppress_ap(mbo, vap);

        if (suppress_resp) {
            /* Drop the probe response */
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "Suppress probe response: %d for vap %pK\n", suppress_resp, vap);
            goto exit;
        }
    }
    /*
     * Skip Probe Requests received while the scan algorithm is setting a new
     * channel, or while in a foreign channel.
     * Trying to transmit a frame (Probe Response) during a channel change
     * (which includes a channel reset) can cause a NMI due to invalid HW
     * addresses.
     * Trying to transmit the Probe Response while in a foreign channel
     * wouldn't do us any good either.
     */
    if (wlan_scan_can_transmit(wlan_vdev_get_pdev(vap->vdev_obj)) && !vap->iv_special_vap_mode) {
        if (likely(ic->ic_curchan == vap->iv_bsschan)) {
            snd_prb_resp = 1;
        }
#if MESH_MODE_SUPPORT
        if (vap->iv_mesh_vap_mode) {
            snd_prb_resp = 0;
        }
#endif
    }
    if (snd_prb_resp) {
        extractx.fectx_assocwar160_reqd = is_assocwar160_reqd_proberesp(vap,
                (struct ieee80211_ie_ssid *)ssid, vhtcap);

        if (extractx.fectx_assocwar160_reqd) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,
                              "%s: Applying 160MHz assoc WAR: probe resp to "
                              "STA %s\n",
                              __func__, ether_sprintf(wh->i_addr2));
        }

        if (ni) {
#if QCN_IE
        /* If channel time is not present then send the unicast response immediately */
        if ((extractx.fils_sta || extractx.oce_sta) && channel_time && vap->iv_bpr_enable &&
                IEEE80211_IS_BROADCAST(wh->i_addr1) && IEEE80211_IS_BROADCAST(wh->i_addr3)) {

            /* If channel time is bigger than beancon interval, slightly discard the probe-req
               as beacon will be sent instead */
            if (channel_time > ic->ic_intval) {
                goto exit;
            }

            if (!qdf_hrtimer_active(bpr_gen_timer)) {
                /* If its the first STA sending broadcast probe request, start the timer with
                 * the minimum of user configured delay and the channel time.
                 */
                bpr_delay = qdf_ns_to_ktime(QDF_NSEC_PER_MSEC * vap->iv_bpr_delay);

                /* Set the bpr_delay to be the minimum of channel time and user configured value */
                if (qdf_ktime_to_ns(eff_chan_time) < qdf_ktime_to_ns(bpr_delay)) {
                    bpr_delay = eff_chan_time;
                }

                qdf_hrtimer_start(bpr_gen_timer, bpr_delay, QDF_HRTIMER_MODE_REL);
                vap->iv_bpr_timer_start_count++;

                IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,
                    "Start timer: %s | %d | Sequence: %d | Delay: %d | Current time: %lld | Beacon: %lld | effchantime: %lld | "
                    " Timer expires: %lld | Timer cb: %d | Enqueued: %d \n", \
                    __func__, __LINE__, ((le16toh(*(u_int16_t *)wh->i_seq)) & IEEE80211_SEQ_SEQ_MASK) >> IEEE80211_SEQ_SEQ_SHIFT,
                    vap->iv_bpr_delay, qdf_ktime_to_ns(qdf_ktime_get()), qdf_ktime_to_ns(vap->iv_next_beacon_tstamp),
                    eff_chan_time, qdf_ktime_to_ns(qdf_ktime_add(qdf_ktime_get(),
                    qdf_hrtimer_get_remaining(bpr_gen_timer))),
                    qdf_hrtimer_callback_running(bpr_gen_timer), qdf_hrtimer_is_queued(bpr_gen_timer));
            } else {

                /* For rest of the STA sending broadcast probe requests, if the
                 * timer callback is not running and channel time is less than the remaining
                 * time in the timer, resize the timer to the channel time. Ignore if timer callback
                 * is running as it will be served by the broadcast probe response.
                 */
                if(!qdf_hrtimer_callback_running(bpr_gen_timer) &&
                   qdf_ktime_to_ns(qdf_hrtimer_get_remaining(bpr_gen_timer)) > qdf_ktime_to_ns(eff_chan_time)) {

                    qdf_hrtimer_forward(bpr_gen_timer, qdf_hrtimer_cb_get_time(bpr_gen_timer), eff_chan_time);
                    vap->iv_bpr_timer_resize_count++;

                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,
                        "Resize timer: %s| %d | Sequence: %d | Delay: %d | Current time: %lld | Next beacon tstamp: %lld | effchantime: %lld | "
                        "Timer expires in: %lld | Timer cb: %d | Enqueued: %d\n", \
                        __func__, __LINE__, ((le16toh(*(u_int16_t *)wh->i_seq)) & IEEE80211_SEQ_SEQ_MASK) >> IEEE80211_SEQ_SEQ_SHIFT,
                        vap->iv_bpr_delay, qdf_ktime_to_ns(qdf_ktime_get()), qdf_ktime_to_ns(vap->iv_next_beacon_tstamp),
                        eff_chan_time, qdf_ktime_to_ns(qdf_ktime_add(qdf_ktime_get(),qdf_hrtimer_get_remaining(bpr_gen_timer))),
                        qdf_hrtimer_callback_running(bpr_gen_timer), qdf_hrtimer_is_queued(bpr_gen_timer));
                }

            }

        } else if ((extractx.fils_sta || extractx.oce_sta) && channel_time && vap->iv_bpr_enable &&
                   (IEEE80211_ADDR_EQ(wh->i_addr1, vap->iv_myaddr) || IEEE80211_ADDR_EQ(wh->i_addr3, vap->iv_myaddr))) {

            /* If channel time is bigger than beancon interval, slightly discard the probe-req
               as beacon will be sent instead */
            if (channel_time > ic->ic_intval) {
                goto exit;
            }

            /* If STA sends a probe request to the VAP with some channel time, then send unicast
             * response only if there is no beacon to be scheduled before the channel time expires.
             * Otherwise, the beacon will be sent.
             */
            if ((qdf_ktime_to_ns(vap->iv_next_beacon_tstamp) - QDF_NSEC_PER_MSEC * ic->ic_bcn_latency_comp) >  ktime_to_ns(eff_chan_time)) {
                if (IEEE80211_IS_CHAN_2GHZ(vap->iv_bsschan) &&
                    ieee80211_vap_oce_check(vap)) {
                    if (rs->rs_datarate < vap->iv_prb_rate)
                        extractx.datarate = rs->rs_datarate;
                    else
                        extractx.datarate = vap->iv_prb_rate;
                }
                ieee80211_send_proberesp(ni, wh->i_addr2, NULL, 0, &extractx);
                vap->iv_bpr_unicast_resp_count++;

                IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,
                    "Unicast response sent: %s | %d | Sequence: %d | Delay: %d | Current time: %lld | Next beacon tstamp: %lld | effchantime: %lld | "
                    "beacon interval: %d ms | Timer expires in: %lld | Timer cb running: %d\n", \
                    __func__, __LINE__, ((le16toh(*(u_int16_t *)wh->i_seq)) & IEEE80211_SEQ_SEQ_MASK) >> IEEE80211_SEQ_SEQ_SHIFT,
                    vap->iv_bpr_delay, qdf_ktime_to_ns(qdf_ktime_get()), qdf_ktime_to_ns(vap->iv_next_beacon_tstamp),
                    eff_chan_time, ic->ic_intval, qdf_ktime_to_ns(qdf_ktime_add(qdf_ktime_get(),qdf_hrtimer_get_remaining(bpr_gen_timer))),
                    qdf_hrtimer_callback_running(bpr_gen_timer));

            }
        } else
#endif
	  {

          /*
           * When MBSS IE feature is enabled, we send one probe response for a broadcast
           * probe request, so we skip sending here. Response is sent from ieee80211_input_all().
           * It is sent here in 2 cases:
           * 1. Non-MBSS and unicast/broadcast probe req
           * 2. MBSS and unicast probe req
           */
	  if (!wlan_pdev_nif_feat_cap_get(ic->ic_pdev_obj, WLAN_PDEV_F_MBSS_IE_ENABLE) ||
              !IEEE80211_IS_BROADCAST(wh->i_addr3)) {

                if (IEEE80211_IS_CHAN_2GHZ(vap->iv_bsschan) &&
                    ieee80211_vap_oce_check(vap)) {
                    if (rs->rs_datarate < vap->iv_prb_rate)
                        extractx.datarate = rs->rs_datarate;
                    else
                        extractx.datarate = vap->iv_prb_rate;
                }
                ieee80211_send_proberesp(ni, wh->i_addr2, NULL, 0, &extractx);
	  }

	  }
        }
    }
    else {
        goto exit;
    }

    ret = 0;
exit:
    if(found_vap == 1 || found_null_bssid == 1)
        ieee80211_free_node(ni);

    return ret;
}


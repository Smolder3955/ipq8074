/*
 * Copyright (c) 2017-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#include <ieee80211_var.h>
#include <ieee80211_channel.h>
#include <ieee80211_objmgr_priv.h>
#include <ieee80211_mlme_priv.h>
#include <reg_services_public_struct.h>
#include <ieee80211_rateset.h>
#include <ieee80211_node_priv.h>
#include <wlan_mlme_dispatcher.h>
#include <wlan_mlme_dbg.h>
#include <ieee80211_ucfg.h>
#include <cdp_txrx_stats_struct.h>


uint8_t wlan_vdev_get_chan_num(struct wlan_objmgr_vdev *vdev)
{
    struct ieee80211com *ic = NULL;

    if (vdev == NULL) {
       return (uint8_t)IEEE80211_CHAN_ANY;
    }
    ic = wlan_vdev_get_ic(vdev);

    if(ic == NULL) {
       return (uint8_t)IEEE80211_CHAN_ANY;
    }

    return ieee80211_chan2ieee(ic, ic->ic_curchan);
}
EXPORT_SYMBOL(wlan_vdev_get_chan_num);

int16_t wlan_vdev_get_chan_freq(struct wlan_objmgr_vdev *vdev)
{
    struct ieee80211com *ic = NULL;

    if (vdev == NULL) {
       return -1;
    }
    ic = wlan_vdev_get_ic(vdev);

    if(ic == NULL) {
       return -1;
    }

    return ieee80211_chan2freq(ic, ic->ic_curchan);
}
EXPORT_SYMBOL(wlan_vdev_get_chan_freq);

uint8_t chwidth2convchwidth[IEEE80211_CWM_WIDTH_MAX] = {
    CH_WIDTH_20MHZ,    /*IEEE80211_CWM_WIDTH20,    */
    CH_WIDTH_40MHZ,    /*IEEE80211_CWM_WIDTH40,    */
    CH_WIDTH_80MHZ,    /*IEEE80211_CWM_WIDTH80,    */
    CH_WIDTH_160MHZ,   /*IEEE80211_CWM_WIDTH160,   */
    CH_WIDTH_80P80MHZ, /*IEEE80211_CWM_WIDTH80_80, */
};


enum phy_ch_width wlan_vdev_get_ch_width(struct wlan_objmgr_vdev *vdev)
{
    struct ieee80211com *ic = NULL;
    enum ieee80211_cwm_width chwidth;

    if (vdev == NULL) {
       return CH_WIDTH_INVALID;
    }

    ic = wlan_vdev_get_ic(vdev);

    if(ic == NULL) {
       return CH_WIDTH_INVALID;
    }

    chwidth = ic->ic_cwm_get_width(ic);

    if (chwidth < IEEE80211_CWM_WIDTH_MAX)
        return chwidth2convchwidth[chwidth];

    return CH_WIDTH_INVALID;
}
EXPORT_SYMBOL(wlan_vdev_get_ch_width);




uint8_t phymode2convphymode[IEEE80211_MODE_11AXA_HE80_80 + 1] = {
    WLAN_PHYMODE_AUTO,               /* IEEE80211_MODE_AUTO */
    WLAN_PHYMODE_11A,                /* IEEE80211_MODE_11A */
    WLAN_PHYMODE_11B,                /* IEEE80211_MODE_11B */
    WLAN_PHYMODE_11G,                /* IEEE80211_MODE_11G */
    0xff,                            /* IEEE80211_MODE_FH */
    0xff,                            /* IEEE80211_MODE_TURBO_A */
    0xff,                            /* IEEE80211_MODE_TURBO_G */
    WLAN_PHYMODE_11NA_HT20,          /* IEEE80211_MODE_11NA_HT20 */
    WLAN_PHYMODE_11NG_HT20,          /* IEEE80211_MODE_11NG_HT20 */
    WLAN_PHYMODE_11NA_HT40PLUS,      /* IEEE80211_MODE_11NA_HT40PLUS */
    WLAN_PHYMODE_11NA_HT40MINUS,     /* IEEE80211_MODE_11NA_HT40MINUS */
    WLAN_PHYMODE_11NG_HT40PLUS,      /* IEEE80211_MODE_11NG_HT40PLUS */
    WLAN_PHYMODE_11NG_HT40MINUS,     /* IEEE80211_MODE_11NG_HT40MINUS */
    WLAN_PHYMODE_11NG_HT40,          /* IEEE80211_MODE_11NG_HT40 */
    WLAN_PHYMODE_11NA_HT40,          /* IEEE80211_MODE_11NA_HT40 */
    WLAN_PHYMODE_11AC_VHT20,         /* IEEE80211_MODE_11AC_VHT20 */
    WLAN_PHYMODE_11AC_VHT40PLUS,     /* IEEE80211_MODE_11AC_VHT40PLUS */
    WLAN_PHYMODE_11AC_VHT40MINUS,    /* IEEE80211_MODE_11AC_VHT40MINUS */
    WLAN_PHYMODE_11AC_VHT40,         /* IEEE80211_MODE_11AC_VHT40 */
    WLAN_PHYMODE_11AC_VHT80,         /* IEEE80211_MODE_11AC_VHT80 */
    WLAN_PHYMODE_11AC_VHT160,        /* IEEE80211_MODE_11AC_VHT160 */
    WLAN_PHYMODE_11AC_VHT80_80,      /* IEEE80211_MODE_11AC_VHT80_80 */
    WLAN_PHYMODE_11AXA_HE20,         /* IEEE80211_MODE_11AXA_HE20 */
    WLAN_PHYMODE_11AXG_HE20,         /* IEEE80211_MODE_11AXG_HE20 */
    WLAN_PHYMODE_11AXA_HE40PLUS,     /* IEEE80211_MODE_11AXA_HE40PLUS */
    WLAN_PHYMODE_11AXA_HE40MINUS,    /* IEEE80211_MODE_11AXA_HE40MINUS */
    WLAN_PHYMODE_11AXG_HE40PLUS,     /* IEEE80211_MODE_11AXG_HE40PLUS */
    WLAN_PHYMODE_11AXG_HE40MINUS,    /* IEEE80211_MODE_11AXG_HE40MINUS */
    WLAN_PHYMODE_11AXA_HE40,         /* IEEE80211_MODE_11AXA_HE40 */
    WLAN_PHYMODE_11AXG_HE40,         /* IEEE80211_MODE_11AXG_HE40 */
    WLAN_PHYMODE_11AXA_HE80,         /* IEEE80211_MODE_11AXA_HE80 */
    WLAN_PHYMODE_11AXA_HE160,        /* IEEE80211_MODE_11AXA_HE160 */
    WLAN_PHYMODE_11AXA_HE80_80,      /* IEEE80211_MODE_11AXA_HE80_80 */
};
EXPORT_SYMBOL(phymode2convphymode);

enum wlan_phymode wlan_vdev_get_phymode(struct wlan_objmgr_vdev *vdev)
{
    struct ieee80211vap *vap = NULL;
    enum ieee80211_phymode phy_mode;

    if (vdev == NULL) {
       return IEEE80211_MODE_AUTO;
    }

    wlan_vdev_obj_lock(vdev);
    vap = wlan_vdev_get_vap(vdev);
    wlan_vdev_obj_unlock(vdev);

    if(vap == NULL) {
       return IEEE80211_MODE_AUTO;
    }
    phy_mode = vap->iv_des_mode;

    return phymode2convphymode[phy_mode];
}
EXPORT_SYMBOL(wlan_vdev_get_phymode);

int wlan_vdev_get_sec20chan_num(struct wlan_objmgr_vdev *vdev,
        uint8_t *sec20chan_num)
{
    struct ieee80211com *ic = NULL;
    struct ieee80211_ath_channel *sec20chan = NULL;
    int offset = 0;

    if ((vdev == NULL) || (sec20chan_num == NULL)) {
       return -1;
    }

    ic = wlan_vdev_get_ic(vdev);

    if ((ic == NULL) || (ic->ic_curchan == NULL)) {
       return -1;
    }

    offset = ieee80211_secondary20_channel_offset(ic->ic_curchan);

    if (!offset)
    {
        /* If no secondary 20 MHz is present, then we do not treat it as a
         * failure. Rather, we indicate that secondary 20 MHz is NA by setting
         * sec20chan_chan to 0.
         */
        *sec20chan_num = 0;
        return 0;
    }

    /* We can use 11ng/11na as a representative mode to find sec20 channel for
     * any given mode.
     */
    if (IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan)) {
        sec20chan =
            ic->ic_find_channel(ic, ic->ic_curchan->ic_freq + (20 * offset), 0,
                IEEE80211_CHAN_11NG_HT20);
    } else if (IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan)) {
        sec20chan =
            ic->ic_find_channel(ic, ic->ic_curchan->ic_freq + (20 * offset), 0,
                IEEE80211_CHAN_11NA_HT20);
    } else {
        /* We don't handle other cases */
        return -1;
    }

    if (sec20chan == NULL) {
        return -1;
    }

    *sec20chan_num = ieee80211_chan2ieee(ic, sec20chan);

    return 0;
}
EXPORT_SYMBOL(wlan_vdev_get_sec20chan_num);

int wlan_vdev_get_sec20chan_freq_mhz(struct wlan_objmgr_vdev *vdev,
        uint16_t *sec20chan_freq)
{
    struct ieee80211com *ic = NULL;
    int offset = 0;

    if ((vdev == NULL) || (sec20chan_freq == NULL)) {
       return -1;
    }

    ic = wlan_vdev_get_ic(vdev);

    if ((ic == NULL) || (ic->ic_curchan == NULL)) {
       return -1;
    }

    offset = ieee80211_secondary20_channel_offset(ic->ic_curchan);

    if (!offset)
    {
        /* If no secondary 20 MHz is present, then we do not treat it as a
         * failure. Rather, we indicate that secondary 20 MHz is NA by setting
         * sec20chan_freq to 0.
         */
        *sec20chan_freq = 0;
    } else {
        *sec20chan_freq = (ic->ic_curchan->ic_freq + (20 * offset));
    }

    return 0;
}
EXPORT_SYMBOL(wlan_vdev_get_sec20chan_freq_mhz);

uint32_t wlan_pdev_in_gmode(struct wlan_objmgr_pdev *pdev)
{
    struct ieee80211com *ic;
    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (ic)
        return IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan);
    else
        return 0;
}
EXPORT_SYMBOL(wlan_pdev_in_gmode);

uint32_t wlan_pdev_in_amode(struct wlan_objmgr_pdev *pdev)
{
    struct ieee80211com *ic;
    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (ic)
        return IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan);
    else
        return 0;
}
EXPORT_SYMBOL(wlan_pdev_in_amode);

int wlan_pdev_get_esp_info(struct wlan_objmgr_pdev *pdev, map_esp_info_t *map_esp_info)
{
#if QCN_ESP_IE
    struct ieee80211com *ic;
    map_service_ba_window_size_e map_service_ba_window_size[] =
        {map_ba_window_not_used, map_ba_window_size_2,
         map_ba_window_size_4, map_ba_window_size_6,
         map_ba_window_size_8, map_ba_window_size_16,
         map_ba_window_size_32, map_ba_window_size_64};


    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic)
        return -EINVAL;

    /* BA Window size as entered by the user */
    if (ic->ic_esp_ba_window) {
        ic->ic_esp_ba_window =
            map_service_ba_window_size[ic->ic_esp_ba_window];
        map_esp_info->esp_info[map_service_ac_be].ba_window_size =
            ic->ic_esp_ba_window;
    } else {
        /* BA Window Size of 16 */
        map_esp_info->esp_info[map_service_ac_be].ba_window_size =
            map_ba_window_size_16;
    }

    /* Estimated Airtime Fraction in native format */
    if (ic->ic_esp_air_time_fraction) {
        map_esp_info->esp_info[map_service_ac_be].est_air_time_fraction =
            (ic->ic_esp_air_time_fraction * 100) / 255;
    } else {
        map_esp_info->esp_info[map_service_ac_be].est_air_time_fraction =
            ic->ic_fw_esp_air_time;
    }

    if (ic->ic_esp_ppdu_duration) {
        /* PPDU Duration in native format */
        map_esp_info->esp_info[map_service_ac_be].data_ppdu_dur_target =
            ic->ic_esp_ppdu_duration * MAP_PPDU_DURATION_UNITS;
    } else {
        /* Default : 250us PPDU Duration in native format */
        map_esp_info->esp_info[map_service_ac_be].data_ppdu_dur_target =
            MAP_DEFAULT_PPDU_DURATION * MAP_PPDU_DURATION_UNITS;
    }
#endif
    return EOK;
}

int wlan_pdev_get_multi_ap_opclass(struct wlan_objmgr_pdev *pdev, mapapcap_t *apcap,
                                   struct map_op_chan_t *map_op_chan)
{
    struct ieee80211com *ic;
    struct ieee80211_ath_channel *channelmin = NULL, *channelmax = NULL;
    uint8_t minch, maxch, total;
    int8_t maxpow, minpow, pow;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic)
        return -EINVAL;

    channelmax = &(ic->ic_channels[ic->ic_nchans - 1]);
    channelmin = &(ic->ic_channels[0]);

    minch = ieee80211_chan2ieee(ic, channelmin);
    maxch = ieee80211_chan2ieee(ic, channelmax);
    maxpow = channelmax->ic_maxregpower;
    minpow = channelmin->ic_maxregpower;
    pow = maxpow > minpow ? maxpow : minpow;

    total = regdmn_get_map_opclass(minch, maxch, apcap, pow, map_op_chan);
    return total;
}

uint32_t wlan_node_get_peer_chwidth(struct wlan_objmgr_peer *peer)
{
     wlan_node_t ni;

     if(peer == NULL) {
         qdf_print("%s:PEER is NULL ",__func__);
         return 0;
     }

     ni = (wlan_node_t)wlan_peer_get_mlme_ext_obj(peer);

     return (ni ? ni->ni_chwidth : IEEE80211_CWM_WIDTHINVALID);
}
EXPORT_SYMBOL(wlan_node_get_peer_chwidth);

void wlan_node_get_peer_phy_info(struct wlan_objmgr_peer *peer, uint8_t *rxstreams, uint8_t *streams, uint8_t *cap, uint32_t *mode)
{
     wlan_node_t ni;

     if(peer == NULL) {
         qdf_print("%s:PEER is NULL ",__func__);
         return;
     }

     ni = (wlan_node_t)wlan_peer_get_mlme_ext_obj(peer);

     *rxstreams = ni->ni_rxstreams;
     *streams =   ni->ni_streams;
     /*11AX TODO (Phase II) - Is HE OPmode needed here ? */
     *mode = ni->ni_flags;

     *cap = ni->ni_ic->ic_is_mode_offload(ni->ni_ic);
}
EXPORT_SYMBOL(wlan_node_get_peer_phy_info);

#if QCA_SUPPORT_SON
int wlan_vdev_acl_auth(struct wlan_objmgr_vdev *vdev, bool set,
		       struct ieee80211req_athdbg *req_t)
{
	wlan_if_t vap = NULL;

	vap = wlan_vdev_get_vap(vdev);
	if (!vap)
		return -EINVAL;

	if (set) {
		if (req_t->data.bsteering_auth_allow)
			return ieee80211_acl_set_flag(vap, req_t->dstmac,
					      IEEE80211_ACL_FLAG_AUTH_ALLOW);
		else
			return ieee80211_acl_clr_flag(vap, req_t->dstmac,
					      IEEE80211_ACL_FLAG_AUTH_ALLOW);
	} else {/* get */
	/* Note that a lookup failure (no entry in the ACL) will also indicate
	 * that probe responses are not being withheld.
	 */
		req_t->data.bsteering_auth_allow =
			ieee80211_acl_flag_check(vap, req_t->dstmac,
						 IEEE80211_ACL_FLAG_AUTH_ALLOW);
	}
	return EOK;
}

int wlan_vdev_acl_probe(struct wlan_objmgr_vdev *vdev, bool set,
			struct ieee80211req_athdbg *req_t)
{
	wlan_if_t vap = NULL;

	vap = wlan_vdev_get_vap(vdev);
	if (!vap)
		return -EINVAL;

	if (set) {
		if (req_t->data.bsteering_probe_resp_wh)
			return ieee80211_acl_set_flag(vap, req_t->dstmac,
					      IEEE80211_ACL_FLAG_PROBE_RESP_WH);
		else
			return ieee80211_acl_clr_flag(vap, req_t->dstmac,
					      IEEE80211_ACL_FLAG_PROBE_RESP_WH);
	} else {/* get */
	/* Note that a lookup failure (no entry in the ACL) will also indicate
		 * that probe responses are not being withheld.
		 */
		req_t->data.bsteering_probe_resp_wh =
			ieee80211_acl_flag_check(vap, req_t->dstmac,
					 IEEE80211_ACL_FLAG_PROBE_RESP_WH);
	}

	return EOK;
}

int wlan_vdev_local_disassoc(struct wlan_objmgr_vdev *vdev,
			     struct wlan_objmgr_peer *peer)
{
	u_int8_t macaddr[QDF_MAC_ADDR_SIZE];
	struct ieee80211_node *ni = NULL;
	wlan_if_t vap = NULL;

	vap = wlan_vdev_get_vap(vdev);
	if (!vap)
		return -EINVAL;

	ni = wlan_peer_get_mlme_ext_obj(peer);
	if (!ni) {
		return -EINVAL;
	}

	wlan_peer_obj_lock(peer);
	qdf_mem_copy(macaddr, wlan_peer_get_macaddr(peer), QDF_MAC_ADDR_SIZE);
	wlan_peer_obj_unlock(peer);

	IEEE80211_NODE_LEAVE(ni);
	IEEE80211_DELIVER_EVENT_MLME_DISASSOC_COMPLETE(vap, macaddr,
					IEEE80211_REASON_LOCAL,
					IEEE80211_STATUS_SUCCESS);

	return EOK;
}

/**
 * @brief Get the maximum MCS supported based on PHY mode and rate
 *        information provided
 *
 * @param [in] phymode  the PHY mode the VAP/client is operating on
 * @param [in] rx_vht_mcs_map  the VHT RX rate map if supported
 * @param [in] tx_vht_mcs_map  the VHT TX rate map if supported
 * @param [in] htrates  the HT rates supported
 * @param [in] basic_rates  all other rates supported
 *
 * @return the maximum MCS supported on success; otherwise return -1
 */

int wlan_node_get_max_MCS(enum ieee80211_phymode phymode,
			  u_int16_t *hecap_rxmcsnssmap, u_int16_t *hecap_txmcsnssmap,
			  u_int16_t rx_vht_mcs_map, u_int16_t tx_vht_mcs_map,
			  const struct ieee80211_rateset *htrates,
			  const struct ieee80211_rateset *basic_rates)
{
	u_int8_t rx_max_MCS, tx_max_MCS, max_MCS;
	switch (phymode) {
	case IEEE80211_MODE_11AXA_HE20:
	case IEEE80211_MODE_11AXG_HE20:
	case IEEE80211_MODE_11AXA_HE40PLUS:
	case IEEE80211_MODE_11AXA_HE40MINUS:
	case IEEE80211_MODE_11AXG_HE40PLUS:
	case IEEE80211_MODE_11AXG_HE40MINUS:
	case IEEE80211_MODE_11AXA_HE40:
	case IEEE80211_MODE_11AXG_HE40:
	case IEEE80211_MODE_11AXA_HE80:
		if (hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80] &&
			hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80]) {
			rx_max_MCS = hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80] & 0x03;
			tx_max_MCS = hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80] & 0x03;
			max_MCS = rx_max_MCS < tx_max_MCS ? rx_max_MCS : tx_max_MCS;
			if (max_MCS < 0x03) {
				return 7 + 2*max_MCS;
			}
		}
	case IEEE80211_MODE_11AXA_HE160:
		if (hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160] &&
			hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160]) {
			rx_max_MCS = hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160] & 0x03;
			tx_max_MCS = hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160] & 0x03;
			max_MCS = rx_max_MCS < tx_max_MCS ? rx_max_MCS : tx_max_MCS;
			if (max_MCS < 0x03) {
				return 7 + 2*max_MCS;
			}
		}
	case IEEE80211_MODE_11AXA_HE80_80:
		if (hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80] &&
			hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80]) {
			rx_max_MCS = hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80] & 0x03;
			tx_max_MCS = hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80] & 0x03;
			max_MCS = rx_max_MCS < tx_max_MCS ? rx_max_MCS : tx_max_MCS;
			if (max_MCS < 0x03) {
				return 7 + 2*max_MCS;
			}
		}
	case IEEE80211_MODE_11AC_VHT20:
	case IEEE80211_MODE_11AC_VHT40PLUS:
	case IEEE80211_MODE_11AC_VHT40MINUS:
	case IEEE80211_MODE_11AC_VHT40:
	case IEEE80211_MODE_11AC_VHT80:
	case IEEE80211_MODE_11AC_VHT160:
	case IEEE80211_MODE_11AC_VHT80_80:
		if (rx_vht_mcs_map && tx_vht_mcs_map) {
		/* Refer to IEEE P802.11ac/D7.0 Figure 8-401bs for VHT MCS Map definition */
			rx_max_MCS = rx_vht_mcs_map & 0x03;
			tx_max_MCS = tx_vht_mcs_map & 0x03;
			max_MCS = rx_max_MCS < tx_max_MCS ? rx_max_MCS : tx_max_MCS;
			if (max_MCS < 0x03) {
				return 7 + max_MCS;
			}
		}
		/* Invalid 11ac MCS, fallback to report 11n MCS */
	case IEEE80211_MODE_11NA_HT20:
	case IEEE80211_MODE_11NG_HT20:
	case IEEE80211_MODE_11NA_HT40PLUS:
	case IEEE80211_MODE_11NA_HT40MINUS:
	case IEEE80211_MODE_11NG_HT40PLUS:
	case IEEE80211_MODE_11NG_HT40MINUS:
	case IEEE80211_MODE_11NG_HT40:
	case IEEE80211_MODE_11NA_HT40:
		if (htrates && htrates->rs_nrates && (htrates->rs_nrates <= IEEE80211_RATE_MAXSIZE)) {
			return htrates->rs_rates[htrates->rs_nrates - 1];
		}
		/* Invalid 11n MCS, fallback to basic rates */
	default:
		if (basic_rates && basic_rates->rs_nrates && (basic_rates->rs_nrates <= IEEE80211_RATE_MAXSIZE)) {
			return basic_rates->rs_rates[basic_rates->rs_nrates - 1] & IEEE80211_RATE_VAL;
		}
	}

	return -1;
}

/**
 * @brief Get the maximum MCS supported by the client
 *
 * @param [in] ni  the STA to check for maximum MCS supported
 *
 * @return the maximum MCS supported by this client on success;
 *         otherwise return -1
 */
int wlan_peer_get_node_max_MCS(struct wlan_objmgr_peer *peer)
{
	struct ieee80211_node *ni = NULL;

	ni = wlan_peer_get_mlme_ext_obj(peer);
	if( !ni)
		return -EINVAL;

	return wlan_node_get_max_MCS(ni->ni_phymode,
				     ni->ni_he.hecap_rxmcsnssmap,
				     ni->ni_he.hecap_txmcsnssmap,
				     ni->ni_rx_vhtrates, ni->ni_tx_vhtrates,
				     &ni->ni_htrates, &ni->ni_rates);
}

int wlan_vdev_get_node_info(struct wlan_objmgr_vdev *vdev,
			    struct ieee80211req_athdbg *req)
{
	struct ieee80211_node *ni = NULL;
	wlan_if_t vap = NULL;
	struct ieee80211com *ic = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	struct wlan_objmgr_peer *peer = NULL;
	uint16_t he_rxmcsnssmap[HEHANDLE_CAP_TXRX_MCS_NSS_SIZE];
	uint16_t he_txmcsnssmap[HEHANDLE_CAP_TXRX_MCS_NSS_SIZE];
	int max_MCS = 0;
	uint8_t pdev_id;

	if (WLAN_ADDR_EQ(wlan_vdev_mlme_get_macaddr(vdev), req->dstmac) == QDF_STATUS_SUCCESS) {
		vap = wlan_vdev_get_vap(vdev);
		ic = wlan_vdev_get_ic(vdev);
		if (!vap || !ic) {
			qdf_print("%s: Req VAP %02x:%02x:%02x:%02x:%02x:%02x has invalid %s %s.",
				  __func__, req->dstmac[0], req->dstmac[1], req->dstmac[2],
				  req->dstmac[3], req->dstmac[4], req->dstmac[5],
				  vap ? "" : "vap", ic ? "" : "ic");
			return -EINVAL;
		}

		req->data.bsteering_datarate_info.max_chwidth = ic->ic_cwm_get_width(ic);
		req->data.bsteering_datarate_info.num_streams =
			ieee80211_getstreams(ic, ic->ic_tx_chainmask);
		req->data.bsteering_datarate_info.phymode = vap->iv_cur_mode;
		/* Tx power is stored in half dBm */
		req->data.bsteering_datarate_info.max_txpower = vap->iv_bss->ni_txpower / 2;

		/* get the intersected (user-set vs target caps)
		 * values of mcsnssmap */
		ieee80211vap_get_insctd_mcsnssmap(vap, he_rxmcsnssmap, he_txmcsnssmap);

		max_MCS = wlan_node_get_max_MCS(vap->iv_cur_mode,
					  he_rxmcsnssmap,
					  he_txmcsnssmap,
					  vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map,
					  vap->iv_vhtcap_max_mcs.tx_mcs_set.mcs_map,
					  &ic->ic_sup_ht_rates[vap->iv_cur_mode],
					  &ic->ic_sup_rates[vap->iv_cur_mode]);
		if (max_MCS < 0) {
			qdf_print("%s: Requested VAP %02x:%02x:%02x:%02x:%02x:%02x has no valid rate info.",
				  __func__, req->dstmac[0], req->dstmac[1], req->dstmac[2],
				  req->dstmac[3], req->dstmac[4], req->dstmac[5]);
			return -EINVAL;
		} else {
			req->data.bsteering_datarate_info.max_MCS = max_MCS;
		}
	} else {
		psoc = wlan_vdev_get_psoc(vdev);
		if (!psoc) {
			qdf_print("%s: Req STA %02x:%02x:%02x:%02x:%02x:%02x has invalid PSOC.",
					__func__, req->dstmac[0], req->dstmac[1], req->dstmac[2],
					req->dstmac[3], req->dstmac[4], req->dstmac[5]);
			return -EINVAL;
		}
		pdev_id =  wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(vdev));
		peer = wlan_objmgr_get_peer(psoc, pdev_id, req->dstmac, WLAN_SON_ID);
		if (!peer) {
			qdf_print("%s: Requested STA %02x:%02x:%02x:%02x:%02x:%02x has no valid rate info.",
				  __func__, req->dstmac[0], req->dstmac[1], req->dstmac[2],
				  req->dstmac[3], req->dstmac[4], req->dstmac[5]);
			return -EINVAL;
		}

		ni = wlan_peer_get_mlme_ext_obj(peer);
		if (!ni) {
			qdf_print("%s: Requested STA %02x:%02x:%02x:%02x:%02x:%02x is not "
			  "associated", __func__, req->dstmac[0], req->dstmac[1],
			  req->dstmac[2], req->dstmac[3], req->dstmac[4], req->dstmac[5]);
			wlan_objmgr_peer_release_ref(peer, WLAN_SON_ID);
			return -EINVAL;
		}

		req->data.bsteering_datarate_info.max_chwidth = ni->ni_chwidth;
		req->data.bsteering_datarate_info.num_streams = ni->ni_streams;
		req->data.bsteering_datarate_info.phymode = ni->ni_phymode;
		req->data.bsteering_datarate_info.max_txpower = ni->ni_max_txpower;
		req->data.bsteering_datarate_info.is_mu_mimo_supported =
			(bool)(ni->ni_vhtcap & IEEE80211_VHTCAP_MU_BFORMEE);

		max_MCS = wlan_peer_get_node_max_MCS(peer);
		if (max_MCS < 0) {
			qdf_print("%s: Requested STA %02x:%02x:%02x:%02x:%02x:%02x has no valid rate info.",
				  __func__, req->dstmac[0], req->dstmac[1], req->dstmac[2],
				  req->dstmac[3], req->dstmac[4], req->dstmac[5]);
			wlan_objmgr_peer_release_ref(peer, WLAN_SON_ID);
			return -EINVAL;
		} else {
			req->data.bsteering_datarate_info.max_MCS = max_MCS;
		}
		req->data.bsteering_datarate_info.is_static_smps =
			((ni->ni_htcap & IEEE80211_HTCAP_C_SM_MASK) ==
			 IEEE80211_HTCAP_C_SMPOWERSAVE_STATIC);
		wlan_objmgr_peer_release_ref(peer, WLAN_SON_ID);
	}

	return EOK;
}

QDF_STATUS wlan_peer_send_null(struct wlan_objmgr_peer *peer)
{
	struct ieee80211_node *ni = NULL;

	ni = wlan_peer_get_mlme_ext_obj(peer);
	if (!ni)
		return QDF_STATUS_E_INVAL;

	if (wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_QOS))
		ieee80211_send_qosnulldata(ni, WME_AC_VI, false);
	else
		ieee80211_send_nulldata(ni, false);

	return QDF_STATUS_SUCCESS;
}

bool wlan_peer_update_sta_stats(struct wlan_objmgr_peer *peer,
				struct bs_sta_stats_ind *sta_stats,
                                void *peer_stats)
{
	struct cdp_interface_peer_stats *stats = (struct cdp_interface_peer_stats *)peer_stats;

	if (!stats) {
		qdf_print("%s stats is NULL ", __func__);
		return false;
	}
        qdf_mem_copy(sta_stats->peer_stats[sta_stats->peer_count].client_addr,
                 wlan_peer_get_macaddr(peer), QDF_MAC_ADDR_SIZE);
        sta_stats->peer_stats[sta_stats->peer_count].tx_rate =
               stats->last_peer_tx_rate;
        sta_stats->peer_stats[sta_stats->peer_count].tx_packet_count =
               stats->tx_packet_count;
        sta_stats->peer_stats[sta_stats->peer_count].rx_packet_count =
               stats->rx_packet_count;
        sta_stats->peer_stats[sta_stats->peer_count].tx_byte_count =
               stats->tx_byte_count;
        sta_stats->peer_stats[sta_stats->peer_count].rx_byte_count =
               stats->rx_byte_count;
        sta_stats->peer_stats[sta_stats->peer_count].rssi = stats->peer_rssi;
        sta_stats->peer_stats[sta_stats->peer_count].per = stats->per;
        sta_stats->peer_stats[sta_stats->peer_count].ack_rssi = stats->ack_rssi;
        sta_stats->peer_count++;

        return true;
}
u_int8_t wlan_peer_get_rssi(struct wlan_objmgr_peer *peer)
{
	struct ieee80211_node *ni = NULL;

	ni = wlan_peer_get_mlme_ext_obj(peer);
        if (!ni)
            return 0;

	return ni->ni_rssi;
}

u_int32_t wlan_peer_get_rate(struct wlan_objmgr_peer *peer)
{
	ieee80211_rate_info rinfo;
	struct ieee80211_node *ni = NULL;

	ni = wlan_peer_get_mlme_ext_obj(peer);

	if (!ni) {
		return 0;
	}

	wlan_node_txrate_info(ni, &rinfo);
	return rinfo.lastrate;
}
wlan_chan_t wlan_vdev_get_channel(struct wlan_objmgr_vdev *vdev)
{
	wlan_if_t vap = NULL;

	vap = wlan_vdev_get_vap(vdev);
	if (!vap)
		return NULL;

	return wlan_get_bss_channel(vap);
}

int wlan_vdev_acs_set_user_chanlist(struct wlan_objmgr_vdev *vdev
				    , u_int8_t *extra)
{
	wlan_if_t vap = NULL;

	vap = wlan_vdev_get_vap(vdev);
	if (!vap)
		return -EINVAL;

	return wlan_acs_set_user_chanlist(vap, extra);
}
int wlan_vdev_acs_start_scan_report(struct wlan_objmgr_vdev *vdev , int val)
{
	wlan_if_t vap = NULL;

	vap = wlan_vdev_get_vap(vdev);
	if (!vap)
		return -EINVAL;

	return wlan_acs_start_scan_report(vap, 1, IEEE80211_START_ACS_REPORT,
					  (void *)&val);
}

u_int8_t wlan_vdev_get_rx_streams(struct wlan_objmgr_vdev *vdev)
{
	wlan_if_t vap = NULL;
	struct ieee80211com *ic = NULL;

	vap = wlan_vdev_get_vap(vdev);
        if (!vap)
		return 0;

	ic  = wlan_vdev_get_ic(vdev);

	if (!ic)
		return 0;

	return ieee80211_get_rxstreams(ic, vap);
}

int wlan_vdev_get_apcap(struct wlan_objmgr_vdev *vdev, mapapcap_t *apcap)
{
	wlan_if_t vap = NULL;
	struct ieee80211com *ic = NULL;
	u_int32_t rx_streams, tx_streams;
	struct wlan_objmgr_pdev *pdev = NULL;
	uint32_t low_2g=0, high_2g=0, low_5g=0, high_5g=0;

	pdev = wlan_vdev_get_pdev(vdev);
	if(!pdev)
		return -EINVAL;

	vap = wlan_vdev_get_vap(vdev);
	if (!vap)
		return -EINVAL;

	ic = vap->iv_ic;
	if (!ic)
		return -EINVAL;

	apcap->hwcap.max_supported_bss = wlan_pdev_get_max_vdev_count(pdev) - 1;
	rx_streams = ieee80211_get_rxstreams(ic, vap);
	tx_streams = ieee80211_get_txstreams(ic, vap);

	/* Add HT cap */
	if (ieee80211_vap_wme_is_set(vap) &&
	    ieee80211com_has_cap(ic, IEEE80211_C_HT)) {

		apcap->map_ap_ht_capabilities_valid = 1;

		apcap->htcap.max_tx_nss = tx_streams;
		apcap->htcap.max_rx_nss = rx_streams;

		if (ieee80211com_has_htcap(ic, IEEE80211_HTCAP_C_SHORTGI20)) {
			apcap->htcap.short_gi_support_20_mhz = 1;
		}

		if (ieee80211com_has_htcap(ic, IEEE80211_HTCAP_C_SHORTGI40)) {
			apcap->htcap.short_gi_support_40_mhz = 1;
		}

		if (ieee80211com_has_htcap(ic, IEEE80211_HTCAP_C_CHWIDTH40)) {
			apcap->htcap.ht_support_40_mhz = 1;
		}
	}

	/* Check if radio is capable of operating on 5Ghz */
	wlan_reg_get_freq_range(pdev, &low_2g, &high_2g, &low_5g, &high_5g);

	/* Add VHT cap */
	if (ieee80211_vap_wme_is_set(vap) &&
	    ieee80211com_has_cap_ext(ic, IEEE80211_CEXT_11AC) &&
	    ieee80211vap_vhtallowed(vap) &&
	    (low_5g!=0 || high_5g!=0 )) {

		apcap->map_ap_vht_capabilities_valid = 1;

		apcap->vhtcap.supported_tx_mcs = vap->iv_vhtcap_max_mcs.tx_mcs_set.mcs_map;
		apcap->vhtcap.supported_rx_mcs = vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map;

		apcap->vhtcap.max_tx_nss = tx_streams;
		apcap->vhtcap.max_rx_nss = rx_streams;

		if (ieee80211com_has_vhtcap(ic, IEEE80211_VHTCAP_SHORTGI_80)) {
			apcap->vhtcap.short_gi_support_80_mhz = 1;
		}

		if (ieee80211com_has_vhtcap(ic, IEEE80211_VHTCAP_SHORTGI_160)) {
			apcap->vhtcap.short_gi_support_160_mhz_80p_80_mhz = 1;
		}

		if (ic->ic_vhtcap & IEEE80211_VHTCAP_SUP_CHAN_WIDTH_160) {
			apcap->vhtcap.support_160_mhz = 1;
		}

		if (ic->ic_vhtcap & IEEE80211_VHTCAP_SUP_CHAN_WIDTH_80_160) {
			apcap->vhtcap.support_80p_80_mhz = 1;
		}

		if (ic->ic_vhtcap & IEEE80211_VHTCAP_SU_BFORMER) {
			apcap->vhtcap.su_beam_former_capable = 1;
		}

		if (ic->ic_vhtcap & IEEE80211_VHTCAP_MU_BFORMER) {
			apcap->vhtcap.mu_beam_former_capable = 1;
		}
	}

	apcap->map_ap_he_capabilities_valid = 0;

	return EOK;
}

int wlan_vdev_add_acl_validity_timer(struct wlan_objmgr_vdev *vdev,
				     const u_int8_t *mac_addr,
				     u_int16_t validity_timer)
{
	wlan_if_t vap = NULL;
	vap = wlan_vdev_get_vap(vdev);
	if (!vap)
		return -EINVAL;

	return ieee80211_acl_add_with_validity(vap, mac_addr, validity_timer);
}

u_int8_t  wlan_vdev_get_chwidth(struct wlan_objmgr_vdev *vdev)
{
	wlan_if_t vap = NULL;
	struct ieee80211com *ic = NULL;

	vap = wlan_vdev_get_vap(vdev);
        if (!vap)
		return 0;

	ic  = wlan_vdev_get_ic(vdev);

	if (ic == NULL)
		return 0;

	if(vap->iv_chwidth != IEEE80211_CWM_WIDTHINVALID)
		return vap->iv_chwidth;
	else
		return ic->ic_cwm_get_width(ic);
}

bool wlan_vdev_is_deleted_set(struct wlan_objmgr_vdev *vdev)
{
	wlan_if_t vap = NULL;

	vap = wlan_vdev_get_vap(vdev);
	if (!vap)
		return false;

	return ieee80211_vap_deleted_is_set(vap);
}

bool wlan_vdev_acl_is_probe_wh_set(struct wlan_objmgr_vdev *vdev,
				   const u_int8_t *mac_addr)
{
	wlan_if_t vap = NULL;

	vap = wlan_vdev_get_vap(vdev);
	if (!vap)
		return false;

	return ieee80211_acl_flag_check(vap, mac_addr,
					IEEE80211_ACL_FLAG_PROBE_RESP_WH);
}

bool wlan_vdev_acl_is_drop_mgmt_set(struct wlan_objmgr_vdev *vdev,
				    const u_int8_t *mac_addr)
{
	wlan_if_t vap = NULL;

	vap = wlan_vdev_get_vap(vdev);

	return ieee80211_acl_flag_check(vap, mac_addr,
					IEEE80211_ACL_FLAG_BLOCK_MGMT);
}

int wlan_node_get_capability(struct wlan_objmgr_peer *peer,
			     struct bs_node_associated_ind *assoc)
{
	struct ieee80211_node *ni = NULL;

	ni = wlan_peer_get_mlme_ext_obj(peer);
	if (!ni)
		return -EINVAL;

        assoc->isBTMSupported = (bool)(ni->ext_caps.ni_ext_capabilities & IEEE80211_EXTCAPIE_BSSTRANSITION);
	assoc->isRRMSupported = (bool)(ni->ni_flags & IEEE80211_NODE_RRM);
	assoc->isBeaconMeasurementSupported = (bool)(ieee80211node_has_extflag(ni, IEEE80211_NODE_BCN_MEASURE_SUPPORT));
	assoc->datarate_info.max_chwidth = ni->ni_chwidth;
	assoc->datarate_info.num_streams = ni->ni_streams;
	assoc->datarate_info.phymode = ni->ni_phymode;
	assoc->datarate_info.max_txpower = ni->ni_max_txpower;
	assoc->band_cap = ni->ni_operating_bands;
	assoc->datarate_info.is_static_smps =
		((ni->ni_htcap & IEEE80211_HTCAP_C_SM_MASK) ==
		 IEEE80211_HTCAP_C_SMPOWERSAVE_STATIC);
	assoc->datarate_info.is_mu_mimo_supported =
		(bool)(ni->ni_vhtcap & IEEE80211_VHTCAP_MU_BFORMEE);

	return EOK;
}
#endif

void wlan_set_peer_rate_legacy(struct wlan_objmgr_peer *peer, struct ieee80211_rateset *rates)
{
    struct ieee80211_node *ni = NULL;

    ni = wlan_peer_get_mlme_ext_obj(peer);
    if (ni == NULL) {
        mlme_err("ni not found!\n");
        return;
    }
    qdf_mem_copy(&ni->ni_rates, rates, sizeof(struct ieee80211_rateset));
}


void wlan_set_peer_rate_ht(struct wlan_objmgr_peer *peer, struct ieee80211_rateset *rates)
{
    struct ieee80211_node *ni = NULL;

    ni = wlan_peer_get_mlme_ext_obj(peer);
    if (ni == NULL) {
        mlme_err("ni not found!\n");
        return;
    }

    if (rates != NULL){
        qdf_mem_copy(&ni->ni_htrates, rates, sizeof(struct ieee80211_rateset));
        if (ni->ni_streams == 1 && ni->ni_htrates.rs_rates[0] > 7) {
            /* nss=1, but all MCS for nss 1 are zero, not valid, disable HT */
            ni->ni_flags &= ~IEEE80211_NODE_HT;
        }
        if (ni->ni_streams == 2 && ni->ni_htrates.rs_rates[0] > 15) {
            /* nss=2, but MCS for NSS=1 and NSS=2 are zero, not valid, disable HT */
            ni->ni_flags &= ~IEEE80211_NODE_HT;
        }
        if (ni->ni_streams == 3 && ni->ni_htrates.rs_rates[0] > 23) {
            /* nss=3, but MCS for NSS=1,NSS=2 and NSS=3 are zero, not valid, disable HT */
            ni->ni_flags &= ~IEEE80211_NODE_HT;
        }
        if (IEEE80211_NODE_USE_HT(ni)) {
            qdf_mem_copy(&ni->ni_htrates, rates, sizeof(struct ieee80211_rateset));
        } else {
            qdf_mem_zero(&ni->ni_htrates, sizeof(struct ieee80211_rateset));
        }
    } else {
        qdf_mem_zero(&ni->ni_htrates, sizeof(struct ieee80211_rateset));
    }
}

void wlan_set_peer_rate_vht(struct wlan_objmgr_peer *peer, u_int16_t vhtrate_map)
{
    struct ieee80211_node *ni = NULL;

    ni = wlan_peer_get_mlme_ext_obj(peer);
    if (ni == NULL) {
        mlme_err("ni not found!\n");
        return;
    }
    ni->ni_tx_vhtrates = vhtrate_map;
}

void wlan_set_peer_rate_he(struct wlan_objmgr_peer *peer, u_int16_t herate_map)
{
    struct ieee80211_node *ni = NULL;
    uint16_t *ni_he_tx_mcs_set = NULL;

    ni = wlan_peer_get_mlme_ext_obj(peer);
    if (ni == NULL) {
        mlme_err("ni not found!\n");
        return;
    }

    ni_he_tx_mcs_set = ni->ni_he.hecap_txmcsnssmap;
    switch(ni->ni_phymode) {
        case IEEE80211_MODE_11AXA_HE80_80:
            ni_he_tx_mcs_set[HECAP_TXRX_MCS_NSS_IDX_80_80] = herate_map;
            /* fall through */
        case IEEE80211_MODE_11AXA_HE160:
            ni_he_tx_mcs_set[HECAP_TXRX_MCS_NSS_IDX_160] = herate_map;
            /* fall through */
        default:
            ni_he_tx_mcs_set[HECAP_TXRX_MCS_NSS_IDX_80] = herate_map;
            break;
    }
}


void wlan_peer_dump_rates(struct wlan_objmgr_peer *peer)
{
    struct ieee80211_node *ni = NULL;
    int i = 0;
    uint16_t *ni_he_tx_mcs_set = NULL;

    ni = wlan_peer_get_mlme_ext_obj(peer);
    if (ni == NULL) {
        mlme_err("ni not found!\n");
        return;
    }

    mlme_info("add_client: legacy rates\n");
    for (i=0; i<ni->ni_rates.rs_nrates; i++) {
        mlme_info("%d\n", ni->ni_rates.rs_rates[i]);
    }
    mlme_info("\n");

    if (wlan_peer_is_flag_set(peer, IEEE80211_NODE_HT)) {
        mlme_info("add_client: HT rates\n");
        for (i=0; i<ni->ni_htrates.rs_nrates; i++) {
            mlme_info("%d\n", ni->ni_htrates.rs_rates[i]);
        }
        mlme_info("\n");
    }

    if (wlan_peer_is_flag_set(peer, IEEE80211_NODE_VHT)) {
        mlme_info("add_client: VHT rate map=0x%x\n", ni->ni_tx_vhtrates);
    }

    if (wlan_peer_is_extflag_set(peer, IEEE80211_NODE_HE)) {
        ni_he_tx_mcs_set = ni->ni_he.hecap_txmcsnssmap;
        switch(ni->ni_phymode) {
            case IEEE80211_MODE_11AXA_HE80_80:
                mlme_info("add_client: HE rate map=0x%x for 80_80HZ\n", ni_he_tx_mcs_set[HECAP_TXRX_MCS_NSS_IDX_80_80]);
            case IEEE80211_MODE_11AXA_HE160:
                mlme_info("add_client: HE rate map=0x%x for 160HZ\n", ni_he_tx_mcs_set[HECAP_TXRX_MCS_NSS_IDX_160]);
            default:
                mlme_info("add_client: HE rate map=0x%x for 80HZ\n", ni_he_tx_mcs_set[HECAP_TXRX_MCS_NSS_IDX_80]);
                break;
        }
    }
}


void wlan_deliver_mlme_evt_disassoc(struct wlan_objmgr_vdev *vdev, u_int8_t *stamac, struct wlan_objmgr_peer *peer)
{
    struct ieee80211_node *ni = NULL;
    wlan_if_t vap;

    vap = (wlan_if_t)wlan_vdev_get_mlme_ext_obj(vdev);
    if (vap == NULL) {
        mlme_err("vap not found!\n");
        return;
    }

    ni = (wlan_node_t)wlan_peer_get_mlme_ext_obj(peer);
    if (ni == NULL) {
        return;
    }
    IEEE80211_NODE_LEAVE(ni);

    IEEE80211_DELIVER_EVENT_MLME_DISASSOC_COMPLETE(vap, stamac,
                 IEEE80211_REASON_ASSOC_LEAVE, IEEE80211_STATUS_SUCCESS);
}

int wlan_add_client(struct wlan_objmgr_vdev *vdev, struct wlan_objmgr_peer *peer, u_int16_t associd,
                         u_int8_t qos, void *lrates,
                         void *htrates, u_int16_t vhtrates, u_int16_t herates)
{
    u_int8_t newassoc = 0;
    u_int16_t orig_associd = 0;
    ieee80211_vht_rate_t vht;
    u_int16_t vhtrate_map = 0;
    u_int8_t i = 0;
    wlan_if_t vap = NULL;

    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (!vap)
    	return -EINVAL;

    if (IEEE80211_IS_CHAN_2GHZ(vap->iv_ic->ic_curchan) && vhtrates != 0) {
        /* 2G doesn't support VHT */
        return -EINVAL;
    }

    orig_associd = wlan_get_aid(peer);
    if (orig_associd == 0) {
        newassoc = 1;
    }

    if (associd == 0) {
        mlme_err("AID is 0...check\n");
        return -EINVAL;
    }

    if (orig_associd != associd) {
        if (wlan_is_aid_set(vdev, associd)) {
            mlme_err("associd %d already in use...\n", associd);
            return -EINVAL;
        }
    }

    if (orig_associd != 0) {
        mlme_info("replacing old associd %d with new id %d\n", orig_associd, associd);
        wlan_clear_aid(vdev, orig_associd);
    } else {
        mlme_err("New AID is %d\n",associd);
    }

    wlan_set_peer_aid(peer, associd);

    /* override qos flag */
    if (qos)
        wlan_set_peer_flag(peer, WLAN_PEER_F_QOS);
    else
        wlan_clear_peer_flag(peer, WLAN_PEER_F_QOS);

    /* override data rates */
    wlan_set_peer_rate_legacy(peer, (struct ieee80211_rateset *)lrates);

    if (wlan_peer_is_flag_set(peer, IEEE80211_NODE_HT)) {
        wlan_set_peer_rate_ht(peer, (struct ieee80211_rateset *)htrates);
    } else {
        wlan_set_peer_rate_ht(peer, NULL);
    }


    if (wlan_peer_is_flag_set(peer, IEEE80211_NODE_VHT)) {
        qdf_mem_zero(&vht, sizeof(ieee80211_vht_rate_t));
        qdf_mem_set(&(vht.rates), MAX_VHT_STREAMS, 0xff);

        vht.num_streams = wlan_get_peer_num_streams(peer);
        if (vht.num_streams > MAX_VHT_STREAMS) {
            mlme_err("stream num %d beyond max VHT streams\n", vht.num_streams);
            return -EINVAL;
        }
        for(i=0; i < vht.num_streams; i++){
            vht.rates[i] = vhtrates;
        }
        vhtrate_map = ieee80211_get_vht_rate_map(&vht);
        wlan_set_peer_rate_vht(peer, vhtrate_map);
    }else{
        wlan_set_peer_rate_vht(peer, vhtrate_map);
    }

    if (wlan_peer_is_extflag_set(peer, IEEE80211_NODE_HE)) {
        wlan_set_peer_rate_he(peer, herates);
    }

    wlan_peer_dump_rates(peer);

    /* do mlme stuff for processing assoc req here */
    wlan_peer_mlme_recv_assoc_request(peer);

    return 0;
}

u_int16_t wlan_get_aid(struct wlan_objmgr_peer *peer)
{
    struct ieee80211_node *ni = NULL;

    ni = (wlan_node_t)wlan_peer_get_mlme_ext_obj(peer);
    if (ni == NULL) {
        return -EINVAL;
    }
    return IEEE80211_AID(ni->ni_associd);
}
EXPORT_SYMBOL(wlan_get_aid);

int wlan_is_aid_set(struct wlan_objmgr_vdev *vdev, u_int16_t associd)
{
    wlan_if_t vap;

    vap = (wlan_if_t)wlan_vdev_get_mlme_ext_obj(vdev);
    if (vap == NULL) {
        return -EINVAL;
    }
    return IEEE80211_AID_ISSET(vap, associd);
}

void wlan_set_aid(struct wlan_objmgr_vdev *vdev, u_int16_t associd)
{
    wlan_if_t vap;

    vap = (wlan_if_t)wlan_vdev_get_mlme_ext_obj(vdev);
    if (vap == NULL) {
        return;
    }
    IEEE80211_AID_SET(vap, associd);
}

void wlan_clear_aid(struct wlan_objmgr_vdev *vdev, u_int16_t associd)
{
    wlan_if_t vap;

    vap = (wlan_if_t)wlan_vdev_get_mlme_ext_obj(vdev);
    if (vap == NULL) {
      return;
    }
    IEEE80211_AID_CLR(vap, associd);
}

void wlan_set_peer_aid(struct wlan_objmgr_peer *peer, u_int16_t associd)
{
    struct ieee80211_node *ni = NULL;
    struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);

    ni = wlan_peer_get_mlme_ext_obj(peer);
    if (ni == NULL) {
        return;
    }

    ni->ni_associd = associd | IEEE80211_RESV_AID_BITS;
    wlan_set_aid(vdev, ni->ni_associd);
}

void wlan_set_peer_flag(struct wlan_objmgr_peer *peer, int flag)
{
    struct ieee80211_node *ni = NULL;

    ni = wlan_peer_get_mlme_ext_obj(peer);
    if (ni == NULL) {
        return;
    }

    ni->ni_flags |= flag;
}

void wlan_clear_peer_flag(struct wlan_objmgr_peer *peer, int flag)
{
    struct ieee80211_node *ni = NULL;

    ni = wlan_peer_get_mlme_ext_obj(peer);
    if (ni == NULL) {
        return;
    }

    ni->ni_flags &= ~flag;
}

int wlan_peer_is_flag_set(struct wlan_objmgr_peer *peer, int flag)
{
    struct ieee80211_node *ni = NULL;

    ni = wlan_peer_get_mlme_ext_obj(peer);
    if (ni == NULL) {
        return -EINVAL;
    }

    return (ni->ni_flags & flag);
}

int wlan_peer_is_extflag_set(struct wlan_objmgr_peer *peer, int flag)
{
    struct ieee80211_node *ni = NULL;

    ni = wlan_peer_get_mlme_ext_obj(peer);
    if (ni == NULL) {
        return -EINVAL;
    }

    return (ni->ni_ext_flags & flag);
}

int wlan_get_peer_num_streams(struct wlan_objmgr_peer *peer)
{
    struct ieee80211_node *ni = NULL;

    ni = wlan_peer_get_mlme_ext_obj(peer);
    if (ni == NULL) {
        return -EINVAL;
    }
    return ni->ni_streams;
}

void wlan_peer_auth(struct wlan_objmgr_vdev *vdev, u_int8_t *stamac, u_int32_t authorize)
{
    wlan_if_t vap;
    struct ieee80211_node *ni = NULL;

    vap = (wlan_if_t)wlan_vdev_get_mlme_ext_obj(vdev);
    if (vap == NULL) {
        return;
    }

    wlan_node_authorize(vap, authorize, stamac);

    ni = ieee80211_find_node(&vap->iv_ic->ic_sta, stamac);
    if (ni == NULL) {
        return;
    }

    if(!authorize){
        /* Call node leave so that its AID can be released and reused by
        * another client.
        */
        IEEE80211_NODE_LEAVE(ni);
    }
    ieee80211_free_node(ni);
}

void wlan_peer_mlme_recv_assoc_request(struct wlan_objmgr_peer *peer)
{
    struct ieee80211_node *ni = NULL;

    ni = wlan_peer_get_mlme_ext_obj(peer);
    if (ni == NULL) {
        return;
    }

    ieee80211_mlme_recv_assoc_request(ni, 0, NULL, NULL);
}

int wlan_vdev_del_key(struct wlan_objmgr_vdev *vdev, u_int16_t keyix, u_int8_t *macaddr)
{
    wlan_if_t vap;

    vap = (wlan_if_t)wlan_vdev_get_mlme_ext_obj(vdev);
    if (vap == NULL) {
        return -EINVAL;
    }

    wlan_del_key(vap, keyix, macaddr);

    return 0;
}

#define RXMIC_OFFSET 8
int wlan_vdev_set_key(struct wlan_objmgr_vdev *vdev, u_int8_t *macaddr, u_int8_t cipher,
                      u_int16_t keyix, u_int32_t keylen, u_int8_t *keydata)
{
    ieee80211_keyval key_val;
    wlan_if_t vap;

    vap = (wlan_if_t)wlan_vdev_get_mlme_ext_obj(vdev);
    if (vap == NULL) {
        return -EINVAL;
    }

    if (keylen > IEEE80211_KEYBUF_SIZE+IEEE80211_MICBUF_SIZE) {
        return -EINVAL;
    }

    if ((keyix != IEEE80211_KEYIX_NONE) &&
        (keyix >= IEEE80211_WEP_NKID) && (cipher != IEEE80211_CIPHER_AES_CMAC)) {
            return -EINVAL;
    }

    qdf_mem_zero(&key_val, sizeof(ieee80211_keyval));

    key_val.keylen  = keylen;
    if (key_val.keylen > IEEE80211_KEYBUF_SIZE) {
        key_val.keylen  = IEEE80211_KEYBUF_SIZE;
    }
    key_val.rxmic_offset = IEEE80211_KEYBUF_SIZE + RXMIC_OFFSET;
    key_val.txmic_offset =  IEEE80211_KEYBUF_SIZE;
    key_val.keytype = cipher;
    key_val.macaddr = macaddr;
    key_val.keydata = keydata;

    /* allow keys to allocate anywhere in key cache */
    wlan_set_param(vap, IEEE80211_WEP_MBSSID, 1);
    wlan_set_key(vap, keyix, &key_val);
    wlan_set_param(vap, IEEE80211_WEP_MBSSID, 0);  /* put it back to default */

    return 0;
}

u_int32_t wlan_ucfg_get_maxphyrate(struct wlan_objmgr_vdev *vdev)
{
	wlan_if_t vap;

	vap = (wlan_if_t)wlan_vdev_get_mlme_ext_obj(vdev);
	if (vap == NULL) {
		return 0;
	}

	return ieee80211_ucfg_get_maxphyrate(vap);
}

uint8_t wlan_get_opclass(struct wlan_objmgr_vdev *vdev)
{
	wlan_if_t vap;
	char country_iso[4];

	vap = (wlan_if_t)wlan_vdev_get_mlme_ext_obj(vdev);
	if (vap == NULL) {
		return 0;
	}

    ieee80211_getCurrentCountryISO(vap->iv_ic, country_iso);
    return regdmn_get_opclass(country_iso, vap->iv_ic->ic_curchan);
}

uint8_t wlan_get_prim_chan(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_channel *chan = NULL;

	if (vdev == NULL) {
		QDF_BUG(0);
		return 0;
	}
	chan = wlan_vdev_mlme_get_bss_chan(vdev);

	if (chan == NULL) {
		return 0;
	}

	return (chan->ch_ieee);
}

uint8_t wlan_get_seg1(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_channel *chan = NULL;

	if (vdev == NULL) {
		QDF_BUG(0);
		return 0;
	}
	chan = wlan_vdev_mlme_get_bss_chan(vdev);

	if (chan == NULL) {
		return 0;
	}

	return (chan->ch_freq_seg2);
}

uint16_t wlan_peer_get_beacon_interval(struct wlan_objmgr_peer *peer)
{
	struct ieee80211_node *ni;

	if (peer == NULL)
		return 0;

	ni = wlan_peer_get_mlme_ext_obj(peer);
	if (ni == NULL)
		return 0;

	return ieee80211_node_get_beacon_interval(ni);
}
EXPORT_SYMBOL(wlan_peer_get_beacon_interval);

QDF_STATUS wlan_vdev_get_elemid(struct wlan_objmgr_vdev *vdev,
				ieee80211_frame_type ftype, uint8_t *iebuf,
				uint32_t *ielen, uint32_t elem_id)
{
	struct ieee80211vap *vap;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (vdev == NULL)
		return QDF_STATUS_E_INVAL;

	vap = wlan_vdev_get_mlme_ext_obj(vdev);
	if (vap == NULL)
		return QDF_STATUS_E_INVAL;

	if (wlan_mlme_app_ie_get_elemid(vap->vie_handle, ftype,
			iebuf, ielen, elem_id)) {
		status = QDF_STATUS_E_FAILURE;
	}

	return status;
}



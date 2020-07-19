/*
 * Copyright (c) 2017, 2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

#include <ieee80211_var.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <if_athvar.h>
#include <wlan_osif_priv.h>
#include "ieee80211_mlme_dfs_dispatcher.h"
#include "ieee80211_objmgr_priv.h"
#include <wlan_dfs_ioctl.h>

#ifdef WLAN_DFS_PRECAC_AUTO_CHAN_SUPPORT
extern void ieee80211_autochan_switch_chan_change_csa(struct ieee80211com *ic,
						      uint8_t ch_ieee,
						      uint16_t ch_width);
#endif

/**
 * Array to convert wlan_phymode enum to ieee80211_phymode
 */
uint8_t convphymode2phymode[WLAN_PHYMODE_11AXA_HE80_80 + 1] = {
    IEEE80211_MODE_AUTO,                   /* autoselect */
    IEEE80211_MODE_11A,                    /* 5GHz, OFDM */
    IEEE80211_MODE_11B,                    /* 2GHz, CCK */
    IEEE80211_MODE_11G,                    /* 2GHz, OFDM */
/*
    IEEE80211_MODE_FH,
    IEEE80211_MODE_TURBO_A,
    IEEE80211_MODE_TURBO_G,
*/
    IEEE80211_MODE_11NA_HT20,              /* 5Ghz, HT20 */
    IEEE80211_MODE_11NG_HT20,              /* 2Ghz, HT20 */
    IEEE80211_MODE_11NA_HT40PLUS,          /* 5Ghz, HT40 (ext ch +1) */
    IEEE80211_MODE_11NA_HT40MINUS,         /* 5Ghz, HT40 (ext ch -1) */
    IEEE80211_MODE_11NG_HT40PLUS,          /* 2Ghz, HT40 (ext ch +1) */
    IEEE80211_MODE_11NG_HT40MINUS,         /* 2Ghz, HT40 (ext ch -1) */
    IEEE80211_MODE_11NG_HT40,              /* 2Ghz, Auto HT40 */
    IEEE80211_MODE_11NA_HT40,              /* 5Ghz, Auto HT40 */
    IEEE80211_MODE_11AC_VHT20,             /* 5Ghz, VHT20 */
    IEEE80211_MODE_11AC_VHT40PLUS,         /* 5Ghz, VHT40 (Ext ch +1) */
    IEEE80211_MODE_11AC_VHT40MINUS,        /* 5Ghz  VHT40 (Ext ch -1) */
    IEEE80211_MODE_11AC_VHT40,             /* 5Ghz, VHT40 */
    IEEE80211_MODE_11AC_VHT80,             /* 5Ghz, VHT80 */
    IEEE80211_MODE_11AC_VHT160,            /* 5Ghz, VHT160 */
    IEEE80211_MODE_11AC_VHT80_80,          /* 5Ghz, VHT80_80 */
    IEEE80211_MODE_11AXA_HE20,             /* 5GHz, HE20 */
    IEEE80211_MODE_11AXG_HE20,             /* 2GHz, HE20 */
    IEEE80211_MODE_11AXA_HE40PLUS,         /* 5GHz, HE40 (ext ch +1) */
    IEEE80211_MODE_11AXA_HE40MINUS,        /* 5GHz, HE40 (ext ch -1) */
    IEEE80211_MODE_11AXG_HE40PLUS,         /* 2GHz, HE40 (ext ch +1) */
    IEEE80211_MODE_11AXG_HE40MINUS,        /* 2GHz, HE40 (ext ch -1) */
    IEEE80211_MODE_11AXA_HE40,             /* 5GHz, HE40 */
    IEEE80211_MODE_11AXG_HE40,             /* 2GHz, HE40 */
    IEEE80211_MODE_11AXA_HE80,             /* 5GHz, HE80 */
    IEEE80211_MODE_11AXA_HE160,            /* 5GHz, HE160 */
    IEEE80211_MODE_11AXA_HE80_80,          /* 5GHz, HE80_80 */
};

/**
 * DOC : These functions are called by DFS modules through dfs_rx_ops.
 */


QDF_STATUS mlme_dfs_start_rcsa(struct wlan_objmgr_pdev *pdev,
        bool *wait_for_csa)
{
	struct ieee80211com *ic;

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if(ic == NULL)
		return QDF_STATUS_E_FAILURE;

	*wait_for_csa = ieee80211_dfs_start_rcsa(ic);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_dfs_mark_dfs(struct wlan_objmgr_pdev *pdev,
		uint8_t ieee,
		uint16_t freq,
		uint8_t vhtop_ch_freq_seg2,
		uint64_t flags)
{
	struct ieee80211com *ic;

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if(ic == NULL)
		return QDF_STATUS_E_FAILURE;

	ieee80211_mark_dfs(ic, ieee, freq, vhtop_ch_freq_seg2, flags);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_dfs_start_csa(struct wlan_objmgr_pdev *pdev,
		uint8_t ieeeChan, uint16_t freq,
		uint8_t cfreq2, uint64_t flags)
{
	struct ieee80211com *ic;

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if(ic == NULL)
		return QDF_STATUS_E_FAILURE;

	if(ieee80211_start_csa(ic, ieeeChan, freq, cfreq2, flags) ==
           QDF_STATUS_E_FAILURE)
           return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_dfs_proc_cac(struct wlan_objmgr_pdev *pdev)
{
	struct ieee80211com *ic;

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if(ic == NULL)
		return QDF_STATUS_E_FAILURE;

	ieee80211_dfs_proc_cac(ic);

	return QDF_STATUS_SUCCESS;
}

bool mlme_dfs_is_enhanced_ind_repeater(struct wlan_objmgr_pdev *pdev)
{
	struct ieee80211com *ic;

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if (ic == NULL)
		return false;

	if (ic->ic_sta_vap && ieee80211_ic_enh_ind_rpt_is_set(ic)) {
		return true;
	}

	return false;
}

QDF_STATUS mlme_dfs_deliver_event_up_after_cac(struct wlan_objmgr_pdev *pdev)
{
	struct ieee80211com *ic;

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if(ic == NULL)
		return QDF_STATUS_E_FAILURE;

	ieee80211_dfs_deliver_events(ic, ic->ic_curchan, WLAN_EV_CAC_COMPLETED);

	OSIF_RADIO_DELIVER_EVENT_UP_AFTER_CAC(ic);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_dfs_get_dfs_ch_nchans(struct wlan_objmgr_pdev *pdev,
		int *nchans)
{
	struct ieee80211com *ic;

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if(ic == NULL)
		return QDF_STATUS_E_FAILURE;

	*nchans = ic->ic_nchans;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_dfs_get_dfs_ch_no_weather_radar_chan(struct wlan_objmgr_pdev *pdev,
		uint8_t *no_wradar)
{
	struct ieee80211com *ic;

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if(ic == NULL)
		return QDF_STATUS_E_FAILURE;

	*no_wradar = ic->ic_no_weather_radar_chan;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_dfs_get_extchan(struct wlan_objmgr_pdev *pdev,
		uint16_t *ic_freq,
		uint64_t *ic_flags,
		uint16_t *ic_flagext,
		uint8_t *ic_ieee,
		uint8_t *ic_vhtop_ch_freq_seg1,
		uint8_t *ic_vhtop_ch_freq_seg2)
{
	struct ieee80211com *ic;
	struct ieee80211_ath_channel *chan;

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if(ic == NULL)
		return QDF_STATUS_E_FAILURE;

	chan = ieee80211_get_extchan(ic);

	if (!chan)
		return QDF_STATUS_E_FAILURE;

	*ic_freq = chan->ic_freq;
	*ic_flags = chan->ic_flags;
	*ic_flagext = chan->ic_flagext;
	*ic_ieee = chan->ic_ieee;
	*ic_vhtop_ch_freq_seg1 = chan->ic_vhtop_ch_freq_seg1;
	*ic_vhtop_ch_freq_seg2 = chan->ic_vhtop_ch_freq_seg2;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_dfs_set_no_chans_available(struct wlan_objmgr_pdev *pdev,
		int val)
{
	struct ieee80211com *ic;

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if(ic == NULL)
		return QDF_STATUS_E_FAILURE;

	ic->no_chans_available = val;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_dfs_ieee2mhz(struct wlan_objmgr_pdev *pdev,
		int ieee,
		uint64_t flag,
		int *freq)
{
	struct ieee80211com *ic;

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if(ic == NULL)
		return QDF_STATUS_E_FAILURE;

	*freq = ieee80211_ieee2mhz(ic, ieee, flag);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_dfs_find_dot11_channel(struct wlan_objmgr_pdev *pdev,
		uint8_t ieee,
		uint8_t des_cfreq2,
		int mode,
		uint16_t *ic_freq,
		uint64_t *ic_flags,
		uint16_t *ic_flagext,
		uint8_t *ic_ieee,
		uint8_t *ic_vhtop_ch_freq_seg1,
		uint8_t *ic_vhtop_ch_freq_seg2)
{
	struct ieee80211_ath_channel *chan;
	struct ieee80211com *ic;
	enum ieee80211_phymode phy_mode;

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if(ic == NULL)
		return QDF_STATUS_E_FAILURE;

	phy_mode = convphymode2phymode[mode];
	chan = ieee80211_find_dot11_channel(ic, ieee, des_cfreq2,
					    phy_mode);
	if (!chan)
		return QDF_STATUS_E_FAILURE;

	*ic_freq = chan->ic_freq;
	*ic_flags = chan->ic_flags;
	*ic_flagext = chan->ic_flagext;
	*ic_ieee = chan->ic_ieee;
	*ic_vhtop_ch_freq_seg1 = chan->ic_vhtop_ch_freq_seg1;
	*ic_vhtop_ch_freq_seg2 = chan->ic_vhtop_ch_freq_seg2;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_dfs_get_dfs_ch_channels(struct wlan_objmgr_pdev *pdev,
		uint16_t *ic_freq,
		uint64_t *ic_flags,
		uint16_t *ic_flagext,
		uint8_t *ic_ieee,
		uint8_t *ic_vhtop_ch_freq_seg1,
		uint8_t *ic_vhtop_ch_freq_seg2,
		int index)
{
	struct ieee80211_ath_channel *c;
	struct ieee80211com *ic;

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if(ic == NULL)
		return QDF_STATUS_E_FAILURE;

	c = &ic->ic_channels[index];
	if (c == NULL)
		return QDF_STATUS_E_FAILURE;

	*ic_freq = c->ic_freq;
	*ic_flags = c->ic_flags;
	*ic_flagext = c->ic_flagext;
	*ic_ieee = c->ic_ieee;
	*ic_vhtop_ch_freq_seg1 = c->ic_vhtop_ch_freq_seg1;
	*ic_vhtop_ch_freq_seg2 = c->ic_vhtop_ch_freq_seg2;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_dfs_dfs_ch_flags_ext(struct wlan_objmgr_pdev *pdev,
		uint16_t *flag_ext)
{
	struct ieee80211com *ic;

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if(ic == NULL)
		return QDF_STATUS_E_FAILURE;

	*flag_ext = ic->ic_flags_ext;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_dfs_channel_change_by_precac(struct wlan_objmgr_pdev *pdev)
{
	struct ieee80211com *ic;

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if(ic == NULL)
		return QDF_STATUS_E_FAILURE;

	ieee80211_dfs_channel_change_by_precac(ic);

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_DFS_PRECAC_AUTO_CHAN_SUPPORT
QDF_STATUS mlme_dfs_precac_chan_change_csa(struct wlan_objmgr_pdev *pdev,
					   uint8_t des_chan,
					   enum wlan_phymode des_mode)
{
	struct ieee80211com *ic;

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if(ic == NULL)
		return QDF_STATUS_E_FAILURE;

	ieee80211_autochan_switch_chan_change_csa(ic, des_chan,
						  convphymode2phymode[des_mode]);

	return QDF_STATUS_SUCCESS;

}
#endif

QDF_STATUS mlme_dfs_nol_timeout_notification(struct wlan_objmgr_pdev *pdev)
{
	struct ieee80211com *ic;

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if(ic == NULL)
		return QDF_STATUS_E_FAILURE;

	ieee80211_dfs_nol_timeout_notification(ic);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_dfs_clist_update(struct wlan_objmgr_pdev *pdev,
		void *nollist,
		int nentries)
{
	struct ieee80211com *ic;

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if(ic == NULL)
		return QDF_STATUS_E_FAILURE;

	ieee80211_dfs_clist_update(ic, nollist, nentries);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_dfs_get_cac_timeout(struct wlan_objmgr_pdev *pdev,
		uint16_t ic_freq,
		uint8_t ic_vhtop_ch_freq_seg2,
		uint64_t ic_flags,
		int *cac_timeout)
{
	struct ieee80211_ath_channel *chan;
	struct ieee80211com *ic;

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if(ic == NULL)
		return QDF_STATUS_E_FAILURE;

	chan = (ieee80211_find_channel(ic, ic_freq, ic_vhtop_ch_freq_seg2, ic_flags));

	if (chan) {
		*cac_timeout = ieee80211_dfs_get_cac_timeout(ic, chan);
		return QDF_STATUS_SUCCESS;
	}

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS mlme_dfs_get_precac_timeout(struct wlan_objmgr_pdev *pdev,
		uint16_t ic_freq,
		uint8_t ic_vhtop_ch_freq_seg2,
		uint64_t ic_flags,
		int *precac_timeout)
{
	struct ieee80211_ath_channel *chan;
	struct ieee80211com *ic;

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if(ic == NULL)
		return QDF_STATUS_E_FAILURE;

	chan = (ieee80211_find_channel(ic, ic_freq, ic_vhtop_ch_freq_seg2, ic_flags));

	if (chan) {
		*precac_timeout = ieee80211_dfs_get_precac_timeout(ic, chan);
		return QDF_STATUS_SUCCESS;
	}

	return QDF_STATUS_E_FAILURE;
}


bool mlme_dfs_is_opmode_sta(struct wlan_objmgr_pdev *pdev)
{
	struct ieee80211com *ic;

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if(ic != NULL)
		return (ic->ic_opmode == IEEE80211_M_STA);

	return false;
}

#if defined(WLAN_DFS_PARTIAL_OFFLOAD) && defined(HOST_DFS_SPOOF_TEST)
QDF_STATUS mlme_dfs_rebuild_chan_list_with_non_dfs_channels(struct wlan_objmgr_pdev *pdev)
{
	struct ieee80211com *ic;
	QDF_STATUS ret = QDF_STATUS_E_FAILURE;

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if(ic != NULL)
		ret = ieee80211_dfs_rebuild_chan_list_with_non_dfs_channels(ic);

	return ret;
}

QDF_STATUS mlme_dfs_restart_vaps_with_non_dfs_chan(struct wlan_objmgr_pdev *pdev, int no_chans_avail)
{
	struct ieee80211com *ic;
	QDF_STATUS ret = QDF_STATUS_E_FAILURE;

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if(ic != NULL){
		ieee80211_restart_vaps_with_non_dfs_channels(ic, no_chans_avail);
		ret = QDF_STATUS_SUCCESS;
	}

	return ret;
}
#else
QDF_STATUS mlme_dfs_rebuild_chan_list_with_non_dfs_channels(struct wlan_objmgr_pdev *pdev)
{
	return 0;
}

QDF_STATUS mlme_dfs_restart_vaps_with_non_dfs_chan(struct wlan_objmgr_pdev *pdev, int no_chans_avail)
{
	return 0;
}
#endif /* HOST_DFS_SPOOF_TEST */

#if defined(WLAN_SUPPORT_PRIMARY_ALLOWED_CHAN)
bool mlme_dfs_check_allowed_prim_chanlist(struct wlan_objmgr_pdev *pdev,
                                                uint32_t chan_num)
{
        struct ieee80211com *ic;

        ic = (struct ieee80211com *)wlan_pdev_get_mlme_ext_obj(pdev);

        if(ic != NULL && ic->ic_primary_allowed_enable)
                return ieee80211_check_allowed_prim_chanlist(ic, chan_num);

        return true;
}
#endif /* WLAN_SUPPORT_PRIMARY_ALLOWED_CHAN */

QDF_STATUS mlme_dfs_bringdown_vaps(struct wlan_objmgr_pdev
                                   *pdev)
{
    struct ieee80211com *ic;
    QDF_STATUS ret = QDF_STATUS_E_FAILURE;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (ic) {
        ic->no_chans_available = 1;
        ieee80211_bringdown_vaps(ic);
        ret = QDF_STATUS_SUCCESS;
    }

    return ret;
}

void mlme_dfs_deliver_event(struct wlan_objmgr_pdev *pdev,
        u_int16_t freq, enum WLAN_DFS_EVENTS event)
{
    struct ieee80211com *ic;

    ic = (struct ieee80211com *) wlan_pdev_get_mlme_ext_obj(pdev);
    if(ic == NULL)
        return;

    ieee80211_dfs_deliver_event(ic, freq, event);
}

QDF_STATUS mlme_dfs_update_scan_channel_list(struct wlan_objmgr_pdev *pdev)
{
    struct ieee80211com *ic;
    QDF_STATUS ret = QDF_STATUS_E_FAILURE;
    int num_chans;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    mlme_dfs_get_dfs_ch_nchans(pdev, &num_chans);

    if (ic && num_chans) {
        ret = QDF_STATUS_SUCCESS;
        wlan_scan_update_channel_list(ic);
    }
    return ret;
}

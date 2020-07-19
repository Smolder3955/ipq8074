/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

/* This file implements all public function api for WIFI SON */
#include <wlan_son_pub.h>
#include "wlan_son_internal.h"
#include <wlan_son_utils_api.h>
#include <ath_band_steering.h>
#include <ieee80211_mlme_ops.h>

#if QCA_SUPPORT_SON
/**
 * @brief Increment repeater count per vap.
 *
 * This is used by assoication routine based on ie detection.
 *
 * @param [in] vdev VAP for incrementing repeater count.
 *
 * @param [inout] void .
 */

void son_repeater_cnt_inc(struct wlan_objmgr_vdev *vdev)
{
	struct son_vdev_priv *vdev_priv = NULL;
	vdev_priv = wlan_son_get_vdev_priv(vdev);

	vdev_priv->iv_connected_REs++;
	return;
}

/**
 * @brief Decrement repeater count per vap.
 *
 * This is used by assoication routine based on ie detection.
 *
 * @param [in] vdev VAP for incrementing repeater count.
 *
 * @param [inout] void .
 */

void son_repeater_cnt_dec(struct wlan_objmgr_vdev *vdev)
{
	struct son_vdev_priv *vdev_priv = NULL;
	vdev_priv = wlan_son_get_vdev_priv(vdev);

	vdev_priv->iv_connected_REs--;
	return;
}

/**
 * @brief Store uplink bssid for SON.
 *
 * This is used by association routine to store bssid upon association.
 *
 * @param [in] pdev
 * @param [in] bssid to store
 *
 * @param [inout] void.
 */

void son_update_uplink_bssid(struct wlan_objmgr_pdev *pdev, char *bssid)
{

	struct son_pdev_priv *pd_priv = NULL;

	pd_priv = wlan_son_get_pdev_priv(pdev);

	qdf_mem_copy(pd_priv->uplink_bssid, bssid, MAC_ADDR_LEN);
	return ;
}

/**
 * @brief To update backhaul rate.
 *
 * This is used by uplink node detection logic.
 *
 * @param [in] vdev participating in uplink node detection.
 * @param [in] rate to be updated.
 * @param [in] bool self , update self rate if true or  uplink rate.
 *
 * @param [inout] void
 */

void son_update_backhaul_rate(struct wlan_objmgr_vdev *vdev, u_int16_t rate ,
			      bool self)
{
	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_pdev_priv *pd_priv = NULL;

	pdev = wlan_vdev_get_pdev(vdev);

	pd_priv = wlan_son_get_pdev_priv(pdev);

	if (self)
		pd_priv->serving_ap_backhaul_rate = rate;
	else
		pd_priv->uplink_rate = rate;
	return;
}

/**
 * @brief To update uplink (backhaul link) snr
 *
 * @param [in] vdev participating in uplink node detection.
 * @param [in] snr to be updated.
 *
 * @param [inout] void
 */

void son_update_uplink_snr(struct wlan_objmgr_vdev *vdev, u_int8_t snr)
{

	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_pdev_priv *pd_priv = NULL;

	pdev = wlan_vdev_get_pdev(vdev);

	pd_priv = wlan_son_get_pdev_priv(pdev);

	pd_priv->uplink_snr = snr;
	return;
}

/**
 * @brief Get backhaul rate.
 *
 * This is used by uplink node detection logic.
 *
 * @param [in] vdev participating in detection logic.
 * @param [in] bool self  true means for serving ap rate
 *
 * @param [inout] return rate.
 */

u_int16_t son_get_backhaul_rate(struct wlan_objmgr_vdev *vdev , bool self)
{
	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_pdev_priv *pd_priv = NULL;

	pdev = wlan_vdev_get_pdev(vdev);

	pd_priv = wlan_son_get_pdev_priv(pdev);

	if (self)
		return pd_priv->serving_ap_backhaul_rate;
	else
		return pd_priv->uplink_rate;
}

/**
 * @brief Get connected repeater count.
 *
 * This is used by uplink node detection
 *
 * @param [in] Vdev participating.
 *
 * @param [inout] count of connected repeater.
 */

u_int8_t son_repeater_cnt_get(struct wlan_objmgr_vdev *vdev)
{
	struct son_vdev_priv *vdev_priv = NULL;

	vdev_priv = wlan_son_get_vdev_priv(vdev);

	return vdev_priv->iv_connected_REs;
}

/**
 * @brief Enable/disable inst rssi log for son
 *
 * @param [in] enable/disable log
 *
 * @param [inout] void.
 */

void son_record_inst_rssi_log_enable (struct wlan_objmgr_vdev *vdev, int enable)
{
	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_pdev_priv *pd_priv = NULL;

	pdev = wlan_vdev_get_pdev(vdev);

	pd_priv = wlan_son_get_pdev_priv(pdev);
	if (wlan_son_is_vdev_enabled(vdev))
	{
		if(enable)
			pd_priv->son_inst_rssi_log = true;
		else
			pd_priv->son_inst_rssi_log = false;
	}
	else
		qdf_err("SON mode not enabled for bsteerrssi_log \n");
}

/**
 * @brief Record RSSI per peer.
 *
 * @param [in] peer to update RSSI.
 * @param [in] rssi value
 *
 * @param [inout] void.
 */

PUBLIC void son_record_inst_peer_rssi(struct wlan_objmgr_peer *peer,
				      u_int8_t rssi)
{
	struct son_pdev_priv *pd_priv = NULL;
	struct son_peer_priv *pe_priv = NULL;
	bool generate_event = false;
	u_int8_t report_rssi = BSTEERING_INVALID_RSSI;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;

	if (!peer) {
		return;
	}

	vdev = wlan_peer_get_vdev(peer);

	pdev = wlan_vdev_get_pdev(vdev);

	psoc = wlan_pdev_get_psoc(pdev);

	pe_priv = wlan_son_get_peer_priv(peer);
	if (!wlan_son_is_vdev_enabled(vdev) ||
		!wlan_son_is_pdev_valid(pdev)) {
		return;
	}

	pd_priv = wlan_son_get_pdev_priv(pdev);

	do {
		SON_LOCK(&pd_priv->son_lock);
		if (!wlan_son_is_pdev_enabled(pdev)) {
			break;
		}

		if (!pd_priv->son_inst_rssi_inprogress ||
		    WLAN_ADDR_EQ(pd_priv->son_inst_rssi_macaddr,
				 peer->macaddr)) {
			/* The RSSI measurement is not for the one requested */
			SON_LOGI("Inst RSSI measurement for [%x] [%x] [%x] [%x] [%x] [%x] is %d",
				 pd_priv->son_inst_rssi_macaddr[0],
				 pd_priv->son_inst_rssi_macaddr[1],
				 pd_priv->son_inst_rssi_macaddr[2],
				 pd_priv->son_inst_rssi_macaddr[3],
				 pd_priv->son_inst_rssi_macaddr[4],
				 pd_priv->son_inst_rssi_macaddr[5],
				 rssi);
			break;
		}

		if (BSTEERING_INVALID_RSSI != rssi) {
			pd_priv->son_avg_inst_rssi = ((pd_priv->son_avg_inst_rssi
					       * pd_priv->son_inst_rssi_count)+
						      rssi) /
				( pd_priv->son_inst_rssi_count + 1);
			++ pd_priv->son_inst_rssi_count;
			if(pd_priv->son_inst_rssi_log)
			{
				qdf_info("Inst RSSI measurement for [%x] [%x] [%x] [%x] [%x] [%x] is %d",
					pd_priv->son_inst_rssi_macaddr[0],
					pd_priv->son_inst_rssi_macaddr[1],
					pd_priv->son_inst_rssi_macaddr[2],
					pd_priv->son_inst_rssi_macaddr[3],
					pd_priv->son_inst_rssi_macaddr[4],
					pd_priv->son_inst_rssi_macaddr[5],
					rssi);
			}
			if ( pd_priv->son_inst_rssi_count >=  pd_priv->son_inst_rssi_num_samples) {
				generate_event = true;
				report_rssi =  pd_priv->son_avg_inst_rssi;
			}
			/* It is important to update the value here as otherwise
			   the threshold crossing logic will not work properly.
			   The example scenario is:
			   1. STA is active and average RSSI is
			   above the crossing threshold.
			   2. STA becomes inactive which triggers
			   an instantaneous RSSI measuremnt, and the triggered
			   measurement is below the threshold.
			   3. Further updates to the average RSSI are above
			   the threshold
			   In this scenario, we want the RSSI update in
			   step 3 to generate an RSSIcrossing event. */
			pe_priv->son_rssi = rssi;
		} else {
			++pd_priv->son_inst_rssi_err_count;
			/* If we get twice as many failed samples
			   as the number of samplesrequested.
			   just give up and indicate a failure to
			   measure the RSSI. */
			if (pd_priv->son_inst_rssi_err_count >= 2 *
			    pd_priv->son_inst_rssi_num_samples) {
				generate_event = true;
			}
		}

		if (generate_event) {
			son_send_rssi_measurement_event(vdev, peer->macaddr,
					report_rssi, false /* is_debug */);
			qdf_timer_stop(&pd_priv->son_inst_rssi_timer);
			pd_priv->son_inst_rssi_inprogress = false;
			break;
		}

		/* More measurements are needed */
		SON_TARGET_IF_TX_NULL(pdev,
				      peer->macaddr, vdev);
	} while (0);

	SON_UNLOCK(&pd_priv->son_lock);
}
EXPORT_SYMBOL(son_record_inst_peer_rssi);

/**
 * @brief Send error to user space if instantanous rssi was failed.
 *
 * This is used by rssi threshold logic in steering.
 *
 * @param [in] peer
 *
 * @param [inout] void.
 */

PUBLIC void son_record_inst_peer_rssi_err(struct wlan_objmgr_peer *peer)
{
	son_record_inst_peer_rssi(peer, BSTEERING_INVALID_RSSI);
}

EXPORT_SYMBOL(son_record_inst_peer_rssi_err);

/**
 * @brief update user space if current rate changes from last reported rate.
 *
 * This is used by rate indication logic in steering.
 *
 * @param [in] peer
 * @param [in] curent rate.
 * @param [in] last tx rate.
 *
 * @param [inout] void.
 */

PUBLIC void son_update_peer_rate(struct wlan_objmgr_peer *peer, u_int32_t txrate,
				 u_int32_t last_txrate)
{
	struct son_pdev_priv *pd_priv = NULL;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_peer_priv *pe_priv = NULL;
	u_int32_t high_threshold, low_threshold;
	BSTEERING_XING_DIRECTION xing = BSTEERING_XING_UNCHANGED;

	if (!peer) {
		return;
	}

	vdev = wlan_peer_get_vdev(peer);

	pdev = wlan_vdev_get_pdev(vdev);

	if (!wlan_son_is_vdev_enabled(vdev) ||
		!wlan_son_is_pdev_enabled(pdev)) {
		return;
	}

	pd_priv = wlan_son_get_pdev_priv(pdev);
	pe_priv = wlan_son_get_peer_priv(peer);

	do {
		SON_LOCK(&pd_priv->son_lock);

		if (pd_priv->son_dbg_config_params.raw_tx_rate_log_enable) {
			son_send_tx_rate_measurement_event(vdev, peer->macaddr,
							   last_txrate);
		}

		low_threshold = pd_priv->son_config_params.low_tx_rate_crossing_threshold;
		high_threshold = 
			pd_priv->son_config_params.high_tx_rate_crossing_threshold[pe_priv->son_peer_class_group];

		if (!last_txrate) {
			/* First Tx rate measurement.In this case,
			   generate an event ifthe rate is above / below
			   the threshold (since there is no history to check).*/
			if (txrate < low_threshold) {
				xing = BSTEERING_XING_DOWN;
			} else if (txrate > high_threshold) {
				xing = BSTEERING_XING_UP;
			}
			break;
		}

		/* Check thresold crossings */
		if (txrate < low_threshold &&
		    last_txrate >= low_threshold) {
			xing = BSTEERING_XING_DOWN;
		} else if (txrate > high_threshold &&
			   last_txrate <= high_threshold) {
			xing = BSTEERING_XING_UP;
		}
	} while (0);

	if (xing != BSTEERING_XING_UNCHANGED) {
		son_send_tx_rate_xing_event(vdev, peer->macaddr, last_txrate,
					    xing);
	}

	SON_UNLOCK(&pd_priv->son_lock);
}
EXPORT_SYMBOL(son_update_peer_rate);

/**
 * @brief To enable or disable SON timers.
 *
 * This is used by vap create or delete functionality to enable or disable SON timers.
 *
 * @param [in] vdev.
 * @param [in] enable - enable or disable SON timers.
 */

PUBLIC int son_enable_disable_steering(struct wlan_objmgr_vdev *vdev,
			     bool enable)
{
    return son_core_pdev_enable_disable_steering(vdev, enable);
}
EXPORT_SYMBOL(son_enable_disable_steering);

/**
 * @brief Determine whether the ACK RSSI is enabled or not for steering
 *
 * @param [in] vdev
 *
 * @return non-zero if it is enabled; otherwise 0
 */
PUBLIC u_int8_t son_is_ackrssi_enabled(struct wlan_objmgr_vdev *vdev)
{
        struct son_vdev_priv *vdev_priv = NULL;

        vdev_priv = wlan_son_get_vdev_priv(vdev);

        if (vdev_priv)
                return atomic_read(&vdev_priv->v_ackrssi_enabled);

        return false;

}
EXPORT_SYMBOL(son_is_ackrssi_enabled);

/**
 * @brief To check if probe response are withheld during steering.
 *
 * This is used by frame input routine to check withheld condition.
 *
 * @param [in] vdev.
 * @param [in] address 2 from mac frame.
 * @param [in] rssi of frame received.
 * @param [inout] false if not enabled otherwise true.
 */

bool son_is_probe_resp_wh_2G(struct wlan_objmgr_vdev *vdev,
			     u_int8_t *mac_addr,
			     u_int8_t sta_rssi)
{
	struct wlan_objmgr_pdev *pdev = NULL;

	pdev = wlan_vdev_get_pdev(vdev);

	if( !wlan_son_is_pdev_enabled(pdev) ||
	    !wlan_son_is_vdev_enabled(vdev))
		return false;

	return son_core_is_probe_resp_wh_2g(vdev, mac_addr, sta_rssi);
}

/**
 * @brief Query the band steering module for whether it is withholding
 *        probe responses for the given MAC address on this VAP.
 *
 * @param [in] vdev  the VAP on which the probe request was received
 * @param [in] mac_addr  the MAC address of the client that sent the probe
 *                       request
 *
 * @return true if the response should be withheld; otherwise false
 */

bool son_is_probe_resp_wh(struct wlan_objmgr_vdev *vdev,
			  const u_int8_t *mac_addr)
{
	struct wlan_objmgr_pdev *pdev = NULL;

	pdev = wlan_vdev_get_pdev(vdev);

	if( !wlan_son_is_pdev_enabled(pdev) ||
	    !wlan_son_is_vdev_enabled(vdev))
		return false;

	return wlan_vdev_acl_is_probe_wh_set(vdev, mac_addr);
}

/**
 * @brief Called when firmware stats are updated for a STA, with
 *        RSSI changed and a valid Tx rate
 *
 * @param [in] ni  the node for which the stats are updated
 * @param [in] current_vdev  vdev for which stats up to this point
 *                          have been collected.  If it does not
 *                          match the VAP the current node is
 *                          on, should start message over so
 *                          each message is only STAs on a
 *                          particular VAP
 * @param [inout] sta_stats  the structure to update with STA
 *                           stats
 *
 * @return true if stats are updated, interference detection is
 *         enabled on the radio, and band steering is enabled on
 *         the VAP; false otherwise
 */
bool son_update_sta_stats(
	struct wlan_objmgr_peer *peer,
	struct wlan_objmgr_vdev *current_vdev,
	struct bs_sta_stats_ind *sta_stats,
	void *stats)
{
	struct son_pdev_priv *pd_priv = NULL;
	bool updated_stats = false;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;

	if (!peer) {
		return false;
	}

	vdev = wlan_peer_get_vdev(peer);

	pdev = wlan_vdev_get_pdev(vdev);

	if (!sta_stats || !wlan_son_is_vdev_enabled(vdev) ||
		!wlan_son_is_pdev_valid(pdev)) {
		return false;
	}

	pd_priv = wlan_son_get_pdev_priv(pdev);

	do {
		SON_LOCK(&pd_priv->son_lock);
		if (!wlan_son_is_pdev_enabled(pdev)) {
			SON_UNLOCK(&pd_priv->son_lock);
			break;
		}

		if ((current_vdev && current_vdev != vdev) ||
		    sta_stats->peer_count >= BSTEERING_MAX_PEERS_PER_EVENT) {
			/* Send the current message immediately, then start over */
			son_send_sta_stats_event(current_vdev, sta_stats);
			sta_stats->peer_count = 0;
		}

		updated_stats = wlan_peer_update_sta_stats(peer, sta_stats, stats);
		SON_UNLOCK(&pd_priv->son_lock);
	} while (0);

	return updated_stats;
}

EXPORT_SYMBOL(son_update_sta_stats);
/**
 * @brief check if rssi seq is matching with last received frame.
 * @param [in] peer .
 * @param [in] rssi sequnece number.
 *
 * @param [inout] True if matches .
 */

PUBLIC int32_t son_match_peer_rssi_seq(struct wlan_objmgr_peer *peer,
				       u_int32_t rssi_seq)
{
	struct son_peer_priv *pe_priv = NULL;
	struct wlan_objmgr_vdev *vdev = NULL;

	vdev = wlan_peer_get_vdev(peer);

	if (!wlan_son_is_vdev_enabled(vdev))
		return false;

	pe_priv = wlan_son_get_peer_priv(peer);

	if ( rssi_seq != pe_priv->son_rssi_seq) {
		pe_priv->son_rssi_seq = rssi_seq;
		return true;
	} else
		return false;
}
EXPORT_SYMBOL(son_match_peer_rssi_seq);
/**
 * @brief Check if an RSSI measurement crossed the threshold
 *
 * @param [in] peer  the node for which the RSSI measurement occurred
 * @param [in] rssi  the measured RSSI
 * @param [in] low_threshold  the lower threshold to compare with
 * @param [in] high_threshold  the upper threshold to compare with
 *
 * @return the crossing direction as enumerated in BSTEERING_XING_DIRECTION
 */
static BSTEERING_XING_DIRECTION son_check_rssi_cross_threshold(
	struct wlan_objmgr_peer *peer,
	u_int8_t rssi, u_int32_t low_threshold,
	u_int32_t high_threshold)
{
	struct wlan_objmgr_pdev *pdev = NULL;
	u_int8_t delta = 0;
	BSTEERING_XING_DIRECTION direction = BSTEERING_XING_UNCHANGED;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct son_pdev_priv *pd_priv = NULL;
	struct son_peer_priv *pe_priv = NULL;

	vdev = wlan_peer_get_vdev(peer);

	pdev = wlan_vdev_get_pdev(vdev);

	pd_priv = wlan_son_get_pdev_priv(pdev);

	delta = pd_priv->son_rssi_xing_report_delta;
	/* In offload the averaging of RSSI value is taken care of.
	   So delta value is 0 */
	/* The RSSI value at a particular position keeps
	   toggling and the approx window size is 6.So the delta value is 2 */

	pe_priv = wlan_son_get_peer_priv(peer);

	if ((rssi < low_threshold && pe_priv->son_rssi >= low_threshold) ||
	    (rssi < high_threshold - delta && pe_priv->son_rssi >= high_threshold - delta))
		direction = BSTEERING_XING_DOWN;
	else if ((rssi > high_threshold + delta && pe_priv->son_rssi <= high_threshold + delta) ||
		 (rssi > low_threshold + delta && pe_priv->son_rssi <= low_threshold + delta))
		direction =  BSTEERING_XING_UP;

	return direction;
}

/**
 * @brief to record rssi per peer.
 *
 * This is used by steering routine to record rssi per peer.
 *
 * @param [in] peer
 * @param [in] rssi to be stored.
 *
 * @param [inout] void.
 */

PUBLIC void son_record_peer_rssi(struct wlan_objmgr_peer *peer, u_int8_t rssi)
{
	BSTEERING_XING_DIRECTION inact_xing = BSTEERING_XING_UNCHANGED;
	BSTEERING_XING_DIRECTION low_xing = BSTEERING_XING_UNCHANGED;
	BSTEERING_XING_DIRECTION rate_xing = BSTEERING_XING_UNCHANGED;
	BSTEERING_XING_DIRECTION ap_xing = BSTEERING_XING_UNCHANGED;
	BSTEERING_XING_DIRECTION map_xing = BSTEERING_XING_UNCHANGED;
	u_int32_t low_rate_rssi_threshold = 0;
	u_int32_t high_rate_rssi_threshold = 0;
	u_int32_t low_rate_rssi_threshold_map = 0;
	u_int32_t high_rate_rssi_threshold_map = 0;
	u_int32_t ap_steer_rssi_xing_low_threshold = 0;
	u_int32_t inactive_rssi_xing_high_threshold = 0;
	u_int32_t inactive_rssi_xing_low_threshold = 0;
	struct son_pdev_priv *pd_priv = NULL;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_peer_priv *pe_priv = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	ieee80211_bsteering_param_t *son_config_params = NULL;
	ieee80211_bsteering_map_param_t *map_config_params = NULL;
	u_int8_t client_class_group = 0;

	if (!peer) {
		return;
	}

	vdev = wlan_peer_get_vdev(peer);

	pdev = wlan_vdev_get_pdev(vdev);

	psoc = wlan_pdev_get_psoc(pdev);

	if (!wlan_son_is_vdev_enabled(vdev) ||
		!wlan_son_is_pdev_valid(pdev)) {
		return;
	}

	pd_priv = wlan_son_get_pdev_priv(pdev);
	pe_priv = wlan_son_get_peer_priv(peer);
	son_config_params = &pd_priv->son_config_params;

	if (!pe_priv) {
		return;
	}

	do {
		SON_LOCK(&pd_priv->son_lock);

		if (!wlan_son_is_pdev_enabled(pdev)) {
			break;
		}

		if (pd_priv->son_dbg_config_params.raw_rssi_log_enable) {
			son_send_rssi_measurement_event(vdev,
						peer->macaddr,
						rssi, true /* is_debug */);
		}

		if (son_config_params->client_classification_enable) {
			client_class_group = pe_priv->son_peer_class_group;
		}

		ap_steer_rssi_xing_low_threshold =
				son_config_params->ap_steer_rssi_xing_low_threshold[client_class_group];
		inactive_rssi_xing_high_threshold =
				son_config_params->inactive_rssi_xing_high_threshold[client_class_group];
		inactive_rssi_xing_low_threshold =
				son_config_params->inactive_rssi_xing_low_threshold[client_class_group];
		high_rate_rssi_threshold =
				son_config_params->high_rate_rssi_crossing_threshold[client_class_group];
		low_rate_rssi_threshold =
				son_config_params->low_rate_rssi_crossing_threshold;

		if (!pe_priv->son_rssi) {
		/* First RSSI measurement */
		/* Check if the RSSI starts above or below the rate threshold
		   (STA will always be considered active at startup) */
			if (rssi < low_rate_rssi_threshold) {
				rate_xing = BSTEERING_XING_DOWN;
			} else if (rssi > high_rate_rssi_threshold) {
				rate_xing = BSTEERING_XING_UP;
			}
			break;
		}

		if(son_core_is_peer_inact(peer)) {
			/* Check inactivity rssi threshold crossing */
			inact_xing = son_check_rssi_cross_threshold(
				peer, rssi,
				inactive_rssi_xing_low_threshold,
				inactive_rssi_xing_high_threshold);
		} else {
			/* Check rate rssi thresold crossing */
			rate_xing = son_check_rssi_cross_threshold(
				peer, rssi, low_rate_rssi_threshold,
				high_rate_rssi_threshold);
		}
		/* Check low rssi thresold crossing */
		low_xing = son_check_rssi_cross_threshold(
			peer, rssi,
			son_config_params->low_rssi_crossing_threshold,
			BSTEER_INVALID_RSSI_HIGH_THRESHOLD);
		/* Check AP rssi threshold crossing */
		ap_xing = son_check_rssi_cross_threshold(
			peer, rssi,
			ap_steer_rssi_xing_low_threshold,
			BSTEER_INVALID_RSSI_HIGH_THRESHOLD);
	} while (0);


	if (inact_xing != BSTEERING_XING_UNCHANGED ||
	    low_xing != BSTEERING_XING_UNCHANGED ||
	    rate_xing != BSTEERING_XING_UNCHANGED ||
	    ap_xing != BSTEERING_XING_UNCHANGED) {
		son_send_rssi_xing_event(
			vdev,
			peer->macaddr, rssi,
			inact_xing, low_xing, rate_xing, ap_xing);
	}

	/* Check if MAP is set */
	map_config_params = &pd_priv->map_config_params;
	if (son_vdev_map_capability_get(vdev, SON_MAP_CAPABILITY)) {
		if (map_config_params->rssi_threshold) {
			low_rate_rssi_threshold_map = map_config_params->rssi_threshold -
							map_config_params->rssi_hysteresis;
			high_rate_rssi_threshold_map = map_config_params->rssi_threshold +
							map_config_params->rssi_hysteresis;

			if (rssi < low_rate_rssi_threshold_map) {
				map_xing = BSTEERING_XING_DOWN;
			} else if (rssi > high_rate_rssi_threshold_map) {
				map_xing = BSTEERING_XING_UP;
			}

			if (pe_priv->ni_bs_rssi_prev_xing_map != map_xing) {
				pe_priv->ni_bs_rssi_prev_xing_map = map_xing;
				son_send_rssi_xing_map_event(vdev, peer->macaddr,
							     rssi, map_xing);
			}
		}
	}

	pe_priv->son_rssi = rssi;

	SON_UNLOCK(&pd_priv->son_lock);
	return;
}
EXPORT_SYMBOL(son_record_peer_rssi);
/**
 * @brief reset inactivity count variables upon assoication .
 *
 * This is used by inactivity logic in steering module.
 *
 * @param [in] peer.
 * @param [inout] void.
 */

PUBLIC void son_peer_authorize(struct wlan_objmgr_peer *peer)
{
	struct son_peer_priv *pe_priv = NULL;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_pdev_priv *pd_priv = NULL;

	vdev = wlan_peer_get_vdev(peer);

	pdev = wlan_vdev_get_pdev(vdev);

	pe_priv = wlan_son_get_peer_priv(peer);

	if( wlan_son_is_pdev_enabled(pdev)&&
	    wlan_son_is_vdev_valid(vdev)) {
		if(pe_priv) {
			pd_priv = wlan_son_get_pdev_priv(pdev);
			pe_priv->peer = peer;
			pe_priv->son_inact_flag = false;
			pe_priv->son_inact_reload =
				pd_priv->son_inact[pe_priv->son_peer_class_group];
			pe_priv->son_inact = pe_priv->son_inact_reload;
			pe_priv->son_steering_flag = false;
		}
	}

	return;
}

/**
 * @brief indicate user space about change in activity.
 * @param [in] pdev.
 * @param [in] mac address to be reported.
 * @param [in] bool active or inactive flag.
 * @param [inout] void.
 */

PUBLIC void son_record_act_change(struct wlan_objmgr_pdev *pdev,
				  u_int8_t *dstmac,
				  bool active)
{
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_peer *peer = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;

	if (!wlan_son_is_pdev_enabled(pdev) || !dstmac) {
		return;
	}

	psoc = wlan_pdev_get_psoc(pdev);

	peer = wlan_objmgr_get_peer(psoc,
				    wlan_objmgr_pdev_get_pdev_id(pdev),
				    dstmac,
				    WLAN_SON_ID);

	if (!peer) {
		return;
	}

	vdev = wlan_peer_get_vdev(peer);

	if ((peer->peer_mlme.peer_flags & WLAN_PEER_F_AUTH) &&
	    wlan_son_is_vdev_enabled(vdev)) {
		son_notify_activity_change(vdev, (char *)dstmac, active);
	}

	wlan_objmgr_peer_release_ref(peer, WLAN_SON_ID);
	return;
}
EXPORT_SYMBOL(son_record_act_change);
/**
 * @brief store pid per vdev for multi instances of user space daemons.
 *
 * This is used by steering infrastructure.
 *
 * @param [in] vdev
 * @param [in] pid to be stored.
 *
 * @param [inout] void.
 */

void son_set_vdev_lbd_pid(struct wlan_objmgr_vdev *vdev, u_int32_t pid)
{
	struct son_vdev_priv *vd_priv = NULL;

	vd_priv = wlan_son_get_vdev_priv(vdev);

	if (vd_priv->lbd_pid != pid) {
		SON_LOGI("Lbd Pid %d",pid);
		vd_priv->lbd_pid = pid;
	}

	return;
}

/**
 * @brief Mark peer as inactive .
 *
 * This is used by Rx path to reset inactive flag.
 *
 * @param [in] peer
 * @param [in] bool inactive flag .
 *
 * @param [inout] void.
 */


PUBLIC void son_mark_node_inact(struct wlan_objmgr_peer *peer, bool inactive)
{
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;

	vdev = wlan_peer_get_vdev(peer);

	pdev = wlan_vdev_get_pdev(vdev);

	if (!wlan_son_is_pdev_enabled(pdev)) {
		return;
	}

	if(!wlan_son_is_vdev_enabled(vdev)) {
		return ;
	}

	son_core_mark_peer_inact(peer, inactive);

	return;
}
EXPORT_SYMBOL(son_mark_node_inact);

/**
 * @brief Inform the band steering module of a channel utilization measurement.
 *
 * If the necessary number of utilization measurements have been obtained,
 * this will result in an event being generated.
 *
 * @param [in] vdev  the vdev for which the utilization report occurred
 * @param [in] ieee_chan_num  the channel on which the utilization measurement
 *                            took place
 * @param [in] chan_utilization  the actual utilization measurement
 */

void son_record_utilization(struct wlan_objmgr_vdev *vdev,
			    u_int ieee_chan_num,
			    u_int32_t chan_utilization)
{
#define MAX_CHANNEL_UTILIZATION 100
	struct son_pdev_priv *pd_priv = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	ieee80211_bsteering_param_t *son_config_params = NULL;

	pdev = wlan_vdev_get_pdev(vdev);

	if (!wlan_son_is_pdev_enabled(pdev)) {
		return;
	}

	if(!wlan_son_is_vdev_enabled(vdev)) {
		return ;
	}

	pd_priv = wlan_son_get_pdev_priv(pdev);
	son_config_params = &pd_priv->son_config_params;

	do {
		SON_LOCK(&pd_priv->son_lock);

		if (!wlan_son_is_pdev_enabled(pdev) ||
		    !pd_priv->son_chan_util_requested ||
		    !pd_priv->son_iv || pd_priv->son_iv != vdev) {
			break;
		}

		if (ieee_chan_num == pd_priv->son_active_ieee_chan_num) {
		/* We have sometimes seen a channel utilization value greater
		than 100%. The current suspicion is that the ACS module did not
		complete the scan properly and thus when it calls back into band
		steering, it is only providing a raw channel clear count instead
		of the computed percentage. This is possible because both the
		intermediate result and the final value are stored in the same
		value and by the time the scan event handler is called
		indicating completion, ACS does not know whether the value is
		the intermediate or final result.

		By checking for this, we force a new measurement to be taken
		in the off chance that this occurs. The real fix will be of
		course to determine why this is happening in ACS. */
			if (chan_utilization <= MAX_CHANNEL_UTILIZATION) {
				pd_priv->son_chan_util_samples_sum += chan_utilization;
				pd_priv->son_chan_util_num_samples++;

				if (pd_priv->son_dbg_config_params.raw_chan_util_log_enable) {
					son_send_utilization_event(vdev,
						   chan_utilization,
						   true /* isDebug */);
				}

		/* If we have reached our desired number of samples, generate an
		   event with the average.*/
				if (pd_priv->son_chan_util_num_samples ==
				    son_config_params->utilization_average_num_samples) {
					u_int8_t average = pd_priv->son_chan_util_samples_sum /
						pd_priv->son_chan_util_num_samples;

					son_send_utilization_event(
						vdev,
						average,
						false /* isDebug */);

					son_core_reset_chan_utilization(pd_priv);
				}
			} else {
				SON_LOGI("%s: Ignoring invalid utilization %u on channel %u\n",
					 __func__, chan_utilization, ieee_chan_num);
			}

			pd_priv->son_chan_util_requested = false;
		}
	} while (0);

	SON_UNLOCK(&pd_priv->son_lock);
#undef MAX_CHANNEL_UTILIZATION
}

/**
 * @brief to update change in ie for all AP vdev.
 *
 * This is used by ioctls interface to update ie across vaps.
 *
 * @param [in] pdev
 * @param [in] vdev
 * @param [in] arg as null.
 * @param [inout] void.
 */

static void son_iterate_vdev_update_ie(struct wlan_objmgr_pdev *pdev,
				       void *obj,
				       void *arg)
{
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)obj;


	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE)
	{
	    son_vdev_fext_capablity(vdev,
		    SON_CAP_SET,
		    WLAN_VDEV_FEXT_SON_INFO_UPDATE);
	}
	return;
}

/**
 * @brief update son ie for all vaps.
 * @param [in] vdev.
 * @param [inout] void.
 */

PUBLIC void son_update_bss_ie(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_pdev *pdev = NULL;

	pdev = wlan_vdev_get_pdev(vdev);

	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_VDEV_OP,
					  son_iterate_vdev_update_ie,
					  NULL, 0,
					  WLAN_SON_ID);

	return;
}

/**
 * @brief update assoc_frame
 * @param [in] peer
 * @param [in] wbuf
 */

PUBLIC void son_update_assoc_frame(struct wlan_objmgr_peer *peer, wbuf_t wbuf)
{
#define VENDOR_OUI_BYTE0 0x00
#define VENDOR_OUI_BYTE1 0x17
#define VENDOR_OUI_BYTE2 0xF2
	uint8_t *frm, *efrm, oui[3];
	struct ieee80211_frame *wh;
	struct son_peer_priv *pe_priv = NULL;
	struct son_pdev_priv *pd_priv = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct wlan_objmgr_vdev *vdev= NULL;
	int subtype;

	pe_priv = wlan_son_get_peer_priv(peer);

	if (!pe_priv)
		return;

	pe_priv->assoc_frame = qdf_nbuf_copy(wbuf);
	pe_priv->son_peer_class_group = 0;

    vdev = wlan_peer_get_vdev(peer);
    pdev = wlan_vdev_get_pdev(vdev);
    pd_priv = wlan_son_get_pdev_priv(pdev);

	if (!pd_priv)
		return;

	if (!(pe_priv->assoc_frame) ||
		!(pd_priv->son_config_params.client_classification_enable)) {
		return;
	}

	wh = (struct ieee80211_frame *) wbuf_header(pe_priv->assoc_frame);
	subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;;
	frm = (u_int8_t *)&wh[1];
	efrm = wbuf_header(pe_priv->assoc_frame) + wbuf_get_pktlen(pe_priv->assoc_frame);
	frm += 4; /* Ignore Capability Information and Listen Interval */

	if (subtype == IEEE80211_FC0_SUBTYPE_REASSOC_REQ) {
		frm += 6; /* Ignore current AP info */
	}

	while (!(pe_priv->son_peer_class_group) &&
		   (((frm + 1) < efrm) && (frm + frm[1] + 1) < efrm)) {
		switch (*frm) {
			case IEEE80211_ELEMID_VENDOR:
				if (!iswpaoui(frm) && !iswmeinfo(frm) && !(pe_priv->son_peer_class_group)) {
					oui[0] = (uint8_t) (*(frm + 2));
					oui[1] = (uint8_t) (*(frm + 3));
					oui[2] = (uint8_t) (*(frm + 4));
					if ((oui[0] == VENDOR_OUI_BYTE0) &&
						(oui[1] == VENDOR_OUI_BYTE1) &&
						(oui[2] == VENDOR_OUI_BYTE2)) {
						pe_priv->son_peer_class_group = 1;
					}
				}
			break;
			default:
			break;
		}
		frm += frm[1] + 2;
	}
#undef VENDOR_OUI_BYTE2
#undef VENDOR_OUI_BYTE1
#undef VENDOR_OUI_BYTE0
}
EXPORT_SYMBOL(son_update_assoc_frame);

/**
 * @brief get uplink rate based on SNR.
 *
 * This is used by uplink node detection logic.
 *
 * @param [in] vdev
 * @param [in] SNR value for computaion.
 *
 * @param [inout] rate corrosponding to SNR.
 */

u_int16_t son_get_uplinkrate(struct wlan_objmgr_vdev *vdev, u_int8_t snr)
{
	u_int8_t nss = 0;
	wlan_phymode_e phymode;
	wlan_chwidth_e chwidth;

	nss     =  wlan_vdev_get_rx_streams(vdev);

	phymode = convert_phymode(vdev);

	chwidth = (wlan_chwidth_e)wlan_vdev_get_chwidth(vdev);

	return son_SNRToPhyRateTablePerformLookup(snr, nss, phymode, chwidth);

}
/**
 * @brief: Change feature capablities per vdev.
 * @param  vaphandle   : vap handle
 * @param action: SET, GET and clear.
 * @param cap: capabilites to change.
 * @return :in case of get it return positive value if cap
 *           is set, defualt is EOK and it can return -EINVAL
 *           if vdev is null.
 *           only valid for STA vaps;
 * Function work under vdev_lock.
 */

u_int8_t son_vdev_feat_capablity(struct wlan_objmgr_vdev *vdev,
				 son_capability_action action,
				 u_int32_t cap)
{
	u_int8_t retv = EOK;
	struct son_pdev_priv *pd_priv = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;

	if (!vdev)
		return -EINVAL;
	pdev = wlan_vdev_get_pdev(vdev);
	pd_priv = wlan_son_get_pdev_priv(pdev);

	switch(action)
	{
	case  SON_CAP_GET:
		retv =wlan_vdev_mlme_feat_cap_get(vdev, cap);
		break;
	case  SON_CAP_SET:
		wlan_vdev_mlme_feat_cap_set(vdev, cap);
		pd_priv->son_num_vap++;
		break;
	case  SON_CAP_CLEAR:
		wlan_vdev_mlme_feat_cap_clear(vdev, cap);
		pd_priv->son_num_vap--;
		break;
	default:
		SON_LOGW(" Invalid action ");
	}


	return retv;
}

/**
 * Function work under vdev_lock.
 * @brief: Get the count of son enabled vdev .
 * @param  vaphandle   : vap handle
 * @param action: GET
 * @return :It returns the number of vaps for which SON is enabled
 *            default is EOK and it can return -EINVAL
 *           if vdev is null.
 *
 */
u_int8_t son_vdev_get_count(struct wlan_objmgr_vdev *vdev,
        son_capability_action action)
{
	u_int8_t retv = EOK;
	struct son_pdev_priv *pd_priv = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;

	if (!vdev)
		return -EINVAL;
	pdev = wlan_vdev_get_pdev(vdev);
	pd_priv = wlan_son_get_pdev_priv(pdev);

	switch(action)
	{
	case SON_CAP_GET:
		retv =  pd_priv->son_num_vap;
		break;
	default:
		SON_LOGW(" Invalid action ");
	}


	return retv;
}


/**
 * @brief: Change feature extended cap per vdev.
 * @param  vaphandle   : vap handle
 * @param action: SET, GET and clear.
 * @param cap: capabilites to change.
 * @return :in case of get it return positive value if cap
 *           is set, defualt is EOK and it can return -EINVAL
 *           if vdev is null.
 *           only valid for STA vaps;
 * Function work under vdev_lock.
 */

u_int8_t son_vdev_fext_capablity(struct wlan_objmgr_vdev *vdev,
				son_capability_action action,
				u_int32_t cap)
{
	u_int8_t retv = EOK;

	if (!vdev)
		return -EINVAL;

	switch(action)
	{
	case  SON_CAP_SET:
		wlan_vdev_mlme_feat_ext_cap_set(vdev, cap);
		break;
	case  SON_CAP_GET:
		retv = wlan_vdev_mlme_feat_ext_cap_get(vdev, cap);
		break;
	case  SON_CAP_CLEAR:
		wlan_vdev_mlme_feat_ext_cap_clear(vdev, cap);
		break;
	default:
		SON_LOGW(" Invalid action ");
	}

	return retv;
}

/**
 * @brief Set or Clear MAP Capability Flags per vdev
 * @param vdev handle
 * @param cap MAP capability to be set/cleared
 * @param value To Set or Clear the capability flags
 *              MAP Capability SON_MAP_VAP_TYPE is used to set/clear following
 *              capability flags by making use of specific bits of the value :
 *                  Fronthaul BSS : Set/Cleared according to Bit 5 of value
 *                  Backhaul BSS : Set/Cleared according to Bit 6 of value
 *                  Backhaul STA : Set/Cleared according to Bit 7 of value
 * @return Default is EOK
 *         -EINVAL if vdev is invalid
 */

int son_vdev_map_capability_set(struct wlan_objmgr_vdev *vdev, son_map_capability cap, int value)
{
	int retv = EOK;
	struct son_vdev_priv *vdev_priv = NULL;
	vdev_priv = wlan_son_get_vdev_priv(vdev);

	if (!vdev_priv)
		return -EINVAL;

	switch(cap)
	{
	case SON_MAP_CAPABILITY:
		vdev_priv->iv_map = value?1:0;
		break;
	case SON_MAP_CAPABILITY_VAP_TYPE:
		vdev_priv->iv_mapbh = (value & MAP_BACKHAUL_BSS)?1:0;
		vdev_priv->iv_mapfh = (value & MAP_FRONTHAUL_BSS)?1:0;
		vdev_priv->iv_mapbsta = (value & MAP_BACKHAUL_STA)?1:0;
		break;
	case SON_MAP_CAPABILITY_VAP_UP:
		vdev_priv->iv_mapvapup = value?1:0;
		break;
	default:
		SON_LOGW(" Invalid MAP Capability ");
	}
	return retv;
}

/**
 * @brief Get MAP Capability Flags per vdev
 * @param vdev handle
 * @param cap MAP capability to get
 * @return Value of MAP Capability, defualt is EOK
 */

int son_vdev_map_capability_get(struct wlan_objmgr_vdev *vdev, son_map_capability cap)
{
	struct son_vdev_priv *vdev_priv = NULL;
	int retv = EOK;

	vdev_priv = wlan_son_get_vdev_priv(vdev);

	if(vdev_priv)
	{
		switch(cap)
		{
		case SON_MAP_CAPABILITY:
			retv = vdev_priv->iv_map?1:0;
			break;
		case SON_MAP_CAPABILITY_VAP_TYPE:
			retv = (vdev_priv->iv_mapbh ? MAP_BACKHAUL_BSS : 0) |
			       (vdev_priv->iv_mapfh ? MAP_FRONTHAUL_BSS : 0) |
			       (vdev_priv->iv_mapbsta ? MAP_BACKHAUL_STA : 0);
			break;
		case SON_MAP_CAPABILITY_VAP_UP:
			retv = vdev_priv->iv_mapvapup?1:0;
			break;
		default:
			SON_LOGW(" Invalid MAP Capability ");
		}
	}
	return retv;
}

/**
 * @brief Set steering in progress flag per node.
 *
 * This is used by user space to set this flag.
 *
 * @param [in] peer to set steering in progress flag.
 *
 * @param [inout] retun trued if success.
 */

bool son_is_steer_in_prog(struct wlan_objmgr_peer *peer)
{
	struct son_peer_priv *pe_priv = NULL;
	struct son_pdev_priv *pd_priv = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct wlan_objmgr_vdev *vdev= NULL;
	bool steer_in_prog = false;

	vdev = wlan_peer_get_vdev(peer);
	pdev = wlan_vdev_get_pdev(vdev);

	pd_priv = wlan_son_get_pdev_priv(pdev);
	pe_priv = wlan_son_get_peer_priv(peer);

	if (!pe_priv)
		return -EINVAL;

	SON_LOCK(&pd_priv->son_lock);
	steer_in_prog = pe_priv->son_steering_flag;
	SON_UNLOCK(&pd_priv->son_lock);

	return steer_in_prog;
}

/**
 * @brief Verify that the son handle is valid within the
 *        struct psoc provided.
 *
 * @param [in] psoc  the handle to the radio where the band steering state
 *                 resides
 *
 * @return true if handle is valid; otherwise false
 */

bool wlan_son_is_pdev_valid(struct wlan_objmgr_pdev *pdev)
{
	struct son_pdev_priv *pdev_priv = NULL;

	if (pdev)
		pdev_priv = wlan_son_get_pdev_priv(pdev);

	return pdev && pdev_priv;
}

/**
 * @brief Determine whether the band steering module is enabled or not.
 *
 * @param [in] ic  the handle to the radio where the band steering state
 *                 resides
 *
 * @return non-zero if it is enabled; otherwise 0
 */

u_int8_t wlan_son_is_pdev_enabled(struct wlan_objmgr_pdev *pdev)
{
	struct son_pdev_priv *pdev_priv = NULL;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_vdev *vdev_next = NULL;
	struct wlan_objmgr_pdev_objmgr *objmgr;
	qdf_list_t *vdev_list;

	if (!pdev)
	{
		qdf_err("%s:pdev is NULL ", __func__);
		return false;
	}

	if (wlan_son_is_pdev_valid(pdev))
		pdev_priv = wlan_son_get_pdev_priv(pdev);

	if (pdev_priv){
		if (atomic_read(&pdev_priv->son_enabled))
			return true;
	}

	/* band steering can be disabled when complete band is teared down
	 * which is possible in MAP.
	 * Add check to see if VAP is up on any band and return true*/

	/* wlan object lock/unlock is needed here to ensure
	 * * proper synchronization and prevent kernel panic */
	wlan_pdev_obj_lock(pdev);

	objmgr = &pdev->pdev_objmgr;
	vdev_list = &objmgr->wlan_vdev_list;
	/* Get first vdev */
	vdev = wlan_pdev_vdev_list_peek_head(vdev_list);

	while (vdev != NULL) {
		if (son_vdev_map_capability_get(vdev, SON_MAP_CAPABILITY_VAP_UP))
		{
			wlan_pdev_obj_unlock(pdev);
			return true;
		}
		/* get next vdev */
		vdev_next = wlan_vdev_get_next_vdev_of_pdev(vdev_list, vdev);
		vdev = vdev_next;
	}

	wlan_pdev_obj_unlock(pdev);
	return false;

}
EXPORT_SYMBOL(wlan_son_is_pdev_enabled);

/**
 * @brief Determine whether band steering events are enabled on
 *        a vdev.
 *
 * @param [in] vdev to check
 *
 * @return non-zero if it is enabled; otherwise 0
 */

u_int8_t wlan_son_is_vdev_event_enabled(struct wlan_objmgr_vdev *vdev)
{
	struct son_vdev_priv *vdev_priv = NULL;

	vdev_priv = wlan_son_get_vdev_priv(vdev);

	if (vdev_priv)
		return atomic_read(&vdev_priv->v_son_enabled);

	return false;
}
EXPORT_SYMBOL(wlan_son_is_vdev_event_enabled);


/**
 * @brief Determine whether broadcasting of band steering events is enabled on
 *        a vdev.
 *
 * @param [in] vdev to check
 *
 * @return non-zero if it is enabled; otherwise 0
 */

u_int8_t wlan_son_is_vdev_event_bcast_enabled(struct wlan_objmgr_vdev *vdev)
{
	struct son_vdev_priv *vdev_priv = NULL;

	vdev_priv = wlan_son_get_vdev_priv(vdev);

	if (vdev_priv)
		return atomic_read(&vdev_priv->event_bcast_enabled);

	return false;
}
EXPORT_SYMBOL(wlan_son_is_vdev_event_bcast_enabled);

/**
 * @brief Enable/Disable broadcasting of band steering events.
 *
 * @param [in] vdev to check and flag to enable/disable
 *
 * @return non-zero if it is failure; otherwise 0
 */

int son_core_enable_disable_vdev_bcast_events(struct wlan_objmgr_vdev *vdev ,
					u_int8_t enable)
{
	struct son_vdev_priv *vpriv = NULL;

	vpriv = wlan_son_get_vdev_priv(vdev);

	if(vpriv) {
		qdf_atomic_set(&vpriv->event_bcast_enabled, enable);
		return EOK;
	}

	return -EINVAL;
}
EXPORT_SYMBOL(son_core_enable_disable_vdev_bcast_events);


/**
 * @brief Determine whether the vdev handle is valid, has a valid band
 *        steering handle, is operating in a mode where band steering
 *        is relevant, and is not in the process of being deleted.
 *
 * @return true if the vdev is valid; otherwise false
 */

bool wlan_son_is_vdev_valid(struct wlan_objmgr_vdev *vdev)
{
	if ((wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE) &&
	    !wlan_vdev_is_deleted_set(vdev))
		return true;

	return false;
}

/**
 * @brief Determine whether the vdev has band steering enabled.
 *
 * Validate that the vdev has a valid band steering handle, that
 * it is operating in the right mode (AP mode), and that band steering has been
 * enabled on the vdev
 *
 * @param [in] vdev the VAP to check
 *
 * @return true if the vdev is valid and has band steering enabled; otherwise
 *         false
 */
bool wlan_son_is_vdev_enabled(struct wlan_objmgr_vdev *vdev)
{
	return ((wlan_son_is_vdev_valid(vdev) && wlan_son_is_vdev_event_enabled(vdev)) ||
		son_vdev_map_capability_get(vdev, SON_MAP_CAPABILITY_VAP_UP));
}
EXPORT_SYMBOL(wlan_son_is_vdev_enabled);
/**
 * @brief Set WHC flag in peer.
 * whc flag is set as per son ie in mangement frames.
 * @param [in] Peer.
 *
 * @return void.
 */

void son_set_whc_apinfo_flag(struct wlan_objmgr_peer *peer, u_int8_t flag)
{
	struct son_peer_priv *pe_priv = NULL;
	pe_priv = wlan_son_get_peer_priv(peer);

	if (pe_priv)
		pe_priv->ni_whc_apinfo_flags |= flag;

	return;
}

/**
 * @brief clear WHC apinfo flag in peer.
 * @param [in] Peer.
 *
 * @return void.
 */

void son_clear_whc_apinfo_flag(struct wlan_objmgr_peer *peer, u_int8_t flag)
{
	struct son_peer_priv *pe_priv = NULL;
	pe_priv = wlan_son_get_peer_priv(peer);

	if (pe_priv)
		pe_priv->ni_whc_apinfo_flags &= ~flag;

	return;
}
/**
 * @brief check if peer has whc apinfo flag set.
 * @param [in] Peer and flag to check.
 * @return true if flag is set otherwise false.
 */

int son_has_whc_apinfo_flag(struct wlan_objmgr_peer *peer, u_int8_t flag)
{
	struct son_peer_priv *pe_priv = NULL;
	pe_priv = wlan_son_get_peer_priv(peer);

	if(pe_priv)
		return ((pe_priv->ni_whc_apinfo_flags & flag) != 0);
	else
		return false;
}
/**
 * @brief Set peer repeater flag.Son ie is parsed for this.
 * @param [in] peer.
 * @return void.
 */
void son_set_whc_rept_info(struct wlan_objmgr_peer *peer)
{
	struct son_peer_priv *pe_priv = NULL;
	pe_priv = wlan_son_get_peer_priv(peer);

	if (pe_priv)
		pe_priv->ni_whc_rept_info = 1;

	return;
}
/**
 * @brief clear whc repeater infor flag.
 * @param [in] Peer.
 * @return void.
 */
void son_clear_whc_rept_info(struct wlan_objmgr_peer *peer)
{
	struct son_peer_priv *pe_priv = NULL;

	if (!peer)
		return;

	pe_priv = wlan_son_get_peer_priv(peer);

	if (pe_priv)
		pe_priv->ni_whc_rept_info = 0;

	return;
}
/**
 * @brief check if peer has whc repeater info flag.
 * @param [in] Peer.
 * @return true if flag is set otherwise false.
 */

int32_t son_get_whc_rept_info(struct wlan_objmgr_peer *peer)
{
	struct son_peer_priv *pe_priv = NULL;

	if (!peer)
		return -EINVAL;

	pe_priv = wlan_son_get_peer_priv(peer);

	if (pe_priv)
		return (pe_priv->ni_whc_rept_info);
	else
		return -EINVAL;
}
int8_t ucfg_son_set_root_dist(struct wlan_objmgr_vdev *vdev,
			      u_int8_t root_distance)
{
	if (vdev)
		return(son_core_set_root_dist(vdev, root_distance));
	else {
		SON_LOGI("SON on Pdev Needs to be enabled for root distance");
		return 0;
	}

	return 0;
}

u_int8_t ucfg_son_get_root_dist(struct wlan_objmgr_vdev *vdev)
{
	if (vdev)
		return(son_core_get_root_dist(vdev));
	else {
		SON_LOGI("SON on Pdev Needs to be enabled for root distance");
		return 0;
	}
}

/**
 * @brief Called to get the in network table for 2G
 *
 * @param [in] vap  the VAP on which the table is requested for
 * @param [in] channel  return all mac address for this channel
 * @param [inout] num_entries  if 0 return total entries in table,
 *                          else return number of entries
 * @param [inout] data  pointer to hold data to return
 */

PUBLIC int8_t son_get_innetwork_table(struct wlan_objmgr_vdev *vdev, void *data, int *num_entries,
                                      int8_t channel) {
	struct son_pdev_priv *pd_priv = NULL;
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct in_network_table *tmpnode;
	wlan_chan_t c = wlan_vdev_get_channel(vdev);
	int index = 0;

	pd_priv = wlan_son_get_pdev_priv(pdev);

	if (c == NULL) {
		SON_LOGI(" failed to resolve channel from vdev \r\n");
		return QDF_STATUS_E_INVAL;
	}
	if (IEEE80211_IS_CHAN_5GHZ(c)) {
		SON_LOGI("channel is not 2.4G return failed \r\n");
		return QDF_STATUS_E_INVAL;
	}

	if (!*num_entries) {
		num_entries = (int *)data;
		TAILQ_FOREACH(tmpnode, &pd_priv->in_network_table_2g, table_list) {
			index++;
		}
		*num_entries = index;
		return 0;
	} else {
		if (!channel) {
			struct ieee80211_bsteering_in_network_2G_table *in_network_2g_data =
				(struct ieee80211_bsteering_in_network_2G_table *)data;

			TAILQ_FOREACH(tmpnode, &pd_priv->in_network_table_2g, table_list) {
				if (index == *num_entries)
				    break;
				memcpy(in_network_2g_data[index].mac_addr, tmpnode->macaddr,IEEE80211_ADDR_LEN);
				in_network_2g_data[index].ch = tmpnode->channel;
				index++;
			}

			in_network_2g_data->total_index = index;
		} else {
			u_int8_t *mac_addr = (u_int8_t *)data;
			TAILQ_FOREACH(tmpnode, &pd_priv->in_network_table_2g, table_list) {
				if (channel == tmpnode->channel) {
					memcpy(mac_addr + (index * IEEE80211_ADDR_LEN), tmpnode->macaddr,IEEE80211_ADDR_LEN);
					index++;
				}
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL(son_get_innetwork_table);

/**
 * @brief To get uplink_rate for mixed backhaul.
 *
 * @param [in] vdev the VAP to get.
 *
 * @return uplink_rate.
 */
u_int16_t son_get_ul_mixedbh(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_pdev_priv *son_pd = NULL;
	u_int16_t uplink_rate = 0;

	pdev = wlan_vdev_get_pdev(vdev);
	if(!pdev) {
		qdf_err("%s:pdev is NULL ", __func__);
		return uplink_rate;
	}

	son_pd = wlan_son_get_pdev_priv(pdev);
	if (son_pd)  {
		uplink_rate = son_pd->curr_ul_rate_mixedbh;
	}
	return uplink_rate;
}

/**
 * @brief To set uplink_rate for mixed backhaul.
 *
 * @param [in] vdev the VAP to set.
 * @param [in] u_int16_t ulrate.
 *
 * @return true if set ul rate succesfully, otherwise return false.
 */
bool son_set_ul_mixedbh(struct wlan_objmgr_vdev *vdev, u_int16_t ulrate)
{
	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_pdev_priv *son_pdev = NULL;

	pdev = wlan_vdev_get_pdev(vdev);
	if(!pdev) {
		qdf_err("%s:pdev is NULL ", __func__);
		return false;
	}

	son_pdev = wlan_son_get_pdev_priv(pdev);
	if (son_pdev)  {
		son_pdev->recv_ul_rate_mixedbh = ulrate;
		return true;
	}
	return false;
}

/**
 * @brief To set sonmode and backhaul type for mixed backhaul.
 *
 * @param [in] vdev the VAP
 * @param [in] u_int16_t sonmode (CAP or RE) and backhaul type used.
 *
 * @return true if set ul rate succesfully, otherwise return false.
 */
bool son_set_backhaul_type_mixedbh(struct wlan_objmgr_vdev *vdev, u_int8_t backhaul_type)
{
	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_pdev_priv *son_pdev = NULL;

	pdev = wlan_vdev_get_pdev(vdev);
	if(!pdev) {
		qdf_err("%s:pdev is NULL ", __func__);
		return false;
	}

	son_pdev = wlan_son_get_pdev_priv(pdev);
	if (son_pdev)  {
		son_pdev->son_backhaul_type = backhaul_type;
		return true;
	}
	return false;
}
#else

PUBLIC QDF_STATUS wlan_son_psoc_close(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

PUBLIC int8_t son_netlink_attach(void)
{
	return EOK;
}

PUBLIC int8_t son_netlink_destroy(void)
{
	return EOK;
}

PUBLIC void son_notify_activity_change(struct wlan_objmgr_vdev *vdev,
				       char *macaddr, bool inactive)
{
	return;
}

PUBLIC void son_send_sta_stats_event(struct wlan_objmgr_vdev *vdev,
				     struct bs_sta_stats_ind *sta_stats)
{
	return;
}
EXPORT_SYMBOL(son_send_sta_stats_event);

PUBLIC void son_send_probereq_event(struct wlan_objmgr_vdev *vdev,
				    const u_int8_t *mac_addr,
				    u_int8_t rssi)
{
	return;
}

PUBLIC void son_send_utilization_event(struct wlan_objmgr_vdev *vdev,
				       u_int8_t chan_utilization,
				       bool is_debug)
{
	return;
}

PUBLIC void son_send_rssi_measurement_event(struct wlan_objmgr_vdev *vdev,
					    const u_int8_t *mac_addr,
					    u_int8_t rssi,
					    bool is_debug)
{
	return;
}

PUBLIC void son_send_rssi_xing_event(struct wlan_objmgr_vdev *vdev,
				     const u_int8_t *mac_addr,
				     u_int8_t rssi,
				     BSTEERING_XING_DIRECTION inact_xing,
				     BSTEERING_XING_DIRECTION low_xing,
				     BSTEERING_XING_DIRECTION rate_xing,
				     BSTEERING_XING_DIRECTION ap_xing)
{
	return;
}

PUBLIC void son_send_rssi_xing_map_event(struct wlan_objmgr_vdev *vdev,
					 const u_int8_t *mac_addr,
					 u_int8_t rssi,
					 BSTEERING_XING_DIRECTION map_xing)
{
	return;
}

PUBLIC void son_send_tx_rate_xing_event(struct wlan_objmgr_vdev *vdev,
					const u_int8_t *mac_addr,
					u_int32_t tx_rate,
					BSTEERING_XING_DIRECTION xing)
{
	return;
}

PUBLIC void son_send_tx_rate_measurement_event(struct wlan_objmgr_vdev *vdev,
					       const u_int8_t *mac_addr,
					       u_int32_t tx_rate)
{
	return;
}

PUBLIC void son_send_node_associated_event(struct wlan_objmgr_vdev *vdev,
					   struct wlan_objmgr_peer *peer)
{
	return;
}

PUBLIC void son_send_auth_fail_event(struct wlan_objmgr_vdev *vdev,
				     const u_int8_t *mac_addr,
				     u_int8_t rssi)

{
	return;
}

PUBLIC void son_send_wnm_bstm_resp_event(struct wlan_objmgr_vdev *vdev, u_int32_t token,
					 u_int8_t *macaddr,
					 struct bs_wnm_bstm_resp *bstm_resp)
{
	return;
}

PUBLIC void son_send_wnm_bstm_query_event(struct wlan_objmgr_vdev *vdev, u_int32_t token,
					  u_int8_t *macaddr,
					  struct bs_wnm_bstm_query *bstm_query)
{
	return;
}

PUBLIC void son_send_rrm_bcnrpt_event(struct wlan_objmgr_vdev *vdev,
				      u_int32_t token, u_int8_t *macaddr,
				      ieee80211_bcnrpt_t *bcnrpt, u_int8_t num_bcnrpt)
{
	return;
}

PUBLIC void son_send_rrm_frame_bcnrpt_event(struct wlan_objmgr_vdev *vdev,
					    u_int32_t token, u_int8_t *macaddr,
					    const u_int8_t *bcnrpt,
					    size_t report_len,
					    u_int8_t num_rpt_elem)
{
	return;
}
PUBLIC void son_dealloc_rrm_bcnrpt(ieee80211_bcnrpt_t **reports)
{
	return;
}

PUBLIC u_int8_t son_alloc_rrm_bcnrpt(struct wlan_objmgr_vdev *vdev,
				     ieee80211_bcnrpt_t **reports)
{
	return 0;
}

PUBLIC void son_send_vap_stop_event(struct wlan_objmgr_vdev *vdev)
{
	return;
}
EXPORT_SYMBOL(son_send_vap_stop_event);

PUBLIC void son_send_node_smps_update_event(struct wlan_objmgr_vdev *vdev,
					    struct wlan_objmgr_peer *peer,
					    u_int8_t is_static)
{
	return;
}

PUBLIC void son_send_opmode_update_event(struct wlan_objmgr_vdev *vdev,
					 char *macaddr,
					 u_int8_t chwidth,
					 u_int8_t streams)
{
	return;
}

PUBLIC void son_send_auth_allow_event(struct wlan_objmgr_vdev *vdev,
				      const u_int8_t *mac_addr,
				      u_int8_t rssi)
{
	return;
}

/**
 * @brief Notify band steering when an error RRM beacon report response is received
 *
 * It will generate a netlink event with error status if band steering is enabled.
 *
 * @param [in] vap  the VAP on which the report is received
 * @param [1n] token  the dialog token matching the one provided in the request
 * @param [in] macaddr  the MAC address of the reporter station
 * @param [1n] mode  the measurement report mode contained in the response
 */
void son_send_rrm_bcnrpt_error_event(struct wlan_objmgr_vdev *vdev,
				     u_int32_t token, u_int8_t *macaddr,
				     u_int8_t mode)
{
	return;
}
void son_send_txpower_change_event(struct wlan_objmgr_vdev *vdev,
				   u_int16_t tx_power)
{
	return;
}
EXPORT_SYMBOL(son_send_txpower_change_event);
/**
 * @brief Verify that the son handle is valid within the
 *        struct psoc provided.
 *
 * @param [in] psoc  the handle to the radio where the band steering state
 *                 resides
 *
 * @return true if handle is valid; otherwise false
 */

bool wlan_son_is_pdev_valid(struct wlan_objmgr_pdev *pdev)
{
	return false;
}

/**
 * @brief Determine whether the band steering module is enabled or not.
 *
 * @param [in] pdev  the handle to the radio where the band steering state
 *                 resides
 *
 * @return non-zero if it is enabled; otherwise 0
 */

u_int8_t wlan_son_is_pdev_enabled(struct wlan_objmgr_pdev *pdev)
{
	return false;
}
EXPORT_SYMBOL(wlan_son_is_pdev_enabled);
/**
 * @brief Determine whether band steering events are enabled on
 *        a VAP.
 *
 * @param [in] vap  VAP to check
 *
 * @return non-zero if it is enabled; otherwise 0
 */

u_int8_t wlan_son_is_vdev_event_enabled(struct wlan_objmgr_vdev *vdev)
{
	return false;

}

EXPORT_SYMBOL(wlan_son_is_vdev_event_enabled);
/**
 * @brief Determine whether the VAP handle is valid, has a valid band
 *        steering handle, is operating in a mode where band steering
 *        is relevant, and is not in the process of being deleted.
 *
 * @return true if the VAP is valid; otherwise false
 */

bool wlan_son_is_vdev_valid(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

/**
 * @brief Determine whether the VAP has band steering enabled.
 *
 * Validate that the VAP has a valid band steering handle, that
 * it is operating in the right mode (AP mode), and that band steering has been
 * enabled on the VAP.
 *
 * @param [in] vap  the VAP to check
 *
 * @return true if the VAP is valid and has band steering enabled; otherwise
 *         false
 */

bool wlan_son_is_vdev_enabled(struct wlan_objmgr_vdev *vdev)
{
	return false;
}
EXPORT_SYMBOL(wlan_son_is_vdev_enabled);

void son_peer_authorize(struct wlan_objmgr_peer *peer)
{
	return;
}

PUBLIC void son_mark_node_inact(struct wlan_objmgr_peer *peer, bool inactive)
{

	return;
}
EXPORT_SYMBOL(son_mark_node_inact);

PUBLIC void son_record_act_change(struct wlan_objmgr_pdev *pdev,
				  u_int8_t *mac_addr,
				  bool active)
{

	return;
}
EXPORT_SYMBOL(son_record_act_change);

PUBLIC void son_record_inst_peer_rssi_err(struct wlan_objmgr_peer *peer)
{

	return;

}
EXPORT_SYMBOL(son_record_inst_peer_rssi_err);

PUBLIC void son_record_inst_peer_rssi(struct wlan_objmgr_peer *peer,
				      u_int8_t rssi)
{
	return;

}
EXPORT_SYMBOL(son_record_inst_peer_rssi);

PUBLIC void son_record_peer_rssi(struct wlan_objmgr_peer *peer, u_int8_t rssi)
{
	return;
}

EXPORT_SYMBOL(son_record_peer_rssi);

PUBLIC int32_t son_match_peer_rssi_seq(struct wlan_objmgr_peer *peer,
				       u_int32_t rssi_seq)
{
	return - EINVAL;

}

EXPORT_SYMBOL(son_match_peer_rssi_seq);

PUBLIC void son_update_peer_rate(struct wlan_objmgr_peer *peer,
				 u_int32_t rssi, u_int32_t last_rate)
{
	return;
}
EXPORT_SYMBOL(son_update_peer_rate);

PUBLIC bool son_update_sta_stats(struct wlan_objmgr_peer *peer,
				 struct wlan_objmgr_vdev *current_vdev,
				 struct bs_sta_stats_ind *sta_stats,
				 void *stats)
{
	return false;
}
EXPORT_SYMBOL(son_update_sta_stats);

PUBLIC int son_enable_disable_steering(struct wlan_objmgr_vdev *vdev,
				 bool enable)
{
    return 0;
}

PUBLIC void son_set_vdev_lbd_pid(struct wlan_objmgr_vdev *vdev, u_int32_t pid)
{
	return;
}

PUBLIC bool son_is_probe_resp_wh_2G(struct wlan_objmgr_vdev *vdev,
				    u_int8_t *mac_addr,
				    u_int8_t sta_rssi)
{
	return false;
}


PUBLIC bool son_is_probe_resp_wh(struct wlan_objmgr_vdev *vdev,
				 const u_int8_t *mac_addr)
{
	return false;


}


PUBLIC void son_record_utilization(struct wlan_objmgr_vdev *vdev,
				   u_int ieee_chan_num,
				   u_int32_t chan_utilization)
{
	return;
}


PUBLIC void son_repeater_cnt_inc(struct wlan_objmgr_vdev *vdev)
{
	return;
}


PUBLIC void son_repeater_cnt_dec(struct wlan_objmgr_vdev *vdev)
{
	return;
}


PUBLIC u_int8_t son_repeater_cnt_get(struct wlan_objmgr_vdev *vdev)
{
	return 0;
}

PUBLIC void son_record_inst_rssi_log_enable (struct wlan_objmgr_vdev *vdev, int enable)
{
	return;
}


PUBLIC void son_update_backhaul_rate(struct wlan_objmgr_vdev *vdev, u_int16_t rate, bool self)
{

	return;
}


PUBLIC u_int16_t son_get_backhaul_rate(struct wlan_objmgr_vdev *vdev, bool self)
{
	return 0;

}


PUBLIC void son_update_uplink_bssid(struct wlan_objmgr_pdev *pdev , char *bssid)
{
	return;
}


PUBLIC void son_update_bss_ie(struct wlan_objmgr_vdev *vdev)
{
	return;
}



/**
 * get uplink estimated rate
 * @param      vaphandle   : vap handle
 * @param      snr         : rssi value from scan entry
 *
 * @return : estimated rate.
 *           only valid for STA vaps;
 *
 */
PUBLIC u_int16_t son_get_uplinkrate(struct wlan_objmgr_vdev *vdev,
				    u_int8_t snr)

{
	return 0;
}


/**
 * Function work under vdev_lock.
 * @brief: Change feature extented capa per vdev.
 * @param  vaphandle   : vap handle
 * @param action: SET, GET and clear.
 * @param cap: capabilites to change.
 * @return :in case of get it return positive value if cap
 *           is set, defualt is EOK and it can return -EINVAL
 *           if vdev is null.
 *           only valid for STA vaps;
 */

PUBLIC u_int8_t son_vdev_fext_capablity(struct wlan_objmgr_vdev *vdev,
					son_capability_action action,
					u_int32_t cap)
{
	return EOK;
}

/**
 * Function work under vdev_lock.
 * @brief: Change feature capablities per vdev.
 * @param  vaphandle   : vap handle
 * @param action: SET, GET and clear.
 * @param cap: capabilites to change.
 * @return :in case of get it return positive value if cap
 *           is set, defualt is EOK and it can return -EINVAL
 *           if vdev is null.
 *           only valid for STA vaps;
 */

PUBLIC u_int8_t son_vdev_feat_capablity(struct wlan_objmgr_vdev *vdev,
					son_capability_action action,
					u_int32_t cap)
{
	return EOK;
}

/**
 * Function work under vdev_lock.
 * @brief: Get the count of son enabled vdev .
 * @param  vaphandle   : vap handle
 * @param action: GET
 * @return :It returns the number of vaps for which SON is enabled
 *            default is EOK and it can return -EINVAL
 *           if vdev is null.
 *
 */
PUBLIC u_int8_t son_vdev_get_count(struct wlan_objmgr_vdev *vdev,
                            son_capability_action action)
{
        return EOK;
}

PUBLIC int son_vdev_map_capability_set(struct wlan_objmgr_vdev *vdev, son_map_capability cap, int val)
{
	return EOK;
}

PUBLIC int son_vdev_map_capability_get(struct wlan_objmgr_vdev *vdev, son_map_capability cap)
{
	return EOK;
}

PUBLIC bool son_is_steer_in_prog(struct wlan_objmgr_peer *peer)
{
	return false;

}

PUBLIC void son_set_whc_apinfo_flag(struct wlan_objmgr_peer *peer, u_int8_t flag)
{
	return;
}

PUBLIC void son_clear_whc_apinfo_flag(struct wlan_objmgr_peer *peer, u_int8_t flag)
{
	return;
}

PUBLIC int son_has_whc_apinfo_flag(struct wlan_objmgr_peer *peer, u_int8_t flag)
{
	return -EINVAL;
}

PUBLIC void son_set_whc_rept_info(struct wlan_objmgr_peer *peer)
{
	return;
}

PUBLIC void son_clear_whc_rept_info(struct wlan_objmgr_peer *peer)
{
	return;
}

PUBLIC int32_t son_get_whc_rept_info(struct wlan_objmgr_peer *peer)
{
	return -EINVAL;
}

PUBLIC int8_t ucfg_son_set_root_dist(struct wlan_objmgr_vdev *vdev,
			      u_int8_t root_distance)
{
	return EOK;
}
PUBLIC u_int8_t ucfg_son_get_root_dist(struct wlan_objmgr_vdev *vdev)
{
	return 0;

}

PUBLIC int8_t son_get_innetwork_table(struct wlan_objmgr_vdev *vdev, void *data, int *num_entries,
                                      int8_t channel)
{
	return false;
}
EXPORT_SYMBOL(son_get_innetwork_table);

bool son_set_backhaul_type_mixedbh(struct wlan_objmgr_vdev *vdev, u_int8_t backhaul_type)
{
	return false;
}

bool son_set_ul_mixedbh(struct wlan_objmgr_vdev *vdev, u_int16_t ulrate)
{
	return false;
}

u_int16_t son_get_ul_mixedbh(struct wlan_objmgr_vdev *vdev)
{
	return false;
}
#endif

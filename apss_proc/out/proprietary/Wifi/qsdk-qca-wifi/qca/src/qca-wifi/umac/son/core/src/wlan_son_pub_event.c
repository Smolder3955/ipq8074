/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

/* This file implements all public function api for WIFI SON */

#include <osif_private.h>
#include "wlan_son_internal.h"
#include "wlan_son_pub.h"
#include <ath_band_steering.h>
#include <wlan_osif_priv.h>

#if QCA_SUPPORT_SON
/**
 * @brief  Attach module to open socket to user space.
 * @param [in]
 *
 * @param [inout] void .
 */

PUBLIC int8_t son_netlink_attach(void)
{
	int8_t retv = EOK;

	if ( EOK != ath_band_steering_netlink_init()) {
		SON_LOGF("SON Socket init failed");
		retv = -EINVAL;
	}

	return retv;
}

/**
 * @brief To close socket to user layer.
 * @param [in] void
 *
 * @param [inout] void .
 */

PUBLIC int8_t son_netlink_destroy(void)
{
	int8_t retv = EOK;

	if ( EOK != ath_band_steering_netlink_delete()) {
		SON_LOGF("SON Socket init failed");
		retv = -EINVAL;
	}

	return retv;
}

/**
 * @brief
 * @param [in]
 *
 * @param [inout] void .
 */

static void son_notify_user(struct wlan_objmgr_vdev *vdev,
			    ATH_BSTEERING_EVENT type,
			    uint32_t len, const char *data)
{
	osif_dev *osifp;
	struct net_device *dev = NULL;
	ath_netlink_bsteering_event_t  netlink_event = {0};
	ath_netlink_bsteering_event_t  *p_netlink_event;
	uint32_t event_size = sizeof(netlink_event);
	struct son_vdev_priv *vd_priv = NULL;
	struct vdev_osif_priv *vdev_osifp = NULL;

	vd_priv = wlan_son_get_vdev_priv(vdev);
	vdev_osifp = wlan_vdev_get_ospriv(vdev);
	osifp = (osif_dev *)vdev_osifp->legacy_osif_priv;
	dev = osifp->netdev;

	if (len > sizeof(netlink_event.data)) {
		event_size = event_size + len - sizeof(netlink_event.data);
		p_netlink_event = (ath_netlink_bsteering_event_t *) qdf_mem_malloc(event_size);
	} else {
		p_netlink_event = &netlink_event;
	}

	if (p_netlink_event) {
		memset(p_netlink_event, 0x0, event_size);
		p_netlink_event->type = type;
		p_netlink_event->sys_index = dev->ifindex;

		if (len && data) {
			qdf_mem_copy(&(p_netlink_event->data), data, len);
		}

		ath_band_steering_netlink_send(p_netlink_event, vd_priv->lbd_pid, wlan_son_is_vdev_event_bcast_enabled(vdev), event_size);

		if (p_netlink_event != &netlink_event) {
			OS_FREE(p_netlink_event);
		}
	}
	return;
}

/**
 * @brief Send STA stats to userspace via netlink message
 *
 * @param [in] vdev  the first vdev on the radio for which STA
 *                  stats were updated
 * @param [in] sta_stats  the STA stats to send
 */
PUBLIC void son_send_sta_stats_event(struct wlan_objmgr_vdev *vdev,
				     struct bs_sta_stats_ind *sta_stats)
{

	if (!sta_stats || !sta_stats->peer_count || !vdev) {
		/* No stats collected, nothing to send */
		return;
	}

	/* No need to do any checks here - should not have any updated stats unless
	   band steering and interference detection are enabled */

	son_notify_user(vdev, ATH_EVENT_BSTEERING_STA_STATS,
			sizeof(struct bs_sta_stats_ind),
			(const char *)sta_stats);
	return;
}
EXPORT_SYMBOL(son_send_sta_stats_event);
/**
 * @brief send activity change event to user space.
 * @param [in] vdev, vdev to which station is connected.
 * @param [in] macaddr, sta mac address.
 * @param [in] boolean flag indicating activity of client.
 * @param [inout] void .
 */

PUBLIC void son_notify_activity_change(struct wlan_objmgr_vdev *vdev,
				       char *macaddr,
				       bool activity)
{
	struct bs_activity_change_ind ind = {0};
	struct wlan_objmgr_pdev *pdev = NULL;

	pdev = wlan_vdev_get_pdev(vdev);

	if(!wlan_son_is_pdev_enabled(pdev)) {
		return;
	}

	if(!wlan_son_is_vdev_enabled(vdev))
		return;

	WLAN_ADDR_COPY(ind.client_addr, macaddr);
	ind.activity = activity ? 1 : 0;
	son_notify_user(vdev,
			ATH_EVENT_BSTEERING_CLIENT_ACTIVITY_CHANGE,
			sizeof(struct bs_activity_change_ind),
			(const char *) &ind);
	return;
}

/**
 * @brief Generate an event indicating that a probe request was received.
 *
 * This is used by user space to determine which nodes are dual band
 * capable.
 *
 * @param [in] vap  the VAP on which the probe was received
 * @param [in] mac_addr  the MAC address of the client that sent the probe
 *                       request
 * @param [in] rssi  the RSSI of the received probe request
 */

void son_send_probereq_event(struct wlan_objmgr_vdev *vdev,
			     const u_int8_t *mac_addr,
			     u_int8_t rssi)
{
	struct bs_probe_req_ind probe;
	struct wlan_objmgr_pdev *pdev = NULL;

	pdev = wlan_vdev_get_pdev(vdev);

	if(!wlan_son_is_pdev_enabled(pdev)) {
		return;
	}

	if(!wlan_son_is_vdev_enabled(vdev))
		return;

	qdf_mem_copy(probe.sender_addr, mac_addr, QDF_MAC_ADDR_SIZE);
	probe.rssi = rssi;
	son_notify_user(vdev, ATH_EVENT_BSTEERING_PROBE_REQ,
			sizeof(probe),
			(const char *) &probe);

	return;
}

/**
 * @brief Generate an event with the provided channel utilization measurement.
 *
 * @pre vap has already been checked and confirmed to be valid and band
 *      steering has been confirmed to be enabled
 *
 * @param [in] vap  the VAP that was used for the sampling
 * @param [in] chan_utilization  the utilization in percent
 * @param [in] is_debug  whether the log generated should be a debug event
 *                       or a regular event; debug events represent an
 *                       instantaneous measurement whereas normal (non-debug)
 *                       represent a filtered/averaged measurement
 */

PUBLIC void son_send_utilization_event(struct wlan_objmgr_vdev *vdev,
				       u_int8_t chan_utilization,
				       bool is_debug)
{
	struct bs_chan_utilization_ind ind;
	ATH_BSTEERING_EVENT event = ATH_EVENT_BSTEERING_CHAN_UTIL;

	ind.utilization = chan_utilization;

	if (is_debug) {
		event = ATH_EVENT_BSTEERING_DBG_CHAN_UTIL;
	}

	son_notify_user(vdev, event, sizeof(ind),
			(const char *) &ind);

	return;
}

/**
 * @brief Send an event to user space when requested RSSI measurement is available
 *
 * @pre vap has already been checked and confirmed to be valid and band
 *      steering has already been confirmed to be enabled
 *
 * @param [in] vap  the VAP that the client whose RSSI is measured associated to
 * @param [in] mac_addr  the MAC address of the client
 * @param [in] rssi  the measured RSSI
 * @param [in] is_debug  whether the log generated should be a debug event
 *                       or a regular event; debug events represent an
 *                       instantaneous (but averaged by firmware already) RSSI
 *                       measurement whereas normal (non-debug) represent the
 *                       RSSI measured by sending NDPs
 */
void son_send_rssi_measurement_event(struct wlan_objmgr_vdev *vdev,
				     const u_int8_t *mac_addr,
				     u_int8_t rssi,
				     bool is_debug)
{
	struct bs_rssi_measurement_ind ind;
	ATH_BSTEERING_EVENT event = ATH_EVENT_BSTEERING_CLIENT_RSSI_MEASUREMENT;

	qdf_mem_copy(ind.client_addr, mac_addr, QDF_MAC_ADDR_SIZE);
	ind.rssi = rssi;

	if (is_debug) {
		event = ATH_EVENT_BSTEERING_DBG_RSSI;
	}

	son_notify_user(vdev, event, sizeof(ind),
			(const char *) &ind);
}

/**
 * @brief Generate an event when Tx power change on a VAP
 *
 * @param [in] vap  the VAP on which Tx power changes
 * @param [in] tx_power  the new Tx power
 */
void son_send_txpower_change_event(struct wlan_objmgr_vdev *vdev,
				   u_int16_t tx_power)
{
	struct bs_tx_power_change_ind event = {0};
	struct wlan_objmgr_pdev *pdev = NULL;

	pdev = wlan_vdev_get_pdev(vdev);

	if (!wlan_son_is_vdev_enabled(vdev) ||
	    !wlan_son_is_pdev_valid(pdev)) {
		return;
	}

	if (!wlan_son_is_pdev_enabled(pdev))
		return;


	/* Tx power is stored in half dBm */
	event.tx_power = tx_power / 2;
	son_notify_user(vdev, ATH_EVENT_BSTEERING_TX_POWER_CHANGE,
			sizeof(struct bs_tx_power_change_ind),
			(const char *)&event);

	return;
}
EXPORT_SYMBOL(son_send_txpower_change_event);
/**
 * @brief Send an event to user space on RSSI measurement crossed threshold
 *
 * @pre vap has already been checked and confirmed to be valid and band
 *      steering has confirmed to be enabled
 *
 * @param [in] vap  the VAP that the client whose RSSI is measured associated to
 * @param [in] mac_addr  the MAC address of the client
 * @param [in] rssi  the measured RSSI
 * @param [in] inact_xing  flag indicating if the RSSI crossed inactivity RSSI threshold.
 * @param [in] low_xing  flag indicating if the RSSI crossed low RSSI threshold
 * @param [in] rate_xing  flag indicating if the RSSI crossed
 *                        the rate RSSI threshold
 * @param [in] ap_xing  flag indicating if the RSSI crossed the AP steering RSSI threshold
 */
void son_send_rssi_xing_event(struct wlan_objmgr_vdev *vdev,
			      const u_int8_t *mac_addr,
			      u_int8_t rssi,
			      BSTEERING_XING_DIRECTION inact_xing,
			      BSTEERING_XING_DIRECTION low_xing,
			      BSTEERING_XING_DIRECTION rate_xing,
			      BSTEERING_XING_DIRECTION ap_xing)
{
	struct bs_rssi_xing_threshold_ind ind;
	ATH_BSTEERING_EVENT event = ATH_EVENT_BSTEERING_CLIENT_RSSI_CROSSING;

	qdf_mem_copy(ind.client_addr, mac_addr, QDF_MAC_ADDR_SIZE);
	ind.rssi = rssi;
	ind.inact_rssi_xing = inact_xing;
	ind.low_rssi_xing = low_xing;
	ind.rate_rssi_xing = rate_xing;
	ind.ap_rssi_xing = ap_xing;

	son_notify_user(vdev, event, sizeof(ind),
			(const char *) &ind);
}

/**
 * @brief Send an event to user space on RSSI measurement crossing threshold for MAP
 *
 * @pre vap has already been checked and confirmed to be valid and band
 *      steering has confirmed to be enabled
 *
 * @param [in] vdev that the client whose RSSI is measured associated to
 * @param [in] mac_addr  the MAC address of the client
 * @param [in] rssi  the measured RSSI
 */
void son_send_rssi_xing_map_event(struct wlan_objmgr_vdev *vdev,
				  const u_int8_t *mac_addr,
				  u_int8_t rssi,
				  BSTEERING_XING_DIRECTION map_xing)
{
	struct bs_rssi_xing_threshold_ind_map ind;
	ATH_BSTEERING_EVENT event = ATH_EVENT_BSTEERING_MAP_CLIENT_RSSI_CROSSING;

	qdf_mem_copy(ind.client_addr, mac_addr, QDF_MAC_ADDR_SIZE);
	ind.rssi = rssi;
	ind.map_rssi_xing = map_xing;

	son_notify_user(vdev, event, sizeof(ind),
			(const char *) &ind);
}

/**
 * @brief Send an event to user space if the Tx rate crossed a
 *        threshold
 *
 * @pre vdev has already been checked and confirmed to be valid
 *      and band steering has confirmed to be enabled
 *
 * @param [in] vdev  the VAP that the client whose Tx rate is
 *                  measured associated to
 * @param [in] mac_addr  the MAC address of the client
 * @param [in] tx_rate  the Tx rate
 * @param [in] xing  flag indicating the direction of the Tx
 *                   rate crossing.
 */
void son_send_tx_rate_xing_event(
	struct wlan_objmgr_vdev *vdev,
	const u_int8_t *mac_addr,
	u_int32_t tx_rate,
	BSTEERING_XING_DIRECTION xing)
{
	struct bs_tx_rate_xing_threshold_ind ind;
	ATH_BSTEERING_EVENT event = ATH_EVENT_BSTEERING_CLIENT_TX_RATE_CROSSING;

	qdf_mem_copy(ind.client_addr, mac_addr, QDF_MAC_ADDR_SIZE);
	ind.tx_rate = tx_rate;
	ind.xing = xing;

	son_notify_user(vdev, event, sizeof(ind),
			(const char *) &ind);
	return;
}

/**
 * @brief Send an event to user space when the Tx rate changes.
 *        Note this is a debug only message.
 *
 * @pre vap has already been checked and confirmed to be valid and band
 *      steering has already been confirmed to be enabled
 *
 * @param [in] vap  the VAP that the client whose RSSI is measured associated to
 * @param [in] mac_addr  the MAC address of the client
 * @param [in] tx_rate  the latest Tx rate
 */
void son_send_tx_rate_measurement_event(struct wlan_objmgr_vdev *vdev,
					const u_int8_t *mac_addr,
					u_int32_t tx_rate)
{
	struct bs_tx_rate_measurement_ind ind;
	ATH_BSTEERING_EVENT event = ATH_EVENT_BSTEERING_DBG_TX_RATE;

	qdf_mem_copy(ind.client_addr, mac_addr, QDF_MAC_ADDR_SIZE);
	ind.tx_rate = tx_rate;

	son_notify_user(vdev, event, sizeof(ind),
			(const char *) &ind);

	return;
}

/**
 * @brief Inform the band steering module that a node is now
 *        associated
 *
 * @param [in] vap  the VAP on which the change occurred
 * @param [in] mac_addr  the MAC address of the client who
 *                       associated
 * @param [in] isBTMSupported  set to true if BSS Transition
 *                             Management is supported by this
 *                             STA (as indicated in the
 *                             association request frame)
 */

void son_send_node_associated_event(struct wlan_objmgr_vdev *vdev,
				    struct wlan_objmgr_peer *peer)
{
	struct bs_node_associated_ind assoc;
	int max_MCS = 0;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_peer_priv *pe_priv = NULL;

	pdev = wlan_vdev_get_pdev(vdev);

	if(!wlan_son_is_vdev_enabled(vdev) ||
	   !wlan_son_is_pdev_enabled(pdev) || !peer) {
		return;
	}

	pe_priv = wlan_son_get_peer_priv(peer);

	qdf_mem_copy(assoc.client_addr, wlan_peer_get_macaddr(peer), QDF_MAC_ADDR_SIZE);
	max_MCS = wlan_peer_get_node_max_MCS(peer);
	assoc.datarate_info.max_MCS = max_MCS < 0 ? 0 : max_MCS;

	if ((wlan_node_get_capability(peer, &assoc) == EOK) && pe_priv) {
		if (!(assoc.isBTMSupported) || (!(assoc.isBeaconMeasurementSupported))) {
			pe_priv->son_peer_class_group = 0;
		}
		assoc.client_class_group = pe_priv->son_peer_class_group;
		son_notify_user(vdev,
				ATH_EVENT_BSTEERING_NODE_ASSOCIATED,
				sizeof(assoc),
				(const char *) &assoc);
	}
	return;
}

/**
 * @brief Generate an event indicating that an authentication message
 *        was sent with a failure code.
 *
 * @param [in] vap  the VAP on which the message was sent
 * @param [in] mac_addr  the MAC address of the client to which the
 *                       message was sent
 * @param [in] rssi  the RSSI of the received authentication message which
 *                   caused the rejection
 */

void son_send_auth_fail_event(struct wlan_objmgr_vdev *vdev,
			      const u_int8_t *mac_addr,
			      u_int8_t rssi)
{
	struct bs_auth_reject_ind auth;
	struct wlan_objmgr_pdev *pdev = NULL;

	pdev = wlan_vdev_get_pdev(vdev);

	if(!wlan_son_is_vdev_enabled(vdev) ||
	   !wlan_son_is_pdev_enabled(pdev) ) {
		return;
	}

	qdf_mem_copy(auth.client_addr, mac_addr,QDF_MAC_ADDR_SIZE);
	auth.rssi = rssi;
	son_notify_user(vdev,
			ATH_EVENT_BSTEERING_TX_AUTH_FAIL,
			sizeof(auth),
			(const char *) &auth);
	return;
}

/**
 * @brief Generate an event indicating a Radio Resource Management report
 *        is received
 *
 * @param [in] vap  the VAP on which the rrm report was received
 * @param [in] report  the report to put in the event
 */

static void son_send_rrm_report_event(
	struct wlan_objmgr_vdev *vdev, const struct bs_rrm_report_ind *report)
{

	son_notify_user(vdev, ATH_EVENT_BSTEERING_RRM_REPORT,
			sizeof(*report), (const char*)report);

	return;
}

/**
 * @brief Generate an event indicating a Radio Resource Management report
 *        is received
 *
 * @param [in] vap  the VAP on which the rrm report was received
 * @param [in] report  the report to put in the event with raw data
 */

static void
son_send_rrm_frame_report_event(
				struct wlan_objmgr_vdev *vdev,
				const struct bs_rrm_frame_report_ind *report,
				size_t len)
{

	son_notify_user(vdev, ATH_EVENT_BSTEERING_RRM_FRAME_REPORT,
			len, (const char*)report);

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
	struct bs_rrm_report_ind event = {0};
	struct wlan_objmgr_pdev *pdev = NULL;

	pdev = wlan_vdev_get_pdev(vdev);


	if(!wlan_son_is_vdev_enabled(vdev) ||
	   !wlan_son_is_pdev_enabled(pdev) ) {
		return;
	}
	event.rrm_type = BSTEERING_RRM_TYPE_BCNRPT;
	event.measrpt_mode = mode;
	event.dialog_token = token;
	IEEE80211_ADDR_COPY(event.macaddr, macaddr);

	son_send_rrm_report_event(vdev, &event);
}

/**
 * @brief Allocate memory to be filled with RRM beacon reports received
 *
 * The caller is responsible to call ieee80211_bsteering_dealloc_rrm_bcnrpt
 * to free the allocated memory.
 *
 * @param [in] vap  the VAP on which the report is received
 * @param [inout] reports  memory allocated enough to hold the requested
 *                         number of beacon reports on success
 * @return maximum number of beacon reports should be filled on success;
 *         0 for error cases
 */

u_int8_t son_alloc_rrm_bcnrpt(struct wlan_objmgr_vdev *vdev,
			      ieee80211_bcnrpt_t **reports)
{
	struct wlan_objmgr_pdev *pdev = NULL;

	pdev = wlan_vdev_get_pdev(vdev);

	if(!wlan_son_is_vdev_enabled(vdev) ||
	   !wlan_son_is_pdev_enabled(pdev) ) {
		return 0;
	}

	if (!reports) {
		return 0;
	}

	*reports = NULL;

	*reports = (ieee80211_bcnrpt_t *)qdf_mem_malloc(
		IEEE80211_BSTEERING_RRM_NUM_BCNRPT_MAX * sizeof(ieee80211_bcnrpt_t));

	if (*reports) {
		qdf_mem_zero(*reports,
			     IEEE80211_BSTEERING_RRM_NUM_BCNRPT_MAX * sizeof(ieee80211_bcnrpt_t));
		return IEEE80211_BSTEERING_RRM_NUM_BCNRPT_MAX;
	} else {
		return 0;
	}
}

/**
 * @brief Free the allocated memory for RRM beacon reports
 *
 * @param [inout] reports  the memory to be freed
 */

void son_dealloc_rrm_bcnrpt(ieee80211_bcnrpt_t **reports)
{
	if (reports) {
		qdf_mem_free(*reports);
		*reports = NULL;
	}
}

/**
 * @brief Generate an event when beacon report response is received
 *
 * It will generate a netlink event if at lease one report is received, and
 * the beacon report memory will be wiped out for further reports.
 *
 * @param [in] vap  the VAP on which the report is received
 * @param [1n] token  the dialog token matching the one provided in the request
 * @param [in] macaddr  the MAC address of the reporter station
 * @param [1nout] bcnrpt  the beacon report(s) received
 * @param [1n] num_bcnrpt  number of the beacon report(s) to send
 */
void son_send_rrm_bcnrpt_event(
	struct wlan_objmgr_vdev *vdev, u_int32_t token, u_int8_t *macaddr,
	ieee80211_bcnrpt_t *bcnrpt, u_int8_t num_bcnrpt)
{
	struct bs_rrm_report_ind event = {0};

	struct wlan_objmgr_pdev *pdev = NULL;

	pdev = wlan_vdev_get_pdev(vdev);

	if(!wlan_son_is_vdev_enabled(vdev) ||
	   !wlan_son_is_pdev_enabled(pdev) ) {
		return;
	}

	event.rrm_type = BSTEERING_RRM_TYPE_BCNRPT;
	event.measrpt_mode = IEEE80211_RRM_MEASRPT_MODE_SUCCESS;
	event.dialog_token = token;
	IEEE80211_ADDR_COPY(event.macaddr, macaddr);
	qdf_mem_copy(&event.data, bcnrpt,
		  (num_bcnrpt <= IEEE80211_BSTEERING_RRM_NUM_BCNRPT_MAX ?
		   num_bcnrpt : IEEE80211_BSTEERING_RRM_NUM_BCNRPT_MAX) *
		  sizeof(ieee80211_bcnrpt_t));

	son_send_rrm_report_event(vdev, &event);

	qdf_mem_zero(bcnrpt,
		     IEEE80211_BSTEERING_RRM_NUM_BCNRPT_MAX * sizeof(ieee80211_bcnrpt_t));
}

/**
 * @brief Generate an event when beacon frame report response is received
 *
 * It will generate a netlink event if at least one report is received
 *
 * @param [in] vdev  the VAP on which the report is received
 * @param [in] token  the dialog token matching the one provided in the request
 * @param [in] macaddr  the MAC address of the reporter station
 * @param [inout] bcnrpt  the beacon report(s) received
 * @param [in] report_len  the total length of the report
 * @param [in] num_rpt_elem  number of the beacon report element(s) to send
 */
void son_send_rrm_frame_bcnrpt_event(struct wlan_objmgr_vdev *vdev,
				     u_int32_t token, u_int8_t *macaddr,
				     const u_int8_t *bcnrpt, size_t report_len,
				     u_int8_t num_rpt_elem)
{
	struct bs_rrm_frame_report_ind *event = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	size_t event_len = 0;

	pdev = wlan_vdev_get_pdev(vdev);

	if (!wlan_son_is_vdev_enabled(vdev) ||
		!wlan_son_is_pdev_enabled(pdev) ) {
		return;
	}
	event_len = sizeof(struct bs_rrm_frame_report_ind) + report_len;
	event = (struct bs_rrm_frame_report_ind*)qdf_mem_malloc(event_len);

	if (!event) {
		qdf_err("%s: Failed to create event buffer\n", __func__);
		return;
	}
	event->rrm_type = BSTEERING_RRM_TYPE_BCNRPT;
	event->measrpt_mode = IEEE80211_RRM_MEASRPT_MODE_SUCCESS;
	event->dialog_token = token;
	event->num_meas_rpts = num_rpt_elem;
	event->data_len = report_len;
	IEEE80211_ADDR_COPY(event->macaddr, macaddr);
	qdf_mem_copy(&event->meas_rpt_data, bcnrpt, report_len);

	son_send_rrm_frame_report_event(vdev, event, event_len);
	qdf_mem_free(event);
}




/**
 * @brief Generate an event when a BSS Transition Management
 *        response frame is received
 *
 * @param [in] vap  the VAP on which the frame is received
 * @param [in] token  the dialog token matching the one provided in
 *                    the request
 * @param [in] macaddr  the MAC address of the sending station
 * @param [in] bstm_resp  response frame information
 */

void son_send_wnm_bstm_resp_event(struct wlan_objmgr_vdev *vdev, u_int32_t token,
				  u_int8_t *macaddr,
				  struct bs_wnm_bstm_resp *bstm_resp)
{
	struct bs_wnm_event_ind event = {0};
	struct wlan_objmgr_pdev *pdev = NULL;

	pdev = wlan_vdev_get_pdev(vdev);

	if(!wlan_son_is_vdev_enabled(vdev) ||
	   !wlan_son_is_pdev_enabled(pdev) ) {
		return;
	}

	event.wnm_type = BSTEERING_WNM_TYPE_BSTM_RESPONSE;
	event.dialog_token = token;
	IEEE80211_ADDR_COPY(event.macaddr, macaddr);
	qdf_mem_copy(&event.data, bstm_resp, sizeof(struct bs_wnm_bstm_resp));

	son_notify_user(vdev, ATH_EVENT_BSTEERING_WNM_EVENT,
			sizeof(struct bs_wnm_event_ind), (const char*)&event);
	return;
}

/**
 * @brief Generate an event when a BSS Transition Management
 *        query frame is received
 *
 * @param [in] vap  the VAP on which the frame is received
 * @param [in] token  the dialog token matching the one provided in
 *                    the request
 * @param [in] macaddr  the MAC address of the sending station
 * @param [in] bstm_query  query frame information
 */
void son_send_wnm_bstm_query_event(struct wlan_objmgr_vdev *vdev, u_int32_t token,
                                   u_int8_t *macaddr,
                                   struct bs_wnm_bstm_query *bstm_query)
{
	struct bs_wnm_event_ind event = {0};
	struct wlan_objmgr_pdev *pdev = NULL;

	pdev = wlan_vdev_get_pdev(vdev);

	if(!wlan_son_is_vdev_enabled(vdev) ||
	   !wlan_son_is_pdev_enabled(pdev) ) {
		return;
	}

    event.wnm_type = BSTEERING_WNM_TYPE_BSTM_QUERY;
    event.dialog_token = token;
    IEEE80211_ADDR_COPY(event.macaddr, macaddr);
    qdf_mem_copy(&event.data, bstm_query, sizeof(struct bs_wnm_bstm_query));

    son_notify_user(vdev, ATH_EVENT_BSTEERING_WNM_EVENT,
                    sizeof(struct bs_wnm_event_ind), (const char*)&event);
}

/**
 * @brief Called when a VAP is stopped (this is only seen on a
 *        RE when the uplink STA interface is disassociated)
 *
 * @param [in] vap  VAP that has stopped
 */

void son_send_vap_stop_event(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_pdev_priv *pd_priv = NULL;

	pdev = wlan_vdev_get_pdev(vdev);

	if(!wlan_son_is_vdev_enabled(vdev) ||
	   !wlan_son_is_pdev_valid(pdev) ) {
		return;
	}

	pd_priv = wlan_son_get_pdev_priv(pdev);

	SON_LOCK(&pd_priv->son_lock);

	if (wlan_son_is_pdev_enabled(pdev)) {
		ATH_BSTEERING_EVENT event = ATH_EVENT_BSTEERING_VAP_STOP;

		/* No data to send with this event */
		son_notify_user(vdev, event, 0, NULL);
	}

	SON_UNLOCK(&pd_priv->son_lock);
	return;
}

EXPORT_SYMBOL(son_send_vap_stop_event);

/**
 * @brief Generate an event when SM Power Save mode is updated for the given node
 *
 * @param [in] vap  the VAP on which the change occurred
 * @param [in] ni  the node whose SM Power Save mode changes
 * @param [in] is_static  whether the node is operating in Static SMPS mode
 */
void son_send_node_smps_update_event(struct wlan_objmgr_vdev *vdev,
				     struct wlan_objmgr_peer *peer,
				     u_int8_t is_static)
{
	ATH_BSTEERING_EVENT event = ATH_EVENT_BSTEERING_SMPS_UPDATE;
	struct bs_node_smps_update_ind smps_update;
	struct wlan_objmgr_pdev *pdev = NULL;

	pdev = wlan_vdev_get_pdev(vdev);

	if (!wlan_son_is_vdev_enabled(vdev) ||
	    !wlan_son_is_pdev_valid(pdev) ) {
		return;
	}

	if (!wlan_son_is_pdev_enabled(pdev) || !peer) {
		return;
	}

	qdf_mem_copy(smps_update.client_addr, peer->macaddr, QDF_MAC_ADDR_SIZE);
	smps_update.is_static = is_static;

	son_notify_user(vdev, event, sizeof(struct bs_node_smps_update_ind),
			(const char *) &smps_update);

	return;
}

/**
 * @brief Generate an event when PHY capabilities are updated from OP_MODE IE
 *
 * @param [in] vap  the VAP on which the change occured
 * @param [in] ni  the node whose PHY capabilities are updated
 */
void son_send_opmode_update_event(struct wlan_objmgr_vdev *vdev,
				  char *macaddr,
				  u_int8_t chwidth,
				  u_int8_t streams)
{
	ATH_BSTEERING_EVENT event = ATH_EVENT_BSTEERING_OPMODE_UPDATE;
	struct bs_node_opmode_update_ind opmode_update;
	struct wlan_objmgr_pdev *pdev = NULL;

	pdev = wlan_vdev_get_pdev(vdev);


	if (!wlan_son_is_vdev_enabled(vdev) ||
	    !wlan_son_is_pdev_valid(pdev) ) {
		return;
	}

	if (!wlan_son_is_pdev_enabled(pdev)) {
		return;
	}

	qdf_mem_copy(opmode_update.client_addr, macaddr, QDF_MAC_ADDR_SIZE);
	/* Only channel width and number of spatial stream will be changed. */
	opmode_update.datarate_info.max_chwidth = chwidth;
	opmode_update.datarate_info.num_streams = streams;

	son_notify_user(vdev, event, sizeof(struct bs_node_opmode_update_ind),
			(const char *) &opmode_update);

	return;
}

/**
 * @brief Generate an event indicating that an authentication is
 *        allowed even if the client is present in the blacklist
 *        due to auth allow flag set.
 *
 * @param [in] vap  the VAP on which the message was sent
 * @param [in] mac_addr  the MAC address of the client to which the
 *                       message was sent
 * @param [in] rssi  the RSSI of the received authentication message which
 *                   caused the association
 */

void son_send_auth_allow_event(struct wlan_objmgr_vdev *vdev,
			       const u_int8_t *mac_addr,
			       u_int8_t rssi)
{
	struct bs_auth_reject_ind auth;
	struct wlan_objmgr_pdev *pdev = NULL;

	pdev = wlan_vdev_get_pdev(vdev);

	if (!wlan_son_is_vdev_enabled(vdev) ||
	    !wlan_son_is_pdev_valid(pdev) ) {
		return;
	}

	if (!wlan_son_is_pdev_enabled(pdev)) {
		return;
	}

	qdf_mem_copy(auth.client_addr, mac_addr, QDF_MAC_ADDR_SIZE);
	auth.rssi = rssi;
	son_notify_user(vdev,
			ATH_EVENT_BSTEERING_DBG_TX_AUTH_ALLOW,
			sizeof(auth),
			(const char *) &auth);

	return;
}
#endif

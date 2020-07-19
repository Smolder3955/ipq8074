/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
*/
/*
 *This File provides framework for SON.
 */
#if QCA_SUPPORT_SON

#include "wlan_son_internal.h"
#include "wlan_son_pub.h"
#include <cdp_txrx_ctrl.h>
#include <wlan_utility.h>

static bool son_core_inact_enable(struct wlan_objmgr_pdev *pdev, bool enable);
static void son_core_set_overload (struct wlan_objmgr_pdev *pdev, bool overload);
static bool son_core_set_inact_params(struct wlan_objmgr_pdev *pdev);

void son_bs_stats_update_cb(void *pdev, enum WDI_EVENT event,
			    void *data, uint16_t data_len,
			    uint32_t status)
{
	struct cdp_interface_peer_stats *bs_stats = (struct cdp_interface_peer_stats *)data;
	struct wlan_objmgr_peer *peer;
	struct wlan_objmgr_vdev *current_vdev = NULL;
	struct wlan_objmgr_vdev *rssi_vdev = NULL;
	struct bs_sta_stats_ind sta_stats = {0};
	struct son_peer_priv *pe_priv = NULL;

	static uint32_t prev_ack_rssi = 0;
	uint8_t is_ack_rssi_enabled = 0;

	peer = bs_stats->peer_hdl;
	if (!peer) {
		qdf_print("%s ctrl peer is null",__func__);
		return;
	}

	/* set rssi_changed flag */
	rssi_vdev = wlan_peer_get_vdev(peer);
	is_ack_rssi_enabled = son_is_ackrssi_enabled(rssi_vdev);
	if (is_ack_rssi_enabled) {
		bs_stats->rssi_changed = false;
		bs_stats->rssi_changed =
			prev_ack_rssi != bs_stats->ack_rssi ? true : false;
	}
	pe_priv = wlan_son_get_peer_priv(peer);

	/* New RSSI measurement */
	if (is_ack_rssi_enabled && bs_stats->rssi_changed) {
		son_record_peer_rssi(peer, bs_stats->ack_rssi);
	}
	else if (bs_stats->rssi_changed) {
		son_record_peer_rssi(peer, bs_stats->peer_rssi);
	}

	/* Tx rate has changed */
	if (bs_stats->peer_tx_rate &&
			(bs_stats->peer_tx_rate != bs_stats->last_peer_tx_rate)) {
		son_update_peer_rate(peer,
				     bs_stats->peer_tx_rate,
				     bs_stats->last_peer_tx_rate);
	}

	/* Only need to send a STA stats update for this peer if the
	 * RSSI changed and the Tx rate is valid
	 */
	if (bs_stats->rssi_changed && bs_stats->peer_tx_rate) {
		if (son_update_sta_stats(peer,
					 current_vdev,
					 &sta_stats,
					 bs_stats)) {
			current_vdev = wlan_peer_get_vdev(peer);
		}
		if (current_vdev) {
			son_send_sta_stats_event(current_vdev, &sta_stats);
		}
	}

	/* Save previous value */
	prev_ack_rssi = bs_stats->ack_rssi;

	/* Check the peer TX and RX packet count for inactivity check */
	if (bs_stats->tx_packet_count > pe_priv->tx_packet_count ||
		bs_stats->rx_packet_count > pe_priv->rx_packet_count) {
	    son_mark_node_inact(peer, false /* inactive */);
	    pe_priv->tx_packet_count = bs_stats->tx_packet_count;
	    pe_priv->rx_packet_count = bs_stats->rx_packet_count;
	}
}
qdf_export_symbol(son_bs_stats_update_cb);

static void son_iterate_vdev_bss_rssi_send(struct wlan_objmgr_pdev *pdev,
					   void *obj,
					   void *arg)
{
	struct son_pdev_priv *pd_priv = (struct son_pdev_priv *) arg;
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)obj;
	struct wlan_objmgr_peer *peer =NULL;

	/*event is already sent through another vdev so return*/
	if (pd_priv->rssi_invalid_event)
		return;

	if (wlan_son_is_vdev_valid(vdev)) {
		if (wlan_son_is_vdev_enabled(vdev)) {
				peer = wlan_vdev_get_bsspeer(vdev);
				if (peer)
					son_send_rssi_measurement_event(vdev,
						wlan_peer_get_macaddr(peer),
						BSTEERING_INVALID_RSSI,
						false /* is_debug */);
		}
	}
}

/**
 * @brief Timeout handler for inst RSSI measurement timer.
 *
 * Inst RSSI measurement has timed out at this point, should generate
 * an event with invalid RSSI value.
 *
 * @param [in] arg  pdev
 *
 */

static OS_TIMER_FUNC(son_core_inst_rssi_timeout_handler)
{
	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_pdev_priv *pd_priv  = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_peer *peer = NULL;

	OS_GET_TIMER_ARG(pdev, struct wlan_objmgr_pdev *);

	if (!wlan_son_is_pdev_valid(pdev)) {
		return;
	}

	pd_priv = wlan_son_get_pdev_priv(pdev);
	psoc = wlan_pdev_get_psoc(pdev);

	SON_LOCK(&pd_priv->son_lock);

	do {
		if (!wlan_son_is_pdev_enabled(pdev)) {
			break;
		}

		if (!pd_priv->son_inst_rssi_inprogress) {
			break;
		}

		peer = wlan_objmgr_get_peer(psoc,
				wlan_objmgr_pdev_get_pdev_id(pdev),
				pd_priv->son_inst_rssi_macaddr,
				WLAN_SON_ID);
		if (peer) {
			vdev = wlan_peer_get_vdev(peer);
			son_send_rssi_measurement_event(vdev,
						wlan_peer_get_macaddr(peer),
						BSTEERING_INVALID_RSSI,
						false /* is_debug */);
		} else {
			wlan_objmgr_pdev_iterate_obj_list(pdev,
					  WLAN_VDEV_OP,
					  son_iterate_vdev_bss_rssi_send,
					  (void *)pd_priv, 0,
					  WLAN_SON_ID);
	/* If no VAP is band steering enabled on this band, it probably means
	   band steering feature has been disabled. So we can live with not
	   sending an RSSI measurement event since no one will be interested. */
		}

		pd_priv->son_inst_rssi_inprogress = false;

		SON_LOGI("%s: Inst RSSI measurement request for STA "
			 "%02x:%02x:%02x:%02x:%02x:%02x timeout.\n",
			 __func__, pd_priv->son_inst_rssi_macaddr[0],
			 pd_priv->son_inst_rssi_macaddr[1],
			 pd_priv->son_inst_rssi_macaddr[2],
			 pd_priv->son_inst_rssi_macaddr[3],
			 pd_priv->son_inst_rssi_macaddr[4],
			 pd_priv->son_inst_rssi_macaddr[5]);
	} while(0);

	if(peer)
		wlan_objmgr_peer_release_ref(peer, WLAN_SON_ID);

	SON_UNLOCK(&pd_priv->son_lock);
	return;
}

/*
**
* @brief VAP iteration callback that measures the channel utilization
*        on the provided VAP if it is the VAP that was used to enable
*        band steering.
*
* If the VAP is not the one used to enable band steering,this does nothing.
* @note This must be called with the band steering lock held.
*
* @param [in] arg pointer to pdev priv for son
* @param [in] obj pointer to vdev.
*/

static void son_iterate_vdev_measure_chan_util(struct wlan_objmgr_pdev *pdev,
					       void *obj,
					       void *arg)
{
#define CHANNEL_LOAD_REQUESTED 2
	struct son_pdev_priv *pd_priv = (struct son_pdev_priv *) arg;
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)obj;
	ieee80211_bsteering_param_t *params = NULL;
	int val = 1;
	wlan_chan_t chan = NULL;
	char myaddr[QDF_MAC_ADDR_SIZE];

	params = &pd_priv->son_config_params;

	/* Check whether the VAP still exists and is in AP mode */
	if (vdev == pd_priv->son_iv  && wlan_son_is_vdev_valid(vdev)) {

		/* This check that the VAP is ready should be
		   sufficient to ensure we do not trigger a
		   scan while in DFS wait state.
		   Note that there is still a small possibility
		   that the channel or state  will change after
		   we check this flag. I do not know how to avoid this
		   at this time.*/
		chan = wlan_vdev_get_channel(vdev);

		qdf_mem_copy(myaddr, wlan_vdev_mlme_get_macaddr(vdev), QDF_MAC_ADDR_SIZE);

		if ((wlan_vdev_is_up(vdev) == QDF_STATUS_SUCCESS) &&
		    !pd_priv->son_chan_util_requested && chan) {
			/* Remember the channel so we can ignore any callbacks that are
			   not for our channel. */
			u_int8_t chanlist[2] = { 0 };

			if (chan->ic_ieee != pd_priv->son_active_ieee_chan_num) {
				/* Channel changed, so invalidate everything. The utilization
				   is not expected to be correlated across channels. */
				son_core_reset_chan_utilization(pd_priv);
			}

			pd_priv->son_active_ieee_chan_num = chan->ic_ieee;

			chanlist[0] = pd_priv->son_active_ieee_chan_num;

			/* Direct attach donot accept delayed acs reports*/

			if(!pd_priv->son_delayed_ch_load_rpt) {
				val = CHANNEL_LOAD_REQUESTED;
			}
			if (wlan_vdev_acs_set_user_chanlist(vdev, chanlist) == EOK) {
				if (wlan_vdev_acs_start_scan_report(vdev, val) == EOK) {
					pd_priv->son_chan_util_requested = true;
				} else {
					SON_LOGW("%s: Failed to start scan report on interface "
						 "%02x:%02x:%02x:%02x:%02x:%02x; "
						 "will retry in next timer expiry\n",
						 __func__, myaddr[0], myaddr[1],
						 myaddr[2], myaddr[3],
						 myaddr[4], myaddr[5]);
				}
			} else {
				SON_LOGW("%s: Failed to set channel list on interface "
					 " %02x:%02x:%02x:%02x:%02x:%02x; "
					 "will retry in next timer expiry\n",
					 __func__, myaddr[0], myaddr[1],
					 myaddr[2], myaddr[3],
					 myaddr[4], myaddr[5]);
			}
		} else {
			SON_LOGW("%s: Already waiting for utilization, VAP is not ready, "
				 "or bsschan is invalid on interface "
				 "%02x:%02x:%02x:%02x:%02x:%02x: %pK\n",
				 __func__, myaddr[0], myaddr[1],
				 myaddr[2], myaddr[3],
				 myaddr[4], myaddr[5], chan);
		}

		qdf_timer_start(&pd_priv->son_chan_util_timer,
				params->utilization_sample_period
				* 1000);
	}
#undef CHANNEL_LOAD_REQUESTED
	return;
}


/**
 * @brief Timeout handler for periodic channel utilization measurements.
 *
 * This will trigger the utilization measurement (assuming one is not already
 * in progress) and reschedule the timer for the next measurement. The
 * assumption is that the measurement will be complete prior to the next
 * timer expiry, but if it is not, this is guarded against and the next
 * measurement will just be delayed.
 *
 * @param [in] arg  pdev
 *
 */
static OS_TIMER_FUNC(son_core_chan_util_timeout_handler)
{
	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_pdev_priv *pd_priv = NULL;

	OS_GET_TIMER_ARG(pdev, struct wlan_objmgr_pdev *);
	if (!wlan_son_is_pdev_valid(pdev)) {
		return;
	}

	pd_priv = wlan_son_get_pdev_priv(pdev);
	SON_LOCK(&pd_priv->son_lock);

	if (!wlan_son_is_pdev_enabled(pdev)) {
		SON_UNLOCK(&pd_priv->son_lock);
		return;
	}

	wlan_objmgr_pdev_iterate_obj_list(pdev,
					  WLAN_VDEV_OP,
					  son_iterate_vdev_measure_chan_util,
					  (void *)pd_priv, 0,
					  WLAN_SON_ID);

	SON_UNLOCK(&pd_priv->son_lock);
}

static void son_iterate_peer_check_inact(struct wlan_objmgr_pdev *pdev,
					 void *obj,
					 void *arg)
{
	struct wlan_objmgr_peer *peer = (struct wlan_objmgr_peer *)obj;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct son_peer_priv *pe_priv = NULL;

	vdev = wlan_peer_get_vdev(peer);

	pe_priv = wlan_son_get_peer_priv(peer);
	if (!(peer->peer_mlme.peer_flags & WLAN_PEER_F_AUTH) || /*check who will set this flag */
	    !wlan_son_is_vdev_enabled(vdev)) {
		/* Inactivity check only interested in connected node */
		return;
	}

	if (pe_priv->son_inact > pe_priv->son_inact_reload) {
		/* This check ensures we do not wait extra long
		   due to the potential race condition */
		pe_priv->son_inact = pe_priv->son_inact_reload;
	}

	if (pe_priv->son_inact > 0) {
		/* Do not let it go negative */
		pe_priv->son_inact--;
	}

	if (pe_priv->son_inact == 0) {
		/* Mark the node as inactive */
		son_mark_node_inact(peer, true);
	}

	return;
}

/**
 * @brief Timeout handler for inactivity timer. Decrease node's inactivity count by 1.
 *        If any node's inactivity count reaches 0, generate an event
 *
 * @param [in] arg  ieee80211com
 *
 */
static OS_TIMER_FUNC(son_core_inact_timeout_handler)
{
	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_pdev_priv *pd_priv = NULL;
	OS_GET_TIMER_ARG(pdev, struct wlan_objmgr_pdev *);
	pd_priv = wlan_son_get_pdev_priv(pdev);


	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_PEER_OP,
					  son_iterate_peer_check_inact,
					  (void *)pd_priv, 0,
					  WLAN_SON_ID);

	qdf_timer_start(&pd_priv->son_inact_timer,
			pd_priv->son_config_params.inactivity_check_period * 1000);
}

/**
 * @brief Initialization for Psoc create handler for SON
 *
 * @param [in] psoc , to handle psoc band steering initialization.
 *
 * @param [in] arg
 *
 * @return QDF_STATUS_SUCCESS if successfully enabled
 *         else QDF_STATUS_FAIL.
 */

static QDF_STATUS wlan_son_psoc_create_handler(struct wlan_objmgr_psoc *psoc,
					       void *arg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct son_psoc_priv *psoc_priv;

	if (!psoc) {
		SON_LOGF("Psoc is null Investigate  %s %d ",
			 __func__,__LINE__);
		status = QDF_STATUS_E_INVAL;
		goto failure;
	}

	psoc_priv = qdf_mem_malloc(sizeof(struct son_psoc_priv));

	if (psoc_priv == NULL) {
		SON_LOGF("Insufficient memory can not allocate Son psoc");
		status = QDF_STATUS_E_NOMEM;
		goto failure;
	}

	psoc_priv->psoc = psoc;
	if (wlan_objmgr_psoc_component_obj_attach(psoc,
				  WLAN_UMAC_COMP_SON,
				  psoc_priv,
				  QDF_STATUS_SUCCESS) != QDF_STATUS_SUCCESS) {
		SON_LOGF("Failed to attach SON  psoc ");
		status = QDF_STATUS_E_FAILURE;
		goto attach_failure;
	}

	return status;

attach_failure:
	qdf_mem_free(psoc_priv);
failure:
	return status;

}


/**
 * @brief Determine whether the band steering module is enabled or not.
 *
 * @param [in] ic  the handle to the radio where the band steering state
 *                 resides
 *
 * @return non-zero if it is enabled; otherwise 0
 */

static QDF_STATUS wlan_son_pdev_create_handler(struct wlan_objmgr_pdev *pdev,
					       void *arg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_objmgr_psoc *psoc = NULL;
	struct son_pdev_priv *pdev_priv = NULL;

	if (!pdev) {
		SON_LOGF("pdev is null Investigate  %s %d ",
			 __func__,__LINE__);
		status = QDF_STATUS_E_INVAL;
		goto failure;
	}

	pdev_priv = qdf_mem_malloc(sizeof(struct son_pdev_priv));

	if (pdev_priv == NULL) {
		SON_LOGF("Insufficient memory can not allocate Son pdev");
		status = QDF_STATUS_E_NOMEM;
		goto failure;
	}

	psoc = wlan_pdev_get_psoc(pdev);
	pdev_priv->pdev = pdev;
	qdf_spinlock_create(&pdev_priv->son_lock);
	qdf_atomic_set(&pdev_priv->son_enabled, false);
	pdev_priv->son_chan_util_requested = false;
	pdev_priv->son_delayed_ch_load_rpt = false;
	pdev_priv->son_inst_rssi_log = false;
	/* RSSI fluctuates for DA hardware so we need to
	   take some Threshold before reporting it */

	pdev_priv->son_rssi_xing_report_delta = 0;

	qdf_timer_init(NULL, &pdev_priv->son_chan_util_timer,
		       son_core_chan_util_timeout_handler, (void *) pdev,
		       QDF_TIMER_TYPE_WAKE_APPS);

	qdf_timer_init(NULL, &pdev_priv->son_inst_rssi_timer,
		       son_core_inst_rssi_timeout_handler,
		       (void *) pdev,
		       QDF_TIMER_TYPE_WAKE_APPS);

	qdf_timer_init(NULL, &pdev_priv->son_inact_timer,
		       son_core_inact_timeout_handler,
		       (void *) pdev,
		       QDF_TIMER_TYPE_WAKE_APPS);

	if (wlan_objmgr_pdev_component_obj_attach(pdev,
				WLAN_UMAC_COMP_SON,
				pdev_priv,
				QDF_STATUS_SUCCESS) != QDF_STATUS_SUCCESS) {
		SON_LOGF("Failed to attach Son  Pdev  ");
		status = QDF_STATUS_E_INVAL;
		goto attach_failure;
	}
	/* Lmac create should be after object manager attach
	   as it needs pdev priv from object manager*/

	if (SON_TARGET_IF_LMAC_CREATE(pdev))
		SON_TARGET_IF_LMAC_CREATE(pdev);

	TAILQ_INIT(&pdev_priv->in_network_table_2g);
	SON_LOGD("%s: Band steering initialized\n", __func__);
	return status;

attach_failure:
	qdf_timer_free(&pdev_priv->son_chan_util_timer);
	qdf_timer_free(&pdev_priv->son_inst_rssi_timer);
	qdf_timer_free(&pdev_priv->son_inact_timer);
	qdf_mem_free(pdev_priv);
failure:
	return status;
}

/**
 * @brief Determine whether the band steering module is enabled or not.
 *
 * @param [in] ic  the handle to the radio where the band steering state
 *                 resides
 *
 * @return non-zero if it is enabled; otherwise 0
 */

static QDF_STATUS wlan_son_vdev_create_handler(struct wlan_objmgr_vdev *vdev,
					       void *arg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct son_vdev_priv *vdev_priv;

	if (!vdev) {
		SON_LOGF("vdev is null Investigate  %s %d ",
			 __func__,__LINE__);
		status = QDF_STATUS_E_INVAL;
		goto failure;
	}

	vdev_priv = qdf_mem_malloc(sizeof(struct son_vdev_priv));

	if (vdev_priv == NULL) {
		SON_LOGF("Insufficient memory can not allocate SON vdev");
		status = QDF_STATUS_E_NOMEM;
		goto failure;
	}

	vdev_priv->vdev = vdev;
	vdev_priv->iv_whc_scaling_factor = WHC_DEFAULT_SFACTOR;
	vdev_priv->iv_whc_skip_hyst = WHC_DEFAULT_SKIPHYST;
	vdev_priv->lbd_pid = WLAN_DEFAULT_NETLINK_PID;
	vdev_priv->son_rept_multi_special = 0;
	vdev_priv->son_osifp = (osif_dev *)wlan_vdev_get_ospriv(vdev);
	qdf_atomic_init(&vdev_priv->v_son_enabled);
	qdf_atomic_init(&vdev_priv->v_ackrssi_enabled);
	qdf_atomic_init(&vdev_priv->event_bcast_enabled);
	vdev_priv->bestul_hyst = SON_BEST_UL_HYST_DEF;
	vdev_priv->iv_map = 0;
	vdev_priv->iv_mapbh = 0;
	vdev_priv->iv_mapfh = 0;
	vdev_priv->iv_mapbsta = 0;
	vdev_priv->iv_mapvapup = 0;

	if (wlan_objmgr_vdev_component_obj_attach(vdev,
				  WLAN_UMAC_COMP_SON,
				  vdev_priv,
				  QDF_STATUS_SUCCESS) != QDF_STATUS_SUCCESS) {
		SON_LOGF("Failed to attach SON  vdev  ");
		status = QDF_STATUS_E_FAILURE;
		goto attach_failure;
	}
	return status;

attach_failure:
	qdf_mem_free(vdev_priv);
failure:
	return status;

}

/**
 * @brief TBD
 *
 * @param [in]
 * @return non-zero if it is enabled; otherwise 0
 */

static QDF_STATUS  wlan_son_peer_create_handler(struct wlan_objmgr_peer *peer,
						void *arg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct son_peer_priv *peer_priv;

	if (!peer) {
		SON_LOGF("Peer is null Investigate  %s %d ",
			 __func__,__LINE__);
		status = QDF_STATUS_E_INVAL;
		goto failure;
	}

	peer_priv = qdf_mem_malloc(sizeof(struct son_peer_priv));

	if (peer_priv == NULL) {
		SON_LOGF("Insufficient memory can not allocate Son peer");
		status = QDF_STATUS_E_NOMEM;
		goto failure;
	}

	son_clear_whc_rept_info(peer);

	if (wlan_objmgr_peer_component_obj_attach(peer,
				  WLAN_UMAC_COMP_SON,
				  peer_priv,
				  QDF_STATUS_SUCCESS) != QDF_STATUS_SUCCESS) {
		SON_LOGF("Failed to attach SON peer ");
		status = QDF_STATUS_E_FAILURE;
		goto attach_failure;
	}

	peer_priv->assoc_frame = NULL;

	return status;

attach_failure:
	qdf_mem_free(peer_priv);
failure:
	return status;
}

/**
 * @brief
 *
 * @param
 *
 * @return
 */

static QDF_STATUS wlan_son_psoc_delete_handler(struct wlan_objmgr_psoc *psoc,
					       void *arg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct son_psoc_priv *pspriv = NULL;

	pspriv = wlan_son_get_psoc_priv(psoc);

	if (pspriv) {
		if (wlan_objmgr_psoc_component_obj_detach(psoc,
							  WLAN_UMAC_COMP_SON,
							  pspriv)) {
			status = QDF_STATUS_E_FAILURE;
		}
		qdf_mem_free(pspriv);
	}
	return status;
}

/**
 * @brief
 *
 * @param
 *
 * @return
 */

static QDF_STATUS wlan_son_pdev_delete_handler(struct wlan_objmgr_pdev *pdev,
					       void *arg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct son_pdev_priv *pd_priv = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;

	pd_priv = wlan_son_get_pdev_priv(pdev);

	if (pd_priv) {
		psoc = wlan_pdev_get_psoc(pdev);

		if (SON_TARGET_IF_LMAC_DESTROY(pdev))
			SON_TARGET_IF_LMAC_DESTROY(pdev);

		if (wlan_objmgr_pdev_component_obj_detach(pdev,
						WLAN_UMAC_COMP_SON,
						pd_priv)) {
			status = QDF_STATUS_E_FAILURE;
		}
		qdf_spinlock_destroy(&pd_priv->son_lock);
		qdf_timer_stop(&pd_priv->son_chan_util_timer);
		qdf_timer_free(&pd_priv->son_chan_util_timer);
		qdf_timer_stop(&pd_priv->son_inst_rssi_timer);
		qdf_timer_free(&pd_priv->son_inst_rssi_timer);
		qdf_timer_stop(&pd_priv->son_inact_timer);
		qdf_timer_free(&pd_priv->son_inact_timer);

		qdf_mem_free(pd_priv);
	}

	return status;
}

/**
 * @brief
 *
 * @param [in]
 *
 * @return non-zero if it is enabled; otherwise 0
 */

static QDF_STATUS wlan_son_vdev_delete_handler(struct wlan_objmgr_vdev *vdev,
					       void *arg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct son_vdev_priv *vpriv = NULL;
	vpriv = wlan_son_get_vdev_priv(vdev);

	if (vpriv) {
		if (wlan_objmgr_vdev_component_obj_detach(vdev,
							  WLAN_UMAC_COMP_SON,
							  vpriv)) {
			status = QDF_STATUS_E_FAILURE;
		}
		qdf_mem_free(vpriv);
	}

	return status;
}

/**
 * @brief
 *
 * @param [in]
 *
 * @return non-zero if it is enabled; otherwise 0
 */

static QDF_STATUS  wlan_son_peer_delete_handler(struct wlan_objmgr_peer *peer,
						void *arg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct son_peer_priv *pepriv = NULL;
	pepriv = wlan_son_get_peer_priv(peer);

	if (pepriv) {
		if (wlan_objmgr_peer_component_obj_detach(peer,
							  WLAN_UMAC_COMP_SON,
							  pepriv)) {
			status = QDF_STATUS_E_FAILURE;
		}
		if (pepriv->assoc_frame) {
			qdf_nbuf_free(pepriv->assoc_frame);
		}
		qdf_mem_free(pepriv);
	}
	return status;
}

PUBLIC QDF_STATUS wlan_son_psoc_open(struct wlan_objmgr_psoc *psoc)
{
	if ( EOK != son_netlink_attach()) {
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

PUBLIC QDF_STATUS wlan_son_psoc_close(struct wlan_objmgr_psoc *psoc)
{

	if (EOK != son_netlink_destroy()) {
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}
/**
 * @brief
 *
 * @param [in]
 *
 * @return non-zero if it is enabled; otherwise 0
 */

QDF_STATUS wlan_son_init(void)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (wlan_objmgr_register_psoc_create_handler(WLAN_UMAC_COMP_SON,
				     wlan_son_psoc_create_handler,
				     NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
		goto psoc_create_failed;
	}

	if (wlan_objmgr_register_pdev_create_handler(WLAN_UMAC_COMP_SON,
					     wlan_son_pdev_create_handler,
					     NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
		goto pdev_create_failed;
	}

	if (wlan_objmgr_register_vdev_create_handler(WLAN_UMAC_COMP_SON,
					     wlan_son_vdev_create_handler,
					     NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
		goto vdev_create_failed;
	}

	if (wlan_objmgr_register_peer_create_handler(WLAN_UMAC_COMP_SON,
					     wlan_son_peer_create_handler,
					     NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
		goto peer_create_failed;
	}

	if (wlan_objmgr_register_psoc_destroy_handler(WLAN_UMAC_COMP_SON,
					      wlan_son_psoc_delete_handler,
					      NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
		goto psoc_destroy_failed;
	}

	if (wlan_objmgr_register_pdev_destroy_handler(WLAN_UMAC_COMP_SON,
					      wlan_son_pdev_delete_handler,
					      NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
		goto pdev_destroy_failed;
	}

	if (wlan_objmgr_register_vdev_destroy_handler(WLAN_UMAC_COMP_SON,
					      wlan_son_vdev_delete_handler,
					      NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
		goto vdev_destroy_failed;
	}

	if (wlan_objmgr_register_peer_destroy_handler(WLAN_UMAC_COMP_SON,
					      wlan_son_peer_delete_handler,
					      NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
		goto peer_destroy_failed;
	}

	return status;

peer_destroy_failed:
	if(wlan_objmgr_unregister_vdev_destroy_handler(WLAN_UMAC_COMP_SON,
					       wlan_son_vdev_delete_handler,
					       NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
	}
vdev_destroy_failed:
	if(wlan_objmgr_unregister_pdev_destroy_handler(WLAN_UMAC_COMP_SON,
					       wlan_son_pdev_delete_handler,
					       NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
	}

pdev_destroy_failed:
	if(wlan_objmgr_unregister_psoc_destroy_handler(WLAN_UMAC_COMP_SON,
					       wlan_son_psoc_delete_handler,
					       NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
	}
psoc_destroy_failed:
	if(wlan_objmgr_unregister_peer_create_handler(WLAN_UMAC_COMP_SON,
					      wlan_son_peer_create_handler,
					      NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
	}
peer_create_failed:
	if (wlan_objmgr_unregister_vdev_create_handler(WLAN_UMAC_COMP_SON,
					       wlan_son_vdev_create_handler,
					       NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
	}
vdev_create_failed:
	if (wlan_objmgr_unregister_pdev_create_handler(WLAN_UMAC_COMP_SON,
					       wlan_son_pdev_create_handler,
					       NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
	}
pdev_create_failed:
	if (wlan_objmgr_unregister_psoc_create_handler(WLAN_UMAC_COMP_SON,
					       wlan_son_psoc_create_handler,
					       NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
	}

psoc_create_failed:
	return status;
}

/**
 * @brief Remove create handler from global data structure.
 *
 * @param [in] void.
 *
 * @return Return QDF_STATUS_SUCCESS or QDF_STATUS_FAIL based
 * on condition.
 */

QDF_STATUS wlan_son_deinit(void)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;


	if (wlan_objmgr_unregister_psoc_create_handler(WLAN_UMAC_COMP_SON,
					       wlan_son_psoc_create_handler,
					       NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
		goto failure;
	}

	if (wlan_objmgr_unregister_pdev_create_handler(WLAN_UMAC_COMP_SON,
					       wlan_son_pdev_create_handler,
					       NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
		goto failure;
	}

	if (wlan_objmgr_unregister_vdev_create_handler(WLAN_UMAC_COMP_SON,
					       wlan_son_vdev_create_handler,
					       NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
		goto failure;
	}

	if(wlan_objmgr_unregister_peer_create_handler(WLAN_UMAC_COMP_SON,
					      wlan_son_peer_create_handler,
					      NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
		goto failure;
	}

	if(wlan_objmgr_unregister_psoc_destroy_handler(WLAN_UMAC_COMP_SON,
					       wlan_son_psoc_delete_handler,
					       NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
		goto failure;
	}

	if(wlan_objmgr_unregister_pdev_destroy_handler(WLAN_UMAC_COMP_SON,
					       wlan_son_pdev_delete_handler,
					       NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
		goto failure;
	}

	if(wlan_objmgr_unregister_vdev_destroy_handler(WLAN_UMAC_COMP_SON,
					       wlan_son_vdev_delete_handler,
					       NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
		goto failure;
	}

	if(wlan_objmgr_unregister_peer_destroy_handler(WLAN_UMAC_COMP_SON,
					       wlan_son_peer_delete_handler,
					       NULL) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
		goto failure;
	}

failure:
	return status;
}


/**
 * @brief Return psoc priv handler for SON
 *
 * @param [in] psoc.
 *
 * @return Return son psoc priv  or NULL if not found
 */

struct son_pdev_priv *wlan_son_get_pdev_priv(
	struct wlan_objmgr_pdev *pdev)
{
	struct son_pdev_priv *pdev_priv = NULL;

	pdev_priv = (struct son_pdev_priv *)
		wlan_objmgr_pdev_get_comp_private_obj(pdev,
						      WLAN_UMAC_COMP_SON);

	return pdev_priv;
}
/**
 * @brief Return psoc priv handler for SON
 *
 * @param [in] psoc.
 *
 * @return Return son psoc priv  or NULL if not found
 */

inline struct son_psoc_priv *wlan_son_get_psoc_priv(
	struct wlan_objmgr_psoc *psoc)
{
	struct son_psoc_priv *psoc_priv = NULL;

	psoc_priv = (struct son_psoc_priv *)
		wlan_objmgr_psoc_get_comp_private_obj(psoc,
						      WLAN_UMAC_COMP_SON);
	return psoc_priv;

}

/**
 * @brief Return vdev priv handler for SON
 *
 * @param [in] vdev.
 *
 * @return Return son vdevc priv  or NULL if not found
 */

struct son_vdev_priv *wlan_son_get_vdev_priv(
	struct wlan_objmgr_vdev *vdev)
{
	struct son_vdev_priv *vdev_priv = NULL;

	vdev_priv = (struct son_vdev_priv *)
		wlan_objmgr_vdev_get_comp_private_obj(vdev,
						      WLAN_UMAC_COMP_SON);
	return vdev_priv;
}
/**
 * @brief Return vdev priv handler for SON
 *
 * @param [in] vdev.
 *
 * @return Return son vdevc priv  or NULL if not found
 */

struct son_peer_priv *wlan_son_get_peer_priv(
	struct wlan_objmgr_peer *peer)
{
	struct son_peer_priv *peer_priv = NULL;

	peer_priv = (struct son_peer_priv *)
		wlan_objmgr_peer_get_comp_private_obj(peer,
						      WLAN_UMAC_COMP_SON);
	return peer_priv;
}


int son_core_enable_disable_vdev_events(struct wlan_objmgr_vdev *vdev ,
					u_int8_t enable)
{
	struct son_vdev_priv *vpriv = NULL;

	vpriv = wlan_son_get_vdev_priv(vdev);

	if(vpriv) {
		qdf_atomic_set(&vpriv->v_son_enabled, enable);
		return EOK;
	}

	return -EINVAL;
}

/**
 * @brief Reset the utilization measurements for the next round of samples.
 *
 * @pre the caller should be holding the band steering spinlock
 *
 * @param [in] pdev priv object to use for the reset
 */

void son_core_reset_chan_utilization(struct son_pdev_priv *pd_priv)
{
	pd_priv->son_chan_util_samples_sum = 0;
	pd_priv->son_chan_util_num_samples = 0;
	pd_priv->son_chan_util_requested = false;

	return;
}

int son_core_start_stop_timer(struct son_pdev_priv *pd_priv, bool enable)
{
	int retv = EOK;
	ieee80211_bsteering_param_t *params = NULL;

	params = &pd_priv->son_config_params;

	if(!qdf_atomic_read(&pd_priv->son_enabled)) {
		if (enable) {
			if (params->utilization_sample_period) {
				son_core_reset_chan_utilization(pd_priv);
				qdf_timer_start(&pd_priv->son_chan_util_timer,
				params->utilization_sample_period
						* 1000);
			}
			/* Reset the inactivity counters and arm the inactivity timer */
			son_core_inact_enable(pd_priv->pdev, true);
		} else {
			qdf_timer_sync_cancel(&pd_priv->son_chan_util_timer);
			qdf_timer_sync_cancel(&pd_priv->son_inst_rssi_timer);
			/* Disarm the inactivity check timer */
			son_core_inact_enable(pd_priv->pdev, false);
		}
	}

	return retv;
}

int son_core_pdev_enable_ackrssi(struct wlan_objmgr_vdev *vdev,
                                        u_int8_t ackrssi_enable)
{
        struct son_vdev_priv *vpriv = NULL;

        vpriv = wlan_son_get_vdev_priv(vdev);

        if(vpriv) {
                qdf_atomic_set(&vpriv->v_ackrssi_enabled, ackrssi_enable);
                return EOK;
        }

        return -EINVAL;
}

int son_core_pdev_enable_disable_steering(struct wlan_objmgr_vdev *vdev,
					  int enable)
{
	int retv = -EINVAL;
	struct son_pdev_priv *pd_priv = NULL;
	ieee80211_bsteering_param_t params = {0};
	struct wlan_objmgr_pdev *pdev = NULL;

	pdev = wlan_vdev_get_pdev(vdev);

	do {
		pd_priv = wlan_son_get_pdev_priv(pdev);

		if (pd_priv) {
			params = pd_priv->son_config_params;
			if (enable) {
				SON_LOCK(&pd_priv->son_lock);
				if (wlan_son_is_pdev_enabled(pdev)) {
					qdf_atomic_inc(&pd_priv->son_enabled);
					retv = EOK;
					SON_UNLOCK(&pd_priv->son_lock);
					break;
				}
				/* Sanity check to make sure valid config parameters have been set */
				if (!params.inactivity_check_period ||
				    (params.utilization_sample_period &&
				     !params.utilization_average_num_samples)) {
					retv = -EINVAL;
					SON_UNLOCK(&pd_priv->son_lock);
					break;
				}

				retv = son_core_start_stop_timer(pd_priv, true);

				if(retv == EOK) {
					qdf_atomic_inc(&pd_priv->son_enabled);
					/* Remember the VAP for later, as we need one to trigger the
					   channel utilization. */
					pd_priv->son_iv = vdev;
				}
				SON_UNLOCK(&pd_priv->son_lock);
			} else { /* disable */
				/* Ensure it is not a double disable.*/
				SON_LOCK(&pd_priv->son_lock);
				if (!wlan_son_is_pdev_enabled(pdev)) {
					retv = -EALREADY;
					SON_UNLOCK(&pd_priv->son_lock);
					break;
				}

				qdf_atomic_dec(&pd_priv->son_enabled);
				if ((retv = son_core_start_stop_timer(pd_priv,
							      false)) !=EOK) {
					SON_UNLOCK(&pd_priv->son_lock);
					break;
				}
				SON_UNLOCK(&pd_priv->son_lock);
			}
		} else
			SON_LOGF("PDEV IS not initialized %s",__func__);
	} while(0);

	return retv;
}

void son_core_mark_peer_inact(struct wlan_objmgr_peer *peer,
			      bool inactive)
{

	bool inactive_old = false ;
	struct son_peer_priv *pe_priv = NULL;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;

	vdev = wlan_peer_get_vdev(peer);

	pdev = wlan_vdev_get_pdev(vdev);

	pe_priv = wlan_son_get_peer_priv(peer);
	inactive_old = pe_priv->son_inact_flag;

	if (!inactive) {
		pe_priv->son_inact = pe_priv->son_inact_reload;
	}

	pe_priv->son_inact_flag = inactive;

	if (inactive_old != inactive) {
		son_notify_activity_change(vdev, peer->macaddr,
					   !inactive);
	}
	return;
}
/**
 * @brief Get the parameters that control whether debugging events are
 *        generated or not.
 *
 * @param [in] vap  the VAP on which the request came in
 * @param [out] req  the object to update with the current debugging parameters
 *
 * @return EOK for success and EINVAL for error cases
 */

int8_t son_core_get_dbg_params(struct wlan_objmgr_vdev *vdev,
	       struct ieee80211_bsteering_dbg_param_t *bsteering_dbg_param)
{
	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_pdev_priv *pd_priv = NULL;

	pdev = wlan_vdev_get_pdev(vdev);
	pd_priv = wlan_son_get_pdev_priv(pdev);
	SON_LOCK(&pd_priv->son_lock);

	qdf_mem_copy(bsteering_dbg_param , &pd_priv->son_dbg_config_params,
		     sizeof(struct ieee80211_bsteering_dbg_param_t));

	SON_UNLOCK(&pd_priv->son_lock);

	return EOK;
}

int8_t son_core_set_dbg_params(struct wlan_objmgr_vdev *vdev,
	       struct ieee80211_bsteering_dbg_param_t *bsteering_dbg_param)
{
	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_pdev_priv *pdpriv = NULL;

	pdev = wlan_vdev_get_pdev(vdev);
	pdpriv = wlan_son_get_pdev_priv(pdev);
	SON_LOCK(&pdpriv->son_lock);

	qdf_mem_copy(&pdpriv->son_dbg_config_params,
		     bsteering_dbg_param,
		     sizeof(struct ieee80211_bsteering_dbg_param_t));

	SON_UNLOCK(&pdpriv->son_lock);

	return EOK;
}

/**
 * @brief Find a STA entry in probe response allow table
 *
 * @param [in] vap  the VAP on which probe response allow check is done
 * @param [in] mac_addr  the MAC address of the client that sent the probe
 *                       request
 *
 * @return reference to entry if found; otherwise NULL.
 */
ieee80211_bsteering_probe_resp_allow_entry_t*
son_probe_resp_allow_entry_find(struct wlan_objmgr_vdev *vdev,
				const u_int8_t *mac_addr)
{
	int i;
	ieee80211_bsteering_probe_resp_allow_entry_t *entry;
	struct son_vdev_priv *vdpriv = NULL;

	vdpriv = wlan_son_get_vdev_priv(vdev);
	for(i = 0; i < IEEE80211_BS_MAX_STA_WH_24G; i++)
	{
		entry = &vdpriv->iv_bs_prb_resp_allow_table[i];

		if(entry->valid && (WLAN_ADDR_EQ(mac_addr,
						entry->mac_addr) == QDF_STATUS_SUCCESS)) {
			return entry;
		}
	}

	return NULL;
}

static ieee80211_bsteering_probe_resp_allow_entry_t*
son_probe_resp_get_free_allow_entry(struct wlan_objmgr_vdev *vdev)
{
	int i = 0;
	unsigned long jiffies_older = qdf_system_ticks();
	ieee80211_bsteering_probe_resp_allow_entry_t *entry = NULL;
	ieee80211_bsteering_probe_resp_allow_entry_t *older_entry;
	struct son_vdev_priv *vdpriv = NULL;

	vdpriv = wlan_son_get_vdev_priv(vdev);
	for(i = 0; i < IEEE80211_BS_MAX_STA_WH_24G; i++)
	{
		entry = &vdpriv->iv_bs_prb_resp_allow_table[i];
		if(!entry->valid) {
			return entry;
		}
	}

	older_entry = &vdpriv->iv_bs_prb_resp_allow_table[0];
	/* No empty slot, find an older entry */
	for(i = 0; i < IEEE80211_BS_MAX_STA_WH_24G; i++) {
		entry = &vdpriv->iv_bs_prb_resp_allow_table[i];
		if(time_before(entry->create_time_jiffies, jiffies_older))
		{
			older_entry = entry;
			jiffies_older = entry->create_time_jiffies;
		}
	}

	return older_entry;
}

/**
 * @brief Find and remove a STA entry in probe response allow table
 *
 * @param [in] vap  the VAP on which probe response allow entry to be removed
 * @param [in] mac_addr  the MAC address of the client that needs to be removed
 *
 * @return void
 */
static void
son_probe_resp_allow_entry_remove(struct wlan_objmgr_vdev *vdev,
				  const u_int8_t *mac_addr)
{
	ieee80211_bsteering_probe_resp_allow_entry_t *entry = NULL;

	entry = son_probe_resp_allow_entry_find(vdev, mac_addr);

	if(entry)
		entry->valid=0;

	return;
}

/**
 * @brief Find and Add a STA entry in probe response allow table
 *
 * @param [in] vap  the VAP on which probe response to be allowed
 * @param [in] mac_addr  the MAC address of the client that may send probe
 *                       request
 *
 * @return void
 */
static void
son_probe_resp_find_and_add_allow_entry(struct wlan_objmgr_vdev *vdev,
					const u_int8_t *mac_addr)
{
	ieee80211_bsteering_probe_resp_allow_entry_t *entry;

	entry = son_probe_resp_allow_entry_find(vdev, mac_addr);

	if(entry == NULL)
	{
		entry = son_probe_resp_get_free_allow_entry(vdev);
		IEEE80211_ADDR_COPY(entry->mac_addr, mac_addr);
		entry->valid=1;
	}

	entry->create_time_jiffies = qdf_system_ticks();
}

int8_t son_core_peer_set_probe_resp_allow_2g(struct wlan_objmgr_vdev *vdev,
					     char *dstmac,
					     u_int8_t action /*add or remove */)
{
	if (action)
		son_probe_resp_find_and_add_allow_entry(vdev, dstmac);
	else
		son_probe_resp_allow_entry_remove(vdev, dstmac);

	return EOK;
}

/**
 * @brief Reset a STA entry in probe response withheld table
 *
 * @param [in] entry  the STA entry to be reseted
 * @param [in] mac_addr  the MAC address of the client that may send probe
 *                       request
 *
 * @return void
 */

static void son_probe_resp_wh_entry_reset(
	ieee80211_bsteering_probe_resp_wh_entry_t *entry)
{
	entry->valid = 1;
	entry->prb_resp_wh_count = 1;
	entry->initial_prb_req_jiffies = qdf_system_ticks();
}


static bool son_probe_resp_wh_entry_update_and_check(
	struct wlan_objmgr_vdev *vdev,
	ieee80211_bsteering_probe_resp_wh_entry_t *entry)
{
	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_pdev_priv *pdpriv = NULL;
	unsigned long jiffies_now = qdf_system_ticks();

	pdev = wlan_vdev_get_pdev(vdev);

	pdpriv = wlan_son_get_pdev_priv(pdev);

	if(time_before_eq(jiffies_now, entry->initial_prb_req_jiffies +
		((unsigned long)pdpriv->son_config_params.delay_24g_probe_time_window * HZ))) {
		entry->prb_resp_wh_count++;
		if(entry->prb_resp_wh_count > pdpriv->son_config_params.delay_24g_probe_min_req_count)
			return false;
	} else {
		son_probe_resp_wh_entry_reset(entry);
	}

	return true;
}
/**
 * @brief Add a STA entry in probe response withheld table
 *
 * @param [in] entry the STA entry to be updated
 * @param [in] mac_addr  the MAC address of the client that may send probe
 *                       request
 *
 * @return void
 */
static void son_probe_resp_wh_entry_add(
	ieee80211_bsteering_probe_resp_wh_entry_t *entry,
	const u_int8_t *mac_addr)
{
	entry->initial_prb_req_jiffies = qdf_system_ticks();
	entry->prb_resp_wh_count = 1;
	entry->valid = 1;
	IEEE80211_ADDR_COPY(entry->mac_addr, mac_addr);

	return;
}


bool son_core_is_probe_resp_wh_2g(struct wlan_objmgr_vdev *vdev,
				  char *dstmac,
				  u_int8_t sta_rssi)
{
	ieee80211_bsteering_probe_resp_allow_entry_t* allow_entry = NULL;
	struct son_vdev_priv *vd_priv = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_pdev_priv *pd_priv = NULL;
	struct wlan_objmgr_vdev *peer_vdev = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	struct wlan_objmgr_peer *peer = NULL;

	if (!vdev) {
		qdf_print("%s: vdev is NULL!", __func__);
		return false;
	}
	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		qdf_print("%s: pdev is NULL!", __func__);
		return false;
	}
	psoc = wlan_pdev_get_psoc(pdev);
	vd_priv = wlan_son_get_vdev_priv(vdev);
	pd_priv = wlan_son_get_pdev_priv(pdev);

	/* If VAP is not 2.4G or station 2.4G RSSI is less than threshold
	 * or config parameters are not valid, send probe responses */
	if(sta_rssi < pd_priv->son_config_params.delay_24g_probe_rssi_threshold ||
	   pd_priv->son_config_params.delay_24g_probe_time_window == 0 ||
	   pd_priv->son_config_params.delay_24g_probe_min_req_count == 0)
		return false;

	/* no need to take lock , its already taken inside  api */
	peer = wlan_objmgr_get_peer(psoc,
				    wlan_objmgr_pdev_get_pdev_id(pdev),
				    dstmac,
				    WLAN_SON_ID);

	if (peer) {
		peer_vdev = wlan_peer_get_vdev(peer);
		wlan_objmgr_peer_release_ref(peer, WLAN_SON_ID);
		if (peer_vdev == vdev) {
			return false;
		}
	}

	SON_LOCK(&pd_priv->son_lock);
	allow_entry = son_probe_resp_allow_entry_find(vdev, dstmac);
	SON_UNLOCK(&pd_priv->son_lock);

	if(allow_entry != NULL)	{
		/* This client is being steered to 2.4G band, allow probe responses */
		return false;
	}
	else {
		ieee80211_bsteering_probe_resp_wh_entry_t *sta_entry = NULL;
		int i;
		u_int8_t entry_idx = 0;
		bool free_entry_found = false;
		/* Find whether the provided MAC is eligible for probe response in 2.4G band */
		for(i = 0; i < IEEE80211_BS_MAX_STA_WH_24G; i++) {
			sta_entry = &vd_priv->iv_bs_prb_resp_wh_table[i];
			if(sta_entry->valid && (WLAN_ADDR_EQ(dstmac, sta_entry->mac_addr) == QDF_STATUS_SUCCESS))
				return son_probe_resp_wh_entry_update_and_check(vdev, sta_entry);
		}

		/* No entry found, try finding empty slot for this STA */
		for(i = 0; i < IEEE80211_BS_MAX_STA_WH_24G; i++) {
			sta_entry = &vd_priv->iv_bs_prb_resp_wh_table[i];
			if(!sta_entry->valid)
			{
				free_entry_found = true;
				entry_idx = i;
				break;
			}
		}
		/* No empty slot, free an older entry and use it */
		if(!free_entry_found) {
			unsigned long jiffies_older = qdf_system_ticks();

			for(i = 0; i < IEEE80211_BS_MAX_STA_WH_24G; i++)
			{
				sta_entry = &vd_priv->iv_bs_prb_resp_wh_table[i];
				if(time_before(sta_entry->initial_prb_req_jiffies, jiffies_older))
				{
					entry_idx = i;
					jiffies_older = sta_entry->initial_prb_req_jiffies;
					free_entry_found = true;
				}
			}
		}

		if(free_entry_found) {
			son_probe_resp_wh_entry_add(&vd_priv->iv_bs_prb_resp_wh_table[entry_idx], dstmac);
			return true;
		}
	}
	/* It should not land here. Failed to find/get an entry,
	 * send probe response for this STA*/
	return false;
}


int8_t son_core_set_stastats_intvl(struct wlan_objmgr_peer *peer,
				   u_int8_t stastats_invl)
{
	struct son_peer_priv *pe_priv = NULL;
	struct son_pdev_priv *pd_priv = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct wlan_objmgr_vdev *vdev= NULL;

	pe_priv = wlan_son_get_peer_priv(peer);

	vdev = wlan_peer_get_vdev(peer);

	pdev = wlan_vdev_get_pdev(vdev);

	pd_priv = wlan_son_get_pdev_priv(pdev);

	if(!pe_priv)
		return -EINVAL;

	SON_LOCK(&pd_priv->son_lock);
	pd_priv->sta_stats_update_interval_da = stastats_invl;
	SON_UNLOCK(&pd_priv->son_lock);

	return EOK;
}

int8_t son_core_set_steer_in_prog(struct wlan_objmgr_peer *peer,
				  u_int8_t steer_in_prog)
{
	struct son_peer_priv *pe_priv = NULL;
	struct son_pdev_priv *pd_priv = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct wlan_objmgr_vdev *vdev= NULL;

	pe_priv = wlan_son_get_peer_priv(peer);
	vdev = wlan_peer_get_vdev(peer);

	pdev = wlan_vdev_get_pdev(vdev);

	pd_priv = wlan_son_get_pdev_priv(pdev);

	if(!pe_priv)
		return -EINVAL;

	SON_LOCK(&pd_priv->son_lock);
	pe_priv->son_steering_flag = steer_in_prog;
	SON_UNLOCK(&pd_priv->son_lock);

	return EOK;
}

int8_t son_core_set_get_steering_params(struct wlan_objmgr_vdev *vdev,
					ieee80211_bsteering_param_t *bsteering_param,
					bool set)
{
	int retval = -EINVAL;
	u_int8_t index = 0;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_pdev_priv *pd_priv = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;

	pdev = wlan_vdev_get_pdev(vdev);

	psoc = wlan_pdev_get_psoc(pdev);
	pd_priv = wlan_son_get_pdev_priv(pdev);

	if (set == true)
	{
		/* for throughput constraints we are not supporting Sample period less than 30*/
#define MIN_SAMPLE_PERIOD_DIRECT_ATTACH 30
		/* Sanity check the values provided. If any are invalid, error out
		   with EINVAL (per the default retval).*/
		for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
			if ( !bsteering_param->inactivity_check_period ||
			     (bsteering_param->utilization_sample_period &&
			      !bsteering_param->utilization_average_num_samples) ||
			     (bsteering_param->inactivity_timeout_normal[index] <=
			      bsteering_param->inactivity_check_period) ||
			     (bsteering_param->inactivity_timeout_overload <=
			      bsteering_param->inactivity_check_period) ||
			     (bsteering_param->high_tx_rate_crossing_threshold[index] <=
			      bsteering_param->low_tx_rate_crossing_threshold)) {
				return -EINVAL;
			}
		}

		if (bsteering_param->utilization_sample_period) {
			/*Due to Throughput Hit for direct attach hardware
			  we are not permitting less than 30 sec*/
			if(bsteering_param->utilization_sample_period < MIN_SAMPLE_PERIOD_DIRECT_ATTACH) {
				if ((retval = SON_TARGET_IF_CHK_UTIL_INTVL(pdev,
						   &bsteering_param->utilization_sample_period,
						   &bsteering_param->utilization_average_num_samples)) != EOK)
					return retval;
			}
		}

		do {
			SON_LOCK(&pd_priv->son_lock);
			if (wlan_son_is_pdev_enabled(pdev)) {
				retval = -EBUSY;
				break;
			}

			qdf_mem_copy(&pd_priv->son_config_params,
				     bsteering_param, sizeof(ieee80211_bsteering_param_t));

			retval = EOK;
		} while (0);

		if (retval == EOK) {
		    /* Set the inactivity parameters */
		    son_core_set_inact_params(pdev);

		}
		SON_UNLOCK(&pd_priv->son_lock);
	} else {/*get */

		SON_LOCK(&pd_priv->son_lock);
		qdf_mem_copy(bsteering_param ,&pd_priv->son_config_params,
			     sizeof(ieee80211_bsteering_param_t)) ;
		SON_UNLOCK(&pd_priv->son_lock);
		retval = EOK;
	}

#undef MIN_SAMPLE_PERIOD_DIRECT_ATTACH
	return retval;

}

int8_t son_core_set_get_overload(struct wlan_objmgr_vdev *vdev,
				 u_int8_t *bsteering_overload,
				 bool set)
{
	struct son_pdev_priv *pd_priv = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;

	pdev = wlan_vdev_get_pdev(vdev);
	pd_priv = wlan_son_get_pdev_priv(pdev);
	psoc = wlan_pdev_get_psoc(pdev);

	if (set) {
		SON_LOCK(&pd_priv->son_lock);
		pd_priv->son_vap_overload = *bsteering_overload ? true : false;
		son_core_set_overload(pdev, pd_priv->son_vap_overload);
		SON_UNLOCK(&pd_priv->son_lock);
	} else {
		SON_LOCK(&pd_priv->son_lock);
		*bsteering_overload = pd_priv->son_vap_overload ? 1 : 0;
		SON_UNLOCK(&pd_priv->son_lock);
	}

	return EOK;
}

int8_t son_core_set_get_peer_class_group(struct wlan_objmgr_vdev *vdev,
                 char *macaddr,
				 u_int8_t *peer_class_group,
				 bool set)
{
	int retv = -EINVAL;
	struct wlan_objmgr_peer *peer = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	struct son_peer_priv *pe_priv = NULL;

	pdev = wlan_vdev_get_pdev(vdev);
	psoc = wlan_pdev_get_psoc(pdev);
	peer = wlan_objmgr_get_peer(psoc, wlan_objmgr_pdev_get_pdev_id(pdev), macaddr, WLAN_SON_ID);

	if (!peer) {
		return retv;
	}

	if (wlan_peer_get_vdev(peer) != vdev) {
		wlan_objmgr_peer_release_ref(peer, WLAN_SON_ID);
		return retv;
	}

	pe_priv = wlan_son_get_peer_priv(peer);
	if (set) {
		pe_priv->son_peer_class_group = *peer_class_group;
	} else {
		*peer_class_group = pe_priv->son_peer_class_group;
	}

	wlan_objmgr_peer_release_ref(peer, WLAN_SON_ID);

	return EOK;
}

int son_core_null_frame_tx(struct wlan_objmgr_vdev *vdev, char *dstmac,
			   u_int8_t rssi_num_samples)
{
	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_pdev_priv *pd_priv = NULL;
	struct wlan_objmgr_peer *peer = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	int retval = EOK;

	pdev = wlan_vdev_get_pdev(vdev);

	psoc = wlan_pdev_get_psoc(pdev);

	peer = wlan_objmgr_get_peer(psoc,
				    wlan_objmgr_pdev_get_pdev_id(pdev),
				    dstmac,
				    WLAN_SON_ID);

	pd_priv = wlan_son_get_pdev_priv(pdev);

	if (!peer) {
		SON_LOGW("%s: Requested STA %02x:%02x:%02x:%02x:%02x:%02x is not "
			 "associated", __func__, dstmac[0], dstmac[1], dstmac[2],
			 dstmac[3],dstmac[4], dstmac[5]);
		return -EINVAL;
	}

	SON_LOCK(&pd_priv->son_lock);
	do{
		if (!wlan_son_is_pdev_enabled(pdev)) {
			SON_LOGW("%s: Band steering is not enabled when measuring RSSI for "
				 "STA %02x:%02x:%02x:%02x:%02x:%02x\n",
				 __func__, dstmac[0], dstmac[1], dstmac[2],
				 dstmac[3], dstmac[4], dstmac[5]);
			retval = -EINVAL;
			break;
		}

		if (pd_priv->son_inst_rssi_inprogress) {
			SON_LOGW("%s: Ignore RSSI measurement request for STA "
				 "%02x:%02x:%02x:%02x:%02x:%02x, since another "
				 "RSSI measurement is in progress (count=%u) for "
				 "STA %02x:%02x:%02x:%02x:%02x:%02x\n",
				 __func__, dstmac[0], dstmac[1], dstmac[2],
				 dstmac[3], dstmac[4], dstmac[5],
				 pd_priv->son_inst_rssi_count,
				 pd_priv->son_inst_rssi_macaddr[0],
				 pd_priv->son_inst_rssi_macaddr[1],
				 pd_priv->son_inst_rssi_macaddr[2],
				 pd_priv->son_inst_rssi_macaddr[3],
				 pd_priv->son_inst_rssi_macaddr[4],
				 pd_priv->son_inst_rssi_macaddr[5]);
			retval = -EINPROGRESS;
			break;
		}

		pd_priv->son_inst_rssi_num_samples = rssi_num_samples;
		pd_priv->son_inst_rssi_count = 0;
		pd_priv->son_inst_rssi_err_count = 0;
		WLAN_ADDR_COPY(pd_priv->son_inst_rssi_macaddr, dstmac);
		qdf_timer_start(&pd_priv->son_inst_rssi_timer,INST_RSSI_TIMEOUT * 1000);
		pd_priv->son_inst_rssi_inprogress = true;
		SON_TARGET_IF_TX_NULL(pdev, dstmac, vdev);
	} while(0);

	SON_UNLOCK(&pd_priv->son_lock);
	wlan_objmgr_peer_release_ref(peer, WLAN_SON_ID);

	return retval;
}



int8_t son_core_get_scaling_factor(struct wlan_objmgr_vdev *vdev)
{
	struct son_vdev_priv *vdev_priv = NULL;
	vdev_priv = wlan_son_get_vdev_priv(vdev);
	return (vdev_priv->iv_whc_scaling_factor);
}

int8_t son_core_get_skip_hyst(struct wlan_objmgr_vdev *vdev)
{
	struct son_vdev_priv *vdev_priv = NULL;
	vdev_priv = wlan_son_get_vdev_priv(vdev);
	return (vdev_priv->iv_whc_skip_hyst);
}


static void son_iterate_to_reset_vdev(struct wlan_objmgr_pdev *pdev,
				      void *obj,
				      void *arg)
{
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)obj;

	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE)
		son_vdev_fext_capablity(vdev,
					SON_CAP_SET,
					WLAN_VDEV_FEXT_SON_INFO_UPDATE);
	return;
}

void son_core_set_cap_rssi(struct wlan_objmgr_vdev *vdev,
			   int8_t rssi)
{
	struct son_vdev_priv *vdev_priv = NULL;
	vdev_priv = wlan_son_get_vdev_priv(vdev);
	vdev_priv->cap_rssi = rssi;
	return;
}
static QDF_STATUS
son_core_get_se_snr(void *arg, wlan_scan_entry_t se)
{
	struct ieee80211_uplinkinfo *rootinfo = (struct ieee80211_uplinkinfo *)arg;
	u_int8_t se_ssid_len;
	u_int8_t *se_ssid = util_scan_entry_ssid(se)->ssid;
	struct ieee80211_ie_whc_apinfo *se_sonadv = NULL;
	u_int8_t se_rootdistance, se_isrootap;
		/* make sure ssid is same */
	se_ssid_len = util_scan_entry_ssid(se)->length;

	if (se_ssid_len == 0 || (se_ssid_len != strlen(rootinfo->essid)) || qdf_mem_cmp(se_ssid, rootinfo->essid, se_ssid_len) != 0)
		return 0;

	se_sonadv = (struct ieee80211_ie_whc_apinfo *)util_scan_entry_sonie(se);

	if (se_sonadv == NULL)
		return 0;

	se_rootdistance = se_sonadv->whc_apinfo_root_ap_dist;
	se_isrootap = se_sonadv->whc_apinfo_is_root_ap;

	if(se_rootdistance == 0 && se_isrootap)
		rootinfo->snr = util_scan_entry_rssi(se);

	return 0;
}

int son_core_get_cap_snr(struct wlan_objmgr_vdev *vdev, int *cap_snr)
{
	struct ieee80211_uplinkinfo rootinfo;
	uint8_t ssid[WLAN_SSID_MAX_LEN + 1] = {0};
	u_int8_t len = 0;

	wlan_vdev_mlme_get_ssid(vdev ,ssid, &len);
	qdf_mem_copy(&rootinfo.essid, ssid, len);

	if (ucfg_scan_db_iterate(wlan_vdev_get_pdev(vdev),
				 son_core_get_se_snr, &rootinfo) == 0) {
		*cap_snr = rootinfo.snr;
	}

	return 0;
}

int8_t son_core_get_cap_rssi(struct wlan_objmgr_vdev *vdev)
{
	struct son_vdev_priv *vdev_priv = NULL;
	vdev_priv = wlan_son_get_vdev_priv(vdev);
	return vdev_priv->cap_rssi;
}

/**
 * @brief Set the RSSI threshold and hysteresis value to
 *        trigger event to upper layer when STA RSSI varies
 *
 * @param vdev
 * @param rssi threshold
 * @param rssi hysteresis
 *
 * @return EOK on success
 */
int son_core_set_map_rssi_policy(struct wlan_objmgr_vdev *vdev,
				 u_int8_t rssi_threshold, u_int8_t rssi_hysteresis)
{
	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_pdev_priv *pd_priv = NULL;

	pdev = wlan_vdev_get_pdev(vdev);
	pd_priv = wlan_son_get_pdev_priv(pdev);

	if(!pd_priv)
		return -EINVAL;

	SON_LOCK(&pd_priv->son_lock);
	pd_priv->map_config_params.rssi_threshold = rssi_threshold;
	pd_priv->map_config_params.rssi_hysteresis = rssi_hysteresis;
	SON_UNLOCK(&pd_priv->son_lock);
	return EOK;
}

void son_core_set_uplink_rate(struct wlan_objmgr_vdev *vdev,
			      u_int32_t uplink_rate)
{
	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_pdev_priv *pd_priv = NULL;
	pdev = wlan_vdev_get_pdev(vdev);

	pd_priv = wlan_son_get_pdev_priv(pdev);
	pd_priv->uplink_rate = uplink_rate;

	wlan_objmgr_pdev_iterate_obj_list(pdev,
					  WLAN_VDEV_OP,
					  son_iterate_to_reset_vdev,
					  NULL, 0,
					  WLAN_SON_ID);

	return;
}

u_int16_t son_core_get_uplink_rate(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_pdev_priv *pd_priv = NULL;

	pdev = wlan_vdev_get_pdev(vdev);
	pd_priv = wlan_son_get_pdev_priv(pdev);

	return pd_priv->uplink_rate;
}

u_int8_t son_core_get_uplink_snr(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_pdev_priv *pd_priv = NULL;

	pdev = wlan_vdev_get_pdev(vdev);
	pd_priv = wlan_son_get_pdev_priv(pdev);

	return pd_priv->uplink_snr;
}

void son_core_set_scaling_factor(struct wlan_objmgr_vdev *vdev,
				 int8_t scaling_factor)
{
	struct son_vdev_priv *vdev_priv = NULL;

	vdev_priv = wlan_son_get_vdev_priv(vdev);
	vdev_priv->iv_whc_scaling_factor = scaling_factor;
	return;
}

void son_core_set_skip_hyst(struct wlan_objmgr_vdev *vdev,
				 int8_t skip_hyst)
{
	struct son_vdev_priv *vdev_priv = NULL;

	vdev_priv = wlan_son_get_vdev_priv(vdev);
	vdev_priv->iv_whc_skip_hyst = skip_hyst;
	return;
}

int8_t son_core_get_root_dist(struct wlan_objmgr_vdev *vdev)
{

	struct son_vdev_priv *vpriv = NULL;

	vpriv = wlan_son_get_vdev_priv(vdev);

	return(vpriv->whc_root_ap_distance);
}

int8_t son_core_set_root_dist(struct wlan_objmgr_vdev *vdev,
			      u_int8_t root_distance)
{

	struct son_vdev_priv *vpriv = NULL;

	vpriv = wlan_son_get_vdev_priv(vdev);
	vpriv->whc_root_ap_distance = root_distance;

	return EOK;
}

int son_core_get_best_otherband_uplink_bssid(struct wlan_objmgr_pdev *pdev, char *bssid)
{
	struct son_pdev_priv *pd_priv = NULL;

	pd_priv = wlan_son_get_pdev_priv(pdev);
	qdf_mem_copy(bssid, &pd_priv->son_otherband_uplink_bssid[0],IEEE80211_ADDR_LEN);

	return EOK;
}

int son_core_set_best_otherband_uplink_bssid(struct wlan_objmgr_vdev *vdev, char *bssid)
{
	struct son_pdev_priv *pd_priv = NULL;
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);

	pd_priv = wlan_son_get_pdev_priv(pdev);
	qdf_mem_copy(&pd_priv->son_otherband_uplink_bssid[0], bssid, IEEE80211_ADDR_LEN);

	return EOK;
}

int8_t son_core_set_otherband_bssid(struct wlan_objmgr_pdev *pdev, int *val)
{

	struct son_pdev_priv *pd_priv = NULL;
	u_int8_t *bssid = NULL;

	pd_priv = wlan_son_get_pdev_priv(pdev);
	bssid = &pd_priv->son_otherband_bssid[0];
	bssid[0] = (uint8_t)((val[0] & 0xff000000) >> 24);
	bssid[1] = (uint8_t)((val[0] & 0x00ff0000) >> 16);
	bssid[2] = (uint8_t)((val[0] & 0x0000ff00) >> 8);
	bssid[3] = (uint8_t)((val[0] & 0x000000ff) >> 0);
	bssid[4] = (uint8_t)((val[1] & 0x0000ff00) >> 8);
	bssid[5] = (uint8_t)((val[1] & 0x000000ff) >> 0);

	SON_LOGI("Set Otherband BSSID %x:%x:%x:%x:%x:%x\n",bssid[0],bssid[1],\
		 bssid[2],bssid[3],bssid[4],bssid[5]);

	return EOK;
}

/* Inline functions */
wlan_phymode_e convert_phymode(struct wlan_objmgr_vdev *vdev)
{
	wlan_chan_t c = wlan_vdev_get_channel(vdev);

	if ((c == NULL) || (c == IEEE80211_CHAN_ANYC)) {
		return wlan_phymode_invalid;
	}

	if (IEEE80211_IS_CHAN_108G(c) ||
	    IEEE80211_IS_CHAN_108A(c) ||
	    IEEE80211_IS_CHAN_TURBO(c) ||
	    IEEE80211_IS_CHAN_ANYG(c) ||
	    IEEE80211_IS_CHAN_A(c) ||
	    IEEE80211_IS_CHAN_B(c))
		return wlan_phymode_basic;
	else if (IEEE80211_IS_CHAN_11NG(c) || IEEE80211_IS_CHAN_11NA(c))
		return wlan_phymode_ht;
	else if (IEEE80211_IS_CHAN_11AC(c))
		return wlan_phymode_vht;
	else if (IEEE80211_IS_CHAN_11AX(c))
		return wlan_phymode_he;
	else
		return wlan_phymode_invalid;
}

u_int8_t son_core_get_bestul_hyst(struct wlan_objmgr_vdev *vdev)
{
	struct son_vdev_priv *vdev_priv = NULL;
	vdev_priv = wlan_son_get_vdev_priv(vdev);
	return vdev_priv->bestul_hyst;
}

void son_core_set_bestul_hyst(struct wlan_objmgr_vdev *vdev, u_int8_t hyst)
{
	struct son_vdev_priv *vdev_priv = NULL;
	vdev_priv = wlan_son_get_vdev_priv(vdev);
	vdev_priv->bestul_hyst = hyst;
}

/**
 * @brief to get in network information
 *
 * @return EOK in case of success
 */
int8_t son_core_get_innetwork_2g_mac(struct wlan_objmgr_vdev *vdev, void *data,
                                     int32_t num_entries, int8_t ch) {
	return son_get_innetwork_table(vdev, data, &num_entries, ch);
}

/**
 * @brief to set in network information
 *
 * @return EOK in case of success
 */
int8_t son_core_set_innetwork_2g_mac(struct wlan_objmgr_vdev *vdev, const u_int8_t *macaddr,
                                     int channel) {
	struct son_pdev_priv *pd_priv = NULL;
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct in_network_table *tmpnode;
	wlan_chan_t c = wlan_vdev_get_channel(vdev);

	pd_priv = wlan_son_get_pdev_priv(pdev);

	if (c == NULL) {
		SON_LOGI("could not resolve channel from vdev \r\n");
		return QDF_STATUS_E_INVAL;
	}
	if (IEEE80211_IS_CHAN_5GHZ(c)) {
		SON_LOGI("channel is not 2.4G return failed \r\n");
		return QDF_STATUS_E_INVAL;
	}

	if (channel == 0x00)  // delete all list
	{
		tmpnode = TAILQ_FIRST(&pd_priv->in_network_table_2g);
		while (tmpnode) {
			TAILQ_REMOVE(&pd_priv->in_network_table_2g, tmpnode, table_list);
			OS_FREE(tmpnode);
			tmpnode = TAILQ_FIRST(&pd_priv->in_network_table_2g);
		}
	} else {
		if (TAILQ_EMPTY(&pd_priv->in_network_table_2g)) {
			tmpnode = qdf_mem_malloc(sizeof(struct in_network_table));
			if (tmpnode == NULL) {
				SON_LOGF("Insufficient memory can not allocate in network table");
				return QDF_STATUS_E_NOMEM;
			}
			OS_MEMCPY(tmpnode->macaddr, macaddr, IEEE80211_ADDR_LEN);
			tmpnode->channel = channel;
			TAILQ_INSERT_TAIL(&pd_priv->in_network_table_2g, tmpnode, table_list);
		} else {
			tmpnode = NULL;
			TAILQ_FOREACH(tmpnode, &pd_priv->in_network_table_2g, table_list) {
				if (OS_MEMCMP(tmpnode->macaddr, macaddr, IEEE80211_ADDR_LEN) == 0) {
					tmpnode->channel = channel;
					break;
				}
			}
			if (tmpnode == NULL) {
				tmpnode = qdf_mem_malloc(sizeof(struct in_network_table));
				if (tmpnode == NULL) {
					SON_LOGF("Insufficient memory can not allocate in network table");
					return QDF_STATUS_E_NOMEM;
				}
				OS_MEMCPY(tmpnode->macaddr, macaddr, IEEE80211_ADDR_LEN);
				tmpnode->channel = channel;
				TAILQ_INSERT_TAIL(&pd_priv->in_network_table_2g, tmpnode, table_list);
			}
		}
	}
	return 0;
}

/**
 * @brief Get assoc frame for MAP use
 *
 * @param vdev object manager
 * @param macaddr
 * @param info pointer to store assoc frame
 * @param length of the assoc frame
 *
 * @return 0 on success
 *         0x02 if client is not connected
 *         0x01 in case memeory is not enough to copy
 */

int son_core_get_assoc_frame(struct wlan_objmgr_vdev *vdev, char *macaddr,
			     char *info, u_int16_t *length)
{
#define MAP_UNSPECIFIED_FAILURE 0x01
#define MAP_UNASSOCIATED 0x02
#define MAP_SUCCESS 0x00

	struct wlan_objmgr_peer *peer = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	struct son_peer_priv *pe_priv = NULL;

	pdev = wlan_vdev_get_pdev(vdev);
	psoc = wlan_pdev_get_psoc(pdev);
	peer = wlan_objmgr_get_peer(psoc, wlan_objmgr_pdev_get_pdev_id(pdev), macaddr, WLAN_SON_ID);

	if (!peer) return MAP_UNASSOCIATED;

	if (wlan_peer_get_vdev(peer) != vdev) {
		wlan_objmgr_peer_release_ref(peer, WLAN_SON_ID);
		return MAP_UNSPECIFIED_FAILURE;
	}

	pe_priv = wlan_son_get_peer_priv(peer);
	if (pe_priv->assoc_frame) {
		if (wbuf_get_pktlen(pe_priv->assoc_frame) <= *length) {
			memcpy(info, wbuf_header(pe_priv->assoc_frame), wbuf_get_pktlen(pe_priv->assoc_frame));
			*length = wbuf_get_pktlen(pe_priv->assoc_frame);
	                wlan_objmgr_peer_release_ref(peer, WLAN_SON_ID);
			return MAP_SUCCESS;
		}
	}

	*length = 0;
	wlan_objmgr_peer_release_ref(peer, WLAN_SON_ID);

	return MAP_UNSPECIFIED_FAILURE;

#undef MAP_UNASSOCIATED
#undef MAP_UNSPECIFIED_FAILURE
#undef MAP_SUCCESS
}

/**
 * @brief Get ESP Info required for MAP
 *
 * @param [in] vdev  the VAP on which the change occurred
 * @param [in] map_esp_info struct to fill info
 *
 * @return EOK on success
 */

int son_core_get_map_esp_info(struct wlan_objmgr_vdev *vdev, map_esp_info_t *map_esp_info)
{
	struct wlan_objmgr_pdev *pdev = NULL;
	pdev = wlan_vdev_get_pdev(vdev);

	if(!pdev)
		return -EINVAL;

	map_esp_info->esp_info[map_service_ac_be].include_esp_info = 0x1;
	/* Access Category - BE */
	map_esp_info->esp_info[map_service_ac_be].ac = map_service_ac_be;
	/* Data Format - AMPDU and AMSDU enabled */
	map_esp_info->esp_info[map_service_ac_be].data_format = map_amsdu_ampdu_aggregation_enabled;

	return wlan_pdev_get_esp_info(pdev, map_esp_info);
}

/**
 * @brief get operable channel for MAP
 * @param vdev handle
 * @param map_op_chan pointer to store operable channels for MAP
 * @return EOK
 */

int son_core_get_map_operable_channels(struct wlan_objmgr_vdev *vdev, struct map_op_chan_t *map_op_chan)
{
	struct wlan_objmgr_pdev *pdev = NULL;

	if(!vdev)
		return -EINVAL;

	pdev = wlan_vdev_get_pdev(vdev);
	if(!pdev)
		return -EINVAL;

	return wlan_pdev_get_multi_ap_opclass(pdev, NULL, map_op_chan);
}

/**
 * @brief get ap capabilities
 * @param vdev handle
 * @param *apcap pointer to store ap capabilities for MAP
 * @return EOK
 */

int son_core_get_map_apcap(struct wlan_objmgr_vdev *vdev, mapapcap_t *apcap)
{
	struct wlan_objmgr_pdev *pdev = NULL;

	if(!vdev)
		return -EINVAL;

	pdev = wlan_vdev_get_pdev(vdev);
	if(!pdev)
		return -EINVAL;

	apcap->hwcap.num_supported_op_classes = wlan_pdev_get_multi_ap_opclass(pdev, apcap, NULL);

	return wlan_vdev_get_apcap(vdev, apcap);
}

/**
 * @brief peer iterate logic for reset inactivity count.
 * @param [in] pdev under execution
 * @param [in]
 * @param [in]
 * @return EOK
 */

static void son_core_iterate_peer_reset_inactivity_count(struct wlan_objmgr_pdev *pdev,
						    void *obj,
						    void *arg)
{
	struct wlan_objmgr_peer *peer = (struct wlan_objmgr_peer *)obj;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct son_peer_priv *pe_priv = NULL;
	struct son_pdev_priv *pd_priv = NULL;

	vdev = wlan_peer_get_vdev(peer);

	pe_priv = wlan_son_get_peer_priv(peer);
	pd_priv = wlan_son_get_pdev_priv(pdev);

	if (!(peer->peer_mlme.peer_flags & WLAN_PEER_F_AUTH) || /*check who will set this flag */
	    !wlan_son_is_vdev_enabled(vdev)) {
		/* Inactivity check only interested in connected node */
		return;
	}
	pe_priv->son_inact = pe_priv->son_inact_reload;
	return;
}

static bool son_core_inact_enable(struct wlan_objmgr_pdev *pdev, bool enable)
{
	struct son_pdev_priv *pd_priv = NULL;

	if(!pdev)
		return false;

	pd_priv = wlan_son_get_pdev_priv(pdev);

	if(enable) {
		wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_PEER_OP,
						  son_core_iterate_peer_reset_inactivity_count,
						  (void *)pd_priv, 0,
						  WLAN_SON_ID);
		/* Start inactivity timer */
		qdf_timer_start(&pd_priv->son_inact_timer,
				pd_priv->son_config_params.inactivity_check_period * 1000);

	} else {
		qdf_timer_stop(&pd_priv->son_inact_timer);
	}

	return EOK;
}

static void son_core_iterate_peer_update_inact_threshold(struct wlan_objmgr_pdev *pdev,
						    void *obj,
						    void *arg)
{
	struct wlan_objmgr_peer *peer = (struct wlan_objmgr_peer *)obj;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct son_peer_priv *pe_priv = NULL;
	struct son_pdev_priv *pd_priv = NULL;

	vdev = wlan_peer_get_vdev(peer);

	pd_priv = wlan_son_get_pdev_priv(pdev);
	pe_priv = wlan_son_get_peer_priv(peer);

	if (!wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_AUTH) || /*check who will set this flag */
		!wlan_son_is_vdev_enabled(vdev)) {
		/* Inactivity check only interested in connected node */
		return;
	}

	if (wlan_son_is_pdev_enabled(pdev)) {
		if (pe_priv->son_inact_reload - pe_priv->son_inact >= pd_priv->son_inact[pe_priv->son_peer_class_group]) {
			son_mark_node_inact(peer, true /* inactive */);
			pe_priv->son_inact = 0;
		} else {
			pe_priv->son_inact = pd_priv->son_inact[pe_priv->son_peer_class_group] -
				(pe_priv->son_inact_reload - pe_priv->son_inact);
		}

		pe_priv->son_inact_reload = pd_priv->son_inact[pe_priv->son_peer_class_group];
	}

	return;
}

/**
 * @brief Update the VAP's inactivity threshold. It will also update the
 *        inactivity count of all nodes associated with AP VAPs on this radio
 *        where band steering is enabled.
 *
 * When this is done during bsteering is enabled, it will check the remaining
 * inactivity count, and generate an event if it has been inactive longer than
 * the new threshold. When bsteering is not enabled, it will just update
 * threshold.
 *
 * @pre vap is valid and the band steering handle has already been validated
 *
 * @param [inout] vap  the VAP to be updated
 * @param [in] new_threshold  the new inactivity threshold
 */


static bool son_core_update_inact_threshold(struct wlan_objmgr_pdev *pdev,
				     u_int16_t *new_threshold)
{
	struct son_pdev_priv *pd_priv = NULL;
	u_int16_t old_threshold = 0;
	u_int8_t index = 0;

	if( !pdev)
		return false;

	pd_priv = wlan_son_get_pdev_priv(pdev);

	for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
		old_threshold = pd_priv->son_inact[index];

		if(old_threshold == new_threshold[index])
			return false;
	}

	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_PEER_OP,
					  son_core_iterate_peer_update_inact_threshold,
					  (void *)pd_priv, 0,
					  WLAN_SON_ID);

	return true;
}

bool son_core_is_peer_inact(struct wlan_objmgr_peer *peer)
{
	struct son_peer_priv *pe_priv = NULL;

	pe_priv = wlan_son_get_peer_priv(peer);
	return pe_priv->son_inact_flag;
}

/* Function pointer to set overload status */
static void son_core_set_overload (struct wlan_objmgr_pdev *pdev, bool overload)
{
	struct son_pdev_priv *pd_priv = NULL;

	pd_priv = wlan_son_get_pdev_priv(pdev);

	son_core_update_inact_threshold(pdev, pd_priv->son_vap_overload ?
					pd_priv->son_inact_overload :
					pd_priv->son_inact);

	/* overload is unused to silence compiler */
	overload = overload ;
	return;

}

/* Function pointer to set band steering parameters */
static bool son_core_set_inact_params(struct wlan_objmgr_pdev *pdev)
{
	struct son_pdev_priv *pd_priv = NULL;
	u_int8_t index = 0;
	u_int16_t  son_inact_overload = 0;

	pd_priv = wlan_son_get_pdev_priv(pdev);

	son_inact_overload = (pd_priv->son_config_params.inactivity_timeout_overload /
								   pd_priv->son_config_params.inactivity_check_period);

	for (index = 0; index < BSTEERING_MAX_CLIENT_CLASS_GROUP; index++) {
		pd_priv->son_inact[index] = (pd_priv->son_config_params.inactivity_timeout_normal[index] /
							  		 pd_priv->son_config_params.inactivity_check_period);
		pd_priv->son_inact_overload[index] = son_inact_overload;
	}

	son_core_update_inact_threshold(pdev, pd_priv->son_inact);

	return EOK;
}

#endif /* QCA_SUPPORT_SON */

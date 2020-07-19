/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

/**
 *  DOC:    wlan_offchan_txrx_main.c
 *  This file contains offchan txrx private API definitions for
 *  offchan txrx component.
 */

#include <qdf_status.h>
#include <qdf_nbuf.h>
#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_cmn.h>
#include <wlan_scan_ucfg_api.h>
#include <wlan_mgmt_txrx_utils_api.h>
#include <wlan_objmgr_peer_obj.h>
#include <wlan_offchan_txrx_api.h>
#include "wlan_offchan_txrx_main.h"
#include <ieee80211_defines.h>
#include <ieee80211_objmgr.h>
#include <ieee80211_api.h>
#include <ieee80211_var.h>
#include <wlan_mlme_dp_dispatcher.h>

static inline bool offchan_send_list(struct wlan_objmgr_vdev *vdev,
				struct offchan_txrx_pdev_obj *offchan_obj,
				qdf_nbuf_queue_t *offchan_list);

static inline void offchan_list_drain(struct wlan_objmgr_vdev *vdev,
				qdf_nbuf_queue_t *offchan_list,
				struct offchan_tx_status *status,
				bool send_nbuf)
{
	qdf_nbuf_t nbuf;

	while (!qdf_nbuf_is_queue_empty(offchan_list)) {
		nbuf = qdf_nbuf_queue_remove(offchan_list);
		status->status[status->count] = OFFCHAN_TX_STATUS_DROPPED;
		if (send_nbuf && nbuf)
			qdf_nbuf_queue_add(&status->offchan_txcomp_list, nbuf);

		status->id[status->count] = status->count;
		status->count++;
		if (!send_nbuf && nbuf)
			wbuf_free(nbuf);
	}
}

static inline void
offchan_stats_update(struct offchan_txrx_pdev_obj *offchan_obj)
{
	struct offchan_evt_tstamp *evt_tstamp;
	struct offchan_stats *stats;

	if (offchan_obj == NULL) {
		offchan_err("Offchan pdev object is NULL\n");
		return;
	}

	offchan_debug("Offchan stats update\n");
	evt_tstamp = &offchan_obj->evt_tstamp;
	stats = &offchan_obj->stats;
	stats->dwell_time = (evt_tstamp->scan_foreign_exit -
			     evt_tstamp->scan_foreign_entry);
	stats->chanswitch_time_htof = (evt_tstamp->scan_foreign_entry -
				       evt_tstamp->scan_start);
	stats->chanswitch_time_ftoh = (evt_tstamp->scan_completed -
				       evt_tstamp->scan_foreign_exit);

	return;
}

static inline void offchan_check_and_send_completion(
		struct wlan_objmgr_vdev *vdev,
		struct offchan_txrx_pdev_obj *offchan_obj)
{
	offchan_tx_complete tx_comp;
	struct offchan_tx_req *cur_req;
	struct offchan_tx_status status;
	struct offchan_stats stats;

	/* Called with offchan_lock held and release inside this function */
	if ((qdf_atomic_read(&offchan_obj->offchan_scan_complete) == 1) &&
		(qdf_atomic_read(&offchan_obj->pending_tx_completion) == 0)) {
		cur_req = &offchan_obj->current_req;
		qdf_atomic_set(&offchan_obj->request_in_progress, 0);
		offchan_list_drain(vdev, &cur_req->offchan_tx_list,
					&offchan_obj->status,
					cur_req->req_nbuf_ontx_comp);
		offchan_stats_update(offchan_obj);

		qdf_mem_copy(&status, &offchan_obj->status,
					sizeof(offchan_obj->status));
		qdf_mem_zero(&offchan_obj->status, sizeof(offchan_obj->status));

		qdf_mem_copy(&stats, &offchan_obj->stats,
					sizeof(offchan_obj->stats));
		qdf_mem_zero(&offchan_obj->stats, sizeof(offchan_obj->stats));

		tx_comp = cur_req->tx_comp;
		qdf_spin_unlock_bh(&offchan_obj->offchan_lock);

		if (tx_comp)
			tx_comp(vdev, &status, &stats);
		return;
	}
	qdf_spin_unlock_bh(&offchan_obj->offchan_lock);
}

static inline QDF_STATUS offchan_tx_scan_cancel(struct wlan_objmgr_vdev *vdev,
		struct offchan_txrx_pdev_obj *offchan_pdev_obj)
{
	struct scan_cancel_request *req;
	QDF_STATUS status;

	offchan_debug("Request to cancel offchan tx\n");

	req = qdf_mem_malloc(sizeof(struct scan_cancel_request));
	if (req == NULL)
		return QDF_STATUS_E_NOMEM;


	req->vdev = vdev;
	req->cancel_req.requester = offchan_pdev_obj->requestor_id;
	req->cancel_req.scan_id = offchan_pdev_obj->scan_id;
	req->cancel_req.req_type = WLAN_SCAN_CANCEL_SINGLE;
	req->cancel_req.vdev_id = wlan_vdev_get_id(vdev);
	req->cancel_req.pdev_id =
		wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(vdev));
	/* Request scan manager to wait for target events during scan cancel */
	req->wait_tgt_cancel = true;

	wlan_objmgr_vdev_get_ref(vdev, WLAN_OFFCHAN_TXRX_ID);
	status = ucfg_scan_cancel(req);
	if (QDF_IS_STATUS_ERROR(status)) {
		offchan_err("Cancel scan request failed");
	}
	wlan_objmgr_vdev_release_ref(vdev, WLAN_OFFCHAN_TXRX_ID);

	return status;
}

static inline void offchan_data_tx_check_and_truncate_dwell(
			struct wlan_objmgr_vdev *vdev,
			struct offchan_txrx_pdev_obj *offchan_obj)
{
	struct offchan_tx_req *cur_req;

	cur_req = &offchan_obj->current_req;
	if ((cur_req->complete_dwell_tx == false) &&
		(qdf_atomic_read(&offchan_obj->pending_tx_completion) == 0)) {
		offchan_tx_scan_cancel(vdev, offchan_obj);
	}
}

static QDF_STATUS offchan_data_tx_complete(void *ctx, qdf_nbuf_t nbuf,
		uint32_t mgmt_status, void *tx_compl_params)
{
	struct wlan_objmgr_vdev *vdev;
	struct offchan_txrx_pdev_obj *offchan_obj;
	ieee80211_xmit_status *ts = (ieee80211_xmit_status *) tx_compl_params;
	struct offchan_tx_req *cur_req;
	struct offchan_tx_status *status;
	uint32_t id, cnt;
	uint8_t dequeue_rate;

	offchan_obj = (struct offchan_txrx_pdev_obj *) ctx;
	cur_req = &offchan_obj->current_req;
	status = &offchan_obj->status;

	qdf_spin_lock_bh(&offchan_obj->offchan_lock);
	wbuf_get_pid(nbuf, &id);

	cnt = id;
	status->id[cnt] = id;
	switch (ts->ts_flags) {
	case 0:
		status->status[cnt] = OFFCHAN_TX_STATUS_SUCCESS;
		break;
	case IEEE80211_TX_ERROR:
		status->status[cnt] = OFFCHAN_TX_STATUS_ERROR;
		break;
	case IEEE80211_TX_XRETRY:
		status->status[cnt] = OFFCHAN_TX_STATUS_XRETRY;
		break;
	default:
		status->status[cnt] = OFFCHAN_TX_STATUS_ERROR;
		break;
	}


	vdev = offchan_obj->curr_req_vdev;

	/*
	 * When application requested for nbuf as part of the
	 * completion handler, nbuf is saved in offchan_txcomp_list list
	 * in status, else nbuf is freed here itself.
	 */
	if (cur_req->req_nbuf_ontx_comp)
		qdf_nbuf_queue_add(&status->offchan_txcomp_list, nbuf);
	else
		wbuf_free(nbuf);


	dequeue_rate = cur_req->dequeue_rate;
	qdf_atomic_dec(&offchan_obj->pending_tx_completion);

	if (qdf_atomic_read(&offchan_obj->offchan_scan_complete) == 0) {
		while (dequeue_rate) {
			if (offchan_send_list(vdev, offchan_obj,
						&cur_req->offchan_tx_list)) {
				offchan_debug("Send frame\n");
			} else {
				offchan_debug("Tx list is empty");
				offchan_data_tx_check_and_truncate_dwell(vdev,
								offchan_obj);
				break;
			}
			dequeue_rate--;
		}
		qdf_spin_unlock_bh(&offchan_obj->offchan_lock);
	} else
		offchan_check_and_send_completion(vdev, offchan_obj);


	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS offchan_tx_send(struct wlan_objmgr_vdev *vdev,
				struct offchan_txrx_pdev_obj *offchan_obj,
				qdf_nbuf_t nbuf)
{
	QDF_STATUS status;
	struct wlan_objmgr_peer *peer;
	uint32_t offchan_tx = true;

	peer = wlan_vdev_get_bsspeer(vdev);
	if (peer == NULL) {
		offchan_err("Cannot send frame Peer NULL\n");
		wbuf_free(nbuf);
		return QDF_STATUS_E_INVAL;
	}

	wlan_wbuf_set_peer(nbuf, peer);
	status = wlan_mgmt_txrx_mgmt_frame_tx(peer, offchan_obj, nbuf, NULL,
				offchan_data_tx_complete,
				WLAN_UMAC_COMP_OFFCHAN_TXRX, &offchan_tx);
	if (QDF_IS_STATUS_ERROR(status)) {
		wbuf_free(nbuf);
		offchan_err("Failed to send offchan frame\n");
	} else {
		qdf_atomic_inc(&offchan_obj->pending_tx_completion);
	}

	return status;
}

static inline bool offchan_send_list(struct wlan_objmgr_vdev *vdev,
				struct offchan_txrx_pdev_obj *offchan_obj,
				qdf_nbuf_queue_t *offchan_list)
{
	qdf_nbuf_t nbuf;
	struct offchan_tx_status *status;

	status = &offchan_obj->status;

	if (!qdf_nbuf_is_queue_empty(offchan_list)) {
		nbuf = qdf_nbuf_queue_remove(offchan_list);
		if (nbuf == NULL)
			return false;

		if (offchan_tx_send(vdev, offchan_obj, nbuf) !=
						QDF_STATUS_SUCCESS) {
			status->status[status->count] = OFFCHAN_TX_STATUS_ERROR;
		} else {
			wbuf_set_pid(nbuf, status->count);
			status->status[status->count] = OFFCHAN_TX_STATUS_SENT;
		}
		status->count++;
		return true;
	}

	return false;
}

static inline void offchan_scan_foreign_chan_handler(
		struct wlan_objmgr_vdev *vdev,
		struct offchan_txrx_pdev_obj *offchan_obj)
{
	struct offchan_tx_req *cur_req;
	struct offchan_tx_status *status;

	cur_req = &offchan_obj->current_req;
	status = &offchan_obj->status;
	/* Start sending off-chan data now*/
	qdf_spin_lock_bh(&offchan_obj->offchan_lock);

	if (!cur_req->offchan_rx) {
		uint8_t dequeue_rate = cur_req->dequeue_rate;

		while (dequeue_rate) {
			if (offchan_send_list(vdev,
						offchan_obj,
						&cur_req->offchan_tx_list)) {
				offchan_debug("Send frame\n");
			} else {
				offchan_debug("Tx list is empty");
			}
			dequeue_rate--;
		}
	} else {
		/* Off-chan RX */
		offchan_rx_ind rx_ind;

		offchan_info("Call callback to update rx\n");
		rx_ind = cur_req->rx_ind;
		if (rx_ind)
			rx_ind(vdev);
	}
	qdf_spin_unlock_bh(&offchan_obj->offchan_lock);
}

static inline void offchan_tx_send_all_frames(struct wlan_objmgr_vdev *vdev,
		struct offchan_txrx_pdev_obj *offchan_obj)
{
	struct offchan_tx_req *cur_req;
	struct offchan_tx_status *status;

	cur_req = &offchan_obj->current_req;
	status = &offchan_obj->status;
	while (offchan_send_list(vdev,
				offchan_obj,
				&cur_req->offchan_tx_list)) {
		offchan_debug("Send frame\n");
	}
}

static inline void offchan_scan_complete_handler(struct wlan_objmgr_vdev *vdev,
		struct offchan_txrx_pdev_obj *offchan_obj)
{
	offchan_debug("Scan completed\n");
	qdf_spin_lock_bh(&offchan_obj->offchan_lock);
	qdf_atomic_set(&offchan_obj->offchan_scan_complete, 1);
	offchan_check_and_send_completion(vdev, offchan_obj);

}

static inline void
offchan_record_scan_event_tstamps(struct offchan_txrx_pdev_obj *offchan_obj,
				  struct scan_event *event)
{
	struct offchan_evt_tstamp *evt_tstamp;

	if (offchan_obj == NULL) {
		offchan_err("Offchan pdev object is NULL\n");
		return;
	}

	if (event == NULL) {
		offchan_err("Offchan scan event is NULL\n");
		return;
	}

	offchan_debug("Record offchan event timestamps\n");
	evt_tstamp = &offchan_obj->evt_tstamp;
	switch (event->type) {
		case SCAN_EVENT_TYPE_STARTED:
			evt_tstamp->scan_start = event->timestamp;
			break;
		case SCAN_EVENT_TYPE_COMPLETED:
			evt_tstamp->scan_completed = event->timestamp;
			break;
		case SCAN_EVENT_TYPE_BSS_CHANNEL:
			evt_tstamp->scan_bss_entry = event->timestamp;
			break;
		case SCAN_EVENT_TYPE_FOREIGN_CHANNEL:
			evt_tstamp->scan_foreign_entry = event->timestamp;
			break;
		case SCAN_EVENT_TYPE_FOREIGN_CHANNEL_EXIT:
			evt_tstamp->scan_foreign_exit = event->timestamp;
			break;
		default:
			break;
	}

	return;
}

static void offchan_scan_event_handler (struct wlan_objmgr_vdev *vdev,
	struct scan_event *event, void *arg)
{
	struct wlan_objmgr_pdev *pdev;
	struct offchan_txrx_pdev_obj *offchan_pdev_obj;

	pdev = wlan_vdev_get_pdev(vdev);
	if (pdev == NULL) {
		offchan_err("Unable to get pdev from vdev\n");
		return;
	}

	offchan_pdev_obj = wlan_objmgr_pdev_get_comp_private_obj(pdev,
			WLAN_UMAC_COMP_OFFCHAN_TXRX);
	if (offchan_pdev_obj == NULL) {
		offchan_err("offchan pdev object is NULL");
		return;
	}

	offchan_debug("Scan event type - %d\n", event->type);
	offchan_record_scan_event_tstamps(offchan_pdev_obj, event);
	switch (event->type) {
	default:
	case SCAN_EVENT_TYPE_STARTED:
		break;
	case SCAN_EVENT_TYPE_COMPLETED:
		offchan_scan_complete_handler(vdev, offchan_pdev_obj);
		break;
	case SCAN_EVENT_TYPE_FOREIGN_CHANNEL:
		offchan_scan_foreign_chan_handler(vdev, offchan_pdev_obj);
		break;
	}

}

QDF_STATUS wlan_offchan_txrx_pdev_created_notification(
	struct wlan_objmgr_pdev *pdev, void *data)
{
	struct offchan_txrx_pdev_obj *offchan_pdev_obj;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (pdev == NULL) {
		offchan_err("vdev context passed is NULL");
		return QDF_STATUS_E_INVAL;
	}

	offchan_pdev_obj = qdf_mem_malloc(sizeof(*offchan_pdev_obj));
	if (offchan_pdev_obj == NULL) {
		offchan_err("Failed to allocate offchan txrx pdev object");
		return QDF_STATUS_E_NOMEM;
	}

	offchan_pdev_obj->pdev = pdev;

	qdf_atomic_init(&offchan_pdev_obj->request_in_progress);
	qdf_atomic_init(&offchan_pdev_obj->offchan_scan_complete);
	qdf_atomic_init(&offchan_pdev_obj->pending_tx_completion);
	qdf_spinlock_create(&offchan_pdev_obj->offchan_lock);
	/* Allocate requester ID without attaching callback function */
	offchan_pdev_obj->requestor_id =
			ucfg_scan_register_requester(wlan_pdev_get_psoc(pdev),
			(uint8_t *)"offchan", offchan_scan_event_handler, NULL);
	if (!offchan_pdev_obj->requestor_id) {
		offchan_err("unable to allocate requester");
		goto register_scan_error;
	}

	status = wlan_objmgr_pdev_component_obj_attach(pdev,
				WLAN_UMAC_COMP_OFFCHAN_TXRX, offchan_pdev_obj,
				QDF_STATUS_SUCCESS);
	if (status != QDF_STATUS_SUCCESS) {
		offchan_err("Failed to attach offchan txrx component, %d",
			status);
		goto attach_error;
	}

	return QDF_STATUS_SUCCESS;

attach_error:
	ucfg_scan_unregister_requester(wlan_pdev_get_psoc(pdev),
					offchan_pdev_obj->requestor_id);
register_scan_error:
	qdf_mem_free(offchan_pdev_obj);

	return status;
}

QDF_STATUS wlan_offchan_txrx_pdev_destroyed_notification(
	struct wlan_objmgr_pdev *pdev, void *data)
{
	struct offchan_txrx_pdev_obj *offchan_pdev_obj;
	QDF_STATUS status;

	if (pdev == NULL) {
		offchan_err("pdev context passed is NULL");
		return QDF_STATUS_E_INVAL;
	}

	offchan_pdev_obj = wlan_objmgr_pdev_get_comp_private_obj(pdev,
			WLAN_UMAC_COMP_OFFCHAN_TXRX);
	if (offchan_pdev_obj == NULL) {
		offchan_err("offchan pdev object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	ucfg_scan_unregister_requester(wlan_pdev_get_psoc(pdev),
					offchan_pdev_obj->requestor_id);
	offchan_pdev_obj->pdev = NULL;

	status = wlan_objmgr_pdev_component_obj_detach(pdev,
				WLAN_UMAC_COMP_OFFCHAN_TXRX, offchan_pdev_obj);
	if (status != QDF_STATUS_SUCCESS) {
		offchan_err("Failed to detach offchan component, %d", status);
		return status;
	}
	/* Free the nbuf queue stored in tx complete status list */
	qdf_nbuf_queue_free(&offchan_pdev_obj->status.offchan_txcomp_list);

	qdf_mem_free(offchan_pdev_obj);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_offchan_txrx_request(struct wlan_objmgr_vdev *vdev,
						struct offchan_tx_req *req)
{
	struct scan_start_request *scan_params = NULL;
	struct wlan_objmgr_pdev *pdev;
	struct offchan_txrx_pdev_obj *offchan_pdev_obj;
	ieee80211_chanlist_t chanlist;
	struct ieee80211vap *vap;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	pdev = wlan_vdev_get_pdev(vdev);
	if (pdev == NULL) {
		offchan_err("Unable to get pdev from vdev\n");
		return QDF_STATUS_E_INVAL;
	}

	offchan_pdev_obj = wlan_objmgr_pdev_get_comp_private_obj(pdev,
			WLAN_UMAC_COMP_OFFCHAN_TXRX);
	if (offchan_pdev_obj == NULL) {
		offchan_err("offchan pdev object is NULL");
		return QDF_STATUS_E_INVAL;
	}

	offchan_debug("Request to send %d frames\n",
				qdf_nbuf_queue_len(&req->offchan_tx_list));
	if (qdf_nbuf_queue_len(&req->offchan_tx_list) > OFFCHAN_TX_MAX)
		return QDF_STATUS_E_INVAL;

	qdf_spin_lock_bh(&offchan_pdev_obj->offchan_lock);
	if (qdf_atomic_read(&offchan_pdev_obj->request_in_progress)) {
		qdf_spin_unlock_bh(&offchan_pdev_obj->offchan_lock);
		return QDF_STATUS_E_AGAIN;
	}

	if (!req->dequeue_rate)
		req->dequeue_rate = 1;

	qdf_atomic_set(&offchan_pdev_obj->request_in_progress, 1);
	qdf_atomic_set(&offchan_pdev_obj->offchan_scan_complete, 0);
	qdf_atomic_set(&offchan_pdev_obj->pending_tx_completion, 0);
	qdf_mem_copy(&offchan_pdev_obj->current_req, req, sizeof(*req));
	qdf_mem_zero(&offchan_pdev_obj->status,
				sizeof(offchan_pdev_obj->status));

	qdf_nbuf_queue_init(&offchan_pdev_obj->status.offchan_txcomp_list);

	offchan_pdev_obj->curr_req_vdev = vdev;

	qdf_spin_unlock_bh(&offchan_pdev_obj->offchan_lock);

	scan_params = qdf_mem_malloc(sizeof(struct scan_start_request));
	if (scan_params == NULL) {
		status =  QDF_STATUS_E_NOMEM;
		goto bad;
	}

	ucfg_scan_init_default_params(vdev, scan_params);
	scan_params->scan_req.dwell_time_active = req->dwell_time;
	scan_params->scan_req.dwell_time_passive = req->dwell_time;
	scan_params->scan_req.scan_flags = 0;
	scan_params->scan_req.scan_f_passive = 1;
	scan_params->scan_req.scan_f_offchan_mgmt_tx = 1;
	scan_params->scan_req.scan_f_offchan_data_tx = 1;
	if (req->offchan_rx)
		scan_params->scan_req.scan_f_chan_stat_evnt = 1;

	/* scan_params->type = IEEE80211_SCAN_FOREGROUND; */
	scan_params->scan_req.min_rest_time = 25;
	scan_params->scan_req.max_rest_time = 100;
	scan_params->scan_req.scan_priority = SCAN_PRIORITY_HIGH;

	/* channel to scan */
	if(req->wide_scan.bw_mode) {
		chanlist.n_chan = 1;
		chanlist.chan[0] = req->wide_scan.chan_no;
		chanlist.chan_width[0] = req->wide_scan.bw_mode;
		chanlist.sec_chan_offset[0] = req->wide_scan.sec_chan_offset;
		vap = wlan_mlme_get_vdev_legacy_obj(vdev);
		if(vap == NULL) {
			status = QDF_STATUS_E_INVAL;
			goto bad;
		}
		status = ieee80211_update_scan_channel_phymode(vap, &chanlist, scan_params);
		if (QDF_IS_STATUS_ERROR(status)) {
			status = QDF_STATUS_E_INVAL;
			goto bad;
		}
	} else {
		scan_params->scan_req.chan_list.num_chan = 1;
		scan_params->scan_req.chan_list.chan[0].freq = req->chan;
	}
	offchan_pdev_obj->scan_id = ucfg_scan_get_scan_id(
						wlan_vdev_get_psoc(vdev));
	scan_params->scan_req.scan_id = offchan_pdev_obj->scan_id;
	scan_params->scan_req.scan_req_id = offchan_pdev_obj->requestor_id;
	scan_params->vdev = vdev;

	qdf_spin_lock_bh(&offchan_pdev_obj->offchan_lock);
	/* If target supports seperate tid for offchan frames, Enqueue frames
	 * by sending it to target, before issuing offchan scan. All enqueued
	 * Frames will be transmitted in foreign channel by target itself.
	 */
	if (tgt_offchan_data_tid_supported(wlan_pdev_get_psoc(pdev), pdev) &&
			!req->offchan_rx)
		offchan_tx_send_all_frames(vdev, offchan_pdev_obj);

	qdf_spin_unlock_bh(&offchan_pdev_obj->offchan_lock);

	wlan_objmgr_vdev_get_ref(vdev, WLAN_OFFCHAN_TXRX_ID);
	if (QDF_IS_STATUS_ERROR(ucfg_scan_start(scan_params))) {
		offchan_err("Failed to start offchan tx scan\n");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_OFFCHAN_TXRX_ID);
		status =  QDF_STATUS_E_INVAL;
		goto bad;
	}
	wlan_objmgr_vdev_release_ref(vdev, WLAN_OFFCHAN_TXRX_ID);

	return QDF_STATUS_SUCCESS;
bad:
	qdf_atomic_set(&offchan_pdev_obj->request_in_progress, 0);
	return status;
}

QDF_STATUS wlan_offchan_txrx_cancel(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_pdev *pdev;
	struct offchan_txrx_pdev_obj *offchan_pdev_obj;
	QDF_STATUS status;

	pdev = wlan_vdev_get_pdev(vdev);
	if (pdev == NULL) {
		offchan_err("Unable to get pdev from vdev\n");
		return QDF_STATUS_E_INVAL;
	}

	offchan_pdev_obj = wlan_objmgr_pdev_get_comp_private_obj(pdev,
			WLAN_UMAC_COMP_OFFCHAN_TXRX);
	if (offchan_pdev_obj == NULL) {
		offchan_err("offchan pdev object is NULL");
		return QDF_STATUS_E_INVAL;
	}

	qdf_spin_lock_bh(&offchan_pdev_obj->offchan_lock);
	if (!qdf_atomic_read(&offchan_pdev_obj->request_in_progress)) {
		qdf_spin_unlock_bh(&offchan_pdev_obj->offchan_lock);
		offchan_err("No active request to cancel");
		return QDF_STATUS_E_INVAL;
	}
	qdf_spin_unlock_bh(&offchan_pdev_obj->offchan_lock);

	status = offchan_tx_scan_cancel(vdev, offchan_pdev_obj);

	return status;
}

/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

#ifndef __WLAN_OFFCHAN_TXRX_MAIN_H
#define __WLAN_OFFCHAN_TXRX_MAIN_H

/**
 * struct offchan_txrx_pdev_obj - offchan txrx private object.
 * @requestor_id: Scan requestor id assigned for off-chan scan
 * @pdev: back pointer to pdev object
 * @request_in_progress: Flag to indicate if a request is active
 * @offchan_scan_complete: Flag to indicate if offchan scan complete
 * @pending_tx_completion: Pending tx completions
 * @offchan_lock: offchan lock to protect its data
 * @current_req: Current active request
 * @status: Return status for offchan tx
 */
struct offchan_txrx_pdev_obj {
	wlan_scan_requester requestor_id;
	struct wlan_objmgr_pdev *pdev;
	qdf_atomic_t request_in_progress;
	qdf_atomic_t offchan_scan_complete;
	qdf_atomic_t pending_tx_completion;
	qdf_spinlock_t offchan_lock;
	uint32_t scan_id;
	struct offchan_tx_req current_req;
	struct offchan_tx_status status;
	struct offchan_stats stats;
	struct offchan_evt_tstamp evt_tstamp;
	struct wlan_objmgr_vdev *curr_req_vdev;
};

/**
 * wlan_offchan_txrx_pdev_created_notificaton() - Pdev created notification
 * callback function
 * @pdev: Pointer to pdev object
 * @data: callback data registered
 *
 * Return: QDF_STATUS - Success or Failure
 */
QDF_STATUS wlan_offchan_txrx_pdev_created_notification(
	struct wlan_objmgr_pdev *pdev, void *data);

/**
 * wlan_offchan_txrx_pdev_destroyed_notificaton() - Pdev destroyed notification
 * callback function
 * @pdev: Pointer to pdev object
 * @data: callback data registered
 *
 * Return: QDF_STATUS - Success or Failure
 */
QDF_STATUS wlan_offchan_txrx_pdev_destroyed_notification(
	struct wlan_objmgr_pdev *pdev, void *data);
/**
 * wlan_offchan_txrx_request() - API to request offchan tx
 * @vdev: Pointer to vdev object
 * @req: pointer to offchan_tx_req request
 *
 * Return: QDF_STATUS - Success or Failure
 */
QDF_STATUS wlan_offchan_txrx_request(struct wlan_objmgr_vdev *vdev,
					struct offchan_tx_req *req);

/**
 * wlan_offchan_txrx_cancel() - API to cancel current offchan tx
 * @vdev: Pointer to vdev object
 *
 * Return: QDF_STATUS - Success or Failure
 */
QDF_STATUS wlan_offchan_txrx_cancel(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_psoc_get_offchan_txrx_ops - Helper routine to get offchan target_if
 *                 ops structure.
 * @psoc: pointer to psoc context.
 * Return: Ponter to offchan ops structure.
 */
static inline struct wlan_lmac_if_offchan_txrx_ops *
wlan_psoc_get_offchan_txrx_ops(struct wlan_objmgr_psoc *psoc)
{
	return &((psoc->soc_cb.tx_ops.offchan_txrx_ops));
}

/**
 * tgt_offchan_data_tid_supported - Helper routine to call target_if to query
 *               offchan data tid support in target.
 *
 * @psoc: pointer to psoc context
 * @pdev: pointer to pdev context
 *
 * Return: True if offchan data tid is supported, else false.
 */
static inline bool tgt_offchan_data_tid_supported(struct wlan_objmgr_psoc *psoc,
					struct wlan_objmgr_pdev *pdev)
{
	struct wlan_lmac_if_offchan_txrx_ops *ops =
		wlan_psoc_get_offchan_txrx_ops(psoc);

	QDF_ASSERT(ops->offchan_data_tid_support);

	if (ops->offchan_data_tid_support) {
		return ops->offchan_data_tid_support(pdev);
	}

	return false;
}
#endif

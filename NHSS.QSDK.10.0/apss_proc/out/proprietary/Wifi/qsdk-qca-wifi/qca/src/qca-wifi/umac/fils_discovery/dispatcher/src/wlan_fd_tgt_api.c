/*
 *
 * Copyright (c) 2018-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#include <wlan_fd_tgt_api.h>
#include <wlan_fd_utils_api.h>
#include "../../core/fd_priv_i.h"

uint8_t tgt_fd_is_fils_enable(struct wlan_objmgr_vdev *vdev)
{
	return wlan_fils_is_enable(vdev);
}

void tgt_fd_alloc(struct wlan_objmgr_vdev *vdev)
{
	struct fd_vdev *fv;

	if (vdev == NULL) {
		fd_err("VDEV is NULL!!\n");
		return;
	}
	fv = wlan_objmgr_vdev_get_comp_private_obj(vdev, WLAN_UMAC_COMP_FD);
	if (fv == NULL) {
		fd_debug("FILS DISC object is NULL!!\n");
		return;
	}

	wlan_fd_vdev_defer_fd_buf_free(vdev);
	fv->fd_wbuf = wlan_fd_alloc(vdev);
	if (!fv->fd_wbuf) {
		fd_err("ERR: FILS Discovery buff Allocation failed!\n");
	}
}

void tgt_fd_stop(struct wlan_objmgr_vdev *vdev)
{
	wlan_fd_vdev_defer_fd_buf_free(vdev);
}

void tgt_fd_free(struct wlan_objmgr_vdev *vdev)
{
	struct fd_vdev *fv;
	struct wlan_objmgr_psoc *psoc;

	if (vdev == NULL) {
		fd_err("VDEV is NULL!!\n");
		return;
	}
	psoc = wlan_vdev_get_psoc(vdev);
	if (psoc == NULL) {
		fd_err("PSOC is NULL!!\n");
		return;
	}
	fv = wlan_objmgr_vdev_get_comp_private_obj(vdev, WLAN_UMAC_COMP_FD);
	if (fv == NULL) {
		fd_debug("FILS DISC object is NULL!!\n");
		return;
	}

	qdf_spin_lock_bh(&fv->fd_lock);
	fd_free_list(psoc, &fv->fd_deferred_list);
	qdf_spin_unlock_bh(&fv->fd_lock);
}

uint32_t tgt_fd_get_valid_fd_period(struct wlan_objmgr_vdev *vdev,
				    uint8_t *is_modified)
{
	struct fd_vdev *fv;
	uint32_t temp_fd_period = 0;

	if (!vdev || !is_modified) {
		fd_err("Invalid Params!!\n");
		return 0;
	}
	fv = wlan_objmgr_vdev_get_comp_private_obj(vdev, WLAN_UMAC_COMP_FD);
	if (fv == NULL) {
		fd_debug("FD object is NULL!!\n");
		return 0;
	}
	temp_fd_period = fv->fd_period;
	wlan_fd_set_valid_fd_period(vdev, fv->fd_period);
	if (temp_fd_period != fv->fd_period)
		*is_modified = 1;

	return fv->fd_period;
}

QDF_STATUS tgt_fd_swfda_handler(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_psoc *psoc;
	struct fd_vdev *fv;

	if (!vdev) {
		fd_err("VDEV is NULL!!\n");
		return QDF_STATUS_E_INVAL;
	}
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		fd_err("PSOC is NULL!!\n");
		return QDF_STATUS_E_INVAL;
	}
	fv = wlan_objmgr_vdev_get_comp_private_obj(vdev, WLAN_UMAC_COMP_FD);
	if (!fv) {
		fd_debug("FD object is NULL!!\n");
		return QDF_STATUS_E_INVAL;
	}

	qdf_spin_lock_bh(&fv->fd_lock);
	/* If vap is not active, no need to queue FD buff to FW, Ignore SWFDA */
	if (wlan_vdev_is_up(vdev) != QDF_STATUS_SUCCESS) {
		fd_err("vdev %d: not active. drop SWFDA event\n",
				wlan_vdev_get_id(vdev));
		qdf_spin_unlock_bh(&fv->fd_lock);
		return -1;
	}
	if (!fv->fd_wbuf) {
		fd_info("FD buff is not allocated. Ignore SWFDA Event.\n");
		qdf_spin_unlock_bh(&fv->fd_lock);
		return QDF_STATUS_E_INVAL;
	}
	if (wlan_fd_update(vdev) == QDF_STATUS_SUCCESS) {
		if (fv->is_fd_dma_mapped) {
			qdf_nbuf_unmap_single(wlan_psoc_get_qdf_dev(psoc),
				fv->fd_wbuf, QDF_DMA_TO_DEVICE);
			fv->is_fd_dma_mapped = false;
		}
		wlan_mgmt_txrx_fd_action_frame_tx(vdev, fv->fd_wbuf,
				WLAN_UMAC_COMP_FD);
		fv->is_fd_dma_mapped = true;
	}
	qdf_spin_unlock_bh(&fv->fd_lock);

	return QDF_STATUS_SUCCESS;
}


/*
 * Copyright (c) 2017-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2011 Atheros Communications Inc.
 *
 */

#include <target_if_fd.h>
#include <target_if.h>
#include <hif.h>
#include <wmi_unified_api.h>
#include <wlan_lmac_if_def.h>
#include <init_deinit_lmac.h>

/* FD RX ops from PSOC RX ops */
#define FD_RX_OPS   psoc->soc_cb.rx_ops.fd_rx_ops

extern uint8_t ol_ath_is_bcn_mode_burst(struct wlan_objmgr_pdev *pdev);

static QDF_STATUS
target_if_fd_vdev_config_fils(struct wlan_objmgr_vdev* vdev, uint32_t fd_period)
{
	struct config_fils_params param;
	struct wlan_objmgr_pdev *pdev;
	void *wmi_hdl;

	if (vdev == NULL) {
		fd_err("VDEV is NULL!!\n");
		return QDF_STATUS_E_INVAL;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (pdev == NULL) {
		fd_err("PDEV is NULL!!\n");
		return QDF_STATUS_E_INVAL;
	}

	if (fd_period && !ol_ath_is_bcn_mode_burst(pdev)) {
		fd_err("Beacon mode set to staggered. Cannot enable FD\n");
		return QDF_STATUS_E_INVAL;
	}

	if ((wlan_vdev_chan_config_valid(vdev) == QDF_STATUS_SUCCESS)) {
		fd_info("Configuring FD frame with period %d\n", fd_period);
		qdf_mem_set(&param, sizeof(param), 0);
		param.vdev_id = wlan_vdev_get_id(vdev);
		param.fd_period = fd_period;
		wmi_hdl = GET_WMI_HDL_FROM_PDEV(pdev);
		if (wmi_hdl == NULL) {
			fd_err("wmi handle is NULL!!\n");
			return QDF_STATUS_E_INVAL;
		}

		return wmi_unified_fils_vdev_config_send_cmd(wmi_hdl, &param);
	}

	return QDF_STATUS_SUCCESS;
}

static int
target_if_fd_swfda_handler(ol_scn_t sc, uint8_t *data, uint32_t datalen)
{
	ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *)sc;
	struct wlan_objmgr_psoc *psoc = soc->psoc_obj;
	struct wlan_objmgr_vdev *vdev = NULL;
	uint32_t vdev_id;
	struct common_wmi_handle *wmi_handle;

	wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
	if (wmi_extract_swfda_vdev_id(wmi_handle, data, &vdev_id)) {
		fd_err("Unable to extact vdev id from swfda event\n");
		return -1;
	}
	if (psoc == NULL) {
		fd_err("PSOC is NULL!!\n");
		return QDF_STATUS_E_INVAL;
	}

	/* Get the VDEV corresponding to the id */
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
				WLAN_FD_ID);
	if (vdev == NULL) {
		fd_err("VDEV not found!\n");
		return -1;
	}

	if (FD_RX_OPS.fd_swfda_handler(vdev) != QDF_STATUS_SUCCESS) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_FD_ID);
		return -1;
	}
	wlan_objmgr_vdev_release_ref(vdev, WLAN_FD_ID);

	return 0;
}

static void
target_if_fd_register_event_handler(struct wlan_objmgr_psoc *psoc)
{
	if (psoc == NULL) {
		fd_err("PSOC is NULL!!\n");
		return;
	}

	wmi_unified_register_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_host_swfda_event_id, target_if_fd_swfda_handler,
			WMI_RX_UMAC_CTX);
}

static void
target_if_fd_unregister_event_handler(struct wlan_objmgr_psoc *psoc)
{
	if (psoc == NULL) {
		fd_err("PSOC is NULL!!\n");
		return;
	}

	wmi_unified_unregister_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_host_swfda_event_id);
}

void target_if_fd_register_tx_ops(struct wlan_lmac_if_tx_ops *tx_ops)
{
	struct wlan_lmac_if_fd_tx_ops *fd_tx_ops = &tx_ops->fd_tx_ops;

	fd_tx_ops->fd_vdev_config_fils = target_if_fd_vdev_config_fils;
	fd_tx_ops->fd_register_event_handler =
				target_if_fd_register_event_handler;
	fd_tx_ops->fd_unregister_event_handler =
				target_if_fd_unregister_event_handler;
}

QDF_STATUS target_if_fd_reconfig(struct wlan_objmgr_vdev *vdev)
{
	uint32_t fd_period = 0;
	struct wlan_objmgr_psoc *psoc;
	uint8_t is_modified = 0;

	if (vdev == NULL) {
		fd_err("VDEV not found!\n");
		return QDF_STATUS_E_INVAL;
	}
	psoc = wlan_vdev_get_psoc(vdev);
	if (psoc == NULL) {
		fd_err("PSOC is NULL\n");
		return QDF_STATUS_E_INVAL;
	}

	/* Allocate FD buff */
	FD_RX_OPS.fd_alloc(vdev);
	/* Get valid FD Period */
	fd_period = FD_RX_OPS.fd_get_valid_fd_period(vdev, &is_modified);

	if (!FD_RX_OPS.fd_is_fils_enable(vdev) && !is_modified) {
		return QDF_STATUS_E_INVAL;
	}

	return target_if_fd_vdev_config_fils(vdev, fd_period);
}

QDF_STATUS target_if_fd_send(struct wlan_objmgr_vdev *vdev, qdf_nbuf_t wbuf)
{
	struct fd_params param;
	uint16_t frame_ctrl;
	struct ieee80211_frame *wh;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	void *wmi_hdl;

	if (vdev == NULL) {
		fd_err("VDEV not found!\n");
		return QDF_STATUS_E_INVAL;
	}
	psoc = wlan_vdev_get_psoc(vdev);
	if (psoc == NULL) {
		fd_err("PSOC is NULL\n");
		return QDF_STATUS_E_INVAL;
	}
	pdev = wlan_vdev_get_pdev(vdev);
	if (pdev == NULL) {
		fd_err("PDEV is NULL\n");
		return QDF_STATUS_E_INVAL;
	}

	qdf_mem_zero(&param, sizeof(param));
	param.wbuf = wbuf;
	param.vdev_id = wlan_vdev_get_id(vdev);

	/* Get the frame ctrl field */
	wh = (struct ieee80211_frame *)qdf_nbuf_data(wbuf);
	frame_ctrl = qdf_le16_to_cpu(*((uint16_t *)wh->i_fc));

	/* Map the FD buffer to DMA region */
	qdf_nbuf_map_single(wlan_psoc_get_qdf_dev(psoc), wbuf,
			QDF_DMA_TO_DEVICE);

	param.frame_ctrl = frame_ctrl;
	wmi_hdl = GET_WMI_HDL_FROM_PDEV(pdev);
	if (wmi_hdl == NULL) {
		fd_err("wmi handle is NULL!!\n");
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_fils_discovery_send_cmd(wmi_hdl, &param);
}

void target_if_fd_stop(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_psoc *psoc;

	if (vdev == NULL) {
		fd_err("VDEV not found!\n");
		return;
	}
	psoc = wlan_vdev_get_psoc(vdev);
	if (psoc == NULL) {
		fd_err("PSOC is NULL\n");
		return;
	}

	FD_RX_OPS.fd_stop(vdev);
}

void target_if_fd_free(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_psoc *psoc;

	if (vdev == NULL) {
		fd_err("VDEV not found!\n");
		return;
	}
	psoc = wlan_vdev_get_psoc(vdev);
	if (psoc == NULL) {
		fd_err("PSOC is NULL\n");
		return;
	}

	FD_RX_OPS.fd_free(vdev);
}

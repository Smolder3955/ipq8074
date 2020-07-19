/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

#include <qdf_status.h>
#include <qdf_nbuf.h>
#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_cmn.h>
#include <wlan_scan_ucfg_api.h>
#include <wlan_offchan_txrx_api.h>
#include "../../core/src/wlan_offchan_txrx_main.h"


QDF_STATUS wlan_offchan_txrx_init(void)
{
	QDF_STATUS status;

	status = wlan_objmgr_register_pdev_create_handler(
		WLAN_UMAC_COMP_OFFCHAN_TXRX,
		wlan_offchan_txrx_pdev_created_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		offchan_err("Failed to register pdev create handler");
		return status;
	}

	status = wlan_objmgr_register_pdev_destroy_handler(
		WLAN_UMAC_COMP_OFFCHAN_TXRX,
		wlan_offchan_txrx_pdev_destroyed_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_objmgr_unregister_pdev_create_handler(
				WLAN_UMAC_COMP_OFFCHAN_TXRX,
				wlan_offchan_txrx_pdev_created_notification,
				NULL);
		offchan_err("Failed to register pdev destroy handler");
		return status;
	}
	offchan_info(
	"offchan pdev create and delete handler registered with objmgr");

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_offchan_txrx_deinit(void)
{
	QDF_STATUS status;

	status = wlan_objmgr_unregister_pdev_create_handler(
			WLAN_UMAC_COMP_OFFCHAN_TXRX,
			wlan_offchan_txrx_pdev_created_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status))
		offchan_err("Failed to register offchan vdev create handler");

	status = wlan_objmgr_unregister_pdev_destroy_handler(
			WLAN_UMAC_COMP_OFFCHAN_TXRX,
			wlan_offchan_txrx_pdev_destroyed_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status))
		offchan_err("Failed to register offchan vdev delete handler");

	offchan_debug("Un-Register offchan obj handler successful");

	return QDF_STATUS_SUCCESS;

}

QDF_STATUS ucfg_offchan_tx_request(struct wlan_objmgr_vdev *vdev,
						struct offchan_tx_req *req)
{
	return wlan_offchan_txrx_request(vdev, req);
}

QDF_STATUS ucfg_offchan_tx_cancel(struct wlan_objmgr_vdev *vdev)
{
	return wlan_offchan_txrx_cancel(vdev);
}

bool ucfg_scan_from_offchan_requestor(struct wlan_objmgr_vdev *vdev,
						struct scan_event *event)
{
	struct wlan_objmgr_pdev *pdev;
	struct offchan_txrx_pdev_obj *offchan_pdev_obj;

	pdev = wlan_vdev_get_pdev(vdev);
	if (pdev == NULL) {
		offchan_err("Unable to get pdev from vdev\n");
		return false;
	}

	offchan_pdev_obj = wlan_objmgr_pdev_get_comp_private_obj(pdev,
			WLAN_UMAC_COMP_OFFCHAN_TXRX);
	if (offchan_pdev_obj == NULL) {
		offchan_err("offchan pdev object is NULL");
		return false;
	}

	return (event->requester == offchan_pdev_obj->requestor_id);
}

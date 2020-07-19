/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2010, Atheros Communications Inc.
 *
 */

#ifndef _TARGET_IF_FD_H_
#define _TARGET_IF_FD_H_

#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>

#define fd_log(level, args...) \
QDF_TRACE(QDF_MODULE_ID_FD, level, ## args)

#define fd_logfl(level, format, args...) fd_log(level, FL(format), ## args)

#define fd_fatal(format, args...) \
	fd_logfl(QDF_TRACE_LEVEL_FATAL, format, ## args)
#define fd_err(format, args...) \
	fd_logfl(QDF_TRACE_LEVEL_ERROR, format, ## args)
#define fd_warn(format, args...) \
	fd_logfl(QDF_TRACE_LEVEL_WARN, format, ## args)
#define fd_info(format, args...) \
	fd_logfl(QDF_TRACE_LEVEL_INFO, format, ## args)
#define fd_debug(format, args...) \
	fd_logfl(QDF_TRACE_LEVEL_DEBUG, format, ## args)

/**
 * target_if_fd_register_tx_ops() - Register Tx Ops function pointers of FD
 * @tx_ops: Pointer to Psoc lmac if Tx ops
 *
 * This function is invoked from object manager to initialise FD related
 * Tx Ops function pointers.
 *
 * Return: None
 */
void target_if_fd_register_tx_ops(struct wlan_lmac_if_tx_ops *tx_ops);

/**
 * target_if_fd_send() - API to send FD frame to FW
 * @vdev: Pointer to vdev object
 * @wbuf: Pointer ro FD frame buffer
 *
 * This function is invoked from WMI trigger handler to send configured
 * FD frame buffer to firmware.
 *
 * Return: QDF_STATUS_SUCCESS
 *         QDF_STATUS_E_FAILURE
 */
QDF_STATUS target_if_fd_send(struct wlan_objmgr_vdev *vdev, qdf_nbuf_t wbuf);

/**
 * target_if_fd_reconfig() - API to reconfigure FD frame
 * @vdev: Pointer to vdev object
 *
 * This function is invoked from ol_if layer during VDEV up.
 * It invokes FD Rx Ops function pointers to reconfigure FD frame.
 *
 * Return: QDF_STATUS_SUCCESS
 *         QDF_STATUS_E_FAILURE
 */
QDF_STATUS target_if_fd_reconfig(struct wlan_objmgr_vdev *vdev);

/**
 * target_if_fd_stop() - API to stop FD in target_if layer
 * @vdev: Pointer to vdev object
 *
 * This function is invoked from ol_if layer during VDEV stop.
 * It invokes FD Rx Ops function pointers to stop FD.
 *
 * Return: None
 */
void target_if_fd_stop(struct wlan_objmgr_vdev *vdev);

/**
 * target_if_fd_free() - API to free FD buffer in target_if layer
 * @vdev: Pointer to vdev object
 *
 * This function is invoked from ol_if layer.
 * It invokes FD Rx Ops function pointers to free FD buffer.
 *
 * Return: None
 */
void target_if_fd_free(struct wlan_objmgr_vdev *vdev);

#endif /* _TARGET_IF_FD_H_ */

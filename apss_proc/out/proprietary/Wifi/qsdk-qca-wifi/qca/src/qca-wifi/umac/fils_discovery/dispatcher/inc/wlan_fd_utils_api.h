/*
 *
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#ifndef _WLAN_FD_UTILS_API_H_
#define _WLAN_FD_UTILS_API_H_

#include <wlan_objmgr_cmn.h>
#include <wlan_utility.h>

/**
 * wlan_fd_init() - API to initialize FD component
 *
 * This API is invoked from dispatcher init to
 * initialize FILS Discovery component.
 *
 * Return: QDF_STATUS_SUCCESS
 *         QDF_STATUS_E_FAILURE
 */
QDF_STATUS wlan_fd_init(void);

/**
 * wlan_fd_deinit() - API to de-initialize FD component
 *
 * This API is invoked from dispatcher de-init to
 * de-initialize FILS Discovery component.
 *
 * Return: QDF_STATUS_SUCCESS
 *         QDF_STATUS_E_FAILURE
 */
QDF_STATUS wlan_fd_deinit(void);

/**
 * wlan_fd_enable() - API to enable FD component
 * @psoc: Pointer to PSOC object
 *
 * This API is invoked from psoc dispatcher enable to
 * enable FILS Discovery component.
 *
 * Return: QDF_STATUS_SUCCESS
 *         QDF_STATUS_E_FAILURE
 */
QDF_STATUS wlan_fd_enable(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_fd_disable() - API to disable FD component
 * @psoc: Pointer to PSOC object
 *
 * This API is invoked from psoc dispatcher disable to
 * disable FILS Discovery component.
 *
 * Return: QDF_STATUS_SUCCESS
 *         QDF_STATUS_E_FAILURE
 */
QDF_STATUS wlan_fd_disable(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_fd_vdev_defer_fd_buf_free() - API to free FD deferred frames
 * @vdev: Pointer to VDEV object
 *
 * This API is invoked to free FD deferred frames before
 * each new allocation.
 *
 * Return: None
 */
void wlan_fd_vdev_defer_fd_buf_free(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_fd_capable() - API to FILS Discovery is supported or not
 * @psoc: Pointer to PSOC object
 *
 * This API is invoked to check FILS Discovery feature is
 * supported by this chip or not.
 *
 * Return: true, if supported
 *         false, if not supported
 */
bool wlan_fd_capable(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_fd_set_valid_fd_period() - API to set valid FD period
 * @vdev:      Pointer to VDEV object
 * @fd_period: Value to set fd period
 *
 * This API is invoked to set valid FD period.
 *
 * Return: None
 */
void wlan_fd_set_valid_fd_period(struct wlan_objmgr_vdev *vdev,
			uint32_t fd_period);

/**
 * wlan_fd_frame_init() - API to initialise FD frame
 * @peer: Pointer to bss PEER object
 * @frm:  Pointer to frame buffer
 *
 * This API is invoked to initialise FD frame.
 *
 * Return: Modified frame pointer
 */
uint8_t* wlan_fd_frame_init(struct wlan_objmgr_peer *peer, uint8_t *frm);

/**
 * wlan_fd_alloc() - API to allocate FD buffer
 * @vdev: Pointer to VDEV object
 *
 * This API is invoked to allocate FD buffer.
 *
 * Return: Allocated Buffer or NULL
 */
qdf_nbuf_t wlan_fd_alloc(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_fd_update() - API to update FD buffer
 * @vdev: Pointer to VDEV object
 *
 * This API is invoked from FD event handler to update
 * FD buffer before sending to FW.
 *
 * Return: QDF_STATUS_SUCCESS
 *         QDF_STATUS_E_FAILURE
 */
QDF_STATUS wlan_fd_update(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_fils_is_enable() - API to check FILS enabled or not
 * @vdev: Pointer to VDEV object
 *
 * This API is to check FILS is enabled or not
 *
 * Return: 1 if enabled or 0 if disbled
 */
uint8_t wlan_fils_is_enable(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_fd_update_trigger() - API to trigger FD buffer update
 * @vdev: Pointer to VDEV object
 *
 * Return: None
 */
void wlan_fd_update_trigger(struct wlan_objmgr_vdev *vdev);

#endif /* _WLAN_FD_UTILS_API_H_ */

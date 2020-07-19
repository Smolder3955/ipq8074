/*
 *
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#ifndef _WLAN_FD_TGT_API_H_
#define _WLAN_FD_TGT_API_H_

#include <wlan_objmgr_cmn.h>

/**
 * tgt_fd_is_fils_enable() - API to check FILS enabled or not
 * @vdev: Pointer to VDEV object
 *
 * This API is invoked from ol_if layer to check FILS enabled or not.
 *
 * Return: 1 if enabled or 0 if disabled
 */
uint8_t tgt_fd_is_fils_enable(struct wlan_objmgr_vdev *vdev);

/**
 * tgt_fd_alloc() - API to allocate FD buffer
 * @vdev: Pointer to VDEV object
 *
 * This API is invoked from ol_if layer to allocate FD buffer
 * by invoking FD Rx ops in UMAC layer during VAP up.
 *
 * Return: None
 */
void tgt_fd_alloc(struct wlan_objmgr_vdev *vdev);

/**
 * tgt_fd_stop() - API to stop FD
 * @vdev: Pointer to VDEV object
 *
 * This API is invoked from ol_if layer to stop FD
 * which frees deferred FD buffers.
 *
 * Return: None
 */
void tgt_fd_stop(struct wlan_objmgr_vdev *vdev);

/**
 * tgt_fd_free() - API to free FD buffer
 * @vdev: Pointer to VDEV object
 *
 * This API is invoked from ol_if layer to free all FD buffers.
 *
 * Return: None
 */
void tgt_fd_free(struct wlan_objmgr_vdev *vdev);

/**
 * tgt_fd_get_valid_fd_period() - API to get valid fd_period
 * @vdev: Pointer to VDEV object
 * @is_modified: Pointer to indicate change
 *
 * This API is invoked from ol_if layer to get valid fd_period.
 *
 * Return: FD period
 */
uint32_t tgt_fd_get_valid_fd_period(struct wlan_objmgr_vdev *vdev,
				    uint8_t *is_modified);

/**
 * tgt_fd_swfda_handler() - swfda event handler
 * @vdev: Pointer to VDEV object
 *
 * This function is invoked from target if layer to handle swfda event.
 *
 * Return: QDF_STATUS_SUCCESS on success or QDF_STATUS_E_INVAL for failure
 */
QDF_STATUS tgt_fd_swfda_handler(struct wlan_objmgr_vdev *vdev);

#endif /* _WLAN_FD_TGT_API_H_ */


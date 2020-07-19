/*
 * Copyright (c) 2010, 2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2018 The Linux Foundation. All rights reserved.
 */

/**
 * DOC: Add purpose of this file
 */
/**
 * DOC: wlan_cp_stats_da_api.c
 *
 * This file provide definitions for following
 * - (de)init cp stat global ctx obj
 * - (de)init common specific ucfg handler
 * - (de)register to WMI events for psoc open
 */
#include <wlan_cp_stats_da_api.h>
#include <wlan_cp_stats_ic_ucfg_api.h>

QDF_STATUS wlan_cp_stats_psoc_obj_init_da(struct psoc_cp_stats *psoc_cs)
{
	qdf_spinlock_create(&psoc_cs->psoc_cp_stats_lock);
	wlan_cp_stats_psoc_cs_init(psoc_cs);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_cp_stats_psoc_obj_deinit_da(struct psoc_cp_stats *psoc_cs)
{
	wlan_cp_stats_psoc_cs_deinit(psoc_cs);
	qdf_spinlock_destroy(&psoc_cs->psoc_cp_stats_lock);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_cp_stats_pdev_obj_init_da(struct pdev_cp_stats *pdev_cs)
{
	qdf_spinlock_create(&pdev_cs->pdev_cp_stats_lock);
	wlan_cp_stats_pdev_cs_init(pdev_cs);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_cp_stats_pdev_obj_deinit_da(struct pdev_cp_stats *pdev_cs)
{
	wlan_cp_stats_pdev_cs_deinit(pdev_cs);
	qdf_spinlock_destroy(&pdev_cs->pdev_cp_stats_lock);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_cp_stats_vdev_obj_init_da(struct vdev_cp_stats *vdev_cs)
{
	qdf_spinlock_create(&vdev_cs->vdev_cp_stats_lock);
	wlan_cp_stats_vdev_cs_init(vdev_cs);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_cp_stats_vdev_obj_deinit_da(struct vdev_cp_stats *vdev_cs)
{
	wlan_cp_stats_vdev_cs_deinit(vdev_cs);
	qdf_spinlock_destroy(&vdev_cs->vdev_cp_stats_lock);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_cp_stats_peer_obj_init_da(struct peer_cp_stats *peer_cs)
{
	qdf_spinlock_create(&peer_cs->peer_cp_stats_lock);
	wlan_cp_stats_peer_cs_init(peer_cs);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_cp_stats_peer_obj_deinit_da(struct peer_cp_stats *peer_cs)
{
	wlan_cp_stats_peer_cs_deinit(peer_cs);
	qdf_spinlock_destroy(&peer_cs->peer_cp_stats_lock);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_cp_stats_open_da(struct wlan_objmgr_psoc *psoc)
{
	if (!psoc) {
		cp_stats_err("PSOC is null!");
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_cp_stats_close_da(struct wlan_objmgr_psoc *psoc)
{
	if (!psoc) {
		cp_stats_err("PSOC is null!");
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_cp_stats_enable_da(struct wlan_objmgr_psoc *psoc)
{
	if (!psoc) {
		cp_stats_err("PSOC is null!");
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_cp_stats_disable_da(struct wlan_objmgr_psoc *psoc)
{
	if (!psoc) {
		cp_stats_err("PSOC is null!");
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_cp_stats_ctx_init_da(struct cp_stats_context *csc)
{
	csc->cp_stats_open = wlan_cp_stats_open_da;
	csc->cp_stats_close = wlan_cp_stats_close_da;
	csc->cp_stats_enable = wlan_cp_stats_enable_da;
	csc->cp_stats_disable = wlan_cp_stats_disable_da;
	csc->cp_stats_psoc_obj_init = wlan_cp_stats_psoc_obj_init_da;
	csc->cp_stats_psoc_obj_deinit = wlan_cp_stats_psoc_obj_deinit_da;
	csc->cp_stats_pdev_obj_init = wlan_cp_stats_pdev_obj_init_da;
	csc->cp_stats_pdev_obj_deinit = wlan_cp_stats_pdev_obj_deinit_da;
	csc->cp_stats_vdev_obj_init = wlan_cp_stats_vdev_obj_init_da;
	csc->cp_stats_vdev_obj_deinit = wlan_cp_stats_vdev_obj_deinit_da;
	csc->cp_stats_peer_obj_init = wlan_cp_stats_peer_obj_init_da;
	csc->cp_stats_peer_obj_deinit = wlan_cp_stats_peer_obj_deinit_da;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_cp_stats_ctx_deinit_da(struct cp_stats_context *csc)
{
	csc->cp_stats_open = NULL;
	csc->cp_stats_close = NULL;
	csc->cp_stats_enable = NULL;
	csc->cp_stats_disable = NULL;
	csc->cp_stats_psoc_obj_init = NULL;
	csc->cp_stats_psoc_obj_deinit = NULL;
	csc->cp_stats_pdev_obj_init = NULL;
	csc->cp_stats_pdev_obj_deinit = NULL;
	csc->cp_stats_vdev_obj_init = NULL;
	csc->cp_stats_vdev_obj_deinit = NULL;
	csc->cp_stats_peer_obj_init = NULL;
	csc->cp_stats_peer_obj_deinit = NULL;

	return QDF_STATUS_SUCCESS;
}

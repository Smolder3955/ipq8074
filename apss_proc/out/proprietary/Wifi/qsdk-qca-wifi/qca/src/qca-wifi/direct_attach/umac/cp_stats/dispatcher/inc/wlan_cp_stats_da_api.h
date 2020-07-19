/*
 * Copyright (c) 2010, 2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2018 The Linux Foundation. All rights reserved.
 */

/**
 * DOC: wlan_cp_stats_da_api.h
 *
 * Header file hold declarations specific to DA
 */
#ifndef __WLAN_CP_STATS_DA_API_H__
#define __WLAN_CP_STATS_DA_API_H__

#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_objmgr_cmn.h>
#include <wlan_cp_stats_ucfg_api.h>

/**
 * wlan_cp_stats_psoc_obj_init_da() - private API to init psoc cp stats obj
 * @psoc_cs: pointer to psoc cp stat object
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS wlan_cp_stats_psoc_obj_init_da(struct psoc_cp_stats *psoc_cs);

/**
 * wlan_cp_stats_psoc_obj_deinit_da() - private API to deinit psoc cp stats
 * obj
 * @psoc_cs: pointer to psoc cp stat object
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS wlan_cp_stats_psoc_obj_deinit_da(struct psoc_cp_stats *psoc_cs);

/**
 * wlan_cp_stats_pdev_obj_init_da() - private API to init pdev cp stats obj
 * @pdev_cs: pointer to pdev cp stat object
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS wlan_cp_stats_pdev_obj_init_da(struct pdev_cp_stats *pdev_cs);

/**
 * wlan_cp_stats_pdev_obj_deinit_da() - private API to deinit pdev cp stats
 * obj
 * @pdev_cs: pointer to pdev cp stat object
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS wlan_cp_stats_pdev_obj_deinit_da(struct pdev_cp_stats *pdev_cs);

/**
 * wlan_cp_stats_vdev_obj_init_da() - private API to init vdev cp stats obj
 * @vdev_cs: pointer to vdev cp stat object
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS wlan_cp_stats_vdev_obj_init_da(struct vdev_cp_stats *vdev_cs);

/**
 * wlan_cp_stats_vdev_obj_deinit_da() - private API to deinit vdev cp stats
 * obj
 * @vdev_cs: pointer to vdev cp stat object
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS wlan_cp_stats_vdev_obj_deinit_da(struct vdev_cp_stats *vdev_cs);

/**
 * wlan_cp_stats_peer_obj_init_da() - private API to init peer cp stats obj
 * @peer_cs: pointer to peer cp stat object
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS wlan_cp_stats_peer_obj_init_da(struct peer_cp_stats *peer_cs);

/**
 * wlan_cp_stats_peer_obj_deinit_da() - private API to deinit peer cp stats
 * obj
 * @peer_cs: pointer to peer cp stat object
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS wlan_cp_stats_peer_obj_deinit_da(struct peer_cp_stats *peer_cs);

/**
 * wlan_cp_stats_open_da() - private API for psoc open
 * @psoc: pointer to psoc object
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS wlan_cp_stats_open_da(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_cp_stats_close_da() - private API for psoc close
 * @psoc: pointer to psoc object
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS wlan_cp_stats_close_da(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_cp_stats_enable_da() - private API for psoc enable
 * @psoc: pointer to psoc enable
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS wlan_cp_stats_enable_da(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_cp_stats_disable_da() - private API for psoc disable
 * @psoc: pointer to psoc enable
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS wlan_cp_stats_disable_da(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_cp_stats_ctx_init_da() - private API to initialize cp stat global ctx
 * @csc: pointer to cp stats global context object
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
#if DA_SUPPORT
QDF_STATUS wlan_cp_stats_ctx_init_da(struct cp_stats_context *csc);

/**
 * wlan_cp_stats_ctx_deinit_da() - private API to deinit cp stat global ctx
 * @csc: pointer to cp stats global context object
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS wlan_cp_stats_ctx_deinit_da(struct cp_stats_context *csc);
#else
static inline
QDF_STATUS wlan_cp_stats_ctx_init_da(struct cp_stats_context *csc)
{
    return QDF_STATUS_SUCCESS;
}
static inline
QDF_STATUS wlan_cp_stats_ctx_deinit_da(struct cp_stats_context *csc)
{
    return QDF_STATUS_SUCCESS;
}
#endif
#endif /* QCA_SUPPORT_CP_STATS */
#endif /* __WLAN_CP_STATS_DA_API_H__ */

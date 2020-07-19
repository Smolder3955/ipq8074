/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc..
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#ifndef _WLAN_SA_API_UTILS_API_H_
#define _WLAN_SA_API_UTILS_API_H_

#include <wlan_objmgr_cmn.h>

QDF_STATUS
wlan_sa_api_psoc_obj_create_handler(struct wlan_objmgr_psoc *psoc, void *arg);
QDF_STATUS
wlan_sa_api_psoc_obj_destroy_handler(struct wlan_objmgr_psoc *psoc, void *arg);
QDF_STATUS
wlan_sa_api_pdev_obj_create_handler(struct wlan_objmgr_pdev *pdev, void *arg);
QDF_STATUS
wlan_sa_api_pdev_obj_destroy_handler(struct wlan_objmgr_pdev *pdev, void *arg);
QDF_STATUS
wlan_sa_api_peer_obj_create_handler(struct wlan_objmgr_peer *peer, void *arg);
QDF_STATUS
wlan_sa_api_peer_obj_destroy_handler(struct wlan_objmgr_peer *peer, void *arg);

QDF_STATUS
wlan_sa_api_init(void);
QDF_STATUS
wlan_sa_api_deinit(void);
QDF_STATUS
wlan_sa_api_enable(struct wlan_objmgr_psoc *psoc);
QDF_STATUS
wlan_sa_api_disable(struct wlan_objmgr_psoc *psoc);

int wlan_sa_api_start(struct wlan_objmgr_pdev *pdev, struct wlan_objmgr_vdev *vdev, int new_init);
int wlan_sa_api_stop(struct wlan_objmgr_pdev *pdev, struct wlan_objmgr_vdev *vdev, int notify);
void wlan_sa_api_peer_connect(struct wlan_objmgr_pdev *pdev, struct wlan_objmgr_peer *peer, struct sa_rate_cap *rate_cap);
void wlan_sa_api_peer_disconnect(struct wlan_objmgr_peer *peer);
int wlan_sa_api_get_bcn_txantenna(struct wlan_objmgr_pdev *pdev, uint32_t *ant);
void wlan_sa_api_channel_change(struct wlan_objmgr_pdev *pdev);
int wlan_sa_api_cwm_action(struct wlan_objmgr_pdev  *pdev);

#endif /* _WLAN_SA_API_UTILS_API_H_*/

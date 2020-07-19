/*
 * Copyright (c) 2011-2014,2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2014 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2010-2011, Atheros Communications Inc.
 */
#include "dp_txrx.h"

#ifndef __DP_LAG_H_
#define __DP_LAG_H_

#define DP_MAX_RADIO_CNT 3

#include <cdp_txrx_cmn.h>

struct dp_pdev_link_aggr;

/**
 * struct dp_soc_link_aggr - SoC level handle for Link Aggregation
 * @soc: DP SoC handle
 * @enable: Enable DBDC repeater
 * @ast_handle: Context pointer to Address Search Table
 * @num_stavaps_up: Number of STA VAPs up in the SoC
 * @is_multilink: Is the link to RootAP multiband
 * @always_primary: Use only primary link to send frames to Root AP
 * @force_client_mcast_traffic: Set this flag when Repeater wants to send
 * mcast traffic of connected clients to RootAP on corresponding STA VAP
 */
typedef struct dp_soc_link_aggr {
    struct cdp_soc_t *soc;
    bool enable;
    void *ast_handle;
    struct dp_pdev_link_aggr *pdev[DP_MAX_RADIO_CNT];
    int num_radios;
    int num_stavaps_up;
    bool is_multilink;
    bool always_primary;
    bool force_client_mcast_traffic;
    bool drop_secondary_mcast;
    bool ast_override_support;
} dp_soc_link_aggr_t;

/**
 * struct dp_pdev_link_aggr - pdev level handle for Link Aggregation
 * @soc_lag: Soc handle for Link Aggregation
 * @pdev_id: Radio ID across all radios in the platform
 * @priority: Radio priority
 * @is_primary: Is this primary link?
 * @ap_vdev: Connected AP vdev
 * @sta_vdev:  Connected STA vdev
 */
typedef struct dp_pdev_link_aggr {
    struct dp_soc_link_aggr *soc_lag;
    uint8_t pdev_id;
    uint8_t priority;
    bool is_primary;
    void *ap_vdev;
    void *sta_vdev;
} dp_pdev_link_aggr_t;

void dp_lag_pdev_set_priority(struct wlan_objmgr_pdev *pdev, uint8_t value);
void dp_lag_pdev_set_primary_radio(struct wlan_objmgr_pdev *pdev, bool value);
void dp_lag_pdev_set_sta_vdev(struct wlan_objmgr_pdev *pdev, void *vdev);
void dp_lag_pdev_set_ap_vdev(struct wlan_objmgr_pdev *pdev, void *vdev);
void dp_lag_soc_enable(struct wlan_objmgr_pdev *pdev, bool value);
void dp_lag_soc_set_force_client_mcast_traffic(struct wlan_objmgr_pdev *pdev, bool value);
void dp_lag_soc_set_always_primary(struct wlan_objmgr_pdev *pdev, bool value);
void dp_lag_soc_set_num_sta_vdev(struct wlan_objmgr_pdev *pdev, int num);
void dp_lag_soc_set_drop_secondary_mcast(struct wlan_objmgr_pdev *pdev, bool flag);
bool dp_lag_soc_is_multilink(struct wlan_objmgr_vdev *vdev);
bool dp_lag_tx_process(struct wlan_objmgr_vdev *vdev, qdf_nbuf_t nbuf, bool vap_send);
bool dp_lag_rx_process(struct wlan_objmgr_vdev *vdev, qdf_nbuf_t nbuf, bool vap_send);
void dp_lag_pdev_init(struct dp_pdev_link_aggr *pdev_lag, dp_soc_link_aggr_t *soc_lag, uint8_t pdev_id);
bool dp_lag_is_enabled(struct wlan_objmgr_vdev *vdev);
bool dp_lag_pdev_is_primary(struct wlan_objmgr_vdev *vdev);
#endif

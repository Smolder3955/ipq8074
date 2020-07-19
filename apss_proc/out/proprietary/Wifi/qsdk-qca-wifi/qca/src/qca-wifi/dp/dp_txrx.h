/*
 * Copyright (c) 2017, 2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#ifndef _DP_TXRX_H
#define _DP_TXRX_H

#include "dp_extap_mitbl.h"
#include "dp_link_aggr.h"
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>

typedef struct dp_extap {
	mi_node_t *miroot;    /* EXTAP MAC - IP table Root */
} dp_extap_t;

typedef struct dp_txrx_handle {
	dp_extap_t extap_hdl; /* Extap handler */
	dp_pdev_link_aggr_t lag_hdl; /* Link Aggregation handle */
} dp_txrx_handle_t;

typedef struct dp_soc_txrx_handle {
	dp_soc_link_aggr_t lag_hdl; /* Link Aggregation handle */
} dp_soc_txrx_handle_t;

static inline dp_pdev_link_aggr_t *dp_pdev_get_lag_handle(struct wlan_objmgr_pdev *pdev)
{
    ol_txrx_soc_handle soc;
    dp_txrx_handle_t *dp_hdl;

    soc = wlan_psoc_get_dp_handle(wlan_pdev_get_psoc(pdev));

    if (!soc)
        return NULL;

    dp_hdl = cdp_pdev_get_dp_txrx_handle(soc,
            wlan_pdev_get_dp_handle(pdev));

    if (!dp_hdl)
        return NULL;

    return &dp_hdl->lag_hdl;
}

static inline dp_soc_link_aggr_t *dp_soc_get_lag_handle(struct wlan_objmgr_psoc *soc)
{
    dp_soc_txrx_handle_t *dp_hdl;

    if (!soc)
        return NULL;

    dp_hdl = cdp_soc_get_dp_txrx_handle(wlan_psoc_get_dp_handle(soc));

    if (!dp_hdl)
        return NULL;

    return &dp_hdl->lag_hdl;
}


/**
 * dp_get_lag_handle() - get link aggregation handle from vdev
 * @vdev: vdev object pointer
 *
 * Return: pdev Link Aggregation handle
 */
static inline dp_pdev_link_aggr_t *dp_get_lag_handle(struct wlan_objmgr_vdev *vdev)
{
    struct wlan_objmgr_pdev *pdev;

    pdev = wlan_vdev_get_pdev(vdev);

    return dp_pdev_get_lag_handle(pdev);
}

#endif /* _DP_TXRX_H */

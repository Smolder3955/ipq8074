/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
*/

/*
 *This File provides framework direct attach architecture for SON.
*/
#ifndef __WLAN_SON_SUPPORT_TGT_API_H_
#define __WLAN_SON_SUPPORT_TGT_API_H_

#include <wlan_objmgr_cmn.h>
#include <qdf_status.h>

struct wlan_lmac_if_son_tx_ops {
	/* Function pointer to enable/disable band steering */
	bool (*son_enable)(struct wlan_objmgr_pdev *pdev, bool enable);

	int (*lmac_create)(struct wlan_objmgr_pdev *pdev);

	int (*lmac_destroy)(struct wlan_objmgr_pdev *pdev);

	QDF_STATUS (*son_send_null)(struct wlan_objmgr_pdev *pdev,
				    u_int8_t *macaddr,
				    struct wlan_objmgr_vdev *vdev);
	void (*son_rssi_update)(struct wlan_objmgr_pdev *pdev ,u_int8_t *macaddr,
				       u_int8_t status ,int8_t rssi, u_int8_t subtype);
	void (*son_rate_update)(struct wlan_objmgr_pdev *pdev, u_int8_t *macaddres,
				       u_int8_t status ,u_int32_t rateKbps);
	int8_t (*son_sanity_util_intvl)(struct wlan_objmgr_pdev *pdev, u_int32_t *sample_period,
				       u_int32_t *num_of_sample);
	u_int32_t               (*get_peer_rate)(struct wlan_objmgr_peer *peer, u_int8_t type);
};

struct wlan_lmac_if_son_rx_ops {
};

#endif

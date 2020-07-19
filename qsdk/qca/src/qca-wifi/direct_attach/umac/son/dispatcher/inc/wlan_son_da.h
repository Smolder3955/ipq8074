/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
*/

/*
 *This File provides framework direct attach architecture for SON.
*/

#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include <qdf_status.h>

#ifndef __QCA_SUPPORT_SON_DA__
#define __QCA_SUPPORT_SON_DA__

/**
 * @brief Get correct sample interval for channel utlization for direct
 * attach hardware.
 * @param [in] pdev under execution.
 * @param [in] sample period
 * @param [in] number of sample.
 * @return EOK if successful otherwise -EINVAL
 */

int8_t son_da_sanitize_util_intvl(struct wlan_objmgr_pdev *pdev,
				  u_int32_t *sample_period,
				  u_int32_t *num_of_sample);

u_int32_t son_da_get_peer_rate(struct wlan_objmgr_peer *peer, u_int8_t type);

bool son_da_enable (struct wlan_objmgr_pdev *pdev, bool enable);

/* Function pointer to set overload status */

void son_da_set_overload (struct wlan_objmgr_pdev *pdev, bool overload);

/* Function pointer to set band steering parameters */

bool son_da_set_params(struct wlan_objmgr_pdev *dev,
			     u_int32_t inactivity_check_period,
			     u_int32_t inactivity_threshold_normal,
			     u_int32_t inactivity_threshold_overload);

QDF_STATUS son_da_send_null(struct wlan_objmgr_pdev *pdev,
			    u_int8_t *macaddr,
			    struct wlan_objmgr_vdev *vdev);

int son_da_lmac_create(struct wlan_objmgr_pdev *pdev);

int son_da_lmac_destroy(struct wlan_objmgr_pdev *pdev);

void  son_da_rx_rssi_update(struct wlan_objmgr_pdev *pdev ,u_int8_t *macaddres,
			    u_int8_t status ,int8_t rssi, u_int8_t subtype);

void son_da_rx_rate_update(struct wlan_objmgr_pdev *pdev, u_int8_t *macaddres,
			   u_int8_t status ,u_int32_t rateKbps);

bool  son_da_is_peer_inact(struct wlan_objmgr_peer *peer);

#endif /*__QCA_SUPPORT_SON_DA__*/

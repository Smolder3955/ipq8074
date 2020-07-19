/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*/

/*
 *This File provides API for UCFG framework for SON.
 */

#if !defined (__WLAN_SON_SUPPORT_UCFG_H_)
#define __WLAN_SON_SUPPORT_UCFG_H_
struct ieee80211req_athdbg;

#include "wlan_son_types.h"
#include <wlan_utility.h>

/**
 * @brief SON dispatcher API for user space API.
 *
 * @param [inout] vdev  the VAP whose band steering status
 *                     changes
 *
 * @param [in] action   set, get or netlink event .
 *
 * @param [in] data

 * @return EOK for sucessfull or error code .
 */

int ucfg_son_dispatcher(struct wlan_objmgr_vdev *vdev,
		   SON_DISPATCHER_CMD cmd,
		   SON_DISPATCHER_ACTION action, void *data);


int8_t ucfg_son_set_scaling_factor(struct wlan_objmgr_vdev *vdev,
				   int8_t scaling_factor);

u_int8_t ucfg_son_get_scaling_factor(struct wlan_objmgr_vdev *vdev);

int8_t ucfg_son_set_skip_hyst(struct wlan_objmgr_vdev *vdev,
				   int8_t skip_hyst);

u_int8_t ucfg_son_get_skip_hyst(struct wlan_objmgr_vdev *vdev);

int son_ucfg_rep_datarate_estimator(u_int16_t backhaul_rate,
				    u_int16_t ap_rate,
				    u_int8_t root_distance,
				    u_int8_t scaling_factor);

int ucfg_son_find_best_uplink_bssid(struct wlan_objmgr_vdev *vdev, char *bssid, struct wlan_ssid *ssidname);

int son_ucfg_find_cap_bssid(struct wlan_objmgr_vdev *vdev, char *bssid);

int ucfg_son_set_otherband_bssid(struct wlan_objmgr_vdev *vdev, int *val);
int ucfg_son_get_best_otherband_uplink_bssid(struct wlan_objmgr_vdev *vdev, char *bssid);
int ucfg_son_get_otherband_uplink_bssid(struct wlan_objmgr_vdev *vdev, char *addr);

int8_t ucfg_son_set_uplink_rate(struct wlan_objmgr_vdev *vdev,
				u_int32_t uplink_rate);
int16_t ucfg_son_get_uplink_rate(struct wlan_objmgr_vdev *vdev);

int8_t ucfg_son_get_uplink_snr(struct wlan_objmgr_vdev *vdev);

int8_t ucfg_son_get_cap_rssi(struct wlan_objmgr_vdev *vdev);

int8_t ucfg_son_set_cap_rssi(struct wlan_objmgr_vdev *vdev,
			     u_int32_t rssi);
int ucfg_son_get_cap_snr(struct wlan_objmgr_vdev *vdev, int *cap_snr);
#endif /*__WLAN_SON_SUPPORT_UCFG_H_*/


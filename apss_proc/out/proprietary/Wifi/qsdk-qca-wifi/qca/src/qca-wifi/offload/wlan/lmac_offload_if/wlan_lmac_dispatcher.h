/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2011, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/**
 * DOC: wlan_lmac_dispatcher.h
 *
 * Public APIs to leagcy dispatch
 */

#ifndef _INIT_DEINIT_OPS_H_
#define _INIT_DEINIT_OPS_H_

#include <wlan_objmgr_psoc_obj.h>
#include <wmi_unified_param.h>

#define BTCOEX_GPIO_PARAM 1
#define BTCOEX_ENABLE     2
#define BTCOEX_DUTY_CYCLE 3
#define BTCOEX_WL_PRIORITY 4
#define BTCOEX_DURATION 5
#define BTCOEX_PERIOD  6
#define BTCOEX_DURATION_PERIOD 7

#define MAX_ACTIVE_PEERS 1
#define VOW_CONFIG       2
#define AC_BK_MINFREE    3
#define AC_BE_MINFREE    4
#define AC_VI_MINFREE    5
#define AC_VO_MINFREE    6
#define SOC_ID           7
#define MESH_SUPPORT     8
#define IPHDR_PAD        9
#define BYPASSWMI        10
#define LOW_MEMSYS       11
#define SCHED_PARAMS     12
#define CCE_STATE        13
#define TGT_IRAM_BKP_ADDR  14
#define EAPOL_MINRATE_SET    15
#define EAPOL_MINRATE_AC_SET 16
#define CARRIER_VOW_CONFIG 17
#define FW_VOW_STATS_ENABLE 18

#define IRAM_BKP_PADDR_REQ_ID 8
/**
 * wlan_ucfg_get_btcoex_param()- To get btcoex parameters
 * @psoc: PSOC object
 * @param_type: type of parameter
 *
 * API to get btcoex parameters
 *
 * Return: btcoex parameter
 */
int wlan_ucfg_get_btcoex_param(struct wlan_objmgr_psoc *psoc,
				uint16_t param_type);
/**
 * wlan_ucfg_set_btcoex_param()- To set btcoex parameters
 * @psoc: PSOC object
 * @param_type: type of parameter
 * @value: value to be set for the parameters
 * @value1: value corresponding to period
 *
 * API to set btcoex parameters
 *
 * Return: SUCCESS on successful parameter assignment or FAILURE
 */
QDF_STATUS wlan_ucfg_set_btcoex_param(struct wlan_objmgr_psoc *psoc,
			uint16_t param_type, int value, int value1);

/**
 * wlan_ucfg_get_config_param()- To get config parameters
 * @psoc: PSOC object
 * @param_type: type of parameter
 *
 * API to get config  parameters
 *
 * Return: config parameter
 */
uint32_t wlan_ucfg_get_config_param(struct wlan_objmgr_psoc *psoc,
				uint16_t param_type);

/**
 * wlan_create_pdev_netdevice()- To create pdev netdevice
 * @psoc: PSOC object
 * @id: device id
 *
 * API to create pdev netdevice
 *
 * Return: Device created
 */
qdf_netdev_t wlan_create_pdev_netdevice(struct wlan_objmgr_psoc *psoc,
						uint8_t id);

/**
 * wlan_pdev_update_feature_ptr()- Updates feature ptr
 * @psoc: PSOC object
 * @id: device id
 *
 * API to update feature pointer
 *
 * Return: None
 */
void wlan_pdev_update_feature_ptr(struct wlan_objmgr_psoc *psoc,
		struct wlan_objmgr_pdev *pdev, qdf_netdev_t pdev_netdev);


void wlan_psoc_update_peer_count(struct wlan_objmgr_psoc *psoc,
	target_resource_config *tgt_cfg);
#endif /* _INIT_DEINIT_OPS_H_ */

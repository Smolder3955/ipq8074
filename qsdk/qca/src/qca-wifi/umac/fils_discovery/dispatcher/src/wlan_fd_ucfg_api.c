/*
 *
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#include <wlan_fd_ucfg_api.h>
#include <wlan_fd_utils_api.h>
#include "../../core/fd_priv_i.h"

uint32_t ucfg_fd_get_enable_period(uint32_t value1, uint32_t value2)
{
	if (value1) {
		value1 = (1 << WLAN_FILS_ENABLE_BIT);
		value2 &= WLAN_FD_PERIOD_MASK;
	} else {
		value2 = 0;
	}

	return (value1 | value2);
}

void ucfg_fils_config(struct wlan_objmgr_vdev *vdev, uint32_t value)
{
	struct fd_vdev *fv;

	if (vdev == NULL) {
		fd_err("VDEV is NULL!!\n");
		return;
	}
	fv = wlan_objmgr_vdev_get_comp_private_obj(vdev, WLAN_UMAC_COMP_FD);
	if (fv == NULL) {
		fd_err("FILS Discovery obj is NULL\n");
		return;
	}

	if (value & (1 << WLAN_FILS_ENABLE_BIT)) {
		fd_info("FILS Enable\n");
		fv->fils_enable = 1;
	} else {
		fv->fils_enable = 0;
		fd_info("FILS Disable\n");
	}

	fd_vdev_config_fils(vdev, value);
}

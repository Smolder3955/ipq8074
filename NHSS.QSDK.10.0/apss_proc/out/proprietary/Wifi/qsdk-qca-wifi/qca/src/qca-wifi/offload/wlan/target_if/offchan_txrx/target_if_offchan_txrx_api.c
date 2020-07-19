/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc..
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wmi_unified_api.h>
#include <target_if.h>
#include <target_if_offchan_txrx_api.h>
#include <init_deinit_lmac.h>


static bool target_if_offchan_txrx_data_tid_support(struct wlan_objmgr_pdev *pdev)
{
	return wmi_service_enabled(lmac_get_pdev_wmi_handle(pdev),
				wmi_service_offchan_data_tid_support);
}

void target_if_offchan_txrx_ops_register(struct wlan_lmac_if_tx_ops *tx_ops)
{
	tx_ops->offchan_txrx_ops.offchan_data_tid_support =
				target_if_offchan_txrx_data_tid_support;
}


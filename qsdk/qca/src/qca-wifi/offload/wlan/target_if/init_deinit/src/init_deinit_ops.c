/*
 * Copyright (c) 2017-2019 Qualcomm Innovation Center, Inc.
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
 * DOC: init_deinit_ops.c.c
 *
 * API to WIN ops
 */
#include <qdf_status.h>
#include <qdf_types.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <target_if.h>
#include <wlan_tgt_def_config.h>
#include <wlan_reg_ucfg_api.h>
#include <hif.h>
#include <hif_hw_version.h>
#include <target_type.h>
#include "init_event_handler.h"
#include "service_ready_util.h"
#include "init_cmd_api.h"
#include "init_deinit_lmac.h"
#include <wlan_osif_priv.h>
#include <wlan_lmac_dispatcher.h>
#if WIFI_MEM_MANAGER_SUPPORT
#include "mem_manager.h"
#endif
#include <wlan_defs.h>
#if QCA_AIRTIME_FAIRNESS
#include <target_if_atf.h>
#endif
#if UNIFIED_SMARTANTENNA
#include <target_if_sa_api.h>
#endif /* UNIFIED_SMARTANTENNA */
#if WLAN_CFR_ENABLE
#include <target_if_cfr.h>
#endif
#include <sw_version.h>
#include <dp_txrx.h>
#include <dispatcher_init_deinit.h>
#include "cdp_txrx_cmn.h"
#include "fw_dbglog_api.h"

static uint32_t init_deinit_get_host_pltfrm_mode(uint32_t target_type)
{

	switch (target_type) {
	case TARGET_TYPE_AR6002:
		return HOST_PLATFORM_LOW_PERF;
	case TARGET_TYPE_AR6003:
		return HOST_PLATFORM_LOW_PERF;
	case TARGET_TYPE_AR6004:
		return HOST_PLATFORM_LOW_PERF;
	case TARGET_TYPE_AR6006:
		return HOST_PLATFORM_LOW_PERF;
	case TARGET_TYPE_AR9888:
		return HOST_PLATFORM_LOW_PERF;
	case TARGET_TYPE_AR6320:
		return HOST_PLATFORM_LOW_PERF;
#if PEER_FLOW_CONTROL_FORCED_MODE0
	case TARGET_TYPE_AR900B:
		return HOST_PLATFORM_LOW_PERF_NO_FETCH;
	case TARGET_TYPE_QCA9984:
		return HOST_PLATFORM_LOW_PERF_NO_FETCH;
	case TARGET_TYPE_QCA9888:
		return HOST_PLATFORM_LOW_PERF_NO_FETCH;
#elif MIPS_LOW_PERF_SUPPORT
	case TARGET_TYPE_AR900B:
		return HOST_PLATFORM_LOW_PERF;
	case TARGET_TYPE_QCA9984:
		return HOST_PLATFORM_LOW_PERF;
	case TARGET_TYPE_QCA9888:
		return HOST_PLATFORM_LOW_PERF;
#else
	case TARGET_TYPE_AR900B:
		return HOST_PLATFORM_HIGH_PERF;
	case TARGET_TYPE_QCA9984:
		return HOST_PLATFORM_HIGH_PERF;
	case TARGET_TYPE_QCA9888:
		return HOST_PLATFORM_HIGH_PERF;
#endif
	case TARGET_TYPE_IPQ4019:
		return HOST_PLATFORM_HIGH_PERF;
	default:
		target_if_err("!!! Invalid Target Type %d !!!",
			target_type);
		return -EINVAL;
	}
	return -EINVAL;
}



static void init_deinit_coex_gpio_support(struct wlan_objmgr_psoc *psoc,
			 struct target_psoc_info *tgt_hdl, void *wmi_handle)
{

	struct tgt_info *info;

	if (!psoc) {
		target_if_err("psoc is null");
		return;
	}

	if (!tgt_hdl) {
		target_if_err("psoc target_psoc_info is null");
		return;
	}
	info = (&tgt_hdl->info);

	if (wmi_service_enabled(wmi_handle, wmi_service_coex_gpio)) {
		if (info->target_type == TARGET_TYPE_IPQ4019) {
			/* Enable btcoex only for specific boards with btcoex
			 * HW as btcoex GPIO pins may be used for different
			 * purposes on other boards without btcoex HW.
			 * The HW support information is obtained from DT
			 * during init time.
			*/
			if (wlan_psoc_nif_feat_cap_get(psoc,
					WLAN_SOC_F_BTCOEX_SUPPORT)) {
				info->wlan_ext_res_cfg.fw_feature_bitmap |=
					WMI_HOST_FW_FEATURE_COEX_GPIO_SUPPORT;
				info->wlan_ext_res_cfg.wlan_priority_gpio =
					wlan_ucfg_get_btcoex_param(psoc,
							BTCOEX_GPIO_PARAM);

				if (wmi_service_enabled(wmi_handle,
					wmi_service_btcoex_duty_cycle)) {
					wlan_ucfg_set_btcoex_param(psoc,
						BTCOEX_DUTY_CYCLE, 1, 0);
				}
			}
		} else {
			info->wlan_ext_res_cfg.fw_feature_bitmap |=
				WMI_HOST_FW_FEATURE_COEX_GPIO_SUPPORT;
			info->wlan_ext_res_cfg.wlan_priority_gpio = 0;
			wlan_psoc_nif_feat_cap_set(psoc,
					WLAN_SOC_F_BTCOEX_SUPPORT);
		}
	}
}

static QDF_STATUS init_deinit_ext_resource_config_enable(
		 struct wlan_objmgr_psoc *psoc,
		 struct target_psoc_info *tgt_hdl, uint8_t *event)
{
	struct common_wmi_handle *wmi_handle;
	struct tgt_info *info;

	if (!tgt_hdl) {
		target_if_err("target_psoc_info is null");
		return QDF_STATUS_E_INVAL;
	}

	wmi_handle = target_psoc_get_wmi_hdl(tgt_hdl);
	info = (&tgt_hdl->info);

	if (wmi_service_enabled(wmi_handle,
				wmi_service_ext_res_cfg_support)) {
		info->wlan_ext_res_cfg.host_platform_config =
			init_deinit_get_host_pltfrm_mode(info->target_type);
		if (info->wlan_ext_res_cfg.host_platform_config < 0) {
			target_if_err(
			"!!! Host Mode Selection for %d TGT Type FAILED !!!",
				info->target_type);
			return QDF_STATUS_E_FAILURE;
		}
		wlan_psoc_nif_fw_ext_cap_set(psoc, WLAN_SOC_CEXT_HYBRID_MODE);
		if (wlan_psoc_nif_feat_cap_get(psoc, WLAN_SOC_F_LTEU_SUPPORT)) {
			info->wlan_ext_res_cfg.fw_feature_bitmap |=
					WMI_HOST_FW_FEATURE_LTEU_SUPPORT;
			target_if_info("LTEu enabled ");
		}
		init_deinit_coex_gpio_support(psoc, tgt_hdl, wmi_handle);
		info->wlan_ext_res_cfg.fw_feature_bitmap |=
				WMI_HOST_FW_FEATURE_VDEV_STATS;

		if (!ol_target_lithium(psoc)) {
			if (wlan_ucfg_get_config_param(psoc, CARRIER_VOW_CONFIG)) {
				target_if_info("CARRIER_VOW_CONFIG is enabled\n");
				info->wlan_ext_res_cfg.fw_feature_bitmap |= WMI_HOST_FW_FEATURE_VOW_FEATURES;
				if (wlan_ucfg_get_config_param(psoc, FW_VOW_STATS_ENABLE)) {
					target_if_info("FW_VOW_STATS is enabled\n");
					info->wlan_ext_res_cfg.fw_feature_bitmap |= WMI_HOST_FW_FEATURE_VOW_STATS;
				}
			}
		}

		wmi_send_ext_resource_config(wmi_handle,
					&info->wlan_ext_res_cfg);
	} else {
		wlan_psoc_nif_fw_ext_cap_clear(psoc, WLAN_SOC_CEXT_HYBRID_MODE);
	}

	return QDF_STATUS_SUCCESS;
}


#if PEER_CACHEING_HOST_ENABLE
static void init_deinit_peer_cache_support(struct wlan_objmgr_psoc *psoc,
			 struct target_psoc_info *tgt_hdl, void *wmi_handle)
{
	struct tgt_info *info;
	uint32_t num_vdevs;
	uint32_t max_active_peers;
	uint32_t low_mem;

	info = (&tgt_hdl->info);

	low_mem = wlan_ucfg_get_config_param(psoc, LOW_MEMSYS);

	num_vdevs = info->wlan_res_cfg.num_vdevs;
	info->wlan_res_cfg.num_peers = ((low_mem) ?
					(CFG_TGT_NUM_QCACHE_PEERS_MAX_LOW_MEM) :
					(CFG_TGT_NUM_QCACHE_PEERS_MAX));
	info->wlan_res_cfg.num_peers += num_vdevs;

	max_active_peers = wlan_ucfg_get_config_param(psoc, MAX_ACTIVE_PEERS);

	if ((max_active_peers) && (!info->max_descs) &&
		(max_active_peers < CFG_TGT_QCACHE_ACTIVE_PEERS)) {
		info->wlan_res_cfg.num_active_peers = max_active_peers;
	} else if (info->max_descs) {
		info->wlan_res_cfg.num_active_peers = max_active_peers;
	} else {
		info->wlan_res_cfg.num_active_peers =
						CFG_TGT_QCACHE_ACTIVE_PEERS;
	}
	info->wlan_res_cfg.num_active_peers += num_vdevs;

	info->wlan_res_cfg.num_tids = info->wlan_res_cfg.num_active_peers * 2;
	target_if_info(
		"Peer Caching Enabled ; num_peers = %d, num_active_peers = %d num_tids = %d, num_vdevs = %d",

		info->wlan_res_cfg.num_peers,
		info->wlan_res_cfg.num_active_peers,
		info->wlan_res_cfg.num_tids, info->wlan_res_cfg.num_vdevs);
}
#endif

static void init_deinit_update_large_ap_config(struct wlan_objmgr_psoc *psoc,
			 struct target_psoc_info *tgt_hdl, void *wmi_handle)
{
	struct tgt_info *info;
	uint8_t max_vdevs;

	info = (&tgt_hdl->info);

	info->wlan_res_cfg.num_peers = CFG_TGT_NUM_PEERS_MAX;

	if ((wmi_service_enabled(wmi_handle, wmi_service_rtt)) &&
		(info->wlan_res_cfg.num_peers > CFG_TGT_NUM_RTT_PEERS_MAX))
		info->wlan_res_cfg.num_peers = CFG_TGT_NUM_RTT_PEERS_MAX;

	/* Make sure that number of peers is not exceeding smart antenna's
	 * MAX suported
	 */
	if ((info->wlan_res_cfg.smart_ant_cap) &&
		(!(wlan_psoc_nif_feat_cap_get(psoc,
				WLAN_SOC_F_LTEU_SUPPORT))))
		info->wlan_res_cfg.num_peers =
			(info->wlan_res_cfg.num_peers >
				CFG_TGT_NUM_SMART_ANT_PEERS_MAX) ?
				CFG_TGT_NUM_SMART_ANT_PEERS_MAX :
					info->wlan_res_cfg.num_peers;

	if ((info->wlan_res_cfg.num_peers * 2) > CFG_TGT_NUM_TIDS_MAX)
		/* one data tid per peer */
		info->wlan_res_cfg.num_tids = info->wlan_res_cfg.num_peers;
	else if ((info->wlan_res_cfg.num_peers * 4) > CFG_TGT_NUM_TIDS_MAX)
		/* two tids per peer */
		info->wlan_res_cfg.num_tids = info->wlan_res_cfg.num_peers * 2;
	else
		/* four tids per peer */
		info->wlan_res_cfg.num_tids = info->wlan_res_cfg.num_peers * 4;

	max_vdevs = wlan_psoc_get_max_vdev_count(psoc);
	if (max_vdevs != WLAN_UMAC_PSOC_MAX_VDEVS) {
		info->wlan_res_cfg.num_vdevs = max_vdevs;
		info->wlan_res_cfg.num_peers += max_vdevs;
	} else {
		if (lmac_is_target_ar900b(psoc))
			info->wlan_res_cfg.num_peers += CFG_TGT_NUM_VDEV_AR900B;
		else
			info->wlan_res_cfg.num_peers += CFG_TGT_NUM_VDEV_AR988X;
	}
	target_if_info("LARGE_AP enabled. num_peers %d, num_vdevs %d, num_tids %d",
			info->wlan_res_cfg.num_peers,
			info->wlan_res_cfg.num_vdevs,
			info->wlan_res_cfg.num_tids);
}

static int init_deinit_get_sta_num(struct wlan_objmgr_psoc *psoc,
			struct target_psoc_info *tgt_hdl,
			int msdu_desc_size, int peer_size)
{
	/* If VoW is enabled, memory for TOTAL_VOW_ALLOCABLE number of
		descriptors are reserved
	 * for VoW statsions. To accomadate this memory, number of peers
		is reduced to 16.
	 * Incase, vow is configured such that it doesn't need all those
		memory, it can be used
	 * to support more stations.
	 */
	int sta_num = 0;
	uint32_t vow_config = wlan_ucfg_get_config_param(psoc, VOW_CONFIG);

	if (lmac_is_target_ar900b(psoc)) {
		/* As of now no need to allocate any extra station for AR900B */
		sta_num = 0;
	} else if (tgt_hdl->info.target_type == TARGET_TYPE_AR9888) {
		int num_vi_sta = VOW_GET_NUM_VI_STA(vow_config);
		int num_vi_desc_per_sta = VOW_GET_DESC_PER_VI_STA(vow_config);
		int total_vow_desc = num_vi_sta * num_vi_desc_per_sta;
		int total_free_desc = TOTAL_VOW_ALLOCABLE - total_vow_desc;
		if (total_free_desc > 0) {
			int bytes_avail = total_free_desc * msdu_desc_size;
			sta_num = bytes_avail / peer_size;
		}
	}
	return sta_num;
}

/*
 * Update target config for VoW feature.
 *  input soc - soc data structure, update target config within this structure
 */
static void init_deinit_update_vow_config(struct wlan_objmgr_psoc *psoc,
			 struct target_psoc_info *tgt_hdl, void *wmi_handle)
{
	struct tgt_info *info;
	uint8_t peer_caching = 0;

	info = (&tgt_hdl->info);
#if PEER_CACHEING_HOST_ENABLE
	peer_caching = wmi_service_enabled(wmi_handle,
					wmi_service_peer_caching);
#endif
		/*VoW enabled*/
	if (lmac_is_target_ar900b(psoc)) {
		info->wlan_res_cfg.num_vdevs = CFG_TGT_NUM_VDEV_VOW;

		if (peer_caching) {
			/* Configure VoW with Qcache enabled */
			info->wlan_res_cfg.num_peers =
					CFG_TGT_QCACHE_NUM_PEERS_VOW;
			info->wlan_res_cfg.num_active_peers =
					CFG_TGT_NUM_ACTIVE_PEERS_VOW +
						info->wlan_res_cfg.num_vdevs;
			info->wlan_res_cfg.num_tids =
				(2 * info->wlan_res_cfg.num_active_peers);
		} else {
			info->wlan_res_cfg.num_peers = CFG_TGT_NUM_PEERS_VOW +
					info->wlan_res_cfg.num_vdevs;
			info->wlan_res_cfg.num_tids = 2 *
						info->wlan_res_cfg.num_peers;
		}

	} else if (info->target_type == TARGET_TYPE_AR9888) {
		info->wlan_res_cfg.num_vdevs = CFG_TGT_NUM_VDEV_VOW;
		info->wlan_res_cfg.num_peers = CFG_TGT_NUM_PEERS_VOW +
			CFG_TGT_NUM_VDEV_VOW + init_deinit_get_sta_num(psoc,
				tgt_hdl, MSDU_DESC_SIZE, MEMORY_REQ_FOR_PEER);
		info->wlan_res_cfg.num_tids = 2 *
				(info->wlan_res_cfg.num_vdevs +
					info->wlan_res_cfg.num_peers);
		info->wlan_res_cfg.num_wds_entries = CFG_TGT_WDS_ENTRIES_VOW;
	}

	/* Both Host and Firmware has Smart Antenna support */
	if ((info->wlan_res_cfg.smart_ant_cap) &&
		(!(wlan_psoc_nif_feat_cap_get(psoc,
				WLAN_SOC_F_LTEU_SUPPORT))))
		info->wlan_res_cfg.num_peers =
			((info->wlan_res_cfg.num_peers >
				CFG_TGT_NUM_SMART_ANT_PEERS_MAX) ?
				CFG_TGT_NUM_SMART_ANT_PEERS_MAX :
					(info->wlan_res_cfg.num_peers));

	info->wlan_res_cfg.vow_config = wlan_ucfg_get_config_param(
							psoc, VOW_CONFIG);
	target_if_info("VoW Enabled: Num peers = %d Num vdevs = %d Num TIDs = %d",
			info->wlan_res_cfg.num_peers,
			info->wlan_res_cfg.num_vdevs,
			info->wlan_res_cfg.num_tids);
}

static void init_deinit_peer_config(
		 struct wlan_objmgr_psoc *psoc,
		 struct target_psoc_info *tgt_hdl, uint8_t *event)
{
	struct common_wmi_handle *wmi_handle;
	struct tgt_info *info;
	uint32_t vow_config;
	uint8_t peer_caching = 0;
	uint8_t ratectrl_cache;
	uint8_t iram_tids;

	if (!tgt_hdl) {
		target_if_err("psoc target_psoc_info is null");
		return;
	}

	wmi_handle = target_psoc_get_wmi_hdl(tgt_hdl);
	info = (&tgt_hdl->info);
#if PEER_CACHEING_HOST_ENABLE
	peer_caching = wmi_service_enabled(wmi_handle,
					wmi_service_peer_caching);
#endif
	vow_config = wlan_ucfg_get_config_param(psoc, VOW_CONFIG);

	if (peer_caching && !(vow_config >> 16)) {
#if PEER_CACHEING_HOST_ENABLE
		init_deinit_peer_cache_support(psoc, tgt_hdl, wmi_handle);
#endif
	} else {
		ratectrl_cache = wmi_service_enabled(wmi_handle,
						wmi_service_ratectrl_cache);
		iram_tids = wmi_service_enabled(wmi_handle,
						wmi_service_iram_tids);

		if (ratectrl_cache && iram_tids && !(vow_config >> 16)) {
			init_deinit_update_large_ap_config(psoc, tgt_hdl,
							wmi_handle);
		} else if (vow_config>>16) {
			init_deinit_update_vow_config(psoc, tgt_hdl,
							wmi_handle);
		}
	}
}

static void init_deinit_eapol_minrate_enable(
		struct wlan_objmgr_psoc *psoc,
		struct target_psoc_info *tgt_hdl, uint8_t *event)
{
	struct common_wmi_handle *wmi_handle;
	struct tgt_info *info;
	uint32_t enable_eapol_minrate;
	uint32_t eapol_minrate_ac_select;

	if (!tgt_hdl) {
		target_if_err("psoc target_psoc_info is NULL");
		return;
	}
	wmi_handle = target_psoc_get_wmi_hdl(tgt_hdl);
	info = (&tgt_hdl->info);
	enable_eapol_minrate = wlan_ucfg_get_config_param(psoc, EAPOL_MINRATE_SET);
	eapol_minrate_ac_select = wlan_ucfg_get_config_param(psoc, EAPOL_MINRATE_AC_SET);

	if (enable_eapol_minrate) {
		info->wlan_res_cfg.eapol_minrate_set = enable_eapol_minrate;
		info->wlan_res_cfg.eapol_minrate_ac_set = eapol_minrate_ac_select;
	}
}

static void init_deinit_mesh_support_enable(
		 struct wlan_objmgr_psoc *psoc,
		 struct target_psoc_info *tgt_hdl, uint8_t *event)
{
	struct common_wmi_handle *wmi_handle;
	struct tgt_info *info;
	uint32_t enable_mesh_support;

	if (!tgt_hdl) {
		target_if_err("psoc target_psoc_info is null");
		return;
	}
	wmi_handle = target_psoc_get_wmi_hdl(tgt_hdl);
	info = (&tgt_hdl->info);
	enable_mesh_support = wlan_ucfg_get_config_param(psoc, MESH_SUPPORT);

	if (enable_mesh_support &&
		wmi_service_enabled(wmi_handle, wmi_service_mesh)) {
		info->wlan_res_cfg.alloc_frag_desc_for_data_pkt = 1;
		info->wlan_res_cfg.num_vdevs = CFG_TGT_NUM_VDEV_MESH;
	}
}

#if UNIFIED_SMARTANTENNA
static void init_deinit_smart_antenna_enable(
		 struct wlan_objmgr_psoc *psoc,
		 struct target_psoc_info *tgt_hdl, uint8_t *event)
{
	uint32_t enable_smart_antenna;
	uint32_t sa_validate_sw;
	bool smart_ant_enable;
	struct common_wmi_handle *wmi_handle;
	struct tgt_info *info;
	uint8_t wmi_sa_sw;
	uint8_t wmi_sa_hw;

	if (!tgt_hdl) {
		target_if_err("psoc target_psoc_info is null");
		return;
	}
	wmi_handle = target_psoc_get_wmi_hdl(tgt_hdl);
	info = (&tgt_hdl->info);

	enable_smart_antenna = target_if_sa_api_get_sa_supported(psoc);
	sa_validate_sw = target_if_sa_api_get_validate_sw(psoc);
	wmi_sa_sw = wmi_service_enabled(wmi_handle,
					wmi_service_smart_antenna_sw_support);
	wmi_sa_hw = wmi_service_enabled(wmi_handle,
					wmi_service_smart_antenna_hw_support);

	smart_ant_enable = ((wmi_sa_sw && wmi_sa_hw && enable_smart_antenna) ||
				(wmi_sa_sw && sa_validate_sw));

	if (smart_ant_enable) {
		info->wlan_res_cfg.smart_ant_cap = 1;
		target_if_sa_api_set_enable_sa(psoc, 1);
	}

}
#endif

#if WLAN_CFR_ENABLE
static void init_deinit_cfr_support_enable(
                struct wlan_objmgr_psoc *psoc,
                struct target_psoc_info *tgt_hdl, uint8_t *event)
{
        bool is_cfr_supported;
        struct common_wmi_handle *wmi_handle;
        struct tgt_info *info;

        if (!tgt_hdl) {
                target_if_err("psoc target_psoc_info is null");
                return;
        }
        wmi_handle = target_psoc_get_wmi_hdl(tgt_hdl);
        info = (&tgt_hdl->info);

        is_cfr_supported = wmi_service_enabled(wmi_handle,
                                        wmi_service_cfr_capture_support);

        if (is_cfr_supported) {
                target_if_cfr_set_cfr_support(psoc, 1);
        }

}
#endif

#if QCA_AIRTIME_FAIRNESS
static void init_deinit_atf_config_enable(
		 struct wlan_objmgr_psoc *psoc,
		 struct target_psoc_info *tgt_hdl, uint8_t *event)
{
	uint8_t atf_mode;
	struct tgt_info *info;
	struct common_wmi_handle *wmi_handle;
	uint32_t atf_max_vdevs;
	uint32_t atf_peers;

	if (!tgt_hdl) {
		target_if_err("psoc target_psoc_info is null");
		return;
	}

	wmi_handle = target_psoc_get_wmi_hdl(tgt_hdl);
	info = (&tgt_hdl->info);

	if (!wmi_service_enabled(wmi_handle, wmi_service_atf))
		return;

	/*Need to add host input if support this feature, if yes, add sta
			number configuration for resoures as VoW????*/
	target_if_atf_set_fmcap(psoc, 1);
	atf_mode = target_if_atf_get_mode(psoc);

	if (atf_mode) {
		/*Before here, should add if host need to support this one*/
		info->wlan_res_cfg.atf_config = target_if_atf_get_fmcap(psoc);

		if (wmi_service_enabled(wmi_handle, wmi_service_peer_caching)) {
			/* For AR900B chips platform association can
			 * be supported for more than num_active_peers. So avoid
			 * the limitation for these platforms.
			 * Note that air time,
			 * however, is guaranteed only up to num_active_peers.
			 */
			if (!lmac_is_target_ar900b(psoc))
				info->wlan_res_cfg.num_peers =
					info->wlan_res_cfg.num_active_peers;
		}
		target_if_info(
			"Airtime Fairness: num_peers=%d num_active_peer=%d",
					info->wlan_res_cfg.num_peers,
					info->wlan_res_cfg.num_active_peers);
		if ((target_psoc_get_target_type(tgt_hdl) == TARGET_TYPE_AR9888)
			&& target_psoc_get_target_ver(tgt_hdl)
						== AR9888_REV2_VERSION) {
			atf_max_vdevs = target_if_atf_get_max_vdevs(psoc);
			atf_peers = target_if_atf_get_peers(psoc);
			if (atf_max_vdevs) {
				info->wlan_res_cfg.num_vdevs = atf_max_vdevs;
				if (atf_peers)
					info->wlan_res_cfg.num_peers =
						atf_peers +
						info->wlan_res_cfg.num_vdevs;
				else
					info->wlan_res_cfg.num_peers =
						CFG_TGT_NUM_PEERS_ATF +
						info->wlan_res_cfg.num_vdevs;
			} else if (atf_peers) {
				info->wlan_res_cfg.num_peers = atf_peers +
							CFG_TGT_NUM_VDEV_AR988X;
			} else {
				info->wlan_res_cfg.num_peers =
							CFG_TGT_NUM_PEERS_ATF +
							CFG_TGT_NUM_VDEV_AR988X;
			}

			target_if_info("ATF: peers = %d, vdevs = %d ",
					info->wlan_res_cfg.num_peers,
					info->wlan_res_cfg.num_vdevs);
		}
	}
}
#endif


static void init_deinit_qwrap_config_enable(
		 struct wlan_objmgr_psoc *psoc,
		 struct target_psoc_info *tgt_hdl, uint8_t *event)
{
	uint8_t qwrap_enable;
	struct tgt_info *info;
	struct common_wmi_handle *wmi_handle;
#if ATH_SUPPORT_WRAP
	uint32_t num_vdevs;
#endif
	uint32_t num_peers = 0;
	int num_radios;

	qwrap_enable = wlan_psoc_nif_feat_cap_get(psoc,
			WLAN_SOC_F_QWRAP_ENABLE);

	if (!qwrap_enable)
		return;

	if (!tgt_hdl) {
		target_if_err("psoc target_psoc_info is null");
		return;
	}

	num_radios = target_psoc_get_num_radios(tgt_hdl);
	wmi_handle = target_psoc_get_wmi_hdl(tgt_hdl);
	info = (&tgt_hdl->info);

	info->wlan_res_cfg.qwrap_config = qwrap_enable;

#if ATH_SUPPORT_WRAP
	if (info->target_type == TARGET_TYPE_AR9888)
		num_vdevs = CFG_TGT_NUM_WRAP_VDEV_AR988X;
	else if (info->target_type == TARGET_TYPE_QCA9984)
		num_vdevs = CFG_TGT_NUM_WRAP_VDEV_QCA9984;
	else if (info->target_type == TARGET_TYPE_IPQ4019)
		num_vdevs = CFG_TGT_NUM_WRAP_VDEV_IPQ4019;
	else if (info->target_type == TARGET_TYPE_QCA8074)
		num_vdevs = CFG_TGT_NUM_WRAP_VDEV_IPQ8074;
	else if (info->target_type == TARGET_TYPE_QCA8074V2)
		num_vdevs = CFG_TGT_NUM_WRAP_VDEV_IPQ8074;
	else if (info->target_type == TARGET_TYPE_QCA6018)
		num_vdevs = CFG_TGT_NUM_WRAP_VDEV_IPQ8074;
	else
		num_vdevs = CFG_TGT_NUM_WRAP_VDEV_AR900B;

	info->wlan_res_cfg.num_vdevs = num_vdevs;

	/* Every sta vdev target need 2 peer ( self & bss beer )
	 * To no of peers in Qwrap =
	 * no of sta vdev (to no of vdev - 1 ap vdev ) * 2 peer
	 * + 1 ap vdev peer
	 * + max wireless peers supported in AP vdev
	 */
	if (info->target_type == TARGET_TYPE_AR9888)
		num_peers = ((CFG_TGT_NUM_WRAP_VDEV_AR988X - 1) * 2) +
					1 + CFG_TGT_NUM_WRAP_PEERS_MAX_AR988X;
	else if (info->target_type == TARGET_TYPE_QCA9984)
		num_peers = ((CFG_TGT_NUM_WRAP_VDEV_QCA9984 - 1) * 2) +
					1 + CFG_TGT_NUM_WRAP_PEERS_MAX_QCA9984;
	else if (info->target_type == TARGET_TYPE_IPQ4019)
		num_peers = ((CFG_TGT_NUM_WRAP_VDEV_IPQ4019 - 1) * 2) +
					1 + CFG_TGT_NUM_WRAP_PEERS_MAX_IPQ4019;
	else if (info->target_type == TARGET_TYPE_QCA8074)
		num_peers = ((CFG_TGT_NUM_WRAP_VDEV_IPQ8074 - 1) * 2) +
					1 + CFG_TGT_NUM_WRAP_PEERS_MAX_IPQ8074;
	else if (info->target_type == TARGET_TYPE_QCA8074V2)
		num_peers = ((CFG_TGT_NUM_WRAP_VDEV_IPQ8074 - 1) * 2) +
					1 + CFG_TGT_NUM_WRAP_PEERS_MAX_IPQ8074;
	else if (info->target_type == TARGET_TYPE_QCA6018)
		num_peers = ((CFG_TGT_NUM_WRAP_VDEV_IPQ8074 - 1) * 2) +
					1 + CFG_TGT_NUM_WRAP_PEERS_MAX_IPQ8074;
	else
		num_peers = ((CFG_TGT_NUM_WRAP_VDEV_AR900B - 1) * 2) +
					1 + CFG_TGT_NUM_WRAP_PEERS_MAX_AR900B;
#endif
	info->wlan_res_cfg.num_peers = num_peers;
	/* Wrt Qwrap, num_peers & num_active_peers are same.
	 * Target recommends host to set the num_active_peers to
	 * 0 and target will reset num_active_peers accordingly
	 * based on target final total num_peers.
	 */
	info->wlan_res_cfg.num_active_peers = 0;

	if(num_radios > 1) {
		info->wlan_res_cfg.num_peers = info->wlan_res_cfg.num_peers * num_radios;
		target_if_info(
			"Qwrap max client mode enabled: Updating num_peers to %d since num_radios is %d",
			info->wlan_res_cfg.num_peers, num_radios);
	}

	info->wlan_res_cfg.num_tids = info->wlan_res_cfg.num_peers * 2;

	target_if_info(
		"Qwrap max client mode enabled: num_peers = %d, num_active_peers = %d num_tids = %d, num_vdevs = %d",
		info->wlan_res_cfg.num_peers,
		info->wlan_res_cfg.num_active_peers,
		info->wlan_res_cfg.num_tids, info->wlan_res_cfg.num_vdevs);
}

static void init_deinit_btcoex_config_enable(
		 struct wlan_objmgr_psoc *psoc,
		 struct target_psoc_info *tgt_hdl, uint8_t *event)
{
	uint32_t val;
	uint32_t period;
	uint32_t duration;

	if (wlan_ucfg_get_btcoex_param(psoc, BTCOEX_ENABLE)) {
		val = WMI_HOST_PDEV_VI_PRIORITY_BIT |
			WMI_HOST_PDEV_BEACON_PRIORITY_BIT |
			WMI_HOST_PDEV_MGMT_PRIORITY_BIT;

		wlan_ucfg_set_btcoex_param(psoc, BTCOEX_WL_PRIORITY, val, 0);

		if (wlan_ucfg_get_btcoex_param(psoc, BTCOEX_DUTY_CYCLE)) {
			period = DEFAULT_PERIOD;
			duration = DEFAULT_WLAN_DURATION;
			wlan_ucfg_set_btcoex_param(psoc,
				BTCOEX_DURATION_PERIOD, duration, period);
		}
	}
}


static void init_deinit_lteu_ext_support_enable(
		 struct wlan_objmgr_psoc *psoc,
		 struct target_psoc_info *tgt_hdl, uint8_t *event)
{
	struct tgt_info *info;

	 if (!tgt_hdl) {
		target_if_err("psoc target_psoc_info is null");
		return;
	}

	info = (&tgt_hdl->info);

	if (info->wlan_ext_res_cfg.fw_feature_bitmap &
			WMI_HOST_FW_FEATURE_LTEU_SUPPORT) {
		info->wlan_res_cfg.num_peers = 10;
		info->wlan_res_cfg.num_peers += info->wlan_res_cfg.num_vdevs;
		target_if_info("LTEu enabled. num_peers %d, num_vdevs %d, num_tids %d",
				info->wlan_res_cfg.num_peers,
				info->wlan_res_cfg.num_vdevs,
				info->wlan_res_cfg.num_tids);

	}
}

static void init_deinit_set_init_cmd_dev_params(
		struct wlan_objmgr_psoc *psoc,
		struct target_psoc_info *tgt_hdl)
{

	struct tgt_info *info;
	target_resource_config *tgt_cfg;

	if (!tgt_hdl) {
		targetif_nofl_info("psoc target_psoc_info is null");
		return;
	}

	info = (&tgt_hdl->info);

	tgt_cfg = &info->wlan_res_cfg;

	if (info->target_type == TARGET_TYPE_AR9888) {
		tgt_cfg->tx_chain_mask = CFG_TGT_DEFAULT_TX_CHAIN_MASK_3SS;
		tgt_cfg->rx_chain_mask = CFG_TGT_DEFAULT_RX_CHAIN_MASK_3SS;
		tgt_cfg->num_msdu_desc = CFG_TGT_NUM_MSDU_DESC_AR988X;
		tgt_cfg->ast_skid_limit = CFG_TGT_AST_SKID_LIMIT_AR988X;
	} else if (info->target_type == TARGET_TYPE_AR900B ||
			info->target_type == TARGET_TYPE_QCA9984) {
		tgt_cfg->num_vdevs = CFG_TGT_NUM_VDEV_AR900B;
		/* need to reserve an additional peer for each VDEV */
		tgt_cfg->num_peers =
				CFG_TGT_NUM_PEERS + CFG_TGT_NUM_VDEV_AR900B;
		tgt_cfg->num_tids += CFG_TGT_NUM_VDEV_AR900B;
		tgt_cfg->tx_chain_mask = CFG_TGT_DEFAULT_TX_CHAIN_MASK_4SS;
		tgt_cfg->rx_chain_mask = CFG_TGT_DEFAULT_RX_CHAIN_MASK_4SS;
	} else if (info->target_type == TARGET_TYPE_IPQ4019 ||
		info->target_type == TARGET_TYPE_QCA9888) {
		tgt_cfg->num_vdevs = CFG_TGT_NUM_VDEV_AR900B;
		/* need to reserve an additional peer for each VDEV */
		tgt_cfg->num_peers =
				CFG_TGT_NUM_PEERS + CFG_TGT_NUM_VDEV_AR900B;
		tgt_cfg->num_tids += CFG_TGT_NUM_VDEV_AR900B;
		tgt_cfg->tx_chain_mask = CFG_TGT_DEFAULT_TX_CHAIN_MASK_2SS;
		tgt_cfg->rx_chain_mask = CFG_TGT_DEFAULT_RX_CHAIN_MASK_2SS;
	}

#if QCA_AIRTIME_FAIRNESS
	if (target_if_atf_get_mode(psoc) &&
		(target_psoc_get_target_type(tgt_hdl) == TARGET_TYPE_AR9888 &&
		target_psoc_get_target_ver(tgt_hdl) == AR9888_REV2_VERSION))
		tgt_cfg->num_msdu_desc =
			target_if_atf_get_num_msdu_desc(psoc);
#endif

	if (!wlan_psoc_nif_feat_cap_get(psoc, WLAN_SOC_F_BCN_OFFLOAD))
		tgt_cfg->beacon_tx_offload_max_vdev = 0;

	if ((info->target_type == TARGET_TYPE_QCA8074) ||
		(info->target_type == TARGET_TYPE_QCA8074V2) ||
		(info->target_type == TARGET_TYPE_QCA6018))
		tgt_cfg->scheduler_params = wlan_ucfg_get_config_param(
							psoc, SCHED_PARAMS);

	info->wlan_ext_res_cfg.fw_feature_bitmap |=
			WMI_HOST_FW_FEATURE_BSS_CHANNEL_INFO_64;

#ifdef ATHR_WIN_NWF
	tgt_cfg->rx_decap_mode = CFG_TGT_RX_DECAP_MODE_NWIFI;
#else
	tgt_cfg->rx_decap_mode = CFG_TGT_RX_DECAP_MODE;

#endif
	tgt_cfg->rx_batchmode = 1;

	/*
	 * To make the IP header begins at dword aligned address,
	 * we make the decapsulation mode as Native Wifi.
	 */
	if (wlan_psoc_nif_feat_cap_get(psoc, WLAN_SOC_F_HOST_80211_ENABLE))
		tgt_cfg->rx_decap_mode = CFG_TGT_RX_DECAP_MODE_NWIFI;

	/* Set the Min buffer free for each AC, from the module param values */
	tgt_cfg->BK_Minfree = wlan_ucfg_get_config_param(psoc, AC_BK_MINFREE);
	tgt_cfg->BE_Minfree = wlan_ucfg_get_config_param(psoc, AC_BE_MINFREE);
	tgt_cfg->VI_Minfree = wlan_ucfg_get_config_param(psoc, AC_VI_MINFREE);
	tgt_cfg->VO_Minfree = wlan_ucfg_get_config_param(psoc, AC_VI_MINFREE);

	targetif_nofl_debug
		("AC Minfree buf allocation through module param (umac.ko)");
	targetif_nofl_debug
		(" OL_ACBKMinfree : %d", tgt_cfg->BK_Minfree);
	targetif_nofl_debug
		(" OL_ACBEMinfree : %d", tgt_cfg->BE_Minfree);
	targetif_nofl_debug
		(" OL_ACVIMinfree : %d", tgt_cfg->VI_Minfree);
	targetif_nofl_debug
		(" OL_ACVOMinfree : %d", tgt_cfg->VO_Minfree);

	/* Configuring IP header padding, from module param value */
	if (lmac_is_target_ar900b(psoc))
		tgt_cfg->iphdr_pad_config = wlan_ucfg_get_config_param(psoc,
						IPHDR_PAD);
	else
		tgt_cfg->iphdr_pad_config = 0;

	/* Init peer count variable */
	wlan_psoc_update_peer_count(psoc, tgt_cfg);

#if PEER_CACHEING_HOST_ENABLE
	if (!((info->target_type == TARGET_TYPE_QCA8074) ||
		(info->target_type == TARGET_TYPE_QCA8074V2) ||
		(info->target_type == TARGET_TYPE_QCA6018) ||
		(info->target_type == TARGET_TYPE_QCA6290)))
		tgt_cfg->num_active_peers = 0;
#endif
	tgt_cfg->cce_disable = wlan_ucfg_get_config_param(psoc, CCE_STATE);

	targetif_nofl_debug("num_peers %d, num_vdevs %d, num_tids %d",
		tgt_cfg->num_peers, tgt_cfg->num_vdevs, tgt_cfg->num_tids);

	return;
}

QDF_STATUS init_deinit_alloc_pdevs(
		 struct wlan_objmgr_psoc *psoc,
		 struct target_psoc_info *tgt_hdl)
{
	struct tgt_info *info;
	uint16_t i;
	qdf_netdev_t pdev_netdev;
	struct pdev_osif_priv *osdev_priv;
	struct wlan_objmgr_pdev *pdev_umac;
	QDF_STATUS ret_val;

	if (!tgt_hdl) {
		target_if_err("psoc target_psoc_info is null");
		return QDF_STATUS_E_FAILURE;
	}

	info = (&tgt_hdl->info);

        /*
         * Make num_radios 1 if user configures hw mode as 0.
         * This is needed for Napier as Napier does not support DBDC and
         * preferred_hw_mode 0 is considered as an invalid mode in cmn code.
         */
        if (target_psoc_get_preferred_hw_mode(tgt_hdl) == WMI_HOST_HW_MODE_SINGLE)
            target_psoc_set_num_radios(tgt_hdl, 1);

	for (i = 0; i < target_psoc_get_num_radios(tgt_hdl); i++) {
		pdev_umac = wlan_objmgr_get_pdev_by_id(psoc, i, WLAN_INIT_DEINIT_ID);
		if (pdev_umac != NULL) {
			wlan_objmgr_pdev_release_ref(pdev_umac, WLAN_INIT_DEINIT_ID);
			continue;
		}

		pdev_netdev = wlan_create_pdev_netdevice(psoc, i);
		if (pdev_netdev == NULL) {
			target_if_err("Cannot allocate softc");
			return QDF_STATUS_E_NOMEM;
		}

		osdev_priv = qdf_mem_malloc(sizeof(struct pdev_osif_priv));
		if (osdev_priv == NULL) {
			target_if_err("OSIF private member allocation failed");
			return QDF_STATUS_E_NOMEM;
		}

		osdev_priv->legacy_osif_priv = (void *)netdev_priv(pdev_netdev);

		pdev_umac = wlan_objmgr_pdev_obj_create(psoc, osdev_priv);
		if (pdev_umac == NULL) {
			target_if_err("PDEV (creation) failed");
			qdf_mem_free(osdev_priv);
			osdev_priv = NULL;
			return QDF_STATUS_E_FAILURE;
		}

		ret_val = target_if_alloc_pdev_tgt_info(pdev_umac);
		if (ret_val != QDF_STATUS_SUCCESS) {
			target_if_err("pdev tgt alloc failed");
			return ret_val;
		}

		wlan_pdev_update_feature_ptr(psoc, pdev_umac, pdev_netdev);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS init_deinit_update_pdev_tgt_info(
		 struct wlan_objmgr_psoc *psoc,
		 struct target_psoc_info *tgt_hdl)
{
	struct tgt_info *info;
	struct target_pdev_info *pdev_tgt_info;
	struct wlan_objmgr_pdev *pdev;
	uint16_t i;
	uint16_t num_radios;
	void *wmi_handle;
	void *dp_handle;
	uint32_t bypasswmi;
	QDF_STATUS is_service_ext_msg = QDF_STATUS_E_FAILURE;
	dp_txrx_handle_t *dp_ext_hdl;
	bool polled_mode = false;

	if (!psoc) {
		target_if_err("psoc is null");
		return QDF_STATUS_E_FAILURE;
	}
	if (!tgt_hdl) {
		target_if_err("target_psoc_info is null");
		return QDF_STATUS_E_FAILURE;
	}

	info = (&tgt_hdl->info);
	is_service_ext_msg = init_deinit_is_service_ext_msg(psoc, tgt_hdl);

	bypasswmi = wlan_ucfg_get_config_param(psoc, BYPASSWMI);

	num_radios = target_psoc_get_num_radios(tgt_hdl);
	for (i = 0; i < num_radios; i++) {
		pdev = wlan_objmgr_get_pdev_by_id(psoc, i, WLAN_INIT_DEINIT_ID);
		if (pdev == NULL) {
			target_if_err("pdev obj is not allocated");
			return QDF_STATUS_E_FAILURE;
		}
		pdev_tgt_info = wlan_pdev_get_tgt_if_handle(pdev);
		if (pdev_tgt_info == NULL) {
			target_if_err("pdev tgt info is NULL");
		        wlan_objmgr_pdev_release_ref(pdev, WLAN_INIT_DEINIT_ID);
			return QDF_STATUS_E_FAILURE;
		}

		if (is_service_ext_msg == QDF_STATUS_SUCCESS) {
			pdev_tgt_info->phy_idx = info->mac_phy_cap[i].phy_id;
			pdev_tgt_info->pdev_idx = info->mac_phy_cap[i].pdev_id;
		} else {
			pdev_tgt_info->phy_idx = i;
			pdev_tgt_info->pdev_idx = 0;
		}
		target_if_debug("object manager pdev id %d, tgt pdev id %d",
					i, pdev_tgt_info->pdev_idx);

		wmi_handle = wmi_unified_get_pdev_handle(
				wmi_unified_get_soc_handle(
				(struct wmi_unified *)target_psoc_get_wmi_hdl(tgt_hdl)), i);
		if (wmi_handle == NULL) {
			target_if_err("Failed to get pdev wmi_handle");
		        wlan_objmgr_pdev_release_ref(pdev, WLAN_INIT_DEINIT_ID);
			return QDF_STATUS_E_FAILURE;
		}

		target_pdev_set_wmi_handle(pdev_tgt_info, wmi_handle);

		if (!bypasswmi) {
			dp_handle = (void *)cdp_pdev_attach(
					wlan_psoc_get_dp_handle(psoc),
					(struct cdp_ctrl_objmgr_pdev *)pdev,
					target_psoc_get_htc_hdl(tgt_hdl),
					wlan_psoc_get_qdf_dev(psoc), i);
			if (dp_handle == NULL) {
				target_if_err("CDP PDEV ATTACH failed");
		                wlan_objmgr_pdev_release_ref(pdev, WLAN_INIT_DEINIT_ID);
				return QDF_STATUS_E_FAILURE;
			}

			wlan_pdev_set_dp_handle(pdev, dp_handle);
			dp_ext_hdl = qdf_mem_malloc(sizeof(dp_txrx_handle_t));
			cdp_pdev_set_dp_txrx_handle(
					wlan_psoc_get_dp_handle(psoc),
					dp_handle, dp_ext_hdl);

#if DBDC_REPEATER_SUPPORT
			dp_lag_pdev_init(&dp_ext_hdl->lag_hdl, dp_soc_get_lag_handle(psoc), i);
#endif
		}

		wlan_objmgr_pdev_release_ref(pdev, WLAN_INIT_DEINIT_ID);
		target_if_info("CDP PDEV ATTACH success");

	}

	if (info->target_type == TARGET_TYPE_QCA6290)
		polled_mode = true;

	cdp_txrx_intr_attach(wlan_psoc_get_dp_handle(psoc));

	/* Add delay before sending wmi_init to ensure pdev attach is
	 * reflected
	 */
	qdf_mdelay(100);

	return QDF_STATUS_SUCCESS;
}



#if WIFI_MEM_MANAGER_SUPPORT
uint32_t init_deinit_mem_mgr_alloc_chunk(struct wlan_objmgr_psoc *psoc,
			struct target_psoc_info *tgt_hdl,
			u_int32_t req_id, u_int32_t idx, u_int32_t num_units,
			u_int32_t unit_len, u_int32_t num_unit_info)
{
	qdf_dma_addr_t paddr;
	struct tgt_info *info;
	qdf_device_t qdf_dev;
	uint32_t soc_id;
	uint32_t target_type;

	info = (&tgt_hdl->info);

	if (!num_units  || !unit_len)  {
		return 0;
	}

	qdf_dev = wlan_psoc_get_qdf_dev(psoc);
	soc_id = wlan_ucfg_get_config_param(psoc, SOC_ID);
	tgt_hdl = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(psoc);
	target_type = target_psoc_get_target_type(tgt_hdl);

	info->mem_chunks[idx].vaddr = NULL ;

	if (req_id == IRAM_BKP_PADDR_REQ_ID &&
	    (target_type == TARGET_TYPE_QCA9984 ||
	     target_type == TARGET_TYPE_IPQ4019 ||
	     target_type == TARGET_TYPE_AR900B  ||
	     target_type == TARGET_TYPE_QCA9888)) {
		/*
		 * req_id for the address of target iram backup in the host.
		 * There is no need for any m/r allocation for this req_id.
		 * Only the paddr for the FW iram bkp in host needs to be
		 * passed to FW.
		 */
		idx = info->num_mem_chunks;
		info->mem_chunks[idx].paddr = wlan_ucfg_get_config_param(psoc,
							 TGT_IRAM_BKP_ADDR);
		info->mem_chunks[idx].vaddr = NULL;
		info->mem_chunks[idx].len = 0;
		info->mem_chunks[idx].req_id = 8;
		info->num_mem_chunks += 1;
		goto done;
	}

	/* reduce the requested allocation by half until allocation succeeds */
	while (info->mem_chunks[idx].vaddr == NULL && num_units) {
		int intr_ctxt = (in_interrupt() || irqs_disabled()) ? 1 : 0;
		info->mem_chunks[idx].vaddr =
			(uint32_t *) wifi_cmem_allocation(soc_id, 0,
				(CM_FWREQ + req_id), num_units*unit_len,
				(void *)qdf_dev->drv_hdl,
				&paddr, intr_ctxt);
		if (info->mem_chunks[idx].vaddr == NULL) {
			if (num_unit_info & HOST_CONTIGUOUS_MEM_CHUNK_REQUIRED)
				return 0;
			else  /* reduce length by half */
				num_units = (num_units >> 1);
		} else {
			info->mem_chunks[idx].paddr = paddr;
			info->mem_chunks[idx].len = num_units*unit_len;
			info->mem_chunks[idx].req_id =  req_id;
		}
	}
done:
	target_if_info(
		"req_id %d idx %d num_units %d unit_len %d",
		req_id, idx, num_units, unit_len);

	return num_units;
}


static QDF_STATUS init_deinit_mem_mgr_free_chunks(struct wlan_objmgr_psoc *psoc,
			struct target_psoc_info *tgt_hdl)
{
	struct tgt_info *info;
	uint32_t soc_id;
	uint32_t idx;

	info = (&tgt_hdl->info);
	soc_id = wlan_ucfg_get_config_param(psoc, SOC_ID);

	for (idx=0; idx< info->num_mem_chunks; ++idx) {
		wifi_cmem_free(soc_id, 0, info->mem_chunks[idx].req_id,
				(void *)info->mem_chunks[idx].vaddr);
	}
	info->num_mem_chunks = 0;

	return QDF_STATUS_SUCCESS;
}

#endif

void init_deinit_print_service_ready_ex_params(
		 struct wlan_objmgr_psoc *psoc,
		 struct target_psoc_info *tgt_hdl)
{
	struct wlan_psoc_host_service_ext_param *service_ext_param;
	struct wlan_psoc_host_mac_phy_caps *mac_phy_cap;
	struct tgt_info *info;
	uint16_t i;

	if (!tgt_hdl) {
		targetif_nofl_err("target_psoc_info is null");
		return;
	}

	info = (&tgt_hdl->info);

	service_ext_param = &info->service_ext_param;

	targetif_nofl_info("HE_CAP Info = %x", service_ext_param->he_cap_info);
	targetif_nofl_info("Num HW modes = %d",
		service_ext_param->num_hw_modes);
	targetif_nofl_info("Num PHY = %d", service_ext_param->num_phy);
	targetif_nofl_debug("HW mode and MAC PHY CAP");

	for (i = 0; i < target_psoc_get_num_radios(tgt_hdl); i++) {
		mac_phy_cap = &(info->mac_phy_cap[i]);

		targetif_nofl_debug
		("idx = %d hw_mode_id = %d pdev_id = %d phy_id = %d",
			i, mac_phy_cap->hw_mode_id, mac_phy_cap->pdev_id,
			mac_phy_cap->phy_id);

		targetif_nofl_debug(" 11G/A/N/AC/AX  support = %x/%x/%x/%x/%x ",
			mac_phy_cap->supports_11g, mac_phy_cap->supports_11a,
			mac_phy_cap->supports_11n, mac_phy_cap->supports_11ac,
			mac_phy_cap->supports_11ax);
		targetif_nofl_debug(" Supported bands = %x ",
					mac_phy_cap->supported_bands);
		targetif_nofl_debug(" Ampdu density = %d",
					mac_phy_cap->ampdu_density);
		if (mac_phy_cap->supported_bands &
			WMI_HOST_WLAN_2G_CAPABILITY) {
			targetif_nofl_debug(" Max BW supported 2G = %d ",
					mac_phy_cap->max_bw_supported_2G);
			targetif_nofl_debug(" 2G - HT cap info = %x ",
					mac_phy_cap->ht_cap_info_2G);
			targetif_nofl_debug(" 2G - VHT cap info = %x ",
					mac_phy_cap->vht_cap_info_2G);
			targetif_nofl_debug(" 2G - Supported VHT MCS =%x ",
					mac_phy_cap->vht_supp_mcs_2G);
			targetif_nofl_debug(" 2G - HE cap  = %x ",
					mac_phy_cap->he_cap_info_2G);
			targetif_nofl_debug(" 2G - HE supp MCS = %x ",
					mac_phy_cap->he_supp_mcs_2G);
			targetif_nofl_debug(" 2G - TX chain mask = %x ",
					mac_phy_cap->tx_chain_mask_2G);
			targetif_nofl_debug(" 2G - RX chain mask = %d",
					mac_phy_cap->rx_chain_mask_2G);
			targetif_nofl_debug(
			" HE_CAP_PHY_INFO_2G[0] = %x HE_CAP_PHY_INFO_2G[1]=%x HE_CAP_PHY_INFO_2G [2]=%x ",
					mac_phy_cap->he_cap_phy_info_2G[0],
					mac_phy_cap->he_cap_phy_info_2G[1],
					mac_phy_cap->he_cap_phy_info_2G[2]);
			targetif_nofl_debug(
			" 2G - HE_PPET5G_NUMSS_M1 =%d HE_PPET5G_RU_MASK=%x",
					mac_phy_cap->he_ppet2G.numss_m1,
					mac_phy_cap->he_ppet2G.ru_bit_mask);
		}
		if (mac_phy_cap->supported_bands &
			WMI_HOST_WLAN_5G_CAPABILITY) {
			targetif_nofl_debug(" 5G - Max BW supported = %d ",
					mac_phy_cap->max_bw_supported_5G);
			targetif_nofl_debug(" 5G - HT cap info = %x ",
					mac_phy_cap->ht_cap_info_5G);
			targetif_nofl_debug(" 5G - VHT cap info = %x ",
					mac_phy_cap->vht_cap_info_5G);
			targetif_nofl_debug(" 5G - Supported VHT MCS %x",
					mac_phy_cap->vht_supp_mcs_5G);
			targetif_nofl_debug(" 5G - HE cap = %x ",
					mac_phy_cap->he_cap_info_5G);
			targetif_nofl_debug(" 5G - HE supp MCS = %x ",
					mac_phy_cap->he_supp_mcs_5G);
			targetif_nofl_debug(" 5G - tx chain mask = %x",
					mac_phy_cap->tx_chain_mask_5G);
			targetif_nofl_debug(" 5G - RX chain mask = %d",
					mac_phy_cap->rx_chain_mask_5G);
			targetif_nofl_debug(
			" HE_CAP_PHY_INFO_5G[0] = %x HE_CAP_PHY_INFO_5G[1]= %x HE_CAP_PHY_INFO_5G [2]=%x",
					mac_phy_cap->he_cap_phy_info_5G[0],
					mac_phy_cap->he_cap_phy_info_5G[1],
					mac_phy_cap->he_cap_phy_info_5G[2]);
			targetif_nofl_debug
			(" 5G - HE_PPET5G_NUMSS_M1 = %d HE_PPET5G_RU_MASK=%x",
				mac_phy_cap->he_ppet5G.numss_m1,
				mac_phy_cap->he_ppet5G.ru_bit_mask);
		}
		targetif_nofl_debug(" chain mask tableid= %d",
					mac_phy_cap->chainmask_table_id);
	}
	targetif_nofl_info("Preferred HW Mode = %d Num Radios = %d",
		info->preferred_hw_mode, target_psoc_get_num_radios(tgt_hdl));
}

static void add_11ax_mode_flags(
	struct wlan_psoc_host_hal_reg_capabilities_ext *reg_cap,
	struct wlan_psoc_host_mac_phy_caps *mac_phy_cap)
{
	/* 11ax 2.4 GHz */

	/* We check to ensure max BW supported is not 5/10 MHz, just as an
	 * additional precaution.
	 */
	if ((mac_phy_cap->supported_bands & WMI_HOST_WLAN_2G_CAPABILITY)
		&& (mac_phy_cap->max_bw_supported_2G != WMI_HOST_CHAN_WIDTH_5)
		&& (mac_phy_cap->max_bw_supported_2G !=
					WMI_HOST_CHAN_WIDTH_10)) {
		if ((mac_phy_cap->max_bw_supported_2G >=
					WMI_HOST_CHAN_WIDTH_20) &&
			(reg_cap->wireless_modes &
					WMI_HOST_REGDMN_MODE_11NG_HT20)) {
			reg_cap->wireless_modes |=
					WMI_HOST_REGDMN_MODE_11AXG_HE20;
		}

		if (mac_phy_cap->max_bw_supported_2G >=
				WMI_HOST_CHAN_WIDTH_40) {
			if (reg_cap->wireless_modes &
					WMI_HOST_REGDMN_MODE_11NG_HT40PLUS)
				reg_cap->wireless_modes |=
					WMI_HOST_REGDMN_MODE_11AXG_HE40PLUS;

			if (reg_cap->wireless_modes &
					WMI_HOST_REGDMN_MODE_11NG_HT40MINUS)
				reg_cap->wireless_modes |=
					WMI_HOST_REGDMN_MODE_11AXG_HE40MINUS;
		}
	}

	/* 11ax 5 GHz */

	/* We check to ensure max BW supported is not 5/10 MHz, just as an
	 * additional precaution.
	 */
	if ((mac_phy_cap->supported_bands & WMI_HOST_WLAN_5G_CAPABILITY)
		&& (mac_phy_cap->max_bw_supported_5G != WMI_HOST_CHAN_WIDTH_5)
		&& (mac_phy_cap->max_bw_supported_5G !=
				WMI_HOST_CHAN_WIDTH_10)) {
		if ((mac_phy_cap->max_bw_supported_5G >= WMI_HOST_CHAN_WIDTH_20)
		   && (reg_cap->wireless_modes &
			WMI_HOST_REGDMN_MODE_11AC_VHT20)) {
			reg_cap->wireless_modes |=
						WMI_HOST_REGDMN_MODE_11AXA_HE20;
		}

		if (mac_phy_cap->max_bw_supported_5G >=
						WMI_HOST_CHAN_WIDTH_40) {
			if (reg_cap->wireless_modes &
					WMI_HOST_REGDMN_MODE_11AC_VHT40PLUS)
				reg_cap->wireless_modes |=
					WMI_HOST_REGDMN_MODE_11AXA_HE40PLUS;
			if (reg_cap->wireless_modes &
					WMI_HOST_REGDMN_MODE_11AC_VHT40MINUS)
				reg_cap->wireless_modes |=
					WMI_HOST_REGDMN_MODE_11AXA_HE40MINUS;
		}

		if ((mac_phy_cap->max_bw_supported_5G >=
					WMI_HOST_CHAN_WIDTH_80) &&
			(reg_cap->wireless_modes &
				WMI_HOST_REGDMN_MODE_11AC_VHT80)) {
			reg_cap->wireless_modes |=
				WMI_HOST_REGDMN_MODE_11AXA_HE80;
		}

		if ((mac_phy_cap->max_bw_supported_5G >=
					WMI_HOST_CHAN_WIDTH_160) &&
			(reg_cap->wireless_modes &
					WMI_HOST_REGDMN_MODE_11AC_VHT160)) {
			reg_cap->wireless_modes |=
				WMI_HOST_REGDMN_MODE_11AXA_HE160;
		}

		if ((mac_phy_cap->max_bw_supported_5G >=
					WMI_HOST_CHAN_WIDTH_80P80) &&
			(reg_cap->wireless_modes &
					WMI_HOST_REGDMN_MODE_11AC_VHT80_80)) {
			reg_cap->wireless_modes |=
				WMI_HOST_REGDMN_MODE_11AXA_HE80_80;
		}
	}
}




static void init_deinit_add_11ax_modes(
		 struct wlan_objmgr_psoc *psoc,
		 struct target_psoc_info *tgt_hdl)
{
	struct tgt_info *info;
	uint16_t i;
	uint32_t phy_id;
	uint32_t num_phy_reg_cap;
	struct wlan_psoc_host_hal_reg_capabilities_ext *reg_cap;
	struct wlan_psoc_host_mac_phy_caps *mac_phy_cap;

	if (!tgt_hdl) {
		target_if_err("target_psoc_info is null");
		return;
	}

	reg_cap = ucfg_reg_get_hal_reg_cap(psoc);
	if (reg_cap == NULL) {
		target_if_err("reg cap is NULL");
		return;
	}

	info = (&tgt_hdl->info);

	num_phy_reg_cap = info->service_ext_param.num_phy;

	for (i = 0; i < num_phy_reg_cap; i++) {
		phy_id = reg_cap[i].phy_id;
		mac_phy_cap = &info->mac_phy_cap[phy_id];
		if (mac_phy_cap->supports_11ax) {
			target_if_info("Adding 11ax regulatory modes,");
			add_11ax_mode_flags(&reg_cap[i], mac_phy_cap);
			target_if_info(" phy_id = %d wireless modes = %x",
					phy_id,	(&reg_cap[i])->wireless_modes);
		}
	}

}


/* Target config used for Hawkeye */
target_resource_config  tgt_cfg_qca8074 = {
	.num_vdevs = CFG_TGT_NUM_VDEV_QCA8074,
	.num_peers = CFG_TGT_NUM_PEERS_QCA8074,
	.num_offload_peers = CFG_TGT_NUM_OFFLOAD_PEERS_QCA8074,
	.num_offload_reorder_buffs = CFG_TGT_NUM_OFFLOAD_REORDER_BUFFS_QCA8074,
	.num_peer_keys = CFG_TGT_NUM_PEER_KEYS,
	.num_tids = CFG_TGT_NUM_TIDS_QCA8074,
	.ast_skid_limit = CFG_TGT_AST_SKID_LIMIT_QCA8074,
	.tx_chain_mask = CFG_TGT_DEFAULT_TX_CHAIN_MASK_4SS,
	.rx_chain_mask = CFG_TGT_DEFAULT_RX_CHAIN_MASK_4SS,
	.rx_timeout_pri = { CFG_TGT_RX_TIMEOUT_LO_PRI,
				CFG_TGT_RX_TIMEOUT_LO_PRI,
				CFG_TGT_RX_TIMEOUT_LO_PRI,
				CFG_TGT_RX_TIMEOUT_HI_PRI },
	.scan_max_pending_req = CFG_TGT_DEFAULT_SCAN_MAX_REQS,
	.bmiss_offload_max_vdev = CFG_TGT_DEFAULT_BMISS_OFFLOAD_MAX_VDEV,
	.roam_offload_max_vdev = CFG_TGT_DEFAULT_ROAM_OFFLOAD_MAX_VDEV,
	.roam_offload_max_ap_profiles =
		CFG_TGT_DEFAULT_ROAM_OFFLOAD_MAX_PROFILES,
	.num_mcast_groups = CFG_TGT_DEFAULT_NUM_MCAST_GROUPS,
	.num_mcast_table_elems = CFG_TGT_DEFAULT_NUM_MCAST_TABLE_ELEMS,
	.mcast2ucast_mode = CFG_TGT_DEFAULT_MCAST2UCAST_MODE,
	.tx_dbg_log_size = CFG_TGT_DEFAULT_TX_DBG_LOG_SIZE,
	.num_wds_entries = CFG_TGT_WDS_ENTRIES,
	.dma_burst_size = CFG_TGT_DEFAULT_DMA_BURST_SIZE,
	.mac_aggr_delim = CFG_TGT_DEFAULT_MAC_AGGR_DELIM,
	.rx_skip_defrag_timeout_dup_detection_check =
	     CFG_TGT_DEFAULT_RX_SKIP_DEFRAG_TIMEOUT_DUP_DETECTION_CHECK,
	.vow_config = CFG_TGT_DEFAULT_VOW_CONFIG,
	.gtk_offload_max_vdev = CFG_TGT_DEFAULT_GTK_OFFLOAD_MAX_VDEV,
	.num_msdu_desc = CFG_TGT_NUM_MSDU_DESC_AR900B,
	.max_frag_entries = 6,
	.beacon_tx_offload_max_vdev = CFG_TGT_NUM_BCN_OFFLOAD_VDEV,
#ifdef WLAN_SUPPORT_TWT
	.twt_ap_pdev_count = CFG_TGT_TWT_AP_PDEV_COUNT,
	.twt_ap_sta_count = CFG_TGT_TWT_AP_STA_COUNT,
#endif
	.peer_map_unmap_v2 = 1,
};

/* Target config used for Napier */
target_resource_config  tgt_cfg_qca6290 = {
	.num_vdevs = CFG_TGT_NUM_VDEV_QCA6290,
	.num_peers = CFG_TGT_NUM_PEERS_QCA6290,
	.num_offload_peers = CFG_TGT_NUM_OFFLOAD_PEERS_QCA6290,
	.num_offload_reorder_buffs = CFG_TGT_NUM_OFFLOAD_REORDER_BUFFS_QCA8074,
	.num_peer_keys = CFG_TGT_NUM_PEER_KEYS,
	.num_tids = CFG_TGT_NUM_TIDS_QCA6290,
	.ast_skid_limit = CFG_TGT_AST_SKID_LIMIT_QCA8074,
	.tx_chain_mask = CFG_TGT_DEFAULT_TX_CHAIN_MASK_4SS,
	.rx_chain_mask = CFG_TGT_DEFAULT_RX_CHAIN_MASK_4SS,
	.rx_timeout_pri = { CFG_TGT_RX_TIMEOUT_LO_PRI,
				CFG_TGT_RX_TIMEOUT_LO_PRI,
				CFG_TGT_RX_TIMEOUT_LO_PRI,
				CFG_TGT_RX_TIMEOUT_HI_PRI },
	.scan_max_pending_req = CFG_TGT_DEFAULT_SCAN_MAX_REQS,
	.bmiss_offload_max_vdev = CFG_TGT_DEFAULT_BMISS_OFFLOAD_MAX_VDEV,
	.roam_offload_max_vdev = CFG_TGT_DEFAULT_ROAM_OFFLOAD_MAX_VDEV,
	.roam_offload_max_ap_profiles =
		CFG_TGT_DEFAULT_ROAM_OFFLOAD_MAX_PROFILES,
	.num_mcast_groups = CFG_TGT_DEFAULT_NUM_MCAST_GROUPS,
	.num_mcast_table_elems = CFG_TGT_DEFAULT_NUM_MCAST_TABLE_ELEMS,
	.mcast2ucast_mode = CFG_TGT_DEFAULT_MCAST2UCAST_MODE,
	.tx_dbg_log_size = CFG_TGT_DEFAULT_TX_DBG_LOG_SIZE,
	.num_wds_entries = CFG_TGT_WDS_ENTRIES,
	.dma_burst_size = CFG_TGT_DEFAULT_DMA_BURST_SIZE,
	.mac_aggr_delim = CFG_TGT_DEFAULT_MAC_AGGR_DELIM,
	.rx_skip_defrag_timeout_dup_detection_check =
	     CFG_TGT_DEFAULT_RX_SKIP_DEFRAG_TIMEOUT_DUP_DETECTION_CHECK,
	.vow_config = CFG_TGT_DEFAULT_VOW_CONFIG,
	.gtk_offload_max_vdev = CFG_TGT_DEFAULT_GTK_OFFLOAD_MAX_VDEV,
	.num_msdu_desc = CFG_TGT_NUM_MSDU_DESC_AR900B,
	.max_frag_entries = 6,
	.beacon_tx_offload_max_vdev = CFG_TGT_NUM_BCN_OFFLOAD_VDEV,
#ifdef WLAN_SUPPORT_TWT
	.twt_ap_pdev_count = CFG_TGT_TWT_AP_PDEV_COUNT,
	.twt_ap_sta_count = CFG_TGT_TWT_AP_STA_COUNT,
#endif
	.peer_map_unmap_v2 = 1,
};

target_resource_config  tgt_cfg_legacy = {
	.num_vdevs = CFG_TGT_NUM_VDEV_AR988X,
	/* need to reserve an additional peer for each VDEV */
	.num_peers = CFG_TGT_NUM_PEERS + CFG_TGT_NUM_VDEV_AR988X,
	.num_offload_peers = CFG_TGT_NUM_OFFLOAD_PEERS,
	.num_offload_reorder_buffs = CFG_TGT_NUM_OFFLOAD_REORDER_BUFFS,
	.num_peer_keys = CFG_TGT_NUM_PEER_KEYS,
	.num_tids = CFG_TGT_NUM_TIDS,
	.ast_skid_limit = CFG_TGT_AST_SKID_LIMIT,
	.tx_chain_mask = CFG_TGT_DEFAULT_TX_CHAIN_MASK_4SS,
	.rx_chain_mask = CFG_TGT_DEFAULT_RX_CHAIN_MASK_4SS,
	.rx_timeout_pri = { CFG_TGT_RX_TIMEOUT_LO_PRI,
				CFG_TGT_RX_TIMEOUT_LO_PRI,
				CFG_TGT_RX_TIMEOUT_LO_PRI,
				CFG_TGT_RX_TIMEOUT_HI_PRI },
	.scan_max_pending_req = CFG_TGT_DEFAULT_SCAN_MAX_REQS,
	.bmiss_offload_max_vdev = CFG_TGT_DEFAULT_BMISS_OFFLOAD_MAX_VDEV,
	.roam_offload_max_vdev = CFG_TGT_DEFAULT_ROAM_OFFLOAD_MAX_VDEV,
	.roam_offload_max_ap_profiles =
		CFG_TGT_DEFAULT_ROAM_OFFLOAD_MAX_PROFILES,
	.num_mcast_groups = CFG_TGT_DEFAULT_NUM_MCAST_GROUPS,
	.num_mcast_table_elems = CFG_TGT_DEFAULT_NUM_MCAST_TABLE_ELEMS,
	.mcast2ucast_mode = CFG_TGT_DEFAULT_MCAST2UCAST_MODE,
	.tx_dbg_log_size = CFG_TGT_DEFAULT_TX_DBG_LOG_SIZE,
	.num_wds_entries = CFG_TGT_WDS_ENTRIES,
	.dma_burst_size = CFG_TGT_DEFAULT_DMA_BURST_SIZE,
	.mac_aggr_delim = CFG_TGT_DEFAULT_MAC_AGGR_DELIM,
	.rx_skip_defrag_timeout_dup_detection_check =
	    CFG_TGT_DEFAULT_RX_SKIP_DEFRAG_TIMEOUT_DUP_DETECTION_CHECK,
	.vow_config = CFG_TGT_DEFAULT_VOW_CONFIG,
	.gtk_offload_max_vdev = CFG_TGT_DEFAULT_GTK_OFFLOAD_MAX_VDEV,
	.num_msdu_desc = CFG_TGT_NUM_MSDU_DESC_AR900B,
	.max_frag_entries = 0,
	.max_peer_ext_stats = CFG_TGT_DEFAULT_MAX_PEER_EXT_STATS,
	.smart_ant_cap = 0,
	.BK_Minfree = 0,
	.BE_Minfree = 0,
	.VI_Minfree = 0,
	.VO_Minfree = 0,
	.rx_batchmode = 0,
	.tt_support = 0,
	.atf_config = 0,
	.iphdr_pad_config = 1,
};

wmi_host_ext_resource_config tgt_ext_cfg = {
	.host_platform_config = 0,
	.fw_feature_bitmap = 0,
};

void init_deinit_set_default_tgt_config(
		struct wlan_objmgr_psoc *psoc,
		struct target_psoc_info *tgt_hdl)
{
	target_resource_config *tgt_cfg;
	struct tgt_info *info;

	if (!tgt_hdl) {
                target_if_err("target_psoc_info is null in lteu ext support");
		return;
	}

	info = (&tgt_hdl->info);

#if QCA_WIFI_QCA8074
        if (info->target_type == TARGET_TYPE_QCA8074) {
            tgt_cfg = &tgt_cfg_qca8074;
        } else if (info->target_type == TARGET_TYPE_QCA8074V2) {
            tgt_cfg = &tgt_cfg_qca8074;
            if (target_psoc_get_preferred_hw_mode(tgt_hdl) ==
                                        WMI_HOST_HW_MODE_DBS_SBS) {
                tgt_cfg->num_peers = CFG_TGT_NUM_PEERS_QCA8074_3M;
            }
        } else if (info->target_type == TARGET_TYPE_QCA6018) {
            tgt_cfg = &tgt_cfg_qca8074;
        } else if (info->target_type == TARGET_TYPE_QCA6290) {
            tgt_cfg = &tgt_cfg_qca6290;
        } else {
            tgt_cfg = &tgt_cfg_legacy;
        }
#else
        tgt_cfg = &tgt_cfg_legacy;
#endif

	/* num_msdu_desc is initialized to CFG_TGT_NUM_MSDU_DESC_AR900B
	 * here it is re-initialized to user configured value.
	 */
	if (info->max_descs)
		tgt_cfg->num_msdu_desc = info->max_descs;

	qdf_mem_copy(&info->wlan_res_cfg, tgt_cfg,
			sizeof(target_resource_config));

	qdf_mem_copy(&info->wlan_ext_res_cfg, &tgt_ext_cfg,
			sizeof(wmi_host_ext_resource_config));

	target_if_set_init_cmd_dev_param(psoc, tgt_hdl);

}

QDF_STATUS init_deinit_sw_version_check(
		 struct wlan_objmgr_psoc *psoc,
		 struct target_psoc_info *tgt_hdl,
		 uint8_t *evt_buf)
{
	QDF_STATUS ver_match = QDF_STATUS_SUCCESS;
	struct wmi_host_fw_ver fw_ver;
	struct common_wmi_handle *wmi_handle;

	if (!tgt_hdl) {
                target_if_err("psoc target_psoc_info is null in version check");
		return QDF_STATUS_E_INVAL;
	}

	wmi_handle = target_psoc_get_wmi_hdl(tgt_hdl);

	if (wmi_extract_fw_version(wmi_handle, evt_buf, &fw_ver) ==
					QDF_STATUS_SUCCESS) {
		target_if_info("Firmware_Build_Number:%d",
				VER_GET_BUILD_NUM(fw_ver.sw_version_1));

		/* Check if the host driver is compatible with the RAM fw
		   version. If any of the Major/Minor/Patch/BuildNum
		   mismatches, fail attach.
		 */
		if (VER_GET_MAJOR(fw_ver.sw_version) != __VER_MAJOR_) {
			target_if_err(
				"host/RAM_fw Major Ver Mismatch: H:0x%X, F:0x%X !",
				__VER_MAJOR_, VER_GET_MAJOR(fw_ver.sw_version));
			ver_match = QDF_STATUS_E_FAILURE;
		}

		if (VER_GET_MINOR(fw_ver.sw_version) != __VER_MINOR_) {
			target_if_err(
			"host/RAM_fw Minor Ver Mismatch: H:0x%X, F:0x%X !",
			__VER_MINOR_, VER_GET_MINOR(fw_ver.sw_version));
			ver_match = QDF_STATUS_E_FAILURE;
		}

		if (VER_GET_RELEASE(fw_ver.sw_version_1) != __VER_RELEASE_) {
			target_if_err(
			"host/RAM_fw Patch Ver Mismatch: H:0x%X, F:0x%X !",
			__VER_RELEASE_, VER_GET_RELEASE(fw_ver.sw_version_1));
			ver_match = QDF_STATUS_E_FAILURE;
		}

		if (VER_GET_BUILD_NUM(fw_ver.sw_version_1) != __BUILD_NUMBER_) {
			target_if_err(
			"host/RAM_fw Build Ver Mismatch: H:0x%X, F:0x%X !",
			__BUILD_NUMBER_,
			VER_GET_BUILD_NUM(fw_ver.sw_version_1));
			ver_match = QDF_STATUS_E_FAILURE;
		}

		/* update the version info in the soc for the OS-es to check
		*/
		tgt_hdl->info.version.wlan_ver = fw_ver.sw_version;
		tgt_hdl->info.version.wlan_ver_1 = fw_ver.sw_version_1;

	}

	if(ver_match != QDF_STATUS_SUCCESS)
                target_if_err(
                "host/RAM_fw uses same Ver: Major:0x%X, Minor:0x%X,"
                "Release:0x%X, Build:0x%X",
		 __VER_MAJOR_, __VER_MINOR_, __VER_RELEASE_,  __BUILD_NUMBER_);

	return ver_match;
}

static struct target_ops targ_ops = {
	.ext_resource_config_enable = init_deinit_ext_resource_config_enable,
	.peer_config = init_deinit_peer_config,
	.mesh_support_enable = init_deinit_mesh_support_enable,
#if UNIFIED_SMARTANTENNA
	.smart_antenna_enable = init_deinit_smart_antenna_enable,
#else
	.smart_antenna_enable = NULL,
#endif
#if WLAN_CFR_ENABLE
        .cfr_support_enable = init_deinit_cfr_support_enable,
#else
        .cfr_support_enable = NULL,
#endif
#if QCA_AIRTIME_FAIRNESS
	.atf_config_enable = init_deinit_atf_config_enable,
#else
	.atf_config_enable = NULL,
#endif
	.qwrap_config_enable = init_deinit_qwrap_config_enable,
	.btcoex_config_enable = init_deinit_btcoex_config_enable,
	.lteu_ext_support_enable = init_deinit_lteu_ext_support_enable,
	.set_init_cmd_dev_based_params = init_deinit_set_init_cmd_dev_params,
	.alloc_pdevs = init_deinit_alloc_pdevs,
	.update_pdev_tgt_info = init_deinit_update_pdev_tgt_info,
#if WIFI_MEM_MANAGER_SUPPORT
	.mem_mgr_alloc_chunk = init_deinit_mem_mgr_alloc_chunk,
	.mem_mgr_free_chunks = init_deinit_mem_mgr_free_chunks,
#else
	.mem_mgr_alloc_chunk = NULL,
	.mem_mgr_free_chunks = NULL,
#endif
	.print_svc_ready_ex_param = init_deinit_print_service_ready_ex_params,
	.add_11ax_modes = init_deinit_add_11ax_modes,
	.set_default_tgt_config = init_deinit_set_default_tgt_config,
	.sw_version_check = init_deinit_sw_version_check,
	.eapol_minrate_enable = init_deinit_eapol_minrate_enable,
};

QDF_STATUS init_deinit_register_featurs_ops(struct wlan_objmgr_psoc *psoc)
{
	struct target_psoc_info *tgt_hdl;

	if (!psoc) {
		target_if_err("psoc is null");
		return QDF_STATUS_E_INVAL;
	}

	tgt_hdl = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
						psoc);
	if (!tgt_hdl) {
		target_if_err("target_psoc_info is null");
		return QDF_STATUS_E_INVAL;
	}

	tgt_hdl->tif_ops = &targ_ops;

	return QDF_STATUS_SUCCESS;
}

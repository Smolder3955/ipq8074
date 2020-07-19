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
 * DOC: wlan_lmac_dispatcher.c
 *
 * Public APIs to leagcy dispatch
 */
#include <qdf_status.h>
#include <qdf_types.h>
#include <qdf_module.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <target_if.h>
#include <wlan_lmac_dispatcher.h>
#include <ol_if_athvar.h>
#include "ol_if_athpriv.h"
#include "cfg_ucfg_api.h"

int wlan_ucfg_get_btcoex_param(struct wlan_objmgr_psoc *psoc,
					uint16_t param_type)
{
	ol_ath_soc_softc_t *soc;
	struct target_psoc_info *tgt_hdl;

	if (!psoc) {
		qdf_print("%s: psoc is null", __func__);
		return 0;
	}

	tgt_hdl = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
						psoc);
	if (!tgt_hdl) {
		qdf_print("%s: target_psoc_info is null", __func__);
		return 0;
	}

	soc = (ol_ath_soc_softc_t *)target_psoc_get_feature_ptr(tgt_hdl);
	if (!soc) {
		qdf_print("%s: feature ptr is null", __func__);
		return 0;
	}

	switch (param_type) {
	case BTCOEX_GPIO_PARAM:
		return soc->btcoex_gpio;
		break;
	case BTCOEX_ENABLE:
		return soc->btcoex_enable;
		break;
	case BTCOEX_DUTY_CYCLE:
		return soc->btcoex_duty_cycle;
		break;
	case BTCOEX_WL_PRIORITY:
		return soc->btcoex_wl_priority;
		break;
	case BTCOEX_DURATION:
		return soc->btcoex_duration;
		break;
	case BTCOEX_PERIOD:
		return soc->btcoex_period;
		break;
	default:
		return 0;
	}
	return 0;
}

QDF_STATUS wlan_ucfg_set_btcoex_param(struct wlan_objmgr_psoc *psoc,
				uint16_t param_type, int value, int value1)
{
	ol_ath_soc_softc_t *soc;
	struct target_psoc_info *tgt_hdl;

	if (!psoc) {
		qdf_print("%s: psoc is null", __func__);
		return QDF_STATUS_E_INVAL;
	}

	tgt_hdl = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
						psoc);
	if (!tgt_hdl) {
		qdf_print("%s: target_psoc_info is null", __func__);
		return QDF_STATUS_E_INVAL;
	}

	soc = (ol_ath_soc_softc_t *)target_psoc_get_feature_ptr(tgt_hdl);
	if (!soc) {
		qdf_print("%s: feature ptr is null", __func__);
		return QDF_STATUS_E_INVAL;
	}

	switch (param_type) {
	case BTCOEX_GPIO_PARAM:
		soc->btcoex_gpio = value;
		break;
	case BTCOEX_ENABLE:
		soc->btcoex_enable = value;
		break;
	case BTCOEX_DUTY_CYCLE:
		soc->btcoex_duty_cycle = value;
		break;
	case BTCOEX_WL_PRIORITY:
		if (ol_ath_btcoex_wlan_priority(soc, value)
				== QDF_STATUS_SUCCESS)
			soc->btcoex_wl_priority = value;
		else
			return QDF_STATUS_E_FAILURE;

		break;
	case BTCOEX_DURATION_PERIOD:
		if (ol_ath_btcoex_duty_cycle(soc, value, value1)
				== QDF_STATUS_SUCCESS) {
			soc->btcoex_duration = value;
			soc->btcoex_period = value1;
		} else {
			return QDF_STATUS_E_FAILURE;
		}
		break;
	default:
		return QDF_STATUS_E_INVAL;
	}
	return QDF_STATUS_SUCCESS;
}

uint32_t wlan_ucfg_get_config_param(struct wlan_objmgr_psoc *psoc,
				uint16_t param_type)
{
	ol_ath_soc_softc_t *soc;
	struct target_psoc_info *tgt_hdl;
	extern unsigned int bypasswmi;

	if (!psoc) {
		qdf_print("%s: psoc is null", __func__);
		return 0;
	}

	tgt_hdl = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
						psoc);
	if (!tgt_hdl) {
		qdf_print("%s: target_psoc_info is null", __func__);
		return 0;
	}

	soc = (ol_ath_soc_softc_t *)target_psoc_get_feature_ptr(tgt_hdl);
	if (!soc) {
		qdf_print("%s: feature ptr is null", __func__);
		return 0;
	}

	switch (param_type) {
	case MAX_ACTIVE_PEERS:
		return (uint32_t)soc->max_active_peers;
		break;
	case VOW_CONFIG:
		return (uint32_t)soc->vow_config;
		break;
	case AC_BK_MINFREE:
		return (uint32_t)cfg_get(psoc, CFG_OL_ACBK_MIN_FREE);
		break;
	case AC_BE_MINFREE:
		return (uint32_t)cfg_get(psoc, CFG_OL_ACBE_MIN_FREE);
		break;
	case AC_VI_MINFREE:
		return (uint32_t)cfg_get(psoc, CFG_OL_ACVI_MIN_FREE);
		break;
	case AC_VO_MINFREE:
		return (uint32_t)cfg_get(psoc, CFG_OL_ACVO_MIN_FREE);
		break;
	case SOC_ID:
		return (uint32_t)soc->soc_idx;
		break;
	case MESH_SUPPORT:
		return (uint32_t)cfg_get(psoc, CFG_OL_ENABLE_MESH_SUPPORT);
		break;
	case IPHDR_PAD:
		return (uint32_t)cfg_get(psoc, CFG_OL_CFG_IPHDR);
		break;
	case BYPASSWMI:
		return (uint32_t)bypasswmi;
		break;
	case LOW_MEMSYS:
		return (uint32_t)soc->low_mem_system;
		break;
	case SCHED_PARAMS:
		return (uint32_t)soc->tgt_sched_params;
		break;
	case CCE_STATE:
		return (uint32_t)soc->cce_disable;
		break;
	case TGT_IRAM_BKP_ADDR:
		return (uint32_t)soc->tgt_iram_bkp_paddr;
		break;
	case EAPOL_MINRATE_SET:
		return (uint32_t)cfg_get(psoc, CFG_OL_EAPOL_MINRATE_SET);
		break;
	case EAPOL_MINRATE_AC_SET:
		return (uint32_t)cfg_get(psoc, CFG_OL_EAPOL_MINRATE_AC_SET);
		break;
	case CARRIER_VOW_CONFIG:
		return (uint32_t)cfg_get(psoc, CFG_OL_CARRIER_VOW_CONFIG);
	case FW_VOW_STATS_ENABLE:
		return (uint32_t)cfg_get(psoc, CFG_OL_FW_VOW_STATS_ENABLE);
	default:
		return 0;
	}
	return 0;
}
qdf_export_symbol(wlan_ucfg_get_config_param);

#if OS_SUPPORT_ASYNC_Q
static void os_async_pdev_mesg_handler(void  *ctx, u_int16_t  mesg_type,
					u_int16_t  mesg_len, void  *mesg)
{
	if (mesg_type == OS_SCHEDULE_ROUTING_MESG_TYPE) {
		os_schedule_routing_mesg *s_mesg =
					(os_schedule_routing_mesg *) mesg;
		s_mesg->routine(s_mesg->context, NULL);
	}
}
#endif


#define WIFI_DEV_NAME_PDEV	"wifi%d"

qdf_netdev_t wlan_create_pdev_netdevice(struct wlan_objmgr_psoc *psoc,
						uint8_t id)
{
	qdf_netdev_t dev;
	ol_ath_soc_softc_t *soc;
	struct ol_ath_softc_net80211 *scn;
	struct target_psoc_info *tgt_hdl;

	if (!psoc) {
		qdf_print("%s: psoc is null", __func__);
		return 0;
	}

	tgt_hdl = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
						psoc);
	if (!tgt_hdl) {
		qdf_print("%s: target_psoc_info is null", __func__);
		return NULL;
	}

	soc = (ol_ath_soc_softc_t *)target_psoc_get_feature_ptr(tgt_hdl);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
	dev = alloc_netdev(sizeof(struct ol_ath_softc_net80211),
			soc->is_sim ? "wifi-sim%d" : WIFI_DEV_NAME_PDEV,
			0,
			ether_setup);
#else
	dev = alloc_netdev(sizeof(struct ol_ath_softc_net80211),
			soc->is_sim ? "wifi-sim%d" : WIFI_DEV_NAME_PDEV,
			ether_setup);
#endif
	if (dev == NULL)
		return NULL;

	scn = ath_netdev_priv(dev);
	OS_MEMZERO(scn, sizeof(*scn));

	scn->sc_osdev = qdf_mem_malloc(sizeof(*scn->sc_osdev));
	if (scn->sc_osdev == NULL) {
		qdf_print("%s: Error while allocating scn->sc_osdev",
				__func__);
		free_netdev(dev);
		return NULL;
	}
	qdf_mem_copy(scn->sc_osdev, soc->sc_osdev, sizeof(*(scn->sc_osdev)));

#if OS_SUPPORT_ASYNC_Q
	OS_MESGQ_INIT(scn->sc_osdev, &scn->sc_osdev->async_q,
			sizeof(os_async_q_mesg), OS_ASYNC_Q_MAX_MESGS,
			os_async_pdev_mesg_handler, scn->sc_osdev,
			MESGQ_PRIORITY_NORMAL,
			MESGQ_ASYNCHRONOUS_EVENT_DELIVERY);
#endif

	scn->netdev = dev;
	scn->soc = soc;

	return dev;
}

extern bool ol_ath_net80211_is_mode_offload(struct ieee80211com *ic);

void wlan_pdev_update_feature_ptr(struct wlan_objmgr_psoc *psoc,
		struct wlan_objmgr_pdev *pdev, qdf_netdev_t pdev_netdev)
{
	struct target_pdev_info *pdev_tgt_info;
	struct target_psoc_info *psoc_tgt_info;
	struct ol_ath_softc_net80211 *scn;
	struct ieee80211com *ic;
#ifndef REMOVE_PKT_LOG
	extern unsigned int enable_pktlog_support;
#endif
	scn = ath_netdev_priv(pdev_netdev);

	scn->sc_pdev = pdev;

	psoc_tgt_info = wlan_psoc_get_tgt_if_handle(psoc);
	if (!psoc_tgt_info) {
		qdf_print("%s: psoc_tgt_info is NULL", __func__);
		return;
	}

	pdev_tgt_info = wlan_pdev_get_tgt_if_handle(pdev);
	if (!pdev_tgt_info) {
		qdf_print("%s: pdev_tgt_info is NULL", __func__);
		return;
	}

	target_pdev_set_feature_ptr(pdev_tgt_info, (void *)scn);

	/*
	 * pktlog scn initialization
	 */
#ifndef REMOVE_PKT_LOG
	if (enable_pktlog_support) {
		ol_pl_sethandle(&(scn->pl_dev), scn);
		ol_pl_set_name(scn, pdev_netdev);
	} else {
		scn->pl_dev = NULL;
	}
#endif
	ic = &scn->sc_ic;
	qdf_event_create(&ic->ic_wait_for_init_cc_response);

	ic->ic_get_min_and_max_power = ol_ath_get_min_and_max_power;
	ic->ic_fill_hal_chans_from_reg_db = ol_ath_fill_hal_chans_from_reg_db;
	ic->ic_get_modeSelect = ol_ath_get_modeSelect;
	ic->ic_get_chip_mode = ol_ath_get_chip_mode;
	ic->ic_is_mode_offload = ol_ath_net80211_is_mode_offload;
	ic->ic_is_regulatory_offloaded = ol_ath_is_regulatory_offloaded;
}

void wlan_psoc_update_peer_count(struct wlan_objmgr_psoc *psoc,
	target_resource_config *tgt_cfg)
{
	ol_ath_soc_softc_t *soc;
	struct target_psoc_info *psoc_tgt_info;

	psoc_tgt_info = wlan_psoc_get_tgt_if_handle(psoc);
	if (!psoc_tgt_info) {
		qdf_print("%s: psoc_tgt_info is NULL", __func__);
		return;
	}

	soc = target_psoc_get_feature_ptr(psoc_tgt_info);
	if (!soc) {
		qdf_print("%s: soc is NULL", __func__);
		return;
	}

	return;
}

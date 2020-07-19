/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc..
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#ifndef _WLAN_SA_API_TGT_API_H_
#define _WLAN_SA_API_TGT_API_H_

#include "../../core/sa_api_defs.h"

uint32_t tgt_sa_api_get_sa_supported(struct wlan_objmgr_psoc *psoc);
uint32_t tgt_sa_api_get_validate_sw(struct wlan_objmgr_psoc *psoc);
void tgt_sa_api_enable_sa(struct wlan_objmgr_psoc *psoc, uint32_t value);
uint32_t tgt_sa_api_get_sa_enable(struct wlan_objmgr_psoc *psoc);
void tgt_sa_api_peer_assoc_hanldler(struct wlan_objmgr_pdev *pdev, struct wlan_objmgr_peer *peer, struct sa_rate_cap *rate_cap);
uint32_t tgt_sa_api_update_tx_feedback(struct wlan_objmgr_pdev *pdev, struct wlan_objmgr_peer *peer, struct sa_tx_feedback *feedback);
uint32_t tgt_sa_api_update_rx_feedback(struct wlan_objmgr_pdev *pdev, struct wlan_objmgr_peer *peer, struct sa_rx_feedback *feedback);
uint32_t tgt_sa_api_is_tx_feedback_enabled(struct wlan_objmgr_pdev *pdev);
uint32_t tgt_sa_api_is_rx_feedback_enabled(struct wlan_objmgr_pdev *pdev);
uint32_t tgt_sa_api_convert_rate_2g(uint32_t rate);
uint32_t tgt_sa_api_convert_rate_5g(uint32_t rate);
uint32_t tgt_sa_api_get_sa_mode(struct wlan_objmgr_pdev *pdev);
uint32_t tgt_sa_api_ucfg_set_param(struct wlan_objmgr_pdev *pdev, char *val);
uint32_t tgt_sa_api_ucfg_get_param(struct wlan_objmgr_pdev *pdev, char *val);
uint32_t tgt_sa_api_get_beacon_txantenna(struct wlan_objmgr_pdev *pdev);
uint32_t tgt_sa_api_cwm_action(struct wlan_objmgr_pdev *pdev);

static inline struct wlan_lmac_if_sa_api_tx_ops *
wlan_psoc_get_sa_api_txops(struct wlan_objmgr_psoc *psoc)
{
	return &((psoc->soc_cb.tx_ops.sa_api_tx_ops));
}

static inline
void tgt_sa_api_start_sa(struct wlan_objmgr_psoc *psoc,
		struct wlan_objmgr_pdev *pdev, uint32_t enable,
		uint32_t mode, uint32_t rx_antenna)
{
	struct wlan_lmac_if_sa_api_tx_ops *sa_api_ops = NULL;

	sa_api_ops = wlan_psoc_get_sa_api_txops(psoc);

	QDF_ASSERT(sa_api_ops->sa_api_enable_sa);

	if (sa_api_ops->sa_api_enable_sa) {
		sa_api_ops->sa_api_enable_sa(pdev, enable, mode, rx_antenna);
	}
}

static inline
void tgt_sa_api_set_rx_antenna(struct wlan_objmgr_psoc *psoc,
		struct wlan_objmgr_pdev *pdev,
		uint32_t antenna)
{
	struct wlan_lmac_if_sa_api_tx_ops *sa_api_tx_ops = NULL;

	sa_api_tx_ops = wlan_psoc_get_sa_api_txops(psoc);

	QDF_ASSERT(sa_api_tx_ops->sa_api_set_rx_antenna);

	if (sa_api_tx_ops->sa_api_set_rx_antenna) {
		sa_api_tx_ops->sa_api_set_rx_antenna(pdev, antenna);
	}
}

static inline
void tgt_sa_api_set_tx_antenna(struct wlan_objmgr_psoc *psoc,
		struct wlan_objmgr_peer *peer,
		uint32_t *antenna_array)
{
	struct wlan_lmac_if_sa_api_tx_ops *sa_api_tx_ops = NULL;

	sa_api_tx_ops = wlan_psoc_get_sa_api_txops(psoc);

	QDF_ASSERT(sa_api_tx_ops->sa_api_set_tx_antenna);

	if (sa_api_tx_ops->sa_api_set_tx_antenna) {
		sa_api_tx_ops->sa_api_set_tx_antenna(peer, antenna_array);
	}
}

static inline
void tgt_sa_api_set_node_config_ops(struct wlan_objmgr_psoc *psoc,
		struct wlan_objmgr_peer *peer,
		uint32_t cmd_id, uint16_t args_count,
		u_int32_t args_arr[])
{
	struct wlan_lmac_if_sa_api_tx_ops *sa_api_tx_ops = NULL;

	sa_api_tx_ops = wlan_psoc_get_sa_api_txops(psoc);

	QDF_ASSERT(sa_api_tx_ops->sa_api_set_node_config_ops);

	if (sa_api_tx_ops->sa_api_set_node_config_ops) {
		sa_api_tx_ops->sa_api_set_node_config_ops(peer, cmd_id , args_count, args_arr);
	}
}

static inline
void tgt_sa_api_set_training_info(struct wlan_objmgr_psoc *psoc,
		struct wlan_objmgr_peer *peer,
		uint32_t *rate_array,
		uint32_t *antenna_array,
		uint32_t numpkts)
{
	struct wlan_lmac_if_sa_api_tx_ops *sa_api_tx_ops = NULL;

	sa_api_tx_ops = wlan_psoc_get_sa_api_txops(psoc);

	QDF_ASSERT(sa_api_tx_ops->sa_api_set_training_info);

	if (sa_api_tx_ops->sa_api_set_training_info) {
		sa_api_tx_ops->sa_api_set_training_info(peer, rate_array , antenna_array, numpkts);
	}
}

static inline
uint32_t tgt_get_if_id(struct wlan_objmgr_psoc *psoc,
		struct wlan_objmgr_pdev *pdev)
{
	struct wlan_lmac_if_mlme_tx_ops *mops;

	mops = &(psoc->soc_cb.tx_ops.mops);

	if (mops->get_wifi_iface_id) {
		return mops->get_wifi_iface_id(pdev);
	}

	return (uint32_t)-1;
}

static inline
QDF_STATUS tgt_sa_api_prepare_rateset(struct wlan_objmgr_psoc *psoc,
		struct wlan_objmgr_pdev *pdev,
		struct wlan_objmgr_peer *peer,
		struct sa_rate_info *rate_info)
{
	struct wlan_lmac_if_sa_api_tx_ops *sa_api_tx_ops = NULL;

	sa_api_tx_ops = wlan_psoc_get_sa_api_txops(psoc);

	if (sa_api_tx_ops->sa_api_prepare_rateset) {
		sa_api_tx_ops->sa_api_prepare_rateset(pdev, peer, rate_info);
		return QDF_STATUS_SUCCESS;
	}

	return QDF_STATUS_E_FAILURE;
}

static inline
void tgt_sa_api_register_event_handler(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_sa_api_tx_ops *sa_api_tx_ops = NULL;

	sa_api_tx_ops = wlan_psoc_get_sa_api_txops(psoc);

	if (sa_api_tx_ops->sa_api_register_event_handler) {
		sa_api_tx_ops->sa_api_register_event_handler(psoc);
	}
}


static inline
void tgt_sa_api_unregister_event_handler(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_lmac_if_sa_api_tx_ops *sa_api_tx_ops = NULL;

	sa_api_tx_ops = wlan_psoc_get_sa_api_txops(psoc);

	if (sa_api_tx_ops->sa_api_unregister_event_handler) {
		sa_api_tx_ops->sa_api_unregister_event_handler(psoc);
	}

}

#endif /* _WLAN_SA_API_TGT_API_H_*/

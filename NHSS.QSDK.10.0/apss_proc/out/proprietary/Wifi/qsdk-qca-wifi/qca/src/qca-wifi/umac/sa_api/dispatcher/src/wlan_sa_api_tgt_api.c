/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#include <wlan_sa_api_tgt_api.h>
#include "../../core/sa_api_cmn_api.h"

uint32_t tgt_sa_api_get_sa_supported(struct wlan_objmgr_psoc *psoc)
{
	struct sa_api_context *sc = NULL;

	if (NULL == psoc) {
		sa_api_err("PSOC is NULL!\n");
		return 0;
	}

	sc = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_SA_API);
	if (NULL == sc) {
		sa_api_err("sa api context is NULL!\n");
		return 0;
	}

	return sc->sa_suppoted;
}

uint32_t tgt_sa_api_get_validate_sw(struct wlan_objmgr_psoc *psoc)
{
	struct sa_api_context *sc = NULL;

	if (NULL == psoc) {
		sa_api_err("PSOC is NULL!\n");
		return 0;
	}

	sc = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_SA_API);
	if (NULL == sc) {
		sa_api_err("sa api context is NULL!\n");
		return 0;
	}

	return sc->sa_validate_sw;
}

void tgt_sa_api_enable_sa(struct wlan_objmgr_psoc *psoc, uint32_t value)
{
	struct sa_api_context *sc = NULL;

	if (NULL == psoc) {
		sa_api_err("PSOC is NULL!\n");
		return;
	}

	sc = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_SA_API);
	if (NULL == sc) {
		sa_api_err("sa api context is NULL!\n");
		return;
	}

	sc->enable = !!value;
}

uint32_t tgt_sa_api_get_sa_enable(struct wlan_objmgr_psoc *psoc)
{
	struct sa_api_context *sc = NULL;

	if (NULL == psoc) {
		sa_api_err("PSOC is NULL!\n");
		return 0;
	}

	sc = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_SA_API);
	if (NULL == sc) {
		sa_api_err("sa api context is NULL!\n");
		return 0;
	}

	return sc->enable;
}

void tgt_sa_api_peer_assoc_hanldler(struct wlan_objmgr_pdev *pdev, struct wlan_objmgr_peer *peer, struct sa_rate_cap *rate_cap)
{
	sa_api_peer_connect(pdev, peer, rate_cap);
}

uint32_t tgt_sa_api_update_tx_feedback(struct wlan_objmgr_pdev *pdev, struct wlan_objmgr_peer *peer, struct sa_tx_feedback *feedback)
{
	return sa_api_update_tx_feedback(pdev, peer, feedback);
}

uint32_t tgt_sa_api_update_rx_feedback(struct wlan_objmgr_pdev *pdev, struct wlan_objmgr_peer *peer, struct sa_rx_feedback *feedback)
{
	return sa_api_update_rx_feedback(pdev, peer, feedback);
}

uint32_t tgt_sa_api_is_tx_feedback_enabled(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_sa_api *pa;
	uint32_t enable;

	/* avoiding locks in data path*/
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_SA_API);

	enable = SMART_ANTENNA_TX_FEEDBACK_ENABLED(pa);
	return !!enable;
}

uint32_t tgt_sa_api_is_rx_feedback_enabled(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_sa_api *pa;
	uint32_t enable;

	/* avoiding locks in data path*/
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_SA_API);

	enable = SMART_ANTENNA_RX_FEEDBACK_ENABLED(pa);
	return !!enable;
}

uint32_t tgt_sa_api_convert_rate_2g(uint32_t rate)
{
	return sa_api_convert_rate_2g(rate);
}

uint32_t tgt_sa_api_convert_rate_5g(uint32_t rate)
{
	return sa_api_convert_rate_5g(rate);
}

uint32_t tgt_sa_api_get_sa_mode(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_sa_api *pa;
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_SA_API);
	return pa->mode;
}

uint32_t tgt_sa_api_ucfg_set_param(struct wlan_objmgr_pdev *pdev, char *val)
{
	return sa_api_set_param(pdev, val);
}

uint32_t tgt_sa_api_ucfg_get_param(struct wlan_objmgr_pdev *pdev, char *val)
{
	return sa_api_get_param(pdev, val);
}

uint32_t tgt_sa_api_get_beacon_txantenna(struct wlan_objmgr_pdev *pdev)
{
	uint32_t antenna = 0;
	sa_api_get_bcn_txantenna(pdev, &antenna);
	return antenna;
}

uint32_t tgt_sa_api_cwm_action(struct wlan_objmgr_pdev *pdev)
{
	return sa_api_cwm_action(pdev);
}

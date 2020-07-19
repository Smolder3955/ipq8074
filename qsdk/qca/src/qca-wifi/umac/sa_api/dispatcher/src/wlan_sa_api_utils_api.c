/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc..
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#include <../../core/sa_api_defs.h>
#include <wlan_sa_api_utils_api.h>
#include <qdf_module.h>
#include "../../core/sa_api_cmn_api.h"

/* Do sa_api Mode Configuration */
uint32_t sa_api_mode;
qdf_declare_param(sa_api_mode, uint);
EXPORT_SYMBOL(sa_api_mode);

QDF_STATUS wlan_sa_api_init(void)
{
	sa_api_debug("+");

	if (wlan_objmgr_register_psoc_create_handler(WLAN_UMAC_COMP_SA_API,
				wlan_sa_api_psoc_obj_create_handler, NULL) != QDF_STATUS_SUCCESS) {
		sa_api_err("registering psoc create handler failed\n");
		return QDF_STATUS_E_FAILURE;
	}

	if (wlan_objmgr_register_psoc_destroy_handler(WLAN_UMAC_COMP_SA_API,
				wlan_sa_api_psoc_obj_destroy_handler, NULL) != QDF_STATUS_SUCCESS) {
		sa_api_err("registering psoc destroy handler failed\n");
		return QDF_STATUS_E_FAILURE;
	}

	if (wlan_objmgr_register_pdev_create_handler(WLAN_UMAC_COMP_SA_API,
				wlan_sa_api_pdev_obj_create_handler, NULL) != QDF_STATUS_SUCCESS) {
		sa_api_err("registering pdev create handler failed\n");
		return QDF_STATUS_E_FAILURE;
	}

	if (wlan_objmgr_register_pdev_destroy_handler(WLAN_UMAC_COMP_SA_API,
				wlan_sa_api_pdev_obj_destroy_handler, NULL) != QDF_STATUS_SUCCESS) {
		sa_api_err("registering pdev destroy handler failed\n");
		return QDF_STATUS_E_FAILURE;
	}

	if (wlan_objmgr_register_peer_create_handler(WLAN_UMAC_COMP_SA_API,
				wlan_sa_api_peer_obj_create_handler, NULL) != QDF_STATUS_SUCCESS) {
		sa_api_err("registering peer create handler failed\n");
		return QDF_STATUS_E_FAILURE;
	}

	if (wlan_objmgr_register_peer_destroy_handler(WLAN_UMAC_COMP_SA_API,
				wlan_sa_api_peer_obj_destroy_handler, NULL) != QDF_STATUS_SUCCESS) {
		sa_api_err("registering peer destroy handler failed\n");
		return QDF_STATUS_E_FAILURE;
	}

	sa_api_debug("-");
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_sa_api_deinit(void)
{
	sa_api_debug("+");
	if (wlan_objmgr_unregister_psoc_create_handler(WLAN_UMAC_COMP_SA_API,
				wlan_sa_api_psoc_obj_create_handler, NULL) != QDF_STATUS_SUCCESS) {
		sa_api_err("deregistering psoc create handler failed\n");
		return QDF_STATUS_E_FAILURE;
	}

	if (wlan_objmgr_unregister_psoc_destroy_handler(WLAN_UMAC_COMP_SA_API,
				wlan_sa_api_psoc_obj_destroy_handler, NULL) != QDF_STATUS_SUCCESS) {
		sa_api_err("deregistering psoc destroy handler failed\n");
		return QDF_STATUS_E_FAILURE;
	}

	if (wlan_objmgr_unregister_pdev_create_handler(WLAN_UMAC_COMP_SA_API,
				wlan_sa_api_pdev_obj_create_handler, NULL) != QDF_STATUS_SUCCESS) {
		sa_api_err("deregistering pdev create handler failed\n");
		return QDF_STATUS_E_FAILURE;
	}

	if (wlan_objmgr_unregister_pdev_destroy_handler(WLAN_UMAC_COMP_SA_API,
				wlan_sa_api_pdev_obj_destroy_handler, NULL) != QDF_STATUS_SUCCESS) {
		sa_api_err("deregistering pdev destroy handler failed\n");
		return QDF_STATUS_E_FAILURE;
	}

	if (wlan_objmgr_unregister_peer_create_handler(WLAN_UMAC_COMP_SA_API,
				wlan_sa_api_peer_obj_create_handler, NULL) != QDF_STATUS_SUCCESS) {
		sa_api_err("deregistering peer create handler failed\n");
		return QDF_STATUS_E_FAILURE;
	}

	if (wlan_objmgr_unregister_peer_destroy_handler(WLAN_UMAC_COMP_SA_API,
				wlan_sa_api_peer_obj_destroy_handler, NULL) != QDF_STATUS_SUCCESS) {
		sa_api_err("deregistering peer destroy handler failed\n");
		return QDF_STATUS_E_FAILURE;
	}

	sa_api_debug("-");
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_sa_api_enable(struct wlan_objmgr_psoc *psoc)
{
	struct sa_api_context *sc = NULL;

	sa_api_debug("+");
	if (NULL == psoc) {
		sa_api_err("PSOC is null!\n");
		return QDF_STATUS_E_FAILURE;
	}

	sc = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_SA_API);
	if (NULL == sc) {
		sa_api_err("sa_api_context is null!\n");
		return QDF_STATUS_E_FAILURE;
	}

	if (sc->sa_api_enable)
		sc->sa_api_enable(psoc);

	sa_api_debug("-");
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_sa_api_disable(struct wlan_objmgr_psoc *psoc)
{
	struct sa_api_context *sc = NULL;

	sa_api_debug("+");
	if (NULL == psoc) {
		sa_api_err("PSOC is null!\n");
		return QDF_STATUS_E_FAILURE;
	}

	sc = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_SA_API);
	if (NULL == sc) {
		sa_api_err("sa_api_context is null!\n");
		return QDF_STATUS_E_FAILURE;
	}

	if (sc->sa_api_disable)
		sc->sa_api_disable(psoc);

	sa_api_debug("-");
	return QDF_STATUS_SUCCESS;
}

int wlan_sa_api_cwm_action(struct wlan_objmgr_pdev  *pdev)
{
	sa_api_debug("#");
	return sa_api_cwm_action(pdev);
}

int wlan_sa_api_get_bcn_txantenna(struct wlan_objmgr_pdev *pdev, uint32_t *ant)
{
	sa_api_debug("#");
	return sa_api_get_bcn_txantenna(pdev, ant);
}

void wlan_sa_api_channel_change(struct wlan_objmgr_pdev *pdev)
{
	sa_api_debug("#");
	sa_api_channel_change(pdev);
}

void wlan_sa_api_peer_disconnect(struct wlan_objmgr_peer *peer)
{
	sa_api_debug("#");
	sa_api_peer_disconnect(peer);
}

void wlan_sa_api_peer_connect(struct wlan_objmgr_pdev *pdev, struct wlan_objmgr_peer *peer, struct sa_rate_cap *rate_cap)
{
	sa_api_debug("#");
	sa_api_peer_connect(pdev, peer, rate_cap);
}

int wlan_sa_api_start(struct wlan_objmgr_pdev *pdev, struct wlan_objmgr_vdev *vdev, int new_init)
{
	struct pdev_sa_api *pa;

	sa_api_debug("#");
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_SA_API);
	if (pa == NULL) {
		sa_api_err("pa is null!\n");
		return QDF_STATUS_E_FAILURE;
	}

	return sa_api_init(pdev, vdev, pa, new_init);
}

int wlan_sa_api_stop(struct wlan_objmgr_pdev *pdev, struct wlan_objmgr_vdev *vdev, int notify)
{
	struct pdev_sa_api *pa;

	sa_api_debug("#");
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_SA_API);
	if (pa == NULL) {
		sa_api_err("pa is null!\n");
		return QDF_STATUS_E_FAILURE;
	}

	return sa_api_deinit(pdev, vdev, pa, notify);
}

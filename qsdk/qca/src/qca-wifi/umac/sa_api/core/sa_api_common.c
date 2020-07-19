/*
 * Copyright (c) 2013,2017 Qualcomm Innovation Center, Inc..
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 * 2013 Qualcomm Atheros, Inc.
 */

#include "sa_api_defs.h"
#include "sa_api_cmn_api.h"
#include <qdf_mem.h>
#include <qdf_types.h>
#include <osif_private.h>
#include <wlan_mlme_dispatcher.h>
#include "sa_api_da.h"
#include "sa_api_ol.h"
#include "cfg_sa.h"
#include "cfg_ucfg_api.h"

struct smartantenna_ops *g_sa_ops = NULL;
qdf_atomic_t g_sa_init;

uint32_t rate_table_24[MAX_OFDM_CCK_RATES] = {0x1b,0x1a,0x19,0x18,0x0b,0x0f,0x0a,0x0e,0x09,0x0d, 0x08,0x0c};
uint32_t rate_table_5[MAX_OFDM_CCK_RATES] = {0x43,0x42,0x41,0x40,0x03,0x07,0x02,0x06,0x01,0x05,0x00,0x04};

QDF_STATUS
wlan_sa_api_psoc_obj_create_handler(struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct sa_api_context *sc = NULL;

	if (NULL == psoc) {
		sa_api_err("PSOC is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}

	sc = (struct sa_api_context *)qdf_mem_malloc(sizeof(struct sa_api_context));
	if (NULL == sc) {
		sa_api_err("Failed to allocate sa_api_ctx object\n");
		return QDF_STATUS_E_NOMEM;
	}

	sc->psoc_obj = psoc;
	sc->sa_validate_sw = cfg_get(psoc, CFG_SA_VALIDATE_SW);

	if (WLAN_DEV_DA == wlan_objmgr_psoc_get_dev_type(psoc)) {
		sc->enable = cfg_get(psoc, CFG_SA_ENABLE_DA);
		sc->sa_suppoted = cfg_get(psoc, CFG_SA_ENABLE_DA);
	} else {
		sc->sa_suppoted = cfg_get(psoc, CFG_SA_ENABLE);
		sc->enable = 0;
	}

#if DA_SUPPORT
	if (WLAN_DEV_DA == wlan_objmgr_psoc_get_dev_type(psoc))
		sa_api_ctx_init_da(sc);
#endif
	if (WLAN_DEV_OL == wlan_objmgr_psoc_get_dev_type(psoc))
		sa_api_ctx_init_ol(sc);

	sa_api_info("soc %pK sa_suppoted %d, sa_validate_sw %d, enable %d",
			psoc, sc->sa_suppoted, sc->sa_validate_sw, sc->enable);
	wlan_objmgr_psoc_component_obj_attach(psoc, WLAN_UMAC_COMP_SA_API,
			(void *)sc, QDF_STATUS_SUCCESS);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_sa_api_psoc_obj_destroy_handler(struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct sa_api_context *sc = NULL;

	if (NULL == psoc) {
		sa_api_err("PSOC is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}

	sa_api_debug("#");
	sc = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_SA_API);
	if (NULL != sc) {
		wlan_objmgr_psoc_component_obj_detach(psoc, WLAN_UMAC_COMP_SA_API,
				(void *)sc);
		qdf_mem_free(sc);
	}

	return QDF_STATUS_SUCCESS;
}


QDF_STATUS
wlan_sa_api_pdev_obj_create_handler(struct wlan_objmgr_pdev *pdev, void *arg)
{
	struct wlan_objmgr_psoc *psoc;
	struct sa_api_context *sc = NULL;
	struct pdev_sa_api *pa = NULL;

	if (NULL == pdev) {
		sa_api_err("PDEV is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}

	pa = (struct pdev_sa_api *)qdf_mem_malloc(sizeof(struct pdev_sa_api));
	if (NULL == pa) {
		sa_api_err("Failed to allocate pdev_sa_api object\n");
		return QDF_STATUS_E_NOMEM;
	}

	qdf_mem_zero(pa, sizeof(struct pdev_sa_api));

	psoc = wlan_pdev_get_psoc(pdev);
	if (NULL == psoc) {
		sa_api_err("PSOC is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}

	sc = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_SA_API);
	if (NULL == sc) {
		sa_api_err("SA API context is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}

	if (sc->sa_api_pdev_init)
		sc->sa_api_pdev_init(psoc, pdev, sc, pa);

	sa_api_info("pdev %pK state %d, mode %d, max_fallback_rates %d, "
			"radio_id %d, interface_id %d, enable %d", pdev,
			pa->state, pa->mode, pa->max_fallback_rates,
			pa->radio_id, pa->interface_id, pa->enable);
	wlan_objmgr_pdev_component_obj_attach(pdev, WLAN_UMAC_COMP_SA_API,
			(void *)pa, QDF_STATUS_SUCCESS);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_sa_api_pdev_obj_destroy_handler(struct wlan_objmgr_pdev *pdev, void *arg)
{
	struct pdev_sa_api *pa = NULL;

	if (NULL == pdev) {
		sa_api_err("PDEV is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}

	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_SA_API);
	if (NULL != pa) {
		wlan_objmgr_pdev_component_obj_detach(pdev, WLAN_UMAC_COMP_SA_API,
				(void *)pa);
		qdf_mem_free(pa);
	}

	sa_api_debug("#");
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_sa_api_vdev_obj_create_handler(struct wlan_objmgr_vdev *vdev, void *arg)
{
	struct wlan_objmgr_pdev *pdev;
	struct pdev_sa_api *pa = NULL;

	if (NULL == vdev) {
		sa_api_err("VDEV is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}

	pdev = wlan_vdev_get_pdev(vdev);

	if (NULL == pdev) {
		sa_api_err("PDEV is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}

	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_SA_API);
	if (NULL == pa) {
		sa_api_err("aP is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_sa_api_vdev_obj_destroy_handler(struct wlan_objmgr_vdev *vdev, void *arg)
{
	struct wlan_objmgr_pdev *pdev;
	struct pdev_sa_api *pa = NULL;

	if (NULL == vdev) {
		sa_api_err("VDEV is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (NULL == pdev) {
		sa_api_err("PDEV is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}

	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_SA_API);
	if (NULL == pa) {
		sa_api_err("aP is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}

	sa_api_deinit(pdev, vdev, pa, SMART_ANT_NEW_CONFIGURATION);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_sa_api_peer_obj_create_handler(struct wlan_objmgr_peer *peer, void *arg)
{
	struct peer_sa_api *pe = NULL;

	if (NULL == peer) {
		sa_api_err("PEER is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}

	pe = (struct peer_sa_api *)qdf_mem_malloc(sizeof(struct peer_sa_api));
	if (NULL == pe) {
		sa_api_err("Failed to allocate peer_sa_api object\n");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_zero(pe, sizeof(struct peer_sa_api));
	pe->peer_obj = peer;
	pe->ch_width = wlan_node_get_peer_chwidth(peer);

	wlan_objmgr_peer_component_obj_attach(peer, WLAN_UMAC_COMP_SA_API,
			(void *)pe,
			QDF_STATUS_SUCCESS);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_sa_api_peer_obj_destroy_handler(struct wlan_objmgr_peer *peer, void *arg)
{
	struct peer_sa_api *pe = NULL;

	if (NULL == peer) {
		sa_api_err("PEER is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}

	sa_api_peer_disconnect(peer);

	pe = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_SA_API);
	if (pe != NULL) {
		wlan_objmgr_peer_component_obj_detach(peer, WLAN_UMAC_COMP_SA_API,
				(void *)pe);
		qdf_mem_free(pe);
	}

	return QDF_STATUS_SUCCESS;
}

int register_smart_ant_ops(struct smartantenna_ops *sa_ops)
{
	g_sa_ops = sa_ops;
	qdf_atomic_init(&g_sa_init);
	return SMART_ANT_STATUS_SUCCESS;
}

int deregister_smart_ant_ops(char *dev_name)
{
#if 0
	struct ath_softc_net80211 *scn = NULL;
	struct wlan_objmgr_pdev *ic = NULL;
	struct net_device *dev = NULL;
	struct wlan_objmgr_vdev *vap = NULL;

	dev = dev_get_by_name(&init_net, dev_name);
	if (!dev) {
		printk("%s: device %s not Found! \n", __func__, dev_name);
		return SMART_ANT_STATUS_FAILURE;
	}

	scn = ath_netdev_priv(dev);
	if (scn == NULL)  {
		return SMART_ANT_STATUS_FAILURE;
	}

	ic = &scn->sc_ic;
	if (ic == NULL) {
		return SMART_ANT_STATUS_FAILURE;
	}
	vap = TAILQ_FIRST(&ic->ic_vaps);
	sa_api_ant_deinit(pdev, vdev, SMART_ANT_NEW_CONFIGURATION);
	dev_put(dev);
#endif
	if (qdf_atomic_read(&g_sa_init) == 0) {
		g_sa_ops = NULL;
	}
	return SMART_ANT_STATUS_SUCCESS;
}

/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc..
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#include "sa_api_defs.h"
#include <osif_private.h>
#include <osdep.h>
#include "wlan_sa_api_tgt_api.h"

void sa_api_pdev_init_da(struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_pdev *pdev,
				struct sa_api_context *sc,
				struct pdev_sa_api *pa)
{
	pa->pdev_obj = pdev;
	pa->state = SMART_ANT_STATE_DEFAULT;

	pa->enable = sc->enable;

	pa->max_fallback_rates = FALL_BACK_RATES_DIRECT_ATTACH;
	pa->radio_id = RADIO_ID_DIRECT_ATTACH;
	pa->interface_id = tgt_get_if_id(psoc, pdev);

	if (sc->sa_validate_sw == 1) {
		pa->mode = SMART_ANT_MODE_SERIAL;
	} else if (sc->sa_validate_sw) {
		pa->mode = SMART_ANT_MODE_PARALLEL;
	}
}

void sa_api_pdev_deinit_da(struct wlan_objmgr_pdev *pdev, struct sa_api_context *sc, struct pdev_sa_api *pa)
{
	pa->state = SMART_ANT_STATE_DEFAULT;
	pa->enable = 0;
}

void sa_api_enable_da(struct wlan_objmgr_psoc *psoc)
{
	if (NULL == psoc) {
		sa_api_err("PSOC is null!\n");
		return;
	}

	tgt_sa_api_register_event_handler(psoc);
}


void sa_api_disable_da(struct wlan_objmgr_psoc *psoc)
{
	if (NULL == psoc) {
		sa_api_err("PSOC is null!\n");
		return;
	}

	tgt_sa_api_unregister_event_handler(psoc);
}

void sa_api_ctx_init_da(struct sa_api_context *sc)
{
	sc->sa_api_pdev_init = sa_api_pdev_init_da;
	sc->sa_api_pdev_deinit = sa_api_pdev_deinit_da;
	sc->sa_api_enable  = sa_api_enable_da;
	sc->sa_api_disable = sa_api_disable_da;
}

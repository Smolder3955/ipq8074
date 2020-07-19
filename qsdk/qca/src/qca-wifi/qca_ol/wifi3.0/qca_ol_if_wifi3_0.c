/*
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

#include "qca_ol_if.h"
#include "cdp_txrx_cmn_reg.h"

extern void pktlog_init_3_0(struct ol_ath_softc_net80211 *scn);
extern void *diag_event_log_attach(void);
extern void diag_event_log_detach(void *dbglog_handle);
extern void
ol_update_dp_stats(void *soc, void *stats, uint16_t id, uint8_t type);

static inline void *ol_if_dp_soc_attach(u_int16_t devid,
		void *hif_handle, void *psoc, void *htc_handle,
		qdf_device_t qdf_dev, struct ol_if_ops *dp_ol_if_ops)
{
	return dp_soc_attach_wifi3(psoc, hif_handle, htc_handle,
			qdf_dev, dp_ol_if_ops, devid);
}

static inline void *ol_if_dp_soc_init(ol_txrx_soc_handle soc, u_int16_t devid,
		void *hif_handle, void *psoc, void *htc_handle,
		qdf_device_t qdf_dev, struct ol_if_ops *dp_ol_if_ops)
{
	return dp_soc_init_wifi3(soc, psoc, hif_handle, htc_handle,
			qdf_dev, dp_ol_if_ops, devid);
}

static struct ol_if_offload_ops wifi3_0_ops = {
	.cdp_soc_attach = &ol_if_dp_soc_attach,
	.cdp_soc_init = &ol_if_dp_soc_init,
	.wmi_attach = &wmi_tlv_attach,
#ifndef REMOVE_PKT_LOG
	.pktlog_init = &pktlog_init_3_0,
#endif
	.dbglog_attach = &diag_event_log_attach,
	.dbglog_detach = &diag_event_log_detach,
	.update_dp_stats = &ol_update_dp_stats,
};

void ol_if_register_wifi3_0(void)
{
	printk("=======%s==========\n",__func__);
	ol_if_offload_ops_registration(OL_WIFI_3_0, &wifi3_0_ops);
}

/*
 * Copyright (c) 2018-2019 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

#include "qca_ol_if.h"
#include "cdp_txrx_cmn_struct.h"

extern void *dbglog_attach(void);
extern void dbglog_detach(void *dbglog_handle);

ol_txrx_soc_handle ol_txrx_soc_attach(void *scn_handle, struct ol_if_ops *dp_ol_if_ops);
void ol_txrx_soc_detach(ol_txrx_soc_handle soc);
void wds_addr_init(wmi_unified_t wmi_handle);
void wds_addr_detach(wmi_unified_t wmi_handle);
#ifdef WLAN_FEATURE_BMI
extern int ol_target_init(ol_ath_soc_softc_t *soc, bool first);
extern void ol_target_failure(void *instance, QDF_STATUS status);
extern void ol_ath_dump_target(ol_ath_soc_softc_t *soc);
#endif
#ifndef REMOVE_PKT_LOG
extern void pktlog_init_2_0(struct ol_ath_softc_net80211 *scn);
extern QDF_STATUS hif_pktlog_subscribe(
		                struct ol_ath_softc_net80211 *scn);
#endif
extern void ol_get_wlan_dbg_stats(struct ol_ath_softc_net80211 *scn,
        void *dbg_stats);
extern void tpc_config_event_handler(ol_scn_t sc, u_int8_t *data, u_int32_t datalen);
extern int whal_mcs_to_kbps(int preamb, int mcs, int htflag, int gintval);
extern int whal_ratecode_to_kbps(uint8_t ratecode, uint8_t bw, uint8_t gintval);
#if ALL_POSSIBLE_RATES_SUPPORTED
extern int whal_get_supported_rates(int htflag, int shortgi, int **rates);
extern int whal_kbps_to_mcs(int kbps_rate, int shortgi, int htflag);
#else
extern int whal_kbps_to_mcs(int kbps_rate, int shortgi, int htflag, int nss, int ch_width);
extern int whal_get_supported_rates(int htflag, int shortgi, int nss, int ch_width, int **rates);
#endif


/* These APIs are legacy DP API's referrenced from common olif layer.
 * These API needs CDP api as converged interface. Currently these are
 * added here and initialized with legacy functions for wifi2.0 only.
 */
#if ENHANCED_STATS
uint32_t* ol_txrx_get_en_stats_base(struct cdp_pdev *txrx_pdev, uint32_t* stats_base,
	       uint32_t msg_len, uint32_t *type,  uint32_t *status);
#endif
struct ol_tx_desc_t* ol_tx_desc_find(void *pdev, u_int16_t tx_desc_id);
void ol_pdev_set_tid_override_queue_mapping(void *pdev, int value);
int ol_pdev_get_tid_override_queue_mapping(void *pdev);
int htt_rx_msdu_forward(void *pdev, void *msdu_desc);
int htt_rx_msdu_discard(void *pdev, void *msdu_desc);
int ol_txrx_update_peer_stats(void *handle, void *stats,
                              uint32_t last_tx_rate_mcs,
                              uint32_t stats_id);
#if defined(CONFIG_AR900B_SUPPORT) && defined(WLAN_FEATURE_BMI)
void ramdump_work_handler(void *scn);
#endif
#if UNIFIED_SMARTANTENNA
int ol_ath_smart_ant_enable_txfeedback(struct wlan_objmgr_pdev *spdev, int enable);
#endif
void ol_ath_stats_attach_wifi2(struct ieee80211com *ic);

static inline void *ol_if_txrx_soc_attach(ol_txrx_soc_handle soc, u_int16_t devid,
		void *hif_handle, void *psoc, void *htc_handle,
		qdf_device_t qdf_dev, struct ol_if_ops *dp_ol_if_ops)
{
	return ol_txrx_soc_attach(psoc, dp_ol_if_ops);
}

#ifdef OL_ATH_SMART_LOGGING
extern int32_t
ol_ath_enable_smart_log(struct ol_ath_softc_net80211 *scn, uint32_t cfg);
extern QDF_STATUS send_fatal_cmd(struct ol_ath_softc_net80211 *scn,
                                 uint32_t cfg, uint32_t subtype);
extern QDF_STATUS
ol_smart_log_connection_fail_start(struct ol_ath_softc_net80211 *scn);
extern QDF_STATUS
ol_smart_log_connection_fail_stop(struct ol_ath_softc_net80211 *scn);
#endif /* OL_ATH_SMART_LOGGING */

static inline void ol_if_txrx_soc_detach(ol_txrx_soc_handle soc)
{
	ol_txrx_soc_detach(soc);
	return;
}

static struct ol_if_offload_ops wifi2_0_ops = {
	.cdp_soc_attach = NULL,
	.cdp_soc_init = ol_if_txrx_soc_attach,
	.cdp_soc_deinit = ol_if_txrx_soc_detach,
	.wmi_attach = wmi_non_tlv_attach,
	.dbglog_attach = dbglog_attach,
	.dbglog_detach = dbglog_detach,
	.wds_addr_init = &wds_addr_init,
	.wds_addr_detach = &wds_addr_detach,
#ifdef WLAN_FEATURE_BMI
	.target_init = &ol_target_init,
	.target_failure = &ol_target_failure,
	.dump_target = &ol_ath_dump_target,
#endif
#ifndef REMOVE_PKT_LOG
	.pktlog_init = &pktlog_init_2_0,
	.hif_pktlog_subscribe = &hif_pktlog_subscribe,
#endif

#if ENHANCED_STATS
    .ol_txrx_get_en_stats_base = &ol_txrx_get_en_stats_base,
#endif
        .ol_tx_desc_find = ol_tx_desc_find,
        .ol_pdev_set_tid_override_queue_mapping = ol_pdev_set_tid_override_queue_mapping,
        .ol_pdev_get_tid_override_queue_mapping = ol_pdev_get_tid_override_queue_mapping,
        .htt_rx_msdu_forward = htt_rx_msdu_forward,
        .htt_rx_msdu_discard = htt_rx_msdu_discard,
	.ol_txrx_update_peer_stats = ol_txrx_update_peer_stats,
#if defined(CONFIG_AR900B_SUPPORT) && defined(WLAN_FEATURE_BMI)
	.ramdump_handler = ramdump_work_handler,
#endif
#ifdef OL_ATH_SMART_LOGGING
        .enable_smart_log = ol_ath_enable_smart_log,
        .send_fatal_cmd = send_fatal_cmd,
        .smart_log_connection_fail_start = ol_smart_log_connection_fail_start,
        .smart_log_connection_fail_stop = ol_smart_log_connection_fail_stop,
#ifndef REMOVE_PKT_LOG
       .smart_log_fw_pktlog_enable = ol_smart_log_fw_pktlog_enable,
       .smart_log_fw_pktlog_disable = ol_smart_log_fw_pktlog_disable,
       .smart_log_fw_pktlog_start = ol_smart_log_fw_pktlog_start,
       .smart_log_fw_pktlog_stop = ol_smart_log_fw_pktlog_stop,
       .smart_log_fw_pktlog_stop_and_block =
           ol_smart_log_fw_pktlog_stop_and_block,
       .smart_log_fw_pktlog_unblock = ol_smart_log_fw_pktlog_unblock,
#endif /* REMOVE_PKT_LOG */
#endif /* OL_ATH_SMART_LOGGING */
#if UNIFIED_SMARTANTENNA
	.smart_ant_enable_txfeedback = ol_ath_smart_ant_enable_txfeedback,
#endif
	.ol_stats_attach = ol_ath_stats_attach_wifi2,
	.get_wlan_dbg_stats = ol_get_wlan_dbg_stats,
	.tpc_config_handler = tpc_config_event_handler,
	.mcs_to_kbps = whal_mcs_to_kbps,
	.kbps_to_mcs = whal_kbps_to_mcs,
	.ratecode_to_kbps = whal_ratecode_to_kbps,
	.get_supported_rates = whal_get_supported_rates,
};

void ol_if_register_wifi2_0(void)
{
	ol_if_offload_ops_registration(OL_WIFI_2_0, &wifi2_0_ops);
}

/*
 * Copyright (c) 2018-2019 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

#ifndef _QCA_OL_IF_H_
#define _QCA_OL_IF_H_

#include "ol_if_athvar.h"
#include "cdp_txrx_cmn_struct.h"
#include "wmi_unified_priv.h"
#include "qdf_status.h"
#include <linux/platform_device.h>

/**
 *  enum OL_WIFI_DEV_TYPE  - for OL WIFI2.0 & WIFI3.0 architecture types
 *  @OL_WIFI_2_0: WIFI2.0 (Beeliner, Cascade, Dakota, etc)
 *  @OL_WIFI_3_0: WIFI3.0 (Lithium family - Hawkeye, Napier)
 *  @OL_WIFI_TYPE_MAX:  Invalid dev type
 */
typedef enum {
	OL_WIFI_2_0       = 0,
	OL_WIFI_3_0       = 1,
	OL_WIFI_TYPE_MAX  = 3,
} OL_WIFI_DEV_TYPE;

struct ol_if_offload_ops {
	struct platform_device_id *(*ath_ahb_get_pdev_id)(struct platform_device *pdev);

	int (*ath_ahb_recover)(struct ol_ath_soc_softc *soc);
	void (*ahb_defer_reconnect)(void *context);
	int (*ath_pci_recover)(struct ol_ath_soc_softc *soc);
	void (*pci_defer_reconnect)(void *context);
	void *(*cdp_soc_attach)(u_int16_t devid,
		void *hif_handle, void *psoc, void *htc_handle,
		qdf_device_t qdf_dev, struct ol_if_ops *dp_ol_if_ops);
	void *(*cdp_soc_init)(ol_txrx_soc_handle soc, u_int16_t devid,
		void *hif_handle, void *psoc, void *htc_handle,
		qdf_device_t qdf_dev, struct ol_if_ops *dp_ol_if_ops);
	void (*cdp_soc_deinit)(ol_txrx_soc_handle soc);
	void (*wmi_attach)(wmi_unified_t wmi_handle);
	void *(*dbglog_attach)(void);
	void (*dbglog_detach)(void *dbglog_handle);
	void (*wds_addr_init)(wmi_unified_t wmi_handle);
	void (*wds_addr_detach)(wmi_unified_t wmi_handle);
	int (*target_init)(ol_ath_soc_softc_t *soc, bool first);
	void (*target_failure)(void *instance, QDF_STATUS status);
	void (*dump_target)(ol_ath_soc_softc_t *soc);
	void (*pktlog_init)(struct ol_ath_softc_net80211 *scn);
	QDF_STATUS (*hif_pktlog_subscribe)(
	        struct ol_ath_softc_net80211 *scn);
       /* These APIs are legacy DP API's referrenced from common olif layer.
        * These API needs CDP api as converged interface. Currently these are
        * added here and initialized with legacy functions for wifi2.0 only.
        */
       uint32_t* (*ol_txrx_get_en_stats_base)(struct cdp_pdev *txrx_pdev, uint32_t* stats_base, uint32_t msg_len, uint32_t *type,  uint32_t *status);
       struct ol_tx_desc_t* (*ol_tx_desc_find)(void *pdev, u_int16_t tx_desc_id);
       void (*ol_pdev_set_tid_override_queue_mapping)(void *pdev, int value);
       int (*ol_pdev_get_tid_override_queue_mapping)(void *pdev);
       int (*htt_rx_msdu_forward)(void *pdev, void *msdu_desc);
       int (*htt_rx_msdu_discard)(void *pdev, void *msdu_desc);
#if defined(CONFIG_AR900B_SUPPORT) && defined(WLAN_FEATURE_BMI)
       void (*ramdump_handler)(void *scn);
#endif
       int (*ol_txrx_update_peer_stats)(void *handle, void *stats, uint32_t last_tx_rate_mcs, uint32_t stats_id);

#ifdef OL_ATH_SMART_LOGGING
       int32_t (*enable_smart_log)(struct ol_ath_softc_net80211 *scn, uint32_t cfg);
       QDF_STATUS (*send_fatal_cmd)(struct ol_ath_softc_net80211 *scn,
                                 uint32_t cfg, uint32_t subtype);
       QDF_STATUS (*smart_log_connection_fail_start)(
                                 struct ol_ath_softc_net80211 *scn);
       QDF_STATUS (*smart_log_connection_fail_stop)(
                                 struct ol_ath_softc_net80211 *scn);
#ifndef REMOVE_PKT_LOG
       QDF_STATUS (*smart_log_fw_pktlog_enable)(
                    struct ol_ath_softc_net80211 *scn);
       QDF_STATUS (*smart_log_fw_pktlog_disable)(
                    struct ol_ath_softc_net80211 *scn);
       QDF_STATUS (*smart_log_fw_pktlog_start)(
                    struct ol_ath_softc_net80211 *scn,
                    u_int32_t fw_pktlog_types);
       QDF_STATUS (*smart_log_fw_pktlog_stop)(
                    struct ol_ath_softc_net80211 *scn);
       QDF_STATUS (*smart_log_fw_pktlog_stop_and_block)(
                    struct ol_ath_softc_net80211 *scn,
                    int32_t host_pktlog_types, bool block_only_if_started);
       void (*smart_log_fw_pktlog_unblock)(struct ol_ath_softc_net80211 *scn);
#endif /* REMOVE_PKT_LOG */
#endif /* OL_ATH_SMART_LOGGING */
       void (*update_dp_stats)(void *soc, void *stats, uint16_t id, uint8_t type);
       int (*smart_ant_enable_txfeedback)(struct wlan_objmgr_pdev *spdev, int enable);

       void (*ol_stats_attach)(struct ieee80211com *ic);
       void (*get_wlan_dbg_stats)(struct ol_ath_softc_net80211 *scn,
			void *dbg_stats);
       void (*tpc_config_handler)(ol_scn_t sc, u_int8_t *data, u_int32_t datalen);
       int (*mcs_to_kbps)(int preamb, int mcs, int htflag, int gintval);
#if ALL_POSSIBLE_RATES_SUPPORTED
       int (*kbps_to_mcs)(int kbps_rate, int shortgi, int htflag);
       int (*get_supported_rates)(int htflag, int shortgi, int **rates);
#else
       int (*kbps_to_mcs)(int kbps_rate, int shortgi, int htflag, int nss, int ch_width);
       int (*get_supported_rates)(int htflag, int shortgi, int nss, int ch_width, int **rates);
#endif
       int (*ratecode_to_kbps)(uint8_t ratecode, uint8_t bw, uint8_t gintval);

};

/**
 * ol_if_offload_ops_registration() - offload ops registration
 * callback assignment
 * @dev_type: Dev type can be either offlaod WIFI2.0 & WIFI3.0
 * @offload_ops: Offload ops registration
 *
 * API to assign appropriate ops registration based on the
 * device type
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS ol_if_offload_ops_registration(OL_WIFI_DEV_TYPE dev_type,
			struct ol_if_offload_ops *offload_ops);

QDF_STATUS ol_if_offload_ops_attach(ol_ath_soc_softc_t *soc, uint32_t target_type);
#endif /* _QCA_OL_IF_H_ */

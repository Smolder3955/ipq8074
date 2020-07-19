/*
 * Copyright (c) 2016-2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/**
 * @file ol_txrx_api.h
 * @brief Definitions used in multiple external interfaces to the txrx SW.
 */

#ifndef _OL_TXRX_API_INTERNAL_H_
#define _OL_TXRX_API_INTERNAL_H_

#include "cdp_txrx_handle.h"

/******************************************************************************
 *
 * Control Interface (A Interface)
 *
 *****************************************************************************/

int
ol_txrx_pdev_attach_target(struct cdp_pdev *pdev);

struct cdp_vdev *
ol_txrx_vdev_attach(struct cdp_pdev *pdev, uint8_t *vdev_mac_addr,
			 uint8_t vdev_id, enum wlan_op_mode op_mode);

void
ol_txrx_vdev_detach(struct cdp_vdev *vdev,
			 ol_txrx_vdev_delete_cb callback, void *cb_context);

struct cdp_pdev *
ol_txrx_pdev_attach(
	ol_txrx_soc_handle soc,
	struct cdp_ctrl_objmgr_pdev *ctrl_pdev,
	HTC_HANDLE htc_pdev,
	qdf_device_t osdev,
	uint8_t pdev_id);

void
ol_txrx_pdev_detach(struct cdp_pdev *pdev, int force);

void *
ol_txrx_peer_attach(struct cdp_vdev *vdev, uint8_t *peer_mac_addr,
		struct cdp_ctrl_objmgr_peer *ctrl_peer);

int ol_txrx_peer_add_ast(ol_txrx_soc_handle soc_hdl,
        struct cdp_peer *peer_hdl, uint8_t *mac_addr,
        enum cdp_txrx_ast_entry_type type, uint32_t flags);

void ol_txrx_peer_del_ast(ol_txrx_soc_handle soc_hdl,
        void *ast_entry_hdl);

int ol_txrx_peer_update_ast(ol_txrx_soc_handle soc_hdl,
        struct cdp_peer *peer_hdl, uint8_t *wds_macaddr,
        uint32_t flags);

void ol_txrx_wds_reset_ast(ol_txrx_soc_handle soc_hdl, uint8_t *wds_macaddr,
         uint8_t *peer_macaddr, void *vdev_handle);

void ol_txrx_wds_reset_ast_table(ol_txrx_soc_handle soc_hdl,
        void *vdev_handle);

void
ol_txrx_peer_detach(void * peer, uint32_t flags);

QDF_STATUS
ol_txrx_set_monitor_mode(struct cdp_vdev *vdev, uint8_t smart_monitor);

uint8_t
ol_txrx_get_pdev_id_frm_pdev(struct cdp_pdev *pdev);

bool
ol_txrx_get_vow_config_frm_pdev(struct cdp_pdev *pdev);

void ol_txrx_set_nac(struct cdp_peer *peer);

QDF_STATUS
ol_txrx_set_pdev_tx_capture(struct cdp_pdev *pdev, int val);

void
ol_txrx_get_peer_mac_from_peer_id(
		struct cdp_pdev *pdev_handle,
		uint32_t peer_id, uint8_t *peer_mac);

void
ol_txrx_vdev_tx_lock(struct cdp_vdev *vdev);

void
ol_txrx_vdev_tx_unlock(struct cdp_vdev *vdev);

void ol_txrx_ath_getstats(void *dev, struct cdp_dev_stats *stats, uint8_t type);

void
ol_txrx_set_gid_flag(struct cdp_pdev *pdev, u_int8_t *mem_status,
		u_int8_t *user_position);

uint32_t
ol_txrx_fw_supported_enh_stats_version(struct cdp_pdev *pdev);

void
ol_txrx_if_mgmt_drain(void *ni_handle, int force);

void
ol_txrx_set_curchan(
	struct cdp_pdev *pdev,
	uint32_t chan_mhz);

void
ol_txrx_set_privacy_filters(struct cdp_vdev *vdev,
			 void *filter, uint32_t num);

/******************************************************************************
 * Data Interface (B Interface)
 *****************************************************************************/
void
ol_txrx_vdev_register(struct cdp_vdev *vdev,
			 void *osif_vdev, struct cdp_ctrl_objmgr_vdev *ctrl_vdev,
			 struct ol_txrx_ops *txrx_ops);

int
ol_txrx_mgmt_send(
	struct cdp_vdev *vdev,
	qdf_nbuf_t tx_mgmt_frm,
	uint8_t type);

int
ol_txrx_mgmt_send_ext(struct cdp_vdev *vdev,
			 qdf_nbuf_t tx_mgmt_frm,
			 uint8_t type, uint8_t use_6mbps, uint16_t chanfreq);

void
ol_txrx_mgmt_tx_cb_set(struct cdp_pdev *pdev,
			 uint8_t type,
			 ol_txrx_mgmt_tx_cb download_cb,
			 ol_txrx_mgmt_tx_cb ota_ack_cb, void *ctxt);

int ol_txrx_get_tx_pending(struct cdp_pdev *pdev);

void
ol_txrx_data_tx_cb_set(struct cdp_pdev *vdev,
		 ol_txrx_data_tx_cb callback, void *ctxt);

/******************************************************************************
 * Statistics and Debugging Interface (C Inteface)
 *****************************************************************************/

int
ol_txrx_aggr_cfg(struct cdp_vdev *vdev,
			 int max_subfrms_ampdu,
			 int max_subfrms_amsdu);

int
ol_txrx_fw_stats_get(
	 struct cdp_vdev *vdev,
	 struct ol_txrx_stats_req *req,
	 bool per_vdev,
	 bool response_expected);

int
ol_txrx_debug(struct cdp_vdev * vdev, int debug_specs);

void ol_txrx_fw_stats_cfg(
	 struct cdp_vdev *vdev,
	 uint8_t cfg_stats_type,
	 uint32_t cfg_val);

void ol_txrx_print_level_set(unsigned level);

/**
 * ol_txrx_get_vdev_mac_addr() - Return mac addr of vdev
 * @vdev: vdev handle
 *
 * Return: vdev mac address
 */
uint8_t *
ol_txrx_get_vdev_mac_addr(struct cdp_vdev *vdev);

/**
 * ol_txrx_get_vdev_struct_mac_addr() - Return handle to struct qdf_mac_addr of
 * vdev
 * @vdev: vdev handle
 *
 * Return: Handle to struct qdf_mac_addr
 */
struct qdf_mac_addr *
ol_txrx_get_vdev_struct_mac_addr(struct cdp_vdev *vdev);

/**
 * ol_txrx_get_pdev_from_vdev() - Return handle to pdev of vdev
 * @vdev: vdev handle
 *
 * Return: Handle to pdev
 */
void * ol_txrx_get_pdev_from_vdev(struct cdp_vdev *vdev);

/**
 * ol_txrx_get_ctrl_pdev_from_vdev() - Return control pdev of vdev
 * @vdev: vdev handle
 *
 * Return: Handle to control pdev
 */
void *
ol_txrx_get_ctrl_pdev_from_vdev(struct cdp_vdev *vdev);

void *
ol_txrx_get_vdev_from_vdev_id(uint8_t vdev_id);

int
ol_txrx_mempools_attach(void * ctrl_pdev);
int
ol_txrx_set_filter_neighbour_peers(
	struct cdp_pdev *pdev,
	u_int32_t val);
/**
 * @brief set the safemode of the device
 * @details
 *  This flag is used to bypass the encrypt and decrypt processes when send and
 *  receive packets. It works like open AUTH mode, HW will treate all packets
 *  as non-encrypt frames because no key installed. For rx fragmented frames,
 *  it bypasses all the rx defragmentaion.
 *
 * @param vdev - the data virtual device object
 * @param val - the safemode state
 * @return - void
 */

void
ol_txrx_set_safemode(
	struct cdp_vdev *vdev,
	u_int32_t val);
/**
 * @brief configure the drop unencrypted frame flag
 * @details
 *  Rx related. When set this flag, all the unencrypted frames
 *  received over a secure connection will be discarded
 *
 * @param vdev - the data virtual device object
 * @param val - flag
 * @return - void
 */
void
ol_txrx_set_drop_unenc(
	struct cdp_vdev *vdev,
	u_int32_t val);


/**
 * @brief set the Tx encapsulation type of the VDEV
 * @details
 *  This will be used to populate the HTT desc packet type field during Tx
 *
 * @param vdev - the data virtual device object
 * @param val - the Tx encap type
 * @return - void
 */
void
ol_txrx_set_tx_encap_type(
	struct cdp_vdev *vdev,
	u_int32_t val);

/**
 * @brief set the Rx decapsulation type of the VDEV
 * @details
 *  This will be used to configure into firmware and hardware which format to
 *  decap all Rx packets into, for all peers under the VDEV.
 *
 * @param vdev - the data virtual device object
 * @param val - the Rx decap mode
 * @return - void
 */
void
ol_txrx_set_vdev_rx_decap_type(
	struct cdp_vdev *vdev,
	u_int32_t val);

/**
 * @brief get the Rx decapsulation type of the VDEV
 *
 * @param vdev - the data virtual device object
 * @return - the Rx decap type
 */
enum htt_cmn_pkt_type
ol_txrx_get_vdev_rx_decap_type(struct cdp_vdev *vdev);

/* Is this similar to ol_txrx_peer_state_update() in MCL */
/**
 * @brief Update the authorize peer object at association time
 * @details
 *  For the host-based implementation of rate-control, it
 *  updates the peer/node-related parameters within rate-control
 *  context of the peer at association.
 *
 * @param peer - pointer to the node's object
 * @authorize - either to authorize or unauthorize peer
 *
 * @return none
 */
void
ol_txrx_peer_authorize(struct cdp_peer *peer, u_int32_t authorize);

/* Should be ol_txrx_ctrl_api.h */
void ol_txrx_set_mesh_mode(struct cdp_vdev *vdev, u_int32_t val);

void ol_tx_flush_buffers(struct cdp_vdev *vdev);

A_STATUS
wdi_event_sub(struct cdp_pdev *txrx_pdev_handle,
    void *event_cb_sub_handle,
    uint32_t event);

A_STATUS
wdi_event_unsub(struct cdp_pdev *txrx_pdev_handle,
    void *event_cb_sub_handle,
    uint32_t event);

void
ol_tx_me_alloc_descriptor(struct cdp_pdev *pdev);

void
ol_tx_me_free_descriptor(struct cdp_pdev *pdev);

uint16_t
ol_tx_me_convert_ucast(struct cdp_vdev *vdev, qdf_nbuf_t wbuf,
		u_int8_t newmac[][6], uint8_t newmaccnt);

/* Should be a function pointer in ol_txrx_osif_ops{} */
#if ATH_MCAST_HOST_INSPECT
/**
 * @brief notify mcast frame indication from FW.
 * @details
 *      This notification will be used to convert
 *      multicast frame to unicast.
 *
 * @param pdev - handle to the ctrl SW's physical device object
 * @param vdev_id - ID of the virtual device received the special data
 * @param msdu - the multicast msdu returned by FW for host inspect
 */

int ol_mcast_notify(struct cdp_pdev *pdev,
	u_int8_t vdev_id, qdf_nbuf_t msdu);
#endif


/* Need to remove the "req" parameter */
/* Need to rename the function to reflect the functionality "show" / "display"
 * WIN -- to figure out whether to change OSIF to converge (not an immediate AI)
 * */
#ifdef WLAN_FEATURE_FASTPATH
int ol_txrx_host_stats_get(
	struct cdp_vdev *vdev,
	struct ol_txrx_stats_req *req);

void
ol_txrx_host_ce_stats(struct cdp_vdev *vdev);

int
ol_txrx_stats_publish(struct cdp_pdev *pdev, void *buf);
int
ol_txrx_get_vdev_stats(struct cdp_vdev *vdev, void *buf, bool is_aggr);
/**
 * @brief Enable enhanced stats functionality.
 *
 * @param pdev - the physical device object
 * @return - void
 */
void
ol_txrx_enable_enhanced_stats(struct cdp_pdev *pdev);

/**
 * @brief Disable enhanced stats functionality.
 *
 * @param pdev - the physical device object
 * @return - void
 */
void
ol_txrx_disable_enhanced_stats(struct cdp_pdev *pdev);

#if ENHANCED_STATS
/**
 * @brief Get the desired stats from the message.
 *
 * @param pdev - the physical device object
 * @param stats_base - stats buffer recieved from FW
 * @param type - stats type.
 * @return - pointer to requested stat identified by type
 */
uint32_t *ol_txrx_get_stats_base(struct cdp_pdev *pdev,
	uint32_t *stats_base, uint32_t msg_len, uint8_t type);
#endif
#endif /* WLAN_FEATURE_FASTPATH*/
#if (HOST_SW_TSO_ENABLE || HOST_SW_TSO_SG_ENABLE)
void
ol_tx_print_tso_stats(
	struct cdp_vdev *vdev);

void
ol_tx_rst_tso_stats(struct cdp_vdev *vdev);
#endif /* HOST_SW_TSO_ENABLE || HOST_SW_TSO_SG_ENABLE */

#if HOST_SW_SG_ENABLE
void
ol_tx_print_sg_stats(
	struct cdp_vdev *vdev);

void
ol_tx_rst_sg_stats(struct cdp_vdev *vdev);
#endif /* HOST_SW_SG_ENABLE */

#if RX_CHECKSUM_OFFLOAD
void
ol_print_rx_cksum_stats(
	struct cdp_vdev *vdev);

void
ol_rst_rx_cksum_stats(struct cdp_vdev *vdev);
#endif /* RX_CHECKSUM_OFFLOAD */

#if ATH_SUPPORT_IQUE && defined(WLAN_FEATURE_FASTPATH)
A_STATUS
ol_txrx_host_me_stats(struct cdp_vdev *vdev);
#endif /* WLAN_FEATURE_FASTPATH */
#if PEER_FLOW_CONTROL
void
ol_txrx_per_peer_stats(struct cdp_pdev *pdev, char *addr);
#endif
#if defined(WLAN_FEATURE_FASTPATH) && PEER_FLOW_CONTROL
int ol_txrx_host_msdu_ttl_stats(
	struct cdp_vdev *vdev,
	struct ol_txrx_stats_req *req);
#endif

#if HOST_SW_LRO_ENABLE
void
ol_print_lro_stats(struct cdp_vdev *vdev);

void
ol_reset_lro_stats(struct cdp_vdev *vdev);
#endif /* HOST_SW_LRO_ENABLE */

void ol_txrx_monitor_set_filter_ucast_data(struct cdp_pdev *pdev_handle, u_int8_t val);
void ol_txrx_monitor_set_filter_mcast_data(struct cdp_pdev *pdev_handle, u_int8_t val);
void ol_txrx_monitor_set_filter_non_data(struct cdp_pdev *pdev_handle, u_int8_t val);

bool ol_txrx_monitor_get_filter_ucast_data(
				struct cdp_vdev *vdev_txrx_handle);
bool ol_txrx_monitor_get_filter_mcast_data(
				struct cdp_vdev *vdev_txrx_handle);
bool ol_txrx_monitor_get_filter_non_data(
				struct cdp_vdev *vdev_txrx_handle);


QDF_STATUS ol_txrx_reset_monitor_mode(struct cdp_pdev *pdev);

#if PEER_FLOW_CONTROL
uint32_t ol_pflow_update_pdev_params(void *,
		enum _ol_ath_param_t, uint32_t, void *);
#endif

extern int ol_txrx_get_nwifi_mode(struct cdp_vdev *vdev);
#define OL_TXRX_GET_NWIFI_MODE(vdev)  ol_txrx_get_nwifi_mode(vdev)

#if WDS_VENDOR_EXTENSION
void
ol_txrx_set_wds_rx_policy(
	struct cdp_vdev * vdev,
	u_int32_t val);
#endif

/* ol_txrx_peer_get_ast_info_by_pdevid:
 * Retrieve ast_info with the help of the pdev_id and ast_mac_addr
 *
 * @soc_hdl: SoC Handle
 * @ast_mac_addr: MAC Address of AST Peer
 * @pdev_id: PDEV ID
 * @ast_entry_info: AST Entry Info Structure
 *
 * Returns 0 for failure
 *         1 for success
 */
bool ol_txrx_peer_get_ast_info_by_pdevid(struct cdp_soc_t *soc_hdl,
                                         uint8_t *ast_mac_addr,
                                         uint8_t pdev_id,
                                         struct cdp_ast_entry_info *ast_entry_info);

#endif /* _OL_TXRX_API_INTERNAL_H_ */

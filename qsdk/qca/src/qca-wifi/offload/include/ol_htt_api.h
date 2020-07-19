/*
 * Copyright (c) 2011, 2017-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/**
 * @file ol_htt_api.h
 * @brief Specify the general HTT API functions called by the host data SW.
 * @details
 *  This file declares the HTT API functions that are not specific to
 *  either tx nor rx.
 */
#ifndef _OL_HTT_API__H_
#define _OL_HTT_API__H_

#include <qdf_types.h> /* qdf_device_t */
#include <athdefs.h>      /* A_STATUS */
#include <htc_api.h>      /* HTC_HANDLE */
#include <htt.h>          /* htt_dbg_stats_type, etc. */
#include <cdp_txrx_cmn_struct.h>
#include <ar_ops.h>
#include <ol_if_txrx_handles.h>

typedef struct htt_pdev_t *htt_pdev_handle;
struct hif_opaque_softc;
struct ol_txrx_pdev_t;
struct ol_txrx_vdev_t;
struct ol_txrx_peer_t;

/**
 * @brief Allocate and initialize a HTT instance.
 * @details
 *  This function allocates and initializes an HTT instance.
 *  This involves allocating a pool of HTT tx descriptors in
 *  consistent memory, allocating and filling a rx ring (LL only),
 *  and connecting the HTC's HTT_DATA_MSG service.
 *  The HTC service connect call will block, so this function
 *  needs to be called in passive context.
 *  Because HTC setup has not been completed at the time this function
 *  is called, this function cannot send any HTC messages to the target.
 *  Messages to configure the target are instead sent in the
 *  htc_attach_target function.
 *
 * @param soc - Soc's physical device handle
 * @param ctrl_pdev - control SW's physical device handle
 *      (used to query configuration functions)
 * @param osdev - abstract OS device handle
 *      (used for mem allocation)
 * @param desc_pool_size - number of HTT descriptors to (pre)allocate
 * @return success -> HTT pdev handle; failure -> NULL
 */
htt_pdev_handle
htt_attach(
    ol_txrx_soc_handle soc,
    ol_soc_handle ctrl_psoc,
    struct hif_opaque_softc *osc,
    HTC_HANDLE htc_pdev,
    ar_handle_t arh,
    qdf_device_t osdev,
    int desc_pool_size);

int htt_attach_tx_rx(
    ol_txrx_soc_handle soc,
    htt_pdev_handle pdev,
    HTC_HANDLE htc_pdev,
    int desc_pool_size);

/**
 * @brief Send HTT configuration messages to the target.
 * @details
 *  For LL only, this function sends a rx ring configuration message to the
 *  target.  For HL, this function is a no-op.
 *
 * @param htt_pdev - handle to the HTT instance being initialized
 */
A_STATUS
htt_attach_target(htt_pdev_handle htt_pdev);

/**
 * @brief Deallocate a HTT instance.
 *
 * @param htt_pdev - handle to the HTT instance being torn down
 */
void
htt_detach(htt_pdev_handle htt_pdev);

/*
 * @brief Tell the target side of HTT to suspend H2T processing until synced
 * @param htt_pdev - the host HTT object
 * @param sync_cnt - what sync count value the target HTT FW should wait for
 *      before resuming H2T processing
 */
A_STATUS
htt_h2t_sync_msg(htt_pdev_handle htt_pdev, u_int8_t sync_cnt);


int
htt_h2t_aggr_cfg_msg(htt_pdev_handle htt_pdev,
                     int max_subfrms_ampdu,
                     int max_subfrms_amsdu);

/* This function returns number of pending frames in
 * HTT Management descriptors pool
*/
A_UINT32
htt_tx_mgmt_pending_frames_cnt(struct htt_pdev_t *pdev);

/* This function sends down the
 * HTT Management frame descripto to
 * Target
*/
int
htt_h2t_mgmt_tx(struct ol_txrx_pdev_t *pdev,
                A_UINT32          paddr,
                qdf_nbuf_t        mgmt_frm,
                A_UINT32          frag_len,
                A_UINT16          vdev_id,
                A_UINT8           *hdr,
                ol_txrx_vdev_handle vdev);
/**
 * @brief Get the FW status
 * @details
 *  Trigger FW HTT to retrieve FW status.
 *  A separate HTT message will come back with the statistics we want.
 *
 * @param pdev - handle to the HTT instance
 * @param stats_type_upload_mask - bitmask identifying which stats to upload
 * @param stats_type_reset_mask - bitmask identifying which stats to reset
 * @param cookie - unique value to distinguish and identify stats requests
 * @return 0 - succeed to send the request to FW; otherwise, failed to do so.
 */
int
htt_h2t_dbg_stats_get(
    struct htt_pdev_t *pdev,
    u_int32_t stats_type_upload_mask,
    u_int32_t stats_type_reset_mask,
    u_int8_t cfg_stats_type,
    u_int32_t cfg_val,
    u_int64_t cookie,
    u_int32_t vdev_id);

/**
 * @brief Get the fields from HTT T2H stats upload message's stats info header
 * @details
 *  Parse the a HTT T2H message's stats info tag-length-value header,
 *  to obtain the stats type, status, data lenght, and data address.
 *
 * @param stats_info_list - address of stats record's header
 * @param[out] type - which type of FW stats are contained in the record
 * @param[out] status - whether the stats are (fully) present in the record
 * @param[out] length - how large the data portion of the stats record is
 * @param[out] stats_data - where the data portion of the stats record is
 */
void
htt_t2h_dbg_stats_hdr_parse(
    u_int8_t *stats_info_list,
    enum htt_dbg_stats_type *type,
    enum htt_dbg_stats_status *status,
    int *length,
    u_int8_t **stats_data);

/**
 * @brief Display a stats record from the HTT T2H STATS_CONF message.
 * @details
 *  Parse the stats type and status, and invoke a type-specified printout
 *  to display the stats values.
 *
 *  @param stats_data - buffer holding the stats record from the STATS_CONF msg
 *  @param concise - whether to do a verbose or concise printout
 *  @param vdevid - specifies if the received stats are vdev specific or radio specific
 */
void
htt_t2h_stats_print(u_int8_t *stats_data, int concise, u_int32_t target_type, u_int32_t vdevid);

/**
 * @brief Retrieve the packet download length from current configuration.
 * @details
 *  The packet download length is dependent on:
 *  a) Frame type (that this VAP is configured for)
 *  b) OL configuration that determines, the length required for
 *     packet classification
 *
 * @param pdev - HTT pdev handle
 * @return packet download length
 */
u_int32_t
htt_pkt_dl_len_get(htt_pdev_handle pdev);

/**
 * @brief Get the fields from HTT T2H enhanced stats upload message's stats
 * info header
 * @details
 *  Parse the a HTT T2H message's stats info tag-length-value header,
 *  to obtain the stats type, status, data length, and data address.
 *
 * @param msg_word - address of enh stats record's header
 * @param[out] type - which version of FW stats are contained in the record
 * @param[out] status - whether the stats are (fully) present in the record
 * @param[out] length - how large the data portion of the stats record is
 * @return - data address
 */
void
htt_t2h_dbg_enh_stats_hdr_parse(
    u_int32_t *msg_word,
    uint32_t *type,
    uint32_t *status);

#ifndef HTT_DEBUG_LEVEL
#define HTT_DEBUG_LEVEL 10
#endif

#if HTT_DEBUG_LEVEL > 5
void htt_display(htt_pdev_handle pdev, int indent);
#else
#define htt_display(pdev, indent)
#endif

#define HTT_TX_DESC_CLEAR_TSO_INFO(_var)       \
    qdf_mem_set(_var, TX_DESC_TSO_INFO_SIZE, 0);

#endif /* _OL_HTT_API__H_ */

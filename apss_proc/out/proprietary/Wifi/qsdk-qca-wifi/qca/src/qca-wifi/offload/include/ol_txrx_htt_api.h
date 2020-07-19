/*
 * Copyright (c) 2011, 2015-2016,2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011,2015 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/**
 * @file ol_txrx_htt_api.h
 * @brief Define the host data API functions called by the host HTT SW.
 */
#ifndef _OL_TXRX_HTT_API__H_
#define _OL_TXRX_HTT_API__H_

#include <htt.h>         /* HTT_TX_COMPL_IND_STAT */
#include <athdefs.h>     /* A_STATUS */
#include <qdf_nbuf.h>    /* qdf_nbuf_t */

#include <cdp_txrx_cmn_struct.h> /* ol_txrx_pdev_handle */
#include <ol_if_txrx_handles.h>

struct ol_txrx_pdev_t;

A_STATUS
ol_tx_pkt_log_event_handler(void *txrx_pdev,
                void *data);

/**
 * @brief Tx MSDU download completion for a LL system
 * @details
 *  Release the reference to the downloaded tx descriptor.
 *  In the unlikely event that the reference count is zero, free
 *  the tx descriptor and tx frame.
 *
 * @param pdev - (abstract) pointer to the txrx physical device
 * @param status - indication of whether the download succeeded
 * @param msdu - the downloaded tx frame
 * @param msdu_id - the txrx ID of the tx frame - this is used for
 *      locating the frame's tx descriptor
 */
void
ol_tx_download_done_ll(
    void *pdev,
    A_STATUS status,
    qdf_nbuf_t msdu,
    u_int16_t msdu_id);

/**
 * @brief Tx MSDU download completion for HL system without tx completion msgs
 * @details
 *  Free the tx descriptor and tx frame.
 *  Invoke the HL tx download scheduler.
 *
 * @param pdev - (abstract) pointer to the txrx physical device
 * @param status - indication of whether the download succeeded
 * @param msdu - the downloaded tx frame
 * @param msdu_id - the txrx ID of the tx frame - this is used for
 *      locating the frame's tx descriptor
 */
void
ol_tx_download_done_hl_free(
    void *pdev,
    A_STATUS status,
    qdf_nbuf_t msdu,
    u_int16_t msdu_id);

/**
 * @brief Tx MSDU download completion for HL system with tx completion msgs
 * @details
 *  Release the reference to the downloaded tx descriptor.
 *  In the unlikely event that the reference count is zero, free
 *  the tx descriptor and tx frame.
 *  Optionally, invoke the HL tx download scheduler.  (It is probable that
 *  the HL tx download scheduler would operate in response to tx completion
 *  messages rather than download completion events.)
 *
 * @param pdev - (abstract) pointer to the txrx physical device
 * @param status - indication of whether the download succeeded
 * @param msdu - the downloaded tx frame
 * @param msdu_id - the txrx ID of the tx frame - this is used for
 *      locating the frame's tx descriptor
 */
void
ol_tx_download_done_hl_retain(
    void *pdev,
    A_STATUS status,
    qdf_nbuf_t msdu,
    u_int16_t msdu_id);

/*
 * For now, make the host HTT -> host txrx tx completion status
 * match the target HTT -> host HTT tx completion status, so no
 * translation is needed.
 */
/*
 * host-only statuses use a different part of the number space
 * than host-target statuses
 */
#define HTT_HOST_ONLY_STATUS_CODE_START 128
enum htt_tx_status {
    /* ok - successfully sent + acked */
    htt_tx_status_ok = HTT_TX_COMPL_IND_STAT_OK,

    /* discard - not sent (congestion control) */
    htt_tx_status_discard = HTT_TX_COMPL_IND_STAT_DISCARD,

    /* no_ack - sent, but no ack */
    htt_tx_status_no_ack = HTT_TX_COMPL_IND_STAT_NO_ACK,

    /* download_fail - the host could not deliver the tx frame to the target */
    htt_tx_status_download_fail = HTT_HOST_ONLY_STATUS_CODE_START,
};

/**
 * @brief Process a tx completion message sent by the target.
 * @details
 *  When the target is done transmitting a tx frame (either because
 *  the frame was sent + acknowledged, or because the target gave up)
 *  it sends a tx completion message to the host.
 *  This notification function is used regardless of whether the
 *  transmission succeeded or not; the status argument indicates whether
 *  the transmission succeeded.
 *  This tx completion message indicates via the descriptor ID which
 *  tx frames were completed, and indicates via the status whether the
 *  frames were transmitted successfully.
 *  The host frees the completed descriptors / frames (updating stats
 *  in the process).
 *
 * @param pdev - the data physical device that sent the tx frames
 *      (registered with HTT as a context pointer during attach time)
 * @param num_msdus - how many MSDUs are referenced by the tx completion
 *      message
 * @param status - whether transmission was successful
 * @param tx_msdu_id_iterator - abstract method of finding the IDs for the
 *      individual MSDUs referenced by the tx completion message, via the
 *      htt_tx_compl_desc_id API function
 */
void
ol_tx_completion_handler(
    ol_txrx_pdev_handle pdev,
    int num_msdus,
    enum htt_tx_status status,
    int ack_rssi_filled,
    int ppdu_peer_id_filled,
    int ast_index_filled,
    void *tx_msdu_id_iterator,
    int *num_htt_cmpls,
    uint16_t tid);

/**
 * @brief Process an rx indication message sent by the target.
 * @details
 *  The target sends a rx indication message to the host as a
 *  notification that there are new rx frames available for the
 *  host to process.
 *  The HTT host layer locates the rx descriptors and rx frames
 *  associated with the indication, and calls this function to
 *  invoke the rx data processing on the new frames.
 *  (For LL, the rx descriptors and frames are delivered directly
 *  to the host via MAC DMA, while for HL the rx descriptor and
 *  frame for individual frames are combined with the rx indication
 *  message.)
 *  All MPDUs referenced by a rx indication message belong to the
 *  same peer-TID.
 *
 * @param pdev - the data physical device that received the frames
 *      (registered with HTT as a context pointer during attach time)
 * @param rx_ind_msg - the network buffer holding the rx indication message
 *      (For HL, this netbuf also holds the rx desc and rx payload, but
 *      the data SW is agnostic to whether the desc and payload are
 *      piggybacked with the rx indication message.)
 * @param peer_id - which peer sent this rx data
 * @param tid - what (extended) traffic type the rx data is
 * @param num_mpdu_ranges - how many ranges of MPDUs does the message describe.
 *      Each MPDU within the range has the same rx status.
 */
void
ol_rx_indication_handler(
    ol_txrx_pdev_handle pdev,
    qdf_nbuf_t rx_ind_msg,
    u_int16_t peer_id,
    u_int8_t tid,
    int num_mpdu_ranges);

/**
 * @brief Process an rx fragment indication message sent by the target.
 * @details
 *  The target sends a rx fragment indication message to the host as a
 *  notification that there are new rx fragment available for the
 *  host to process.
 *  The HTT host layer locates the rx descriptors and rx fragment
 *  associated with the indication, and calls this function to
 *  invoke the rx fragment data processing on the new fragment.
 *
 * @param pdev - the data physical device that received the frames
 *      (registered with HTT as a context pointer during attach time)
 * @param rx_frag_ind_msg - the network buffer holding the rx fragment indication message
 * @param peer_id - which peer sent this rx data
 * @param tid - what (extended) traffic type the rx data is
 */
void ol_rx_frag_indication_handler(
    ol_txrx_pdev_handle pdev,
    qdf_nbuf_t rx_frag_ind_msg,
    u_int16_t peer_id,
    u_int8_t tid);

/**
 * @brief Process a peer map message sent by the target.
 * @details
 *  Each time the target allocates a new peer ID, it will inform the
 *  host via the "peer map" message.  This function processes that
 *  message.  The host data SW looks for a peer object whose MAC address
 *  matches the MAC address specified in the peer map message, and then
 *  sets up a mapping between the peer ID specified in the message and
 *  the peer object that was found.
 *
 * @param pdev - data physical device handle
 *      (registered with HTT as a context pointer during attach time)
 * @param peer_id - ID generated by the target to refer to the peer in question
 *      The target may create multiple IDs for a single peer.
 * @param vdev_id - Reference to the virtual device the peer is associated with
 * @param peer_mac_addr - MAC address of the peer in question
 */
void
ol_rx_peer_map_handler(
    struct ol_txrx_pdev_t *pdev,
    u_int16_t peer_id,
    u_int8_t vdev_id,
    u_int8_t *peer_mac_addr);

/**
 * @brief Process a peer unmap message sent by the target.
 * @details
 *  Each time the target frees a peer ID, it will inform the host via the
 *  "peer unmap" message.  This function processes that message.
 *  The host data SW uses the peer ID from the message to find the peer
 *  object from peer_map[peer_id], then invalidates peer_map[peer_id]
 *  (by setting it to NULL), and checks whether there are any remaining
 *  references to the peer object.  If not, the function deletes the
 *  peer object.
 *
 * @param pdev - data physical device handle
 *      (registered with HTT as a context pointer during attach time)
 * @param peer_id - ID that is being freed.
 *      The target may create multiple IDs for a single peer.
 */
void
ol_rx_peer_unmap_handler(
    struct ol_txrx_pdev_t *pdev,
    u_int16_t peer_id);

/**
 * @brief Process a security indication message sent by the target.
 * @details
 *  When a key is assigned to a peer, the target will inform the host
 *  with a security indication message.
 *  The host remembers the security type, and infers whether a rx PN
 *  check is needed.
 *
 * @param pdev - data physical device handle
 * @param peer_id - which peer the security info is for
 * @param sec_type - which type of security / key the peer is using
 * @param is_unicast - whether security spec is for a unicast or multicast key
 * @param michael_key - key used for TKIP MIC (if sec_type == TKIP)
 * @param rx_pn - RSC used for WAPI PN replay check (if sec_type == WAPI)
 */
void
ol_rx_sec_ind_handler(
    ol_txrx_pdev_handle pdev,
    u_int16_t peer_id,
    enum htt_sec_type sec_type,
    int is_unicast,
    u_int32_t *michael_key,
    u_int32_t *rx_pn);

/**
 * @brief Process an ADDBA message sent by the target.
 * @details
 *  When the target notifies the host of an ADDBA event for a specified
 *  peer-TID, the host will set up the rx reordering state for the peer-TID.
 *  Specifically, the host will create a rx reordering array whose length
 *  is based on the window size specified in the ADDBA.
 *
 * @param pdev - data physical device handle
 *      (registered with HTT as a context pointer during attach time)
 * @param peer_id - which peer the ADDBA event is for
 * @param tid - which traffic ID within the peer the ADDBA event is for
 * @param win_sz - how many sequence numbers are in the ARQ block ack window
 *      set up by the ADDBA event
 */
void
ol_rx_addba_handler(
    ol_txrx_pdev_handle pdev,
    u_int16_t peer_id,
    u_int8_t tid,
    u_int8_t win_sz);

/**
 * @brief Process a DELBA message sent by the target.
 * @details
 *  When the target notifies the host of a DELBA event for a specified
 *  peer-TID, the host will clean up the rx reordering state for the peer-TID.
 *  Specifically, the host will remove the rx reordering array, and will
 *  set the reorder window size to be 1 (stop and go ARQ).
 *
 * @param pdev - data physical device handle
 *      (registered with HTT as a context pointer during attach time)
 * @param peer_id - which peer the ADDBA event is for
 * @param tid - which traffic ID within the peer the ADDBA event is for
 */
void
ol_rx_delba_handler(
    ol_txrx_pdev_handle pdev,
    u_int16_t peer_id,
    u_int8_t tid);

enum htt_rx_flush_action {
    htt_rx_flush_release,
    htt_rx_flush_discard,
};

/**
 * @brief Process a rx reorder flush message sent by the target.
 * @details
 *  The target's rx reorder logic can send a flush indication to the
 *  host's rx reorder buffering either as a flush IE within a rx
 *  indication message, or as a standalone rx reorder flush message.
 *  This ol_rx_flush_handler function processes the standalone rx
 *  reorder flush message from the target.
 *  The flush message specifies a range of sequence numbers whose
 *  rx frames are flushed.
 *  Some sequence numbers within the specified range may not have
 *  rx frames; the host needs to check for each sequence number in
 *  the specified range whether there are rx frames held for that
 *  sequence number.
 *
 * @param pdev - data physical device handle
 *      (registered with HTT as a context pointer during attach time)
 * @param peer_id - which peer's rx data is being flushed
 * @param tid - which traffic ID within the peer has the rx data being flushed
 * @param seq_num_start - Which sequence number within the rx reordering
 *      buffer the flushing should start with.
 *      This is the LSBs of the 802.11 sequence number.
 *      This sequence number is masked with the rounded-to-power-of-two
 *      window size to generate a reorder buffer index.
 *      The flush includes this initial sequence number.
 * @param seq_num_end - Which sequence number within the rx reordering
 *      buffer the flushing should stop at.
 *      This is the LSBs of the 802.11 sequence number.
 *      This sequence number is masked with the rounded-to-power-of-two
 *      window size to generate a reorder buffer index.
 *      The flush excludes this final sequence number.
 * @param action - whether to release or discard the rx frames
 */
void
ol_rx_flush_handler(
    ol_txrx_pdev_handle pdev,
    u_int16_t peer_id,
    u_int8_t tid,
    int seq_num_start,
    int seq_num_end,
    enum htt_rx_flush_action action);

/**
 * @brief Process a stats message sent by the target.
 * @details
 *  The host can request target for stats.
 *  The target sends the stats to the host via a confirmation message.
 *  This ol_txrx_fw_stats_handler function processes the confirmation message.
 *  Currently, this processing consists of copying the stats from the message
 *  buffer into the txrx pdev object, and waking the sleeping host context
 *  that requested the stats.
 *
 * @param pdev - data physical device handle
 *      (registered with HTT as a context pointer during attach time)
 * @param cookie - Value echoed from the cookie in the stats request
 *      message.  This allows the host SW to find the stats request object.
 *      (Currently, this cookie is unused.)
 * @param stats_info_list - stats confirmation message contents, containing
 *      a list of the stats requested from the target
 */
int
ol_txrx_fw_stats_handler(
    struct ol_txrx_pdev_t *pdev,
    u_int64_t cookie,
    u_int8_t *stats_info_list,
    u_int32_t vdev_id);

/**
 * @brief Process a tx inspect message sent by the target.
 * @details:
 *  TODO: update
 *  This tx inspect message indicates via the descriptor ID
 *  which tx frames are to be inspected by host. The host
 *  re-injects the packet back to the host for a number of
 *  cases.
 *
 * @param pdev - the data physical device that sent the tx frames
 *      (registered with HTT as a context pointer during attach time)
 * @param num_msdus - how many MSDUs are referenced by the tx completion
 *      message
 * @param tx_msdu_id_iterator - abstract method of finding the IDs for the
 *      individual MSDUs referenced by the tx completion message, via the
 *      htt_tx_compl_desc_id API function
 */
void
ol_tx_inspect_handler(
    ol_txrx_pdev_handle pdev,
    int num_msdus,
    void *tx_desc_id_iterator);

#if UNIFIED_SMARTANTENNA
/**
 * @brief Process different stats messages sent by the target.
 *
 * @param pdev - data physical device handle
 *      (registered with HTT as a context pointer during attach time)
 * @param msg_word - stats base pointer
 * @param msg_len - stats lenght.
 *
 */

int ol_ath_smart_ant_stats_handler(struct ol_txrx_pdev_t *txrx_pdev, uint32_t* msg_word, uint32_t msg_len);
#endif

#if WLAN_CFR_ENABLE
/*
 * @brief Process cfr htt indication message
 *
 * @param pdev_ptr - pdev obj ptr
 * @param msg_word - HTT message pointer
 * @param msg_len - HTT message len
 *
 */
QDF_STATUS ol_txrx_htt_cfr_rx_ind_handler(void *pdev_ptr, uint32_t *msg_word, size_t msg_len);
#endif

#if ENHANCED_STATS
/**
 * @brief Process different stats messages sent by the target.
 *
 * @param pdev - data physical device handle
 *      (registered with HTT as a context pointer during attach time)
 * @param msg_word - stats base pointer
 * @param msg_len - stats lenght.
 *
 */

int ol_ath_enh_stats_handler(struct ol_txrx_pdev_t *txrx_pdev,
                                uint32_t* msg_word, uint32_t msg_len);
int ol_tx_update_peer_stats(struct ol_txrx_pdev_t *txrx_pdev,
                                uint32_t* msg_word, uint32_t msg_len);
#endif
/**
 * @brief Process chan change message sent by the target.
 *
 * @param pdev - data physical device handle
 *      (registered with HTT as a context pointer during attach time)
 * @param msg_word - stats base pointer
 * @param msg_len - stats lenght.
 *
 */
void ol_ath_chan_change_msg_handler(struct ol_txrx_pdev_t *txrx_pdev,
                                  uint32_t* msg_word, uint32_t msg_len);
/**
 * @brief Process  the tx pkts that are in flow ctrl queue
 *        in the tx completion path.
 * @param pdev - data physical device handle
 *       (registered with HTT as a context pointer during attach time)
 */
#if PEER_FLOW_CONTROL && !PEER_FLOW_CONTROL_HOST_SCHED
u_int32_t
ol_tx_ll_fetch_sched(struct ol_txrx_pdev_t *pdev, uint32_t num_records, uint32_t *fetch_ind_record, qdf_nbuf_t fetch_resp_buf);
#else
void
ol_tx_ll_sched(struct ol_txrx_pdev_t *txrx_pdev);
#endif

#endif /* _OL_TXRX_HTT_API__H_ */

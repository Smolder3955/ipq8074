/*
 * Copyright (c) 2011-2015, 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 *
 * 2011-2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/**
 * @file ol_htt_rx_api.h
 * @brief Specify the rx HTT API functions called by the host data SW.
 * @details
 *  This file declares the HTT API functions that are specifically
 *  related to receive processing.
 *  In particular, this file specifies methods of the abstract HTT rx
 *  descriptor, and functions to iterate though a series of rx descriptors
 *  and rx MSDU buffers.
 */
#ifndef _OL_HTT_RX_API__H_
#define _OL_HTT_RX_API__H_

//#include <a_osapi.h>     /* u_int16_t, etc. */
#include <osdep.h>       /* u_int16_t, etc. */
#include <qdf_nbuf.h>    /* qdf_nbuf_t */

#include <htt.h>         /* HTT_RX_IND_MPDU_STATUS */
#include <htt_types.h>
#include <ol_htt_api.h>  /* htt_pdev_handle */

#include <ieee80211_defines.h> /* ieee80211_rx_status */
#include <ol_vowext_dbg_defs.h>

/*================ rx indication message field access methods ===============*/

/**
 * @brief Check if a rx indication message has a rx reorder flush command.
 * @details
 *  Space is reserved in each rx indication message for a rx reorder flush
 *  command, to release specified MPDUs from the rx reorder holding array
 *  before processing the new MPDUs referenced by the rx indication message.
 *  This rx reorder flush command contains a flag to show whether the command
 *  is valid within a given rx indication message.
 *  This function checks the validity flag from the rx indication
 *  flush command IE within the rx indication message.
 *
 * @param rx_ind_msg - the netbuf containing the rx indication message
 * @return
 *      1 - the message's rx flush command is valid and should be processed
 *          before processing new rx MPDUs,
 *      -OR-
 *      0 - the message's rx flush command is invalid and should be ignored
 */
int
htt_rx_ind_flush(qdf_nbuf_t rx_ind_msg);

/**
 * @brief Return the sequence number starting the range of MPDUs to flush.
 * @details
 *  Read the fields of the rx indication message that identify the start
 *  and end of the range of MPDUs to flush from the rx reorder holding array
 *  and send on to subsequent stages of rx processing.
 *  These sequence numbers are the 6 LSBs of the 12-bit 802.11 sequence
 *  number.  These sequence numbers are masked with the block ack window size,
 *  rounded up to a power of two (minus one, to create a bitmask) to obtain
 *  the corresponding index into the rx reorder holding array.
 *  The series of MPDUs to flush includes the one specified by the start
 *  sequence number.
 *  The series of MPDUs to flush excludes the one specified by the end
 *  sequence number; the MPDUs up to but not including the end sequence number
 *  are to be flushed.
 *  These start and end seq num fields are only valid if the "flush valid"
 *  flag is set.
 *
 * @param rx_ind_msg - the netbuf containing the rx indication message
 * @param seq_num_start - (call-by-reference output) sequence number
 *      for the start of the range of MPDUs to flush
 * @param seq_num_end - (call-by-reference output) sequence number
 *      for the end of the range of MPDUs to flush
 */
void
htt_rx_ind_flush_seq_num_range(
    qdf_nbuf_t rx_ind_msg,
    int *seq_num_start,
    int *seq_num_end);

/**
 * @brief Check if a rx indication message has a rx reorder release command.
 * @details
 *  Space is reserved in each rx indication message for a rx reorder release
 *  command, to release specified MPDUs from the rx reorder holding array
 *  after processing the new MPDUs referenced by the rx indication message.
 *  This rx reorder release command contains a flag to show whether the command
 *  is valid within a given rx indication message.
 *  This function checks the validity flag from the rx indication
 *  release command IE within the rx indication message.
 *
 * @param rx_ind_msg - the netbuf containing the rx indication message
 * @return
 *      1 - the message's rx release command is valid and should be processed
 *          after processing new rx MPDUs,
 *      -OR-
 *      0 - the message's rx release command is invalid and should be ignored
 */
int
htt_rx_ind_release(qdf_nbuf_t rx_ind_msg);

/**
 * @brief Return the sequence number starting the range of MPDUs to release.
 * @details
 *  Read the fields of the rx indication message that identify the start
 *  and end of the range of MPDUs to release from the rx reorder holding
 *  array and send on to subsequent stages of rx processing.
 *  These sequence numbers are the 6 LSBs of the 12-bit 802.11 sequence
 *  number.  These sequence numbers are masked with the block ack window size,
 *  rounded up to a power of two (minus one, to create a bitmask) to obtain
 *  the corresponding index into the rx reorder holding array.
 *  The series of MPDUs to release includes the one specified by the start
 *  sequence number.
 *  The series of MPDUs to release excludes the one specified by the end
 *  sequence number; the MPDUs up to but not including the end sequence number
 *  are to be released.
 *  These start and end seq num fields are only valid if the "release valid"
 *  flag is set.
 *
 * @param rx_ind_msg - the netbuf containing the rx indication message
 * @param seq_num_start - (call-by-reference output) sequence number
 *      for the start of the range of MPDUs to release
 * @param seq_num_end - (call-by-reference output) sequence number
 *      for the end of the range of MPDUs to release
 */
void
htt_rx_ind_release_seq_num_range(
    qdf_nbuf_t rx_ind_msg,
    int *seq_num_start,
    int *seq_num_end);

/**
 * @brief Check the status MPDU range referenced by a rx indication message.
 * @details
 *  Check the status of a range of MPDUs referenced by a rx indication message.
 *  This status determines whether the MPDUs should be processed or discarded.
 *  If the status is OK, then the MPDUs within the range should be processed
 *  as usual.
 *  Otherwise (FCS error, duplicate error, replay error, unknown sender error,
 *  etc.) the MPDUs within the range should be discarded.
 *
 * @param pdev - the HTT instance the rx data was received on
 *  @param rx_ind_msg - the netbuf containing the rx indication message
 *  @param mpdu_range_num - which MPDU range within the rx ind msg to check,
 *       starting from 0
 *  @param status - (call-by-reference output) MPDU status
 *  @param mpdu_count - (call-by-reference output) count of MPDUs comprising
 *       the specified MPDU range
 */
void
htt_rx_ind_mpdu_range_info(
    htt_pdev_handle pdev,
    qdf_nbuf_t rx_ind_msg,
    int mpdu_range_num,
    enum htt_rx_status *status,
    int *mpdu_count);

/*==================== rx MPDU descriptor access methods ====================*/

/**
 * @brief Return a rx MPDU's sequence number.
 * @details
 *  This function returns the LSBs of the 802.11 sequence number for the
 *  provided rx MPDU descriptor.
 *  Depending on the system, 6-12 LSBs from the 802.11 sequence number are
 *  returned.  (Typically, either the 8 or 12 LSBs are returned.)
 *  This sequence number is masked with the block ack window size,
 *  rounded up to a power of two (minus one, to create a bitmask) to obtain
 *  the corresponding index into the rx reorder holding array.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param mpdu_desc - the abstract descriptor for the MPDU in question
 * @return the LSBs of the sequence number for the MPDU
 */
static inline int
htt_rx_mpdu_desc_seq_num(htt_pdev_handle pdev, void *mpdu_desc)
{
    return pdev->htt_rx_mpdu_desc_seq_num(pdev->arh, mpdu_desc);
}

/**
 * @brief Check whether a MPDU completes an MSDU.
 * @details
 *  This function checks a MPDU's fragmentation flag to determine whether
 *  the MPDU completes an MSDU, or if subsequent MPDUs are needed to add
 *  the remaining fragment(s) of the MSDU.
 *  There are three conditions that result in the "more frags" flag being
 *  cleared:
 *  1.  The MPDU contains a single complete MSDU.
 *  2.  The MPDU contains multiple complete MSDUs (within a A-MSDU).
 *  3.  The MPDU contains the final fragment of a MSDU.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param mpdu_desc - the abstract descriptor for the MPDU in question
 * @return
 *      0 - the MPDU completes an MSDUs,
 *      -OR-
 *      1 - the MPDU contains a beginning or intermediate fragment of a MSDU
 */
int
htt_rx_mpdu_desc_more_frags(htt_pdev_handle pdev, void *mpdu_desc);

union htt_rx_pn_t {
    /* WEP: 24-bit PN */
    u_int32_t pn24;

    /* TKIP or CCMP: 48-bit PN */
    u_int64_t pn48;

    /* WAPI: 128-bit PN */
    u_int64_t pn128[2];
};

/**
 * @brief Find the packet number (PN) for a MPDU.
 * @details
 *  This function only applies when the rx PN check is configured to be
 *  performed in the host rather than the target, and on peers using a
 *  security type for which a PN check applies.
 *  The pn_len_bits argument is used to determine which element of the
 *  htt_rx_pn_t union to deposit the PN value read from the MPDU descriptor
 *  into.
 *  A 24-bit PN is deposited into pn->pn24.
 *  A 48-bit PN is deposited into pn->pn48.
 *  A 128-bit PN is deposited in little-endian order into pn->pn128.
 *  Specifically, bits 63:0 of the PN are copied into pn->pn128[0], while
 *  bits 127:64 of the PN are copied into pn->pn128[1].
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param mpdu_desc - the abstract descriptor for the MPDU in question
 * @param pn - the location to copy the packet number into
 * @param pn_len_bits - the PN size, in bits
 */
static inline void htt_rx_mpdu_desc_pn(
    htt_pdev_handle pdev,
    void *mpdu_desc,
    union htt_rx_pn_t *pn,
    int pn_len_bits)
{
    pdev->htt_rx_mpdu_desc_pn(pdev,
            mpdu_desc,
            pn,
            pn_len_bits);
}

/**
 * @brief Return the TSF timestamp indicating when a MPDU was received.
 * @details
 *  This function provides the timestamp indicating when the PPDU that
 *  the specified MPDU belongs to was received.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param mpdu_desc - the abstract descriptor for the MPDU in question
 * @return 32 LSBs of TSF time at which the MPDU's PPDU was received
 */
u_int32_t
htt_rx_mpdu_desc_tsf32(
    htt_pdev_handle pdev,
    void *mpdu_desc);

/**
 * @brief Return the 802.11 header of the MPDU
 * @details
 *  This function provides a pointer to the start of the 802.11 header
 *  of the Rx MPDU
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param mpdu_desc - the abstract descriptor for the MPDU in question
 * @return pointer to 802.11 header of the received MPDU
 */
static inline char *
htt_rx_mpdu_wifi_hdr_retrieve(
    htt_pdev_handle pdev,
    void *mpdu_desc)
{
    return pdev->ar_rx_ops->wifi_hdr_retrieve(mpdu_desc);
}

/**
 * @brief Return the from DS bit in the frame control field of 802.11
 * header of the Rx MPDU
 * @details
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param mpdu_desc - the abstract descriptor for the MPDU in question
 * @return 1 if "from DS" bit is set in the Rx MPDU, else 0
 */
static inline int
htt_rx_mpdu_desc_frds(htt_pdev_handle pdev, void *mpdu_desc)
{
    return pdev->ar_rx_ops->mpdu_desc_frds(mpdu_desc);
}

/**
 * @brief Return the to DS bit in the frame control field of 802.11
 * header of the Rx MPDU
 * @details
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param mpdu_desc - the abstract descriptor for the MPDU in question
 * @return 1 if "to DS" bit is set in the Rx MPDU, else 0
 */
static inline int
htt_rx_mpdu_desc_tods(htt_pdev_handle pdev, void *mpdu_desc)
{
    return pdev->ar_rx_ops->mpdu_desc_tods(mpdu_desc);
}
#if RX_DEBUG
/**
 * @brief Return htt status of rx mpdu
 * @details
 *
 * @param pdev - the HTT instance the rx data was received on
 * @return htt status of rx mpdu
 */

static inline int
htt_rx_mpdu_status(htt_pdev_handle pdev)
{
    return pdev->htt_rx_mpdu_status(pdev);
}
#endif

/*==================== rx MSDU descriptor access methods ====================*/

/**
 * @brief Check if a MSDU completes a MPDU.
 * @details
 *  When A-MSDU aggregation is used, a single MPDU will consist of
 *  multiple MSDUs.  This function checks a MSDU's rx descriptor to
 *  see whether the MSDU is the final MSDU within a MPDU.
 *
 * @param pdev - the handle of the physical device the rx data was received on
 * @param msdu_desc - the abstract descriptor for the MSDU in question
 * @return
 *      0 - there are subsequent MSDUs within the A-MSDU / MPDU
 *      -OR-
 *      1 - this is the last MSDU within its MPDU
 */
static inline int htt_rx_msdu_desc_completes_mpdu(
        htt_pdev_handle pdev, void *msdu_desc)
{
    return pdev->htt_rx_msdu_desc_completes_mpdu(pdev->arh,
            msdu_desc);
}

/**
 * @brief Check if a MSDU is first msdu of MPDU.
 * @details
 *  When A-MSDU aggregation is used, a single MPDU will consist of
 *  multiple MSDUs.  This function checks a MSDU's rx descriptor to
 *  see whether the MSDU is the first MSDU within a MPDU.
 *
 * @param pdev - the handle of the physical device the rx data was received on
 * @param msdu_desc - the abstract descriptor for the MSDU in question
 * @return
 *      0 - this is interior MSDU in the A-MSDU / MPDU
 *      -OR-
 *      1 - this is the first MSDU within its MPDU
 */
static inline int htt_rx_msdu_first_msdu_flag(
        htt_pdev_handle pdev, void *msdu_desc)
{
    return pdev->htt_rx_msdu_first_msdu_flag(pdev->arh,
            msdu_desc);
}


/**
 * @brief Returns the Key index used to decrypt this msdu.
 * @details
 *  This function checks a MSDU's rx descriptor to
 *  and returns Key index used for decrypting this MSDU.
 *
 * @param pdev - the handle of the physical device the rx data was received on
 * @param msdu_desc - the abstract descriptor for the MSDU in question
 */
static inline uint32_t htt_rx_msdu_key_id_octet(
        htt_pdev_handle pdev, void *msdu_desc)
{
    return pdev->htt_rx_msdu_key_id_octet(pdev->arh, msdu_desc);
}

/**
 * @brief Retrieve encrypt bit from a mpdu desc.
 * @details
 *  Fw will pass all the frame  to the host whether encrypted or not, and will
 *  indicate the encrypt flag in the desc, this function is to get the info and used
 *  to make a judge whether should make pn check, because non-encrypted frames
 *  always get the same pn number 0.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param mpdu_desc - the abstract descriptor for the MPDU in question
 * @return 0 - the frame was not encrypted
 *         1 - the frame was encrypted
 */
static inline int
htt_rx_mpdu_is_encrypted(htt_pdev_handle pdev, void *mpdu_desc)
{
    return pdev->htt_rx_mpdu_is_encrypted(pdev->arh, mpdu_desc);
}

/**
 * @brief Indicate whether a rx desc has a WLAN unicast vs. mcast/bcast flag.
 * @details
 *  A flag indicating whether a MPDU was delivered over WLAN as unicast or
 *  multicast/broadcast may be only valid once per MPDU (LL), or within each
 *  rx descriptor for the MSDUs within the MPDU (HL).  (In practice, it is
 *  unlikely that A-MSDU aggregation will be used in HL, so typically HL will
 *  only have one MSDU per MPDU anyway.)
 *  This function indicates whether the specified rx descriptor contains
 *  a WLAN ucast vs. mcast/bcast flag.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param msdu_desc - the abstract descriptor for the MSDU in question
 * @return
 *      0 - The rx descriptor does not contain a WLAN ucast vs. mcast flag.
 *      -OR-
 *      1 - The rx descriptor has a valid WLAN ucast vs. mcast flag.
 */
static inline int htt_rx_msdu_has_wlan_mcast_flag(
        htt_pdev_handle pdev, void *msdu_desc)
{
    return pdev->htt_rx_msdu_has_wlan_mcast_flag(pdev->arh,
            msdu_desc);
}

/**
 * @brief Indicate whether a MSDU was received as unicast or mcast/bcast
 * @details
 *  Indicate whether the MPDU that the specified MSDU belonged to was
 *  delivered over the WLAN as unicast, or as multicast/broadcast.
 *  This query can only be performed on rx descriptors for which
 *  htt_rx_msdu_has_wlan_mcast_flag is true.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param msdu_desc - the abstract descriptor for the MSDU in question
 * @return
 *      0 - The MSDU was delivered over the WLAN as unicast.
 *      -OR-
 *      1 - The MSDU was delivered over the WLAN as broadcast or multicast.
 */
static inline int htt_rx_msdu_is_wlan_mcast(
        htt_pdev_handle pdev, void *msdu_desc)
{
    return pdev->htt_rx_msdu_is_wlan_mcast(pdev->arh,
            msdu_desc);
}

/**
 * @brief Indicate whether a MSDU was received as a fragmented frame
 * @details
 *  This query can only be performed on LL system.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param msdu_desc - the abstract descriptor for the MSDU in question
 * @return
 *      0 - The MSDU was a non-fragmented frame.
 *      -OR-
 *      1 - The MSDU was fragmented frame.
 */
static inline int htt_rx_msdu_is_frag(
        htt_pdev_handle pdev, void *msdu_desc)
{
    return pdev->htt_rx_msdu_is_frag(pdev->arh,
            msdu_desc);
}

/**
 * @brief Indicate if a MSDU should be delivered to the OS shim or discarded.
 * @details
 *  Indicate whether a MSDU should be discarded or delivered to the OS shim.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param msdu_desc - the abstract descriptor for the MSDU in question
 * @return
 *      0 - The MSDU should be delivered to the OS
 *      -OR-
 *      non-zero - The MSDU should not be delivered to the OS.
 *          If the "forward" flag is set, it should be forwarded to tx.
 *          Else, it should be discarded.
 */
int
htt_rx_msdu_discard(htt_pdev_handle pdev, void *msdu_desc);

/**
 * @brief Indicate whether a MSDU should be forwarded to tx.
 * @details
 *  Indicate whether a MSDU should be forwarded to tx, e.g. for intra-BSS
 *  STA-to-STA forwarding in an AP, or for multicast echo in an AP.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param msdu_desc - the abstract descriptor for the MSDU in question
 * @return
 *      0 - The MSDU should not be forwarded
 *      -OR-
 *      non-zero - The MSDU should be forwarded.
 *          If the "discard" flag is set, then the original MSDU can be
 *          directly forwarded into the tx path.
 *          Else, a copy (clone?) of the rx MSDU needs to be created to
 *          send to the tx path.
 */
int
htt_rx_msdu_forward(htt_pdev_handle pdev, void *msdu_desc);

/**
 * @brief Indicate whether a MSDU's contents need to be inspected.
 * @details
 *  Indicate whether the host data SW needs to examine the contents of the
 *  received MSDU, and based on the packet type infer what special handling
 *  to provide for the MSDU.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param msdu_desc - the abstract descriptor for the MSDU in question
 * @return
 *      0 - No inspection + special handling is required.
 *      -OR-
 *      non-zero - Inspect the MSDU contents to infer what special handling
 *          to apply to the MSDU.
 */
int
htt_rx_msdu_inspect(htt_pdev_handle pdev, void *msdu_desc);


/**
 * @brief Provide all action specifications for a rx MSDU
 * @details
 *  Provide all action specifications together.  This provides the same
 *  information in a single function call as would be provided by calling
 *  the functions htt_rx_msdu_discard, htt_rx_msdu_forward, and
 *  htt_rx_msdu_inspect.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param msdu_desc - the abstract descriptor for the MSDU in question
 * @param[out] discard - 1: discard the MSDU, 0: deliver the MSDU to the OS
 * @param[out] forward - 1: forward the rx MSDU to tx, 0: no rx->tx forward
 * @param[out] inspect - 1: process according to MSDU contents, 0: no inspect
 */
void
htt_rx_msdu_actions(
    htt_pdev_handle pdev,
    void *msdu_desc,
    int *discard,
    int *forward,
    int *inspect);

/*====================== rx MSDU + descriptor delivery ======================*/

/**
 * @brief Return a linked-list of network buffer holding the next rx A-MSDU.
 * @details
 *  In some systems, the rx MSDUs are uploaded along with the rx
 *  indication message, while in other systems the rx MSDUs are uploaded
 *  out of band, via MAC DMA.
 *  This function provides an abstract way to obtain a linked-list of the
 *  next MSDUs, regardless of whether the MSDU was delivered in-band with
 *  the rx indication message, or out of band through MAC DMA.
 *  In a LL system, this function returns a linked list of the one or more
 *  MSDUs that together comprise an A-MSDU.
 *  In a HL system, this function returns a degenerate linked list consisting
 *  of a single MSDU (head_msdu == tail_msdu).
 *  This function also makes sure each MSDU's rx descriptor can be found
 *  through the MSDU's network buffer.
 *  In most systems, this is trivial - a single network buffer stores both
 *  the MSDU rx descriptor and the MSDU payload.
 *  In systems where the rx descriptor is in a separate buffer from the
 *  network buffer holding the MSDU payload, a pointer to the rx descriptor
 *  has to be stored in the network buffer.
 *  After this function call, the descriptor for a given MSDU can be
 *  obtained via the htt_rx_msdu_desc_retrieve function.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param rx_ind_msg - the netbuf containing the rx indication message
 * @param head_msdu - call-by-reference network buffer handle, which gets set
 *      in this function to point to the head MSDU of the A-MSDU
 * @param tail_msdu - call-by-reference network buffer handle, which gets set
 *      in this function to point to the tail MSDU of the A-MSDU, or the
 *      same MSDU that the head_msdu points to if only a single MSDU is
 *      delivered at a time.
 * @return indication of whether any MSDUs in the AMSDU use chaining:
 * 0 - no buffer chaining
 * 1 - buffers are chained
 */
static inline int
htt_rx_amsdu_pop(
    htt_pdev_handle pdev,
#if QCA_OL_SUPPORT_RAWMODE_TXRX
    struct ol_txrx_vdev_t *vdev,
#endif
    qdf_nbuf_t rx_ind_msg,
    qdf_nbuf_t *head_msdu,
    qdf_nbuf_t *tail_msdu,
    u_int32_t *npackets
    )
{
    return pdev->htt_rx_amsdu_pop(pdev->arh,
#if QCA_OL_SUPPORT_RAWMODE_TXRX
                vdev,
#endif
                rx_ind_msg,
                head_msdu,
                tail_msdu,
                npackets);
}

#ifndef REMOVE_PKT_LOG
static inline u_int32_t
htt_rx_check_desc_mgmt_type(htt_pdev_handle pdev,
   qdf_nbuf_t head_msdu)
{
    return pdev->ar_rx_ops->check_desc_mgmt_type(head_msdu);
}
static inline u_int32_t
htt_rx_check_desc_ctrl_type(htt_pdev_handle pdev,
           qdf_nbuf_t head_msdu)
{
        return pdev->ar_rx_ops->check_desc_ctrl_type(head_msdu);
}
static inline u_int32_t
htt_rx_check_desc_phy_data_type(htt_pdev_handle pdev,
   qdf_nbuf_t head_msdu)
{
    uint8_t rx_phy_data_filter;

    return pdev->ar_rx_ops->check_desc_phy_data_type(head_msdu, &rx_phy_data_filter);
}

#endif

/**
 * @brief Return the rx descriptor for the next rx MPDU.
 * @details
 *  The rx MSDU descriptors may be uploaded as part of the rx indication
 *  message, or delivered separately out of band.
 *  This function provides an abstract way to obtain the next MPDU descriptor,
 *  regardless of whether the MPDU descriptors are delivered in-band with
 *  the rx indication message, or out of band.
 *  This is used to iterate through the series of MPDU descriptors referenced
 *  by a rx indication message.
 *  The htt_rx_amsdu_pop function should be called before this function
 *  (or at least before using the returned rx descriptor handle), so that
 *  the cache location for the rx descriptor will be flushed before the
 *  rx descriptor gets used.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param rx_ind_msg - the netbuf containing the rx indication message
 * @return next abstract rx descriptor from the series of MPDUs referenced
 *      by an rx ind msg
 */
static inline void *
htt_rx_mpdu_desc_list_next(htt_pdev_handle pdev, qdf_nbuf_t rx_ind_msg)
{
    return pdev->htt_rx_mpdu_desc_list_next(pdev,
            rx_ind_msg);
}

/**
 * @brief Retrieve a previously-stored rx descriptor from a MSDU buffer.
 * @details
 *  The data SW will call the htt_rx_msdu_desc_link macro/function to
 *  link a MSDU's rx descriptor with the buffer holding the MSDU payload.
 *  This function retrieves the rx MSDU descriptor.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param msdu - the buffer containing the MSDU payload
 * @return the corresponding abstract rx MSDU descriptor
 */
static inline void *
htt_rx_msdu_desc_retrieve(htt_pdev_handle pdev, qdf_nbuf_t msdu)
{
    return pdev->htt_rx_msdu_desc_retrieve(pdev,
            msdu);
}

/**
 * @brief Free both an rx MSDU descriptor and the associated MSDU buffer.
 * @details
 *  Usually the WLAN driver does not free rx MSDU buffers, but needs to
 *  do so when an invalid frame (e.g. FCS error) was deposited into the
 *  queue of rx buffers.
 *  This function frees both the rx descriptor and the rx frame.
 *  On some systems, the rx descriptor and rx frame are stored in the
 *  same buffer, and thus one free suffices for both objects.
 *  On other systems, the rx descriptor and rx frame are stored
 *  separately, so distinct frees are internally needed.
 *  However, in either case, the rx descriptor has been associated with
 *  the MSDU buffer, and can be retrieved by htt_rx_msdu_desc_retrieve.
 *  Hence, it is only necessary to provide the MSDU buffer; the HTT SW
 *  internally finds the corresponding MSDU rx descriptor.
 *
 * @param htt_pdev - the HTT instance the rx data was received on
 * @param rx_msdu_desc - rx descriptor for the MSDU being freed
 * @param msdu - rx frame buffer for the MSDU being freed
 */
void
htt_rx_desc_frame_free(
    htt_pdev_handle htt_pdev,
    qdf_nbuf_t msdu);

/**
 * @brief Look up and free the rx descriptor for a MSDU.
 * @details
 *  When the driver delivers rx frames to the OS, it first needs
 *  to free the associated rx descriptors.
 *  In some systems the rx descriptors are allocated in the same
 *  buffer as the rx frames, so this operation is a no-op.
 *  In other systems, the rx descriptors are stored separately
 *  from the rx frames, so the rx descriptor has to be freed.
 *  The descriptor is located from the MSDU buffer with the
 *  htt_rx_desc_frame_free macro/function.
 *
 * @param htt_pdev - the HTT instance the rx data was received on
 * @param msdu - rx frame buffer for the rx MSDU descriptor being freed
 */
void
htt_rx_msdu_desc_free(htt_pdev_handle htt_pdev, qdf_nbuf_t msdu);

/**
 * @brief Add new MSDU buffers for the target to fill.
 * @details
 *  In some systems, the underlying upload mechanism (HIF) allocates new rx
 *  buffers itself.  In other systems, the underlying upload mechanism
 *  (MAC DMA) needs to be provided with new rx buffers.
 *  This function is used as an abstract method to indicate to the underlying
 *  data upload mechanism when it is an appropriate time to allocate new rx
 *  buffers.
 *  If the allocation is automatically handled, a la HIF, then this function
 *  call is ignored.
 *  If the allocation has to be done explicitly, a la MAC DMA, then this
 *  function provides the context and timing for such replenishment
 *  allocations.
 *
 * @param pdev - the HTT instance the rx data will be received on
 */
void
htt_rx_msdu_buff_replenish(htt_pdev_handle pdev);

/**
 * @brief Links list of MSDUs into an single MPDU. Updates RX stats
 * @details
 *  When HW MSDU splitting is turned on each MSDU in an AMSDU MPDU occupies
 *  a separate wbuf for delivery to the network stack. For delivery to the
 *  monitor mode interface they need to be restitched into an MPDU. This
 *  function does this. Also updates the RX status if the MPDU starts
 *  a new PPDU
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param head_msdu - network buffer handle, which points to the first MSDU
 *      in the list. This is a NULL terminated list
 * @param rx_staus - pointer to the status associated with this MPDU.
 *      Updated only if there is a new PPDU and new status associated with it
 * @param clone_not_reqd - If set the MPDU linking destroys the passed in
 *      list, else operates on a cloned nbuf
 * @return network buffer handle to the MPDU
 */
static inline qdf_nbuf_t
htt_rx_restitch_mpdu_from_msdus(
    htt_pdev_handle pdev,
    qdf_nbuf_t head_msdu,
    struct ieee80211_rx_status *rx_status,
    unsigned clone_not_reqd)
{
    return pdev->ar_rx_ops->restitch_mpdu_from_msdus(pdev->arh,
            head_msdu,
            rx_status,
            clone_not_reqd);
}

/**
 * @brief Return the sequence number of MPDUs to flush.
 * @param rx_frag_ind_msg - the netbuf containing the rx fragment indication message
 * @param seq_num_start - (call-by-reference output) sequence number
 *      for the start of the range of MPDUs to flush
 * @param seq_num_end - (call-by-reference output) sequence number
 *      for the end of the range of MPDUs to flush
 */
void
htt_rx_frag_ind_flush_seq_num_range(
    qdf_nbuf_t rx_frag_ind_msg,
    int *seq_num_start,
    int *seq_num_end);

/**
 * @brief Return the HL rx desc size
 * @param pdev - the HTT instance the rx data was received on
 * @param msdu_desc - the hl rx desc pointer
 *
 */
int
htt_rx_msdu_rx_desc_size_hl(
        htt_pdev_handle pdev,
        void *msdu_desc);

/**
 * @brief populates vowext stats by processing RX desc.
 * @param msdu - network buffer handle
 * @param vowstats - handle to vow ext stats.
 */
static inline void htt_rx_get_vowext_stats(htt_pdev_handle pdev,
        qdf_nbuf_t msdu,
        struct vow_extstats *vowstats)
{
    pdev->ar_rx_ops->get_vowext_stats(msdu, vowstats);
}


/**
 * @brief populates smart antenna rx feedback structure.
 * @param head_msdu - network buffer handle for head msdu.
 * @param tail_msdu - network buffer handle for tail msdu.
 * @param feedback - handle to sa_rx_feedback.
 */

#define MASK_BYTE3 0xff000000
#define SHIFT_BYTE3 24
#define RX_DESC_PREAMBLE_OFFSET 5
#define RX_DESC_MASK_LSIG_SEL  0x00000010
#define RX_DESC_MASK_LSIG_RATE 0x0000000f
#define RX_DESC_HT_RATE_OFFSET 6
#define RX_DESC_HT_MCS_MASK 0x7f
#define RX_DESC_VHT_RATE_OFFSET 7
#define RX_DESC_VHT_MCS_MASK 0xf
#define RX_DESC_VHT_NSS_MASK 0x7
#define RX_DESC_VHT_BW_MASK 3
#define RX_DESC_VHT_SGI_MASK 1

#ifdef CONFIG_AR900B_SUPPORT
#define RX_DESC_ANTENNA_OFFSET 25
#else
#define RX_DESC_ANTENNA_OFFSET 19
#endif

#if UNIFIED_SMARTANTENNA
static inline int htt_rx_get_smart_ant_stats(htt_pdev_handle pdev,
        qdf_nbuf_t rx_ind_msg,
        qdf_nbuf_t head_msdu,
        qdf_nbuf_t tail_msdu,
        struct sa_rx_feedback *feedback)
{
    return pdev->ar_rx_ops->get_smart_ant_stats(pdev->arh,
            rx_ind_msg,
            head_msdu,
            tail_msdu,
            feedback);
}
#endif /* UNIFIED_SMARTANTENNA */

#if RX_DEBUG
void dump_pkt(qdf_nbuf_t msdu, int len);
void dump_htt_rx_desc(htt_pdev_handle pdev, void *rx_desc);
#endif /* RX_DEBUG */

int htt_rx_update_pkt_info(qdf_nbuf_t head_msdu, qdf_nbuf_t tail_msdu);

#endif /* _OL_HTT_RX_API__H_ */

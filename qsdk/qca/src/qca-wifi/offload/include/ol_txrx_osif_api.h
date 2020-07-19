/*
 * Copyright (c) 2012 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/**
 * @file ol_txrx_osif_api.h
 * @brief Define the host data API functions called by the host OS shim SW.
 */
#ifndef _OL_TXRX_OSIF_API__H_
#define _OL_TXRX_OSIF_API__H_

#include <qdf_nbuf.h>     /* qdf_nbuf_t */

#include <cdp_txrx_cmn_struct.h> /* ol_osif_vdev_handle */

/**
 * @enum ol_txrx_osif_tx_spec
 * @brief indicate what non-standard transmission actions to apply
 * @details
 *  Indicate one or more of the following:
 *    - The tx frame already has a complete 802.11 header.
 *      Thus, skip 802.3/native-WiFi to 802.11 header encapsulation and
 *      A-MSDU aggregation.
 *    - The tx frame should not be aggregated (A-MPDU or A-MSDU)
 *    - The tx frame is already encrypted - don't attempt encryption.
 *    - The tx frame is a segment of a TCP jumbo frame.
 *  More than one of these specification can apply, though typically
 *  only a single specification is applied to a tx frame.
 *  A compound specification can be created, as a bit-OR of these
 *  specifications.
 */
enum ol_txrx_osif_tx_spec {
     ol_txrx_osif_tx_spec_std         = 0x0,  /* do regular processing */
     ol_txrx_osif_tx_spec_raw         = 0x1,  /* skip encap + A-MSDU aggr */
     ol_txrx_osif_tx_spec_no_aggr     = 0x2,  /* skip encap + all aggr */
     ol_txrx_osif_tx_spec_no_encrypt  = 0x4,  /* skip encap + encrypt */
     ol_txrx_osif_tx_spec_tso         = 0x8,  /* TCP segmented */
     ol_txrx_osif_tx_spect_nwifi_no_encrypt = 0x10, /* skip encrypt for nwifi */
};

#define OL_TXRX_OSIF_TX_EXT_TID_NON_QOS_MCAST_BCAST 16
#define OL_TXRX_OSIF_TX_EXT_TID_MGMT                17
#define OL_TXRX_OSIF_TX_EXT_TID_INVALID             31


/**
 * @brief Divide a jumbo TCP frame into smaller segments.
 * @details
 *  For efficiency, the protocol stack above the WLAN driver may operate
 *  on jumbo tx frames, which are larger than the 802.11 MTU.
 *  The OSIF SW uses this txrx API function to divide the jumbo tx TCP frame
 *  into a series of segment frames.
 *  The segments are created as clones of the input jumbo frame.
 *  The txrx SW generates a new encapsulation header (ethernet + IP + TCP)
 *  for each of the output segment frames.  The exact format of this header,
 *  e.g. 802.3 vs. Ethernet II, and IPv4 vs. IPv6, is chosen to match the
 *  header format of the input jumbo frame.
 *  The input jumbo frame is not modified.
 *  After the ol_txrx_osif_tso_segment returns, the OSIF SW needs to perform
 *  DMA mapping on each of the segment network buffers, and also needs to
 *
 * @param txrx_vdev - which virtual device will transmit the TSO segments
 * @param max_seg_payload_bytes - the maximum size for the TCP payload of
 *      each segment frame.
 *      This does not include the ethernet + IP + TCP header sizes.
 * @param jumbo_tcp_frame - jumbo frame which needs to be cloned+segmented
 * @return
 *      NULL if the segmentation fails, - OR -
 *      a NULL-terminated list of segment network buffers
 */
qdf_nbuf_t ol_txrx_osif_tso_segment(
    ol_txrx_vdev_handle txrx_vdev,
    int max_seg_payload_bytes,
    qdf_nbuf_t jumbo_tcp_frame);


#endif /* _OL_TXRX_OSIF_API__H_ */

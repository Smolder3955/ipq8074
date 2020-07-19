/* AUTO-GENERATED FILE - DO NOT EDIT DIRECTLY */
/*
 * Copyright (c) 2012 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/**
 *   @addtogroup WDIAPI
 *@{
 */
/**
 * @file wdi_out.h
 * @brief Functions outside the WDI boundary called by datapath functions
 */
#ifndef _WDI_OUT__H_
#define _WDI_OUT__H_

#include <wdi_types.h>

#ifdef WDI_API_AS_FUNCS

#include <qdf_types.h> /* u_int32_t */

/**
 * @brief Specify whether the system is high-latency or low-latency.
 * @details
 *  Indicate whether the system is operating in high-latency (message
 *  based, e.g. USB) mode or low-latency (memory-mapped, e.g. PCIe) mode.
 *  Some chips support just one type of host / target interface.
 *  Other chips support both LL and HL interfaces (e.g. PCIe and USB),
 *  so the selection will be made based on which bus HW is present, or
 *  which is preferred if both are present.
 *
 * @param pdev - handle to the physical device
 * @return 1 -> high-latency -OR- 0 -> low-latency
 */
int wdi_out_cfg_is_high_latency(ol_pdev_handle pdev);

/**
 * @brief Specify the range of peer IDs.
 * @details
 *  Specify the maximum peer ID.  This is the maximum number of peers,
 *  minus one.
 *  This is used by the host to determine the size of arrays indexed by
 *  peer ID.
 *
 * @param pdev - handle to the physical device
 * @return maximum peer ID
 */
int wdi_out_cfg_max_peer_id(ol_pdev_handle pdev);

/**
 * @brief Specify the max number of virtual devices within a physical device.
 * @details
 *  Specify how many virtual devices may exist within a physical device.
 *
 * @param pdev - handle to the physical device
 * @return maximum number of virtual devices
 */
int wdi_out_cfg_max_vdevs(ol_pdev_handle pdev);

/**
 * @brief Check whether host-side rx PN check is enabled or disabled.
 * @details
 *  Choose whether to allocate rx PN state information and perform
 *  rx PN checks (if applicable, based on security type) on the host.
 *  If the rx PN check is specified to be done on the host, the host SW
 *  will determine which peers are using a security type (e.g. CCMP) that
 *  requires a PN check.
 *
 * @param pdev - handle to the physical device
 * @return 1 -> host performs rx PN check -OR- 0 -> no host-side rx PN check
 */
int wdi_out_cfg_rx_pn_check(ol_pdev_handle pdev);

/**
 * @brief Check whether host-side rx forwarding is enabled or disabled.
 * @details
 *  Choose whether to check whether to forward rx frames to tx on the host.
 *  For LL systems, this rx -> tx host-side forwarding check is typically
 *  enabled.
 *  For HL systems, the rx -> tx forwarding check is typically done on the
 *  target.  However, even in HL systems, the host-side rx -> tx forwarding
 *  will typically be enabled, as a second-tier safety net in case the
 *  target doesn't have enough memory to store all rx -> tx forwarded frames.
 *
 * @param pdev - handle to the physical device
 * @return 1 -> host does rx->tx forward -OR- 0 -> no host-side rx->tx forward
 */
int wdi_out_cfg_rx_fwd_check(ol_pdev_handle pdev);

/**
 * @brief Check whether to perform inter-BSS or intra-BSS rx->tx forwarding.
 * @details
 *  Check whether data received by an AP on one virtual device destined
 *  to a STA associated with a different virtual device within the same
 *  physical device should be forwarded within the driver, or whether
 *  forwarding should only be done within a virtual device.
 *
 * @param pdev - handle to the physical device
 * @return
 *      1 -> forward both within and between vdevs
 *      -OR-
 *      0 -> forward only within a vdev
 */
int wdi_out_cfg_rx_fwd_inter_bss(ol_pdev_handle pdev);

/**
 * @brief format of data frames delivered to/from the WLAN driver by/to the OS
 * @details
 *  Note: This must correspond to the ordering in the htt_pkt_type enumeration.
 */
enum wlan_frm_fmt {
    wlan_frm_fmt_raw,
    wlan_frm_fmt_native_wifi,
    wlan_frm_fmt_802_3,
};

/**
 * @brief Specify data frame format used by the OS.
 * @details
 *  Specify what type of frame (802.3 or native WiFi) the host data SW
 *  should expect from and provide to the OS shim.
 *
 * @param pdev - handle to the physical device
 * @return enumerated data frame format
 */
enum wlan_frm_fmt wdi_out_cfg_frame_type(ol_pdev_handle pdev);

/**
 * @brief Specify the peak throughput.
 * @details
 *  Specify the peak throughput that a system is expected to support.
 *  The data SW uses this configuration to help choose the size for its
 *  tx descriptor pool and rx buffer ring.
 *  The data SW assumes that the peak throughput applies to either rx or tx,
 *  rather than having separate specs of the rx max throughput vs. the tx
 *  max throughput.
 *
 * @param pdev - handle to the physical device
 * @return maximum supported throughput in Mbps (not MBps)
 */
int wdi_out_cfg_max_thruput_mbps(ol_pdev_handle pdev);

/**
 * @brief Specify the maximum number of fragments per tx network buffer.
 * @details
 *  Specify the maximum number of fragments that a tx frame provided to
 *  the WLAN driver by the OS may contain.
 *  In LL systems, the host data SW uses this maximum fragment count to
 *  determine how many elements to allocate in the fragmentation descriptor
 *  it creates to specify to the tx MAC DMA where to locate the tx frame's
 *  data.
 *  This maximum fragments count is only for regular frames, not TSO frames,
 *  since TSO frames are sent in segments with a limited number of fragments
 *  per segment.
 *
 * @param pdev - handle to the physical device
 * @return maximum number of fragments that can occur in a regular tx frame
 */
int wdi_out_cfg_netbuf_frags_max(ol_pdev_handle pdev);

/**
 * @brief For HL systems, specify when to free tx frames.
 * @details
 *  In LL systems, the host's tx frame is referenced by the MAC DMA, and
 *  thus cannot be freed until the target indicates that it is finished
 *  transmitting the frame.
 *  In HL systems, the entire tx frame is downloaded to the target.
 *  Consequently, the target has its own copy of the tx frame, and the
 *  host can free the tx frame as soon as the download completes.
 *  Alternatively, the HL host can keep the frame allocated until the
 *  target explicitly tells the HL host it is done transmitting the frame.
 *  This gives the target the option of discarding its copy of the tx
 *  frame, and then later getting a new copy from the host.
 *  This function tells the host whether it should retain its copy of the
 *  transmit frames until the target explicitly indicates it is finished
 *  transmitting them, or if it should free its copy as soon as the
 *  tx frame is downloaded to the target.
 *  
 * @param pdev - handle to the physical device
 * @return
 *      0 -> retain the tx frame until the target indicates it is done
 *          transmitting the frame
 *      -OR-
 *      1 -> free the tx frame as soon as the download completes
 */
int wdi_out_cfg_tx_free_at_download(ol_pdev_handle pdev);

/**
 * @brief In a HL system, specify the target initial credit count.
 * @details
 *  The HL host tx data SW includes a module for determining which tx frames
 *  to download to the target at a given time.
 *  To make this judgement, the HL tx download scheduler has to know
 *  how many buffers the HL target has available to hold tx frames.
 *  Due to the possibility that a single target buffer pool can be shared
 *  between rx and tx frames, the host may not be able to obtain a precise
 *  specification of the tx buffer space available in the target, but it
 *  uses the best estimate, as provided by this configuration function,
 *  to determine how best to schedule the tx frame downloads.
 *
 * @param pdev - handle to the physical device
 * @return the number of tx buffers available in a HL target
 */
int wdi_out_cfg_target_tx_credit(ol_pdev_handle pdev);

/**
 * @brief Specify the LL tx MSDU header download size.
 * @details
 *  In LL systems, determine how many bytes from a tx frame to download,
 *  in order to provide the target FW's Descriptor Engine with enough of
 *  the packet's payload to interpret what kind of traffic this is,
 *  and who it is for.
 *  This download size specification does not include the 802.3 / 802.11
 *  frame encapsulation headers; it starts with the encapsulated IP packet
 *  (or whatever ethertype is carried within the ethernet-ish frame).
 *  The LL host data SW will determine how many bytes of the MSDU header to
 *  download by adding this download size specification to the size of the
 *  frame header format specified by the wdi_out_cfg_frame_type configuration
 *  function.
 *
 * @param pdev - handle to the physical device
 * @return the number of bytes beyond the 802.3 or native WiFi header to
 *      download to the target for tx classification
 */
int wdi_out_cfg_tx_download_size(ol_pdev_handle pdev);

//#include <a_osapi.h>      /* u_int8_t */
#include <osdep.h>        /* u_int8_t */
#include <qdf_nbuf.h>     /* qdf_nbuf_t */

enum ol_rx_err_type {
    OL_RX_ERR_DEFRAG_MIC,
    OL_RX_ERR_PN,
    OL_RX_ERR_UNKNOWN_PEER,
    OL_RX_ERR_MALFORMED,
};

/**
 * @brief Provide notification of failure during host rx processing
 * @details
 *  Indicate an error during host rx data processing, including what
 *  kind of error happened, when it happened, which peer and TID the
 *  erroneous rx frame is from, and what the erroneous rx frame itself
 *  is.
 *
 * @param vdev_id - ID of the virtual device received the erroneous rx frame
 * @param peer_mac_addr - MAC address of the peer that sent the erroneous
 *      rx frame
 * @param tid - which TID within the peer sent the erroneous rx frame
 * @param tsf32  - the timstamp in TSF units of the erroneous rx frame, or
 *      one of the fragments that when reassembled, constitute the rx frame
 * @param err_type - what kind of error occurred
 * @param rx_frame - the rx frame that had an error
 */
void
wdi_out_rx_err(
    u_int8_t vdev_id,
    u_int8_t *peer_mac_addr,
    int tid,
    u_int32_t tsf32,
    enum ol_rx_err_type err_type,
    qdf_nbuf_t rx_frame);

enum ol_rx_notify_type {
    OL_RX_NOTIFY_IPV4_IGMP,
};

/**
 * @brief Provide notification of reception of data of special interest.
 * @details
 *  Indicate when "special" data has been received.  The nature of the
 *  data that results in it being considered special is specified in the
 *  notify_type argument.
 *  This function is currently used by the data-path SW to notify the
 *  control path SW when the following types of rx data are received:
 *    + IPv4 IGMP frames
 *      The control SW can use these to learn about multicast group
 *      membership, if it so chooses.
 *
 * @param pdev - handle to the ctrl SW's physical device object
 * @param vdev_id - ID of the virtual device received the special data
 * @param peer_mac_addr - MAC address of the peer that sent the special data
 * @param tid - which TID within the peer sent the special data
 * @param tsf32  - the timstamp in TSF units of the special data
 * @param notify_type - what kind of special data was received
 * @param rx_frame - the rx frame containing the special data
 */
void
wdi_out_rx_notify(
    ol_pdev_handle pdev,
    u_int8_t vdev_id,
    u_int8_t *peer_mac_addr,
    int tid,
    u_int32_t tsf32,
    enum ol_rx_notify_type notify_type,
    qdf_nbuf_t rx_frame);

/**
 * @brief Indicate when a paused STA has tx data available.
 * @details
 *  Indicate to the control SW when a paused peer that previously
 *  has all its peer-TID queues empty gets a MSDU to transmit.
 *  Conversely, indicate when a paused peer that had data in one or more of
 *  its peer-TID queues has all queued data removed (e.g. due to a U-APSD
 *  triggered transmission), but is still paused.
 *  It is up to the control SW to determine whether the peer is paused due to
 *  being in power-save sleep, or some other reason, and thus whether it is
 *  necessary to set the TIM in beacons to notify a sleeping STA that it has
 *  data.
 *  The data SW will also issue this wdi_out_tx_paused_peer_data call when an
 *  unpaused peer that currently has tx data in one or more of its
 *  peer-TID queues becomes paused.
 *  The data SW will not issue this wdi_out_tx_paused_peer_data call when a
 *  peer with data in one or more of its peer-TID queues becomes unpaused.
 *
 * @param peer - the paused peer
 * @param has_tx_data -
 *      1 -> a paused peer that previously had no tx data now does, -OR-
 *      0 -> a paused peer that previously had tx data now doesnt
 */
void
wdi_out_tx_paused_peer_data(ol_peer_handle peer, int has_tx_data);

#else
/* WDI_API_AS_MACROS */

#include <ol_cfg.h>

#define wdi_out_cfg_is_high_latency ol_cfg_is_high_latency
#define wdi_out_cfg_max_peer_id ol_cfg_max_peer_id
#define wdi_out_cfg_max_vdevs ol_cfg_max_vdevs
#define wdi_out_cfg_rx_pn_check ol_cfg_rx_pn_check
#define wdi_out_cfg_rx_fwd_check ol_cfg_rx_fwd_check
#define wdi_out_cfg_rx_fwd_inter_bss ol_cfg_rx_fwd_inter_bss
#define wdi_out_cfg_frame_type ol_cfg_frame_type
#define wdi_out_cfg_max_thruput_mbps ol_cfg_max_thruput_mbps
#define wdi_out_cfg_netbuf_frags_max ol_cfg_netbuf_frags_max
#define wdi_out_cfg_tx_free_at_download ol_cfg_tx_free_at_download
#define wdi_out_cfg_tx_download_size ol_cfg_tx_download_size

#include <ol_ctrl_txrx_api.h>

#define wdi_out_rx_err ol_rx_err
#define wdi_out_rx_notify ol_rx_notify
#define wdi_out_tx_paused_peer_data ol_tx_paused_peer_data

#endif /* WDI_API_AS_FUNCS / MACROS */

#endif /* _WDI_OUT__H_ */

/**@}*/

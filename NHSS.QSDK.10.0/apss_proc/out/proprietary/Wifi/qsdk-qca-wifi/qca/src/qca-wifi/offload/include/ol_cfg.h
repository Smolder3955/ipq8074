/*
 * Copyright (c) 2011-2014, 2017-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 *
 * 2011-2014 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#ifndef _OL_CFG__H_
#define _OL_CFG__H_

#include <qdf_types.h> /* u_int32_t */
#include <ol_if_txrx_handles.h>
#include <cdp_txrx_cmn_struct.h>
#include <ieee80211.h>    /* ieee80211_qosframe_htc_addr4 */
#include <enet.h>         /* LLC_SNAP_HDR_LEN */
#include <ol_cfg_raw.h>   /* Raw Mode specific configuration defines */

#if defined(CONFIG_HL_SUPPORT)
#include "wlan_tgt_def_config_hl.h"
#else
#include "wlan_tgt_def_config.h"
#endif

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
int ol_cfg_is_high_latency(ol_soc_handle psoc);

/**
 *@brief Specify hardware CCE component is enable or disable
 *@details
 * Specify CCE enable diable in harwdare based on the parameter
 * This is used by host to determine whether to do
 * cce host classification or not.
 *
 * @param soc -handle to the soc
 * @return - 0(cce enabled) 1 (cce diable)
 */

int ol_cfg_is_cce_disable(ol_soc_handle soc);

/**
 * @brief Specify the range of peer IDs.
 * @details
 *  Specify the maximum peer ID.  This is the maximum number of peers,
 *  minus one.
 *  This is used by the host to determine the size of arrays indexed by
 *  peer ID.
 *
 * @param soc - handle to the soc
 * @return maximum peer ID
 */
int ol_cfg_max_peer_id(ol_soc_handle soc);

/**
 * @brief Specify the max number of virtual devices within a physical device.
 * @details
 *  Specify how many virtual devices may exist within a physical device.
 *
 * @param pdev - handle to the physical device
 * @return maximum number of virtual devices
 */
int ol_cfg_max_vdevs(ol_pdev_handle pdev);

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
int ol_cfg_rx_pn_check(ol_pdev_handle pdev);


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
int ol_cfg_rx_fwd_check(ol_pdev_handle pdev);

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
int ol_cfg_rx_fwd_inter_bss(ol_pdev_handle pdev);

/**
 * @brief format of data frames delivered to/from the WLAN driver by/to the OS
 * @details
 *  Note: This must correspond to the ordering in the htt_pkt_type
 *  enumeration.
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
enum wlan_frm_fmt ol_cfg_frame_type(ol_pdev_handle pdev);

/** * @brief Specify data frame format used by the OS.
 * @details *  Specify what type of frame
 * (802.3 or native WiFi or RAW) the host data SW
 *  should expect from and provide to the OS shim
 * during init. This can be changed to different modes in Tx and Rx at runtime.
 * @param
 * pdev - handle to the physical device * @return enumerated data frame format
 */
enum htt_cmn_pkt_type ol_cfg_pkt_type(ol_pdev_handle pdev);

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
int ol_cfg_max_thruput_mbps(ol_soc_handle psoc);


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
int ol_cfg_netbuf_frags_max(ol_soc_handle psoc);


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
 * @param psoc - handle to the physical device
 * @return
 *      0 -> retain the tx frame until the target indicates it is done
 *          transmitting the frame
 *      -OR-
 *      1 -> free the tx frame as soon as the download completes
 */
int ol_cfg_tx_free_at_download(ol_soc_handle psoc);

/**
 * @brief In a HL system, specify the target initial credit count.
 * @details
 * The HL host tx data SW includes a module for determining which tx frames
 * to download to the target at a given time.
 * To make this judgement, the HL tx download scheduler has to know
 * how many buffers the HL target has available to hold tx frames.
 * Due to the possibility that a single target buffer pool can be shared
 * between rx and tx frames, the host may not be able to obtain a precise
 * specification of the tx buffer space available in the target, but it
 * uses the best estimate, as provided by this configuration function,
 * to determine how best to schedule the tx frame downloads.
 *
 * @param soc - handle to soc
 * @return the number of tx buffers available in a HL target
 */

int ol_cfg_target_tx_credit(ol_soc_handle ol_soc);

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
 *  frame header format specified by the ol_cfg_frame_type configuration
 *  function.
 *
 * @param pdev - handle to the physical device
 * @return the number of bytes beyond the 802.3 or native WiFi header to
 *      download to the target for tx classification
 */
int ol_cfg_tx_download_size(ol_pdev_handle pdev);

/**
 * brief Specify where defrag timeout and duplicate detection is handled
 * @details
 *   non-aggregate duplicate detection and timing out stale fragments
 *   requires additional target memory. To reach max client
 *   configurations (128+), non-aggregate duplicate detection and the
 *   logic to time out stale fragments is moved to the host.
 *
 * @param pdev - handle to the physical device
 * @return
 *  0 -> target is responsible non-aggregate duplicate detection and
 *          timing out stale fragments.
 *
 *  1 -> host is responsible non-aggregate duplicate detection and
 *          timing out stale fragments.
 */
int ol_cfg_rx_host_defrag_timeout_duplicate_check(ol_pdev_handle pdev);

#ifdef QCA_LOWMEM_PLATFORM
/**
 * @brief OL_CFG_RX_RING_SIZE_MIN - minimum RX ring size on host
 * @details
 *   slightly larger than one large A-MPDU
 */
#define OL_CFG_RX_RING_SIZE_MIN 128

/**
 * @brief OL_CFG_RX_RING_SIZE_MAX - maximum RX ring size on host
 * @details
 *    roughly 20 ms @ 1 Gbps of 1500B MSDUs
 */
#define OL_CFG_RX_RING_SIZE_MAX 1024
#else
/**
 * @brief OL_CFG_RX_RING_SIZE_MIN - minimum RX ring size on host
 * @details
 *   slightly larger than one large A-MPDU
 */
#define OL_CFG_RX_RING_SIZE_MIN 128

/**
 * @brief OL_CFG_RX_RING_SIZE_MAX - maximum RX ring size on host
 * @details
 *    roughly 20 ms @ 1 Gbps of 1500B MSDUs
 */
#define OL_CFG_RX_RING_SIZE_MAX 2048
#endif

#endif /* _OL_CFG__H_ */

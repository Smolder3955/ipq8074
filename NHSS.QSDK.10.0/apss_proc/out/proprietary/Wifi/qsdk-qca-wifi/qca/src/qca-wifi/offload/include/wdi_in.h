/*
 * Copyright (c) 2012, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2012 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/**
 *   @addtogroup WDIAPI
 *@{
 */
/**
 * @file wdi_in.h
 * @brief Datapath functions called by SW outside the WDI boundary
 */
#ifndef _WDI_IN__H_
#define _WDI_IN__H_

#include <wdi_types.h>

#ifdef WDI_API_AS_FUNCS

#include <athdefs.h>      /* A_STATUS */
#include <qdf_nbuf.h>     /* qdf_nbuf_t */
#include <qdf_types.h> /* qdf_device_t */
#include <htc_api.h>      /* HTC_HANDLE */

#include <wlan_defs.h>   /* MAX_SPATIAL_STREAM */

/**
 * @brief Set up the data SW subsystem.
 * @details
 *  As part of the WLAN device attach, the data SW subsystem has
 *  to be attached as a component within the WLAN device.
 *  This attach allocates and initializes the physical device object
 *  used by the data SW.
 *  The data SW subsystem attach needs to happen after the target has
 *  be started, and host / target parameter negotiation has completed,
 *  since the host data SW uses some of these host/target negotiated
 *  parameters (e.g. peer ID range) during the initializations within
 *  its attach function.
 *  However, the host data SW is not allowed to send HTC messages to the
 *  target within this pdev_attach function call, since the HTC setup
 *  has not complete at this stage of initializations.  Any messaging
 *  to the target has to be done in the separate pdev_attach_target call
 *  that is invoked after HTC setup is complete.
 *
 * @param ctrl_pdev - control SW's physical device handle, needed as an
 *      argument for dynamic configuration queries
 * @param htc_pdev - the HTC physical device handle.  This is not needed
 *      by the txrx module, but needs to be passed along to the HTT module.
 * @param osdev - OS handle needed as an argument for some OS primitives
 * @return the data physical device object
 */
ol_txrx_pdev_handle
wdi_in_pdev_attach(
    ol_pdev_handle ctrl_pdev,
    HTC_HANDLE htc_pdev,
    qdf_device_t osdev);

/**
 * @brief Do final steps of data SW setup that send messages to the target.
 * @details
 *  The majority of the data SW setup are done by the pdev_attach function,
 *  but this function completes the data SW setup by sending datapath
 *  configuration messages to the target.
 *
 * @param data_pdev - the physical device being initialized
 */
A_STATUS
wdi_in_pdev_attach_target(ol_txrx_pdev_handle data_pdev);

/**
 * @brief Wrapper for rate-control context initialization
 * @details
 *  Enables the switch that controls the allocation of the
 *  rate-control contexts on the host.
 *
 * @param pdev   - the physical device being initialized
 * @param enable - 1: enabled 0: disabled
 */
void wdi_in_enable_host_ratectrl(ol_txrx_pdev_handle pdev, u_int32_t enable);

/**
 * @brief modes that a virtual device can operate as
 * @details
 *  A virtual device can operate as an AP, an IBSS, or a STA (client).
 *  or in monitor mode
 */
enum wlan_op_mode {
   wlan_op_mode_unknown,
   wlan_op_mode_ap,
   wlan_op_mode_ibss,
   wlan_op_mode_sta,
   wlan_op_mode_monitor,
};

/**
 * @brief Allocate and initialize the data object for a new virtual device.
 * @param data_pdev - the physical device the virtual device belongs to
 * @param vdev_mac_addr - the MAC address of the virtual device
 * @param vdev_id - the ID used to identify the virtual device to the target
 * @param op_mode - whether this virtual device is operating as an AP,
 *      an IBSS, or a STA
 * @return
 *      success: handle to new data vdev object, -OR-
 *      failure: NULL
 */
ol_txrx_vdev_handle
wdi_in_vdev_attach(
    ol_txrx_pdev_handle data_pdev,
    u_int8_t *vdev_mac_addr,
    u_int8_t vdev_id,
    enum wlan_op_mode op_mode);

/**
 * @brief Allocate and set up references for a data peer object.
 * @details
 *  When an association with a peer starts, the host's control SW
 *  uses this function to inform the host data SW.
 *  The host data SW allocates its own peer object, and stores a
 *  reference to the control peer object within the data peer object.
 *  The host data SW also stores a reference to the virtual device
 *  that the peer is associated with.  This virtual device handle is
 *  used when the data SW delivers rx data frames to the OS shim layer.
 *  The host data SW returns a handle to the new peer data object,
 *  so a reference within the control peer object can be set to the
 *  data peer object.
 *
 * @param data_pdev - data physical device object that will indirectly
 *      own the data_peer object
 * @param data_vdev - data virtual device object that will directly
 *      own the data_peer object
 * @param peer_mac_addr - MAC address of the new peer
 * @return handle to new data peer object, or NULL if the attach fails
 */
ol_txrx_peer_handle
wdi_in_peer_attach(
    ol_txrx_pdev_handle data_pdev,
    ol_txrx_vdev_handle data_vdev,
    u_int8_t *peer_mac_addr);

/**
 * @brief Template for passing ieee80211_node members to rate-control
 * @details
 *  This structure is used in order to maintain the isolation between umac and
 *  ol while initializing the peer-level rate-control context with peer-specific
 *  parameters.
 */
struct peer_ratectrl_params_t {
    u_int8_t ni_streams;
    u_int8_t is_auth_wpa;
    u_int8_t is_auth_wpa2;
    u_int8_t is_auth_8021x;
    u_int32_t ni_flags;
    u_int32_t ni_chwidth;
    u_int16_t ni_htcap;
    u_int16_t ni_vhtcap;
    u_int16_t ni_phymode;
    u_int16_t ni_rx_vhtrates;
    u_int8_t ht_rates[MAX_SPATIAL_STREAM * 8];
};

/**
 * @brief Update the data peer object at association time
 * @details
 *  For the host-based implementation of rate-control, it
 *  updates the peer/node-related parameters within rate-control
 *  context of the peer at association.
 *
 * @param peer - pointer to the node's object
 * @param peer_ratectrl_params - peer params in peer-level ratecontrol context
 *
 * @return none
 */
void
wdi_in_peer_update(struct ol_txrx_peer_t *peer,
        struct peer_ratectrl_params_t *peer_ratectrl_params);

#if WDS_VENDOR_EXTENSTION
/**
 * @brief Update the data peer object at association time with wds tx policy
 * @details
 *  update the wds tx policy of peer/node at association.
 *
 * @param peer - pointer to the node's object
 * @param wds_tx_policy bit mask - 0 - disable wds vendor extension tx policy
 *                                b1 - use 4-addr ucast frames
 *                                b2 - use 4-addr mcast frames
 *
 * @return none
 */
void wdi_in_peer_wds_tx_policy_update (struct cdp_peer *peer, int wds_tx_policy)
#endif

/**
 * @brief Notify tx data SW that a peer's transmissions are suspended.
 * @details
 *  This function applies only to HL systems - in LL systems, tx flow control
 *  is handled entirely within the target FW.
 *  The HL host tx data SW is doing tx classification and tx download
 *  scheduling, and therefore also needs to actively participate in tx
 *  flow control.  Specifically, the HL tx data SW needs to check whether a
 *  given peer is available to transmit to, or is paused.
 *  This function is used to tell the HL tx data SW when a peer is paused,
 *  so the host tx data SW can hold the tx frames for that SW.
 *
 * @param data_peer - which peer is being paused
 */
void
wdi_in_peer_pause(ol_txrx_peer_handle data_peer);

/**
 * @brief Notify tx data SW that a peer-TID is ready to transmit to.
 * @details
 *  This function applies only to HL systems - in LL systems, tx flow control
 *  is handled entirely within the target FW.
 *  If a peer-TID has tx paused, then the tx datapath will end up queuing
 *  any tx frames that arrive from the OS shim for that peer-TID.
 *  In a HL system, the host tx data SW itself will classify the tx frame,
 *  and determine that it needs to be queued rather than downloaded to the
 *  target for transmission.
 *  Once the peer-TID is ready to accept data, the host control SW will call
 *  this function to notify the host data SW that the queued frames can be
 *  enabled for transmission, or specifically to download the tx frames
 *  to the target to transmit.
 *  The TID parameter is an extended version of the QoS TID.  Values 0-15
 *  indicate a regular QoS TID, and the value 16 indicates either non-QoS
 *  data, multicast data, or broadcast data.
 *
 * @param data_peer - which peer is being unpaused
 * @param tid - which TID within the peer is being unpaused, or -1 as a
 *      wildcard to unpause all TIDs within the peer
 */
void
wdi_in_peer_tid_unpause(ol_txrx_peer_handle data_peer, int tid);

/**
 * @brief Tell a paused peer to release a specified number of tx frames.
 * @details
 *  This function applies only to HL systems - in LL systems, tx flow control
 *  is handled entirely within the target FW.
 *  Download up to a specified maximum number of tx frames from the tx
 *  queues of the specified TIDs within the specified paused peer, usually
 *  in response to a U-APSD trigger from the peer.
 *  It is up to the host data SW to determine how to choose frames from the
 *  tx queues of the specified TIDs.  However, the host data SW does need to
 *  provide long-term fairness across the U-APSD enabled TIDs.
 *  The host data SW will notify the target data FW when it is done downloading
 *  the batch of U-APSD triggered tx frames, so the target data FW can
 *  differentiate between an in-progress download versus a case when there are
 *  fewer tx frames available than the specified limit.
 *  This function is relevant primarily to HL U-APSD, where the frames are
 *  held in the host.
 *
 * @param peer - which peer sent the U-APSD trigger
 * @param tid_mask - bitmask of U-APSD enabled TIDs from whose tx queues
 *      tx frames can be released
 * @param max_frms - limit on the number of tx frames to release from the
 *      specified TID's queues within the specified peer
 */
void
wdi_in_tx_release(
    ol_txrx_peer_handle peer,
    u_int32_t tid_mask,
    int max_frms);

/**
 * @brief Suspend all tx data for the specified virtual device.
 * @details
 *  This function applies only to HL systems - in LL systems, tx flow control
 *  is handled entirely within the target FW.
 *  As an example, this function could be used when a single-channel physical
 *  device supports multiple channels by jumping back and forth between the
 *  channels in a time-shared manner.  As the device is switched from channel
 *  A to channel B, the virtual devices that operate on channel A will be
 *  paused.
 *
 * @param data_vdev - the virtual device being paused
 */
void
wdi_in_vdev_pause(ol_txrx_vdev_handle data_vdev);

/**
 * @brief Resume tx for the specified virtual device.
 * @details
 *  This function applies only to HL systems - in LL systems, tx flow control
 *  is handled entirely within the target FW.
 *
 * @param data_vdev - the virtual device being unpaused
 */
void
wdi_in_vdev_unpause(ol_txrx_vdev_handle data_vdev);

/**
 * @brief Suspend all tx data for the specified physical device.
 * @details
 *  This function applies only to HL systems - in LL systems, tx flow control
 *  is handled entirely within the target FW.
 *  In some systems it is necessary to be able to temporarily
 *  suspend all WLAN traffic, e.g. to allow another device such as bluetooth
 *  to temporarily have exclusive access to shared RF chain resources.
 *  This function suspends tx traffic within the specified physical device.
 *
 * @param data_pdev - the physical device being paused
 */
void
wdi_in_pdev_pause(ol_txrx_pdev_handle data_pdev);

/**
 * @brief Resume tx for the specified physical device.
 * @details
 *  This function applies only to HL systems - in LL systems, tx flow control
 *  is handled entirely within the target FW.
 *
 * @param data_pdev - the physical device being unpaused
 */
void
wdi_in_pdev_unpause(ol_txrx_pdev_handle data_pdev);

/**
 * @brief Synchronize the data-path tx with a control-path target download
 * @dtails
 * @param data_pdev - the data-path physical device object
 * @param sync_cnt - after the host data-path SW downloads this sync request
 *      to the target data-path FW, the target tx data-path will hold itself
 *      in suspension until it is given an out-of-band sync counter value that
 *      is equal to or greater than this counter value
 */
void
wdi_in_tx_sync(ol_txrx_pdev_handle data_pdev, u_int8_t sync_cnt);

/**
 * @brief Delete a peer's data object.
 * @details
 *  When the host's control SW disassociates a peer, it calls this
 *  function to delete the peer's data object.
 *  The reference stored in the control peer object to the data peer
 *  object (set up by a call to ol_peer_store()) is provided.
 *
 * @param data_peer - the object to delete
 */
void
wdi_in_peer_detach(ol_txrx_peer_handle data_peer);

typedef void (*ol_txrx_vdev_delete_cb)(void *context);

/**
 * @brief Deallocate the specified data virtual device object.
 * @details
 *  All peers associated with the virtual device need to be deleted
 *  (wdi_in_peer_detach) before the virtual device itself is deleted.
 *  However, for the peers to be fully deleted, the peer deletion has to
 *  percolate through the target data FW and back up to the host data SW.
 *  Thus, even though the host control SW may have issued a peer_detach
 *  call for each of the vdev's peers, the peer objects may still be
 *  allocated, pending removal of all references to them by the target FW.
 *  In this case, though the vdev_detach function call will still return
 *  immediately, the vdev itself won't actually be deleted, until the
 *  deletions of all its peers complete.
 *  The caller can provide a callback function pointer to be notified when
 *  the vdev deletion actually happens - whether it's directly within the
 *  vdev_detach call, or if it's deferred until all in-progress peer
 *  deletions have completed.
 *
 * @param data_vdev - data object for the virtual device in question
 * @param callback - function to call (if non-NULL) once the vdev has
 *      been wholly deleted
 * @param callback_context - context to provide in the callback
 */
void
wdi_in_vdev_detach(
    ol_txrx_vdev_handle data_vdev,
    ol_txrx_vdev_delete_cb callback,
    void *callback_context);

/**
 * @brief Delete the data SW state.
 * @details
 *  This function is used when the WLAN driver is being removed to
 *  remove the host data component within the driver.
 *  All virtual devices within the physical device need to be deleted
 *  (wdi_in_vdev_detach) before the physical device itself is deleted.
 *
 * @param data_pdev - the data physical device object being removed
 * @param force - delete the pdev (and its vdevs and peers) even if there
 *      are outstanding references by the target to the vdevs and peers
 *      within the pdev
 */
void
wdi_in_pdev_detach(ol_txrx_pdev_handle data_pdev, int force);

typedef void
(*ol_txrx_mgmt_tx_cb)(void *ctxt, qdf_nbuf_t tx_mgmt_frm, int had_error);

/**
 * @brief Store a callback for delivery notifications for managements frames.
 * @details
 *  When the txrx SW receives notifications from the target that a tx frame
 *  has been delivered to its recipient, it will check if the tx frame
 *  is a management frame.  If so, the txrx SW will check the management
 *  frame type specified when the frame was submitted for transmission.
 *  If there is a callback function registered for the type of managment
 *  frame in question, the txrx code will invoke the callback to inform
 *  the management + control SW that the mgmt frame was delivered.
 *  This function is used by the control SW to store a callback pointer
 *  for a given type of management frame.
 *
 * @param pdev - the data physical device object
 * @param type - the type of mgmt frame the callback is used for
 * @param cb - the callback for delivery notification
 * @param ctxt - context to use with the callback
 */
void
wdi_in_mgmt_tx_cb_set(
    ol_txrx_pdev_handle pdev,
    u_int8_t type,
    ol_txrx_mgmt_tx_cb cb,
    void *ctxt);

/**
 * @brief Transmit a management frame.
 * @details
 *  Send the specified management frame from the specified virtual device.
 *  The type is used for determining whether to invoke a callback to inform
 *  the sender that the tx mgmt frame was delivered, and if so, which
 *  callback to use.
 *
 * @param vdev - virtual device transmitting the frame
 * @param tx_mgmt_frm - management frame to transmit
 * @param type - the type of managment frame (determines what callback to use)
 * @return
 *      0 -> the frame is accepted for transmission, -OR-
 *      1 -> the frame was not accepted
 */
int
wdi_in_mgmt_send(
    ol_txrx_vdev_handle vdev,
    qdf_nbuf_t tx_mgmt_frm,
    u_int8_t type);

/**
 * @brief Setup the monitor mode vap (vdev) for this pdev
 * @details
 *  When a non-NULL vdev handle is registered as the monitor mode vdev, all
 *  packets received by the system are delivered to the OS stack on this
 *  interface in 802.11 MPDU format. Only a single monitor mode interface
 *  can be up at any timer. When the vdev handle is set to NULL the monitor
 *  mode delivery is stopped. This handle may either be a unique vdev
 *  object that only receives monitor mode packets OR a point to a a vdev
 *  object that also receives non-monitor traffic. In the second case the
 *  OS stack is responsible for delivering the two streams using approprate
 *  OS APIs
 *
 * @param pdev - the data physical device object
 * @param vdev - the data virtual device object to deliver monitor mode
 *                  packets on
 * @return
 *       0 -> the monitor mode vap was sucessfully setup
 *      -1 -> Unable to setup monitor mode
 */
int
wdi_in_set_monitor_mode_vap(
    ol_txrx_pdev_handle pdev,
    ol_txrx_vdev_handle vdev);

/**
 * @brief Setup the current operating channel of the device
 * @details
 *  Mainly used when populating monitor mode status that requires the
 *  current operating channel
 *
 * @param pdev - the data physical device object
 * @param chan_mhz - the channel frequency (mhz)
 *                  packets on
 * @return - void
 */
void
wdi_in_set_curchan(
    ol_txrx_pdev_handle pdev,
    u_int32_t chan_mhz);

/**
 * @brief Get the number of pending transmit frames that are awaiting completion.
 * @details
 *  Mainly used in clean up path to make sure all buffers have been free'ed
 *
 * @param pdev - the data physical device object
 * @return - count of pending frames
 */
int
wdi_in_get_tx_pending(
    ol_txrx_pdev_handle pdev);

/**
 * @typedef ol_txrx_tx_fp
 * @brief top-level transmit function
 */
typedef qdf_nbuf_t (*ol_txrx_tx_fp)(
    ol_txrx_vdev_handle data_vdev, qdf_nbuf_t msdu_list);

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
};

#define OL_TXRX_OSIF_TX_EXT_TID_NON_QOS_MCAST_BCAST 16
#define OL_TXRX_OSIF_TX_EXT_TID_MGMT                17
#define OL_TXRX_OSIF_TX_EXT_TID_INVALID             31

/**
 * @typedef ol_txrx_tx_non_std_fp
 * @brief top-level transmit function for non-standard tx frames
 * @details
 *  This function pointer provides an alternative to the ol_txrx_tx_fp
 *  to support non-standard transmits.  In particular, this function
 *  supports transmission of:
 *  1. "Raw" frames
 *     These raw frames already have an 802.11 header; the usual
 *     802.11 header encapsulation by the driver does not apply.
 *  2. TSO segments
 *     During tx completion, the txrx layer needs to reclaim the buffer
 *     that holds the ethernet/IP/TCP header created for the TSO segment.
 *     Thus, these tx frames need to be marked as TSO, to show that they
 *     need this special handling during tx completion.
 *  3. Out-of-band priority frames
 *     In the standard case, the frame's traffic type / priority (TID) is
 *     inferred from fields within the IP header, such as the IPv4 header's
 *     ToS/DSCP byte.  The OS shim can override this standard behavior by
 *     directly specifying the TID to use when queuing, aggregating, and
 *     transmitting the tx frame.
 *
 * @param data_vdev - which virtual device should transmit the frame
 * @param ext_tid - The frame's traffic type / priority.
 *      This value is an extension of the 4-bit 802.11 TID.
 *      Extension values can specify that a frame is a non-QoS or multicast
 *      data frame, or a management/control frame.
 *      To avoid explicitly specifying the TID, and instead have the WLAN
 *      driver infer the TID as usual from the IP header, specify the
 *      OL_TXRX_OSIF_TX_EXT_TID_INVALID value.
 * @param tx_spec - what non-standard operations to apply to the tx frame
 * @param msdu_list - tx frame(s), in a null-terminated list
 */
typedef qdf_nbuf_t (*ol_txrx_tx_non_std_fp)(
    ol_txrx_vdev_handle data_vdev,
    u_int8_t ext_tid,
    enum ol_txrx_osif_tx_spec tx_spec,
    qdf_nbuf_t msdu_list);

#if UMAC_SUPPORT_PROXY_ARP
typedef int (*ol_txrx_proxy_arp_fp)(ol_osif_vdev_handle vdev, qdf_nbuf_t netbuf);
#endif

/**
 * @typedef ol_txrx_rx_fp
 * @brief receive function to hand batches of data frames from txrx to OS shim
 */
typedef void (*ol_txrx_rx_fp)(ol_osif_vdev_handle vdev, qdf_nbuf_t msdus);
#if ATH_SUPPORT_WAPI
typedef bool (*ol_rx_check_wai_fp)(ol_osif_vdev_handle vdev, qdf_nbuf_t mpdu_head, qdf_nbuf_t mpdu_tail);
#endif

/**
 * @typedef ol_txrx_rx_mon_fp
 * @brief OSIF monitor mode receive function for single MPDU (802.11 format)
 */
typedef void (*ol_txrx_rx_mon_fp)(
    ol_osif_vdev_handle vdev,
    qdf_nbuf_t mpdu,
    void *rx_status);

struct ol_txrx_osif_ops {
    /* tx function pointers - specified by txrx, stored by OS shim */
    struct {
        ol_txrx_tx_fp         std;
        ol_txrx_tx_non_std_fp non_std;
    } tx;

    /* rx function pointers - specified by OS shim, stored by txrx */
    struct {
        ol_txrx_rx_fp         std;
#if ATH_SUPPORT_WAPI
        ol_rx_check_wai_fp    wai_check;
#endif
        ol_txrx_rx_mon_fp     mon;
    } rx;

#if UMAC_SUPPORT_PROXY_ARP
    /* proxy arp function pointer - specified by OS shim, stored by txrx */
    ol_txrx_proxy_arp_fp      proxy_arp;
#endif
};

/**
 * @brief Link a vdev's data object with the matching OS shim vdev object.
 * @details
 *  The data object for a virtual device is created by the function
 *  cdp_vdev_attach.  However, rather than fully linking the
 *  data vdev object with the vdev objects from the other subsystems
 *  that the data vdev object interacts with, the txrx_vdev_attach
 *  function focuses primarily on creating the data vdev object.
 *  After the creation of both the data vdev object and the OS shim
 *  vdev object, this txrx_osif_vdev_attach function is used to connect
 *  the two vdev objects, so the data SW can use the OS shim vdev handle
 *  when passing rx data received by a vdev up to the OS shim.
 *
 * @param txrx_vdev - the virtual device's data object
 * @param osif_vdev - the virtual device's OS shim object
 * @param txrx_ops - (pointers to) the functions used for tx and rx data xfer
 *      There are two portions of these txrx operations.
 *      The rx portion is filled in by OSIF SW before calling
 *      wdi_in_osif_vdev_register; inside the wdi_in_osif_vdev_register
 *      the txrx SW stores a copy of these rx function pointers, to use
 *      as it delivers rx data frames to the OSIF SW.
 *      The tx portion is filled in by the txrx SW inside
 *      wdi_in_osif_vdev_register; when the function call returns,
 *      the OSIF SW stores a copy of these tx functions to use as it
 *      delivers tx data frames to the txrx SW.
 *      The rx function pointer inputs consist of the following:
 *      rx: the OS shim rx function to deliver rx data frames to.
 *          This can have different values for different virtual devices,
 *          e.g. so one virtual device's OS shim directly hands rx frames to
 *          the OS, but another virtual device's OS shim filters out P2P
 *          messages before sending the rx frames to the OS.
 *          The netbufs delivered to the osif_rx function are in the format
 *          specified by the OS to use for tx and rx frames (either 802.3 or
 *          native WiFi).
 *      rx_mon: the OS shim rx monitor function to deliver monitor data to
 *          Though in practice, it is probable that the same function will
 *          be used for delivering rx monitor data for all virtual devices,
 *          in theory each different virtual device can have a different
 *          OS shim function for accepting rx monitor data.
 *          The netbufs delivered to the osif_rx_mon function are in 802.11
 *          format.  Each netbuf holds a 802.11 MPDU, not an 802.11 MSDU.
 *          Depending on compile-time configuration, each netbuf may also
 *          have a monitor-mode encapsulation header such as a radiotap
 *          header added before the MPDU contents.
 *      The tx function pointer outputs consist of the following:
 *      tx: the tx function pointer for standard data frames
 *          This function pointer is set by the txrx SW to perform
 *          host-side transmit operations based on whether a HL or LL
 *          host/target interface is in use.
 *      tx_non_std: the tx function pointer for non-standard data frames,
 *          such as TSO frames, explicitly-prioritized frames, or "raw"
 *          frames which skip some of the tx operations, such as 802.11
 *          MAC header encapsulation.
 */
void
wdi_in_osif_vdev_register(
    ol_txrx_vdev_handle txrx_vdev,
    ol_osif_vdev_handle osif_vdev,
    struct ol_txrx_osif_ops *txrx_ops);

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
 *  After the wdi_in_osif_tso_segment returns, the OSIF SW needs to perform
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
qdf_nbuf_t wdi_in_osif_tso_segment(
    ol_txrx_vdev_handle txrx_vdev,
    int max_seg_payload_bytes,
    qdf_nbuf_t jumbo_tcp_frame);

#define WDI_EVENT_BASE 0x100 	 /* Event starting number */

enum WDI_EVENT {
    WDI_EVENT_TX_STATUS = WDI_EVENT_BASE,
    WDI_EVENT_RX_DESC,
    WDI_EVENT_RX_DESC_REMOTE,
    WDI_EVENT_RATE_FIND,
    WDI_EVENT_RATE_UPDATE,
    WDI_EVENT_RX_PEER_INVALID,
    /* End of new event items */

    WDI_EVENT_LAST
};

struct wdi_event_rx_peer_invalid_msg {
    qdf_nbuf_t msdu;
    struct ieee80211_frame *wh;
    u_int8_t vdev_id;
};

#define WDI_NUM_EVENTS WDI_EVENT_LAST - WDI_EVENT_BASE

#define WDI_EVENT_NOTIFY_BASE 0x200
enum WDI_EVENT_NOTIFY {
    WDI_EVENT_SUB_DEALLOCATE = WDI_EVENT_NOTIFY_BASE,
    /* End of new notification types */

    WDI_EVENT_NOTIFY_LAST
};

/* Opaque event callback */
typedef void (*wdi_event_cb)(void *pdev, enum WDI_EVENT event, void *data);

/* Opaque event notify */
typedef void (*wdi_event_notify)(enum WDI_EVENT_NOTIFY notify,
            enum WDI_EVENT event);
/**
 * @typedef wdi_event_subscribe
 * @brief Used by consumers to subscribe to WDI event notifications.
 * @details
 *  The event_subscribe struct includes pointers to other event_subscribe
 *  objects.  These pointers are simply to simplify the management of
 *  lists of event subscribers.  These pointers are set during the
 *  event_sub() function, and shall not be modified except by the
 *  WDI event management SW, until after the object's event subscription
 *  is canceled by calling event_unsub().
 */

typedef struct wdi_event_subscribe_t {
    wdi_event_cb callback; /* subscriber event callback structure head*/
    void *context; /* subscriber object that processes the event callback */
    struct {
        /* private - the event subscriber SW shall not use this struct */
        struct wdi_event_subscribe_t *next;
        struct wdi_event_subscribe_t *prev;
    } priv;
} wdi_event_subscribe;

struct wdi_event_pdev_t;
typedef struct wdi_event_pdev_t *wdi_event_pdev_handle;

/**
 * @brief Subscribe to a specified WDI event.
 * @details
 *  This function adds the provided wdi_event_subscribe object to a list of
 *  subscribers for the specified WDI event.
 *  When the event in question happens, each subscriber for the event will
 *  have their callback function invoked.
 *  The order in which callback functions from multiple subscribers are
 *  invoked is unspecified.
 *
 * @param pdev - the event physical device, that maintains the event lists
 * @param event_cb_sub - the callback and context for the event subscriber
 * @param event - which event's notifications are being subscribed to
 * @return error code, or A_OK for success
 */
A_STATUS wdi_in_event_sub(
    struct ol_txrx_pdev_t *txrx_pdev,
    wdi_event_subscribe *event_cb_sub,
    enum WDI_EVENT event);

/**
 * @brief Unsubscribe from a specified WDI event.
 * @details
 *  This function removes the provided event subscription object from the
 *  list of subscribers for its event.
 *  This function shall only be called if there was a successful prior call
 *  to event_sub() on the same wdi_event_subscribe object.
 *
 * @param pdev - the event physical device with the list of event subscribers
 * @param event_cb_sub - the event subscription object
 * @param event - which event is being unsubscribed
 * @return error code, or A_OK for success
 */
A_STATUS wdi_in_event_unsub(
    struct ol_txrx_pdev_t *txrx_pdev,
    wdi_event_subscribe *event_cb_sub,
    enum WDI_EVENT event);

#include <qdf_lock.h>   /* qdf_mutex_t */
#include <ol_txrx_stats.h> /* ol_txrx_stats */

typedef void (*ol_txrx_stats_callback)(
    void *ctxt,
    u_int32_t type,
    u_int8_t *buf,
    int bytes);

struct ol_txrx_stats_req {
    u_int32_t stats_type_upload_mask; /* which stats to upload */
    u_int32_t stats_type_reset_mask;  /* which stats to reset */

    /* stats will be printed if either print element is set */
    struct {
        int verbose; /* verbose stats printout */
        int concise; /* concise stats printout (takes precedence) */
    } print; /* print uploaded stats */

    /* stats notify callback will be invoked if fp is non-NULL */
    struct {
        ol_txrx_stats_callback fp;
        void *ctxt;
    } callback;

    /* stats will be copied into the specified buffer if buf is non-NULL */
    struct {
        u_int8_t *buf;
        int byte_limit; /* don't copy more than this */
    } copy;

    /*
     * If blocking is true, the caller will take the specified semaphore
     * to wait for the stats to be uploaded, and the driver will release
     * the semaphore when the stats are done being uploaded.
     */
    struct {
        int blocking;
        qdf_mutex_t *sem_ptr;
    } wait;
};

#if ATH_PERF_PWR_OFFLOAD == 0 /*---------------------------------------------*/

#define wdi_in_debug(vdev, debug_specs) 0
#define wdi_in_fw_stats_cfg(vdev, type, val) 0
#define wdi_in_fw_stats_get(vdev, req) 0
#define wdi_in_aggr_cfg(vdev, max_subfrms_ampdu, max_subfrms_amsdu) 0

#else /*---------------------------------------------------------------------*/

int wdi_in_debug(ol_txrx_vdev_handle vdev, int debug_specs);

void wdi_in_fw_stats_cfg(
    ol_txrx_vdev_handle vdev,
    u_int8_t cfg_stats_type,
    u_int32_t cfg_val);

int wdi_in_fw_stats_get(
    ol_txrx_vdev_handle vdev,
    struct ol_txrx_stats_req *req);

#if defined(TEMP_AGGR_CFG)
int wdi_in_aggr_cfg(ol_txrx_vdev_handle vdev,
                     int max_subfrms_ampdu,
                     int max_subfrms_amsdu);
#else
#define wdi_in_aggr_cfg(vdev, max_subfrms_ampdu, max_subfrms_amsdu) 0
#endif

enum {
    TXRX_DBG_MASK_OBJS             = 0x01,
    TXRX_DBG_MASK_STATS            = 0x02,
    TXRX_DBG_MASK_PROT_ANALYZE     = 0x04,
    TXRX_DBG_MASK_RX_REORDER_TRACE = 0x08,
    TXRX_DBG_MASK_RX_PN_TRACE      = 0x10
};

/*--- txrx printouts ---*/

/*
 * Uncomment this to enable txrx printouts with dynamically adjustable
 * verbosity.  These printouts should not impact performance.
 */
#define TXRX_PRINT_ENABLE 0
/* uncomment this for verbose txrx printouts (may impact performance) */
//#define TXRX_PRINT_VERBOSE_ENABLE 1

void wdi_in_print_level_set(unsigned level);

/*--- txrx object (pdev, vdev, peer) display debug functions ---*/

#ifndef TXRX_DEBUG_LEVEL
#define TXRX_DEBUG_LEVEL 0 /* no debug info */
#endif

#if TXRX_DEBUG_LEVEL > 5
void wdi_in_pdev_display(ol_txrx_pdev_handle pdev, int indent);
void wdi_in_vdev_display(ol_txrx_vdev_handle vdev, int indent);
void wdi_in_peer_display(ol_txrx_peer_handle peer, int indent);
#else
#define wdi_in_pdev_display(pdev, indent)
#define wdi_in_vdev_display(vdev, indent)
#define wdi_in_peer_display(peer, indent)
#endif

/*--- txrx stats display debug functions ---*/

#if TXRX_STATS_LEVEL != TXRX_STATS_LEVEL_OFF

void wdi_in_stats_display(ol_txrx_pdev_handle pdev);

int
wdi_in_stats_publish(ol_txrx_pdev_handle pdev, void *buf);

#else
#define wdi_in_stats_display(pdev)
#define wdi_in_stats_publish(pdev, buf) TXRX_STATS_LEVEL_OFF
#endif /* TXRX_STATS_LEVEL */

/*--- txrx protocol analyzer debug feature ---*/

/* uncomment this to enable the protocol analzyer feature */
//#define ENABLE_TXRX_PROT_ANALYZE 1

#if defined(ENABLE_TXRX_PROT_ANALYZE)

void wdi_in_prot_ans_display(ol_txrx_pdev_handle pdev);

#else

#define wdi_in_prot_ans_display(pdev)

#endif /* ENABLE_TXRX_PROT_ANALYZE */

/*--- txrx sequence number trace debug feature ---*/

/* uncomment this to enable the rx reorder trace feature */
//#define ENABLE_RX_REORDER_TRACE 1

#define wdi_in_seq_num_trace_display(pdev) \
    ol_rx_reorder_trace_display(pdev, 0, 0)

#if defined(ENABLE_RX_REORDER_TRACE)

void
ol_rx_reorder_trace_display(ol_txrx_pdev_handle pdev, int just_once, int limit);

#else

#define ol_rx_reorder_trace_display(pdev, just_once, limit)

#endif /* ENABLE_RX_REORDER_TRACE */

/*--- txrx packet number trace debug feature ---*/

/* uncomment this to enable the rx PN trace feature */
//#define ENABLE_RX_PN_TRACE 1

#define wdi_in_pn_trace_display(pdev) ol_rx_pn_trace_display(pdev, 0)

#if defined(ENABLE_RX_PN_TRACE)

void
ol_rx_pn_trace_display(ol_txrx_pdev_handle pdev, int just_once);

#else

#define ol_rx_pn_trace_display(pdev, just_once)

#endif /* ENABLE_RX_PN_TRACE */

#endif /* ATH_PERF_PWR_OFFLOAD  */ /*----------------------------------------*/

#else
/* WDI_API_AS_MACROS */

#include <ol_txrx_ctrl_api.h>

#define wdi_in_pdev_attach cdp_pdev_attach
#define wdi_in_pdev_attach_target cdp_pdev_attach_target
#define wdi_in_enable_host_ratectrl ol_txrx_enable_host_ratectrl
#define wdi_in_vdev_attach cdp_vdev_attach
#define wdi_in_peer_attach cdp_peer_attach
#define wdi_in_peer_update ol_txrx_peer_update
#if WDS_VENDOR_EXTENSTION
#define wdi_in_peer_wds_tx_policy_update ol_txrx_peer_wds_tx_policy_update
#endif
#define wdi_in_peer_pause ol_txrx_peer_pause
#define wdi_in_peer_tid_unpause ol_txrx_peer_tid_unpause
#define wdi_in_tx_release ol_txrx_tx_release
#define wdi_in_vdev_pause ol_txrx_vdev_pause
#define wdi_in_vdev_unpause ol_txrx_vdev_unpause
#define wdi_in_pdev_pause ol_txrx_pdev_pause
#define wdi_in_pdev_unpause ol_txrx_pdev_unpause
#define wdi_in_tx_sync ol_txrx_tx_sync
#define wdi_in_peer_detach cdp_peer_detach
#define wdi_in_vdev_detach cdp_vdev_detach
#define wdi_in_pdev_detach cdp_pdev_detach
#define wdi_in_mgmt_tx_cb_set cdp_mgmt_tx_cb_set
#define wdi_in_mgmt_send cdp_mgmt_send
#define wdi_in_set_monitor_mode_vap cdp_set_monitor_mode_vap
#define wdi_in_set_curchan cdp_set_curchan
#define wdi_in_get_tx_pending cdp_get_tx_pending

#include <ol_txrx_dbg.h>

#define wdi_in_debug cdp_debug
#define wdi_in_fw_stats_cfg cdp_fw_stats_cfg
#define wdi_in_fw_stats_get cdp_fw_stats_get
#define wdi_in_aggr_cfg cdp_aggr_cfg
#define wdi_in_debug cdp_debug
#define wdi_in_fw_stats_cfg cdp_fw_stats_cfg
#define wdi_in_fw_stats_get cdp_fw_stats_get
#define wdi_in_aggr_cfg cdp_aggr_cfg
#define wdi_in_aggr_cfg cdp_aggr_cfg
#define wdi_in_print_level_set ol_txrx_print_level_set
#define wdi_in_pdev_display ol_txrx_pdev_display
#define wdi_in_vdev_display ol_txrx_vdev_display
#define wdi_in_peer_display ol_txrx_peer_display
#define wdi_in_pdev_display ol_txrx_pdev_display
#define wdi_in_vdev_display ol_txrx_vdev_display
#define wdi_in_peer_display ol_txrx_peer_display
#define wdi_in_stats_display ol_txrx_stats_display
#define wdi_in_stats_publish cdp_stats_publish
#define wdi_in_stats_display ol_txrx_stats_display
#define wdi_in_stats_publish cdp_stats_publish
#define wdi_in_prot_ans_display ol_txrx_prot_ans_display
#define wdi_in_prot_ans_display ol_txrx_prot_ans_display
#define wdi_in_seq_num_trace_display ol_txrx_seq_num_trace_display
#define wdi_in_pn_trace_display ol_txrx_pn_trace_display

#include <ol_txrx_osif_api.h>

#define wdi_in_osif_vdev_register ol_txrx_osif_vdev_register
#define wdi_in_osif_tso_segment ol_txrx_osif_tso_segment

#include <wdi_event_api.h>

#define wdi_in_event_sub wdi_event_sub
#define wdi_in_event_unsub wdi_event_unsub

#endif /* WDI_API_AS_FUNCS / MACROS */

#endif /* _WDI_IN__H_ */

/**@}*/

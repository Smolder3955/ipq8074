/*
 * Copyright (c) 2011-2014, 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 *
 * 2011-2014 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/**
 * @file ol_txrx_ctrl_api.h
 * @brief Define the host data API functions called by the host control SW.
 */
#ifndef _OL_TXRX_CTRL_API__H_
#define _OL_TXRX_CTRL_API__H_

#include <athdefs.h>      /* A_STATUS */
#include <qdf_nbuf.h>     /* qdf_nbuf_t */
#include <qdf_types.h> /* qdf_device_t */
#include <htc_api.h>      /* HTC_HANDLE */
#include <htt.h>
#include "wlan_defs.h"   /* MAX_SPATIAL_STREAM */
#include <cdp_txrx_handle.h>

/**
 * @brief Wrapper for rate-control context initialization
 * @details
 *  Enables the switch that controls the allocation of the
 *  rate-control contexts on the host.
 *
 * @param pdev   - the physical device being initialized
 * @param enable - 1: enabled 0: disabled
 */
void ol_txrx_enable_host_ratectrl(ol_txrx_pdev_handle pdev, u_int32_t enable);


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
    u_int32_t ni_ext_flags;
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
ol_txrx_peer_update(struct ol_txrx_peer_t *peer,
        struct peer_ratectrl_params_t *peer_ratectrl_params);

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
ol_txrx_peer_pause(void *data_peer);

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
ol_txrx_peer_tid_unpause(void *data_peer, int tid);

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
ol_txrx_tx_release(
    void *peer,
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
ol_txrx_vdev_pause(ol_txrx_vdev_handle data_vdev);

/**
 * @brief Resume tx for the specified virtual device.
 * @details
 *  This function applies only to HL systems - in LL systems, tx flow control
 *  is handled entirely within the target FW.
 *
 * @param data_vdev - the virtual device being unpaused
 */
void
ol_txrx_vdev_unpause(ol_txrx_vdev_handle data_vdev);

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
ol_txrx_pdev_pause(ol_txrx_pdev_handle data_pdev);

/**
 * @brief Resume tx for the specified physical device.
 * @details
 *  This function applies only to HL systems - in LL systems, tx flow control
 *  is handled entirely within the target FW.
 *
 * @param data_pdev - the physical device being unpaused
 */
void
ol_txrx_pdev_unpause(ol_txrx_pdev_handle data_pdev);

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
ol_txrx_tx_sync(ol_txrx_pdev_handle data_pdev, u_int8_t sync_cnt);

int
ol_txrx_set_monitor_data_filter(
    ol_txrx_pdev_handle pdev,
    u_int32_t val);

/**
 * @brief get the Tx encapsulation type of the VDEV
 *
 * @param vdev - the data virtual device object
 * @return - the Tx encap type
 */
enum htt_pkt_type
ol_txrx_get_tx_encap_type(ol_txrx_vdev_handle vdev);

#if WDS_VENDOR_EXTENSION
/**
 *  @brief Update the data peer object at association time with wds tx policy
 *  @details
 *  update the wds tx policy of peer/node at association.
 *
 *  @param peer - pointer to the node's object
 *  @param wds_tx_policy bit mask - 0 - disable wds vendor extension tx policy
 *                                 b1 - use 4-addr ucast frames
 *                                 b2 - use 4-addr mcast frames
 *
 *  @return none
 */

void
ol_txrx_peer_wds_tx_policy_update(
    struct cdp_peer *peer,
    int wds_tx_policy_ucast,
    int wds_tx_policy_mcast);

#endif

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

#endif /* _OL_TXRX_CTRL_API__H_ */

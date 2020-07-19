/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#ifndef _OL_RX__H_
#define _OL_RX__H_

#include <qdf_nbuf.h> /* qdf_nbuf_t */
#include <ol_txrx_types.h> /* ol_txrx_vdev_t, etc. */

void
ol_rx_deliver(
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    qdf_nbuf_t head_msdu);

void
ol_rx_discard(
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    qdf_nbuf_t head_msdu);

void 
ol_rx_frames_free(
    htt_pdev_handle htt_pdev, 
    qdf_nbuf_t frames);

void
ol_rx_peer_init(struct ol_txrx_pdev_t *pdev, struct ol_txrx_peer_t *peer);

void
ol_rx_peer_cleanup(struct ol_txrx_vdev_t *vdev, struct ol_txrx_peer_t *peer);

A_STATUS
ol_rx_update_peer_stats(void *pdev, qdf_nbuf_t msdu,
    uint16_t peer_id,
    enum htt_cmn_rx_status status);


#if UMAC_SUPPORT_WNM
int
ol_ath_wnm_update_bssidle_time(
    struct ol_ath_softc_net80211 *scn ,
    struct ol_txrx_peer_t *peer,
    htt_pdev_handle htt_pdev ,
    void *rx_mpdu_desc );
#endif

#endif /* _OL_RX__H_ */

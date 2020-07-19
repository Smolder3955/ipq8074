/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _OL_RX_REORDER__H_
#define _OL_RX_REORDER__H_

#include <qdf_nbuf.h>        /* qdf_nbuf_t, etc. */

#include <cdp_txrx_cmn_struct.h>     /* ol_txrx_peer_t, etc. */

#include <ol_txrx_types.h>   /* ol_rx_reorder_t */

int
ol_rx_reorder_store(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    unsigned seq_num,
    qdf_nbuf_t head_msdu,
    qdf_nbuf_t tail_msdu);

void
ol_rx_reorder_release(
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    unsigned seq_num_start,
    unsigned seq_num_end);

void
ol_rx_reorder_flush(
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    unsigned seq_num_start,
    unsigned seq_num_end,
    enum htt_rx_flush_action action);

void
ol_non_aggr_re_order_flush(
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer, unsigned tid);

void
ol_rx_reorder_peer_cleanup(
    struct ol_txrx_vdev_t *vdev, struct ol_txrx_peer_t *peer);

void
ol_rx_reorder_init(struct ol_rx_reorder_t *rx_reorder, int tid);

#endif /* _OL_RX_REORDER__H_ */

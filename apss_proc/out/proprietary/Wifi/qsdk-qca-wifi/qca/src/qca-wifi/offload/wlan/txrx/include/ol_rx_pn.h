/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _OL_RX_PN_H_
#define _OL_RX_PN_H_

#include <qdf_nbuf.h>        /* qdf_nbuf_t, etc. */

#include <cdp_txrx_cmn_struct.h>     /* ol_txrx_peer_t, etc. */

int ol_rx_pn_cmp24(
    union htt_rx_pn_t *new_pn,
    union htt_rx_pn_t *old_pn,
    int is_unicast,
    int opmode);

int ol_rx_pn_cmp48(
    union htt_rx_pn_t *new_pn,
    union htt_rx_pn_t *old_pn,
    int is_unicast,
    int opmode);

int ol_rx_pn_wapi_cmp(
    union htt_rx_pn_t *new_pn,
    union htt_rx_pn_t *old_pn,
    int is_unicast,
    int opmode);

/**
 * @brief If applicable, check the Packet Number to detect replays.
 * @details
 *  Determine whether a PN check is needed, and if so, what the PN size is.
 *  (A PN size of 0 is used to indirectly bypass the PN check for security
 *  methods that don't involve a PN check.)
 *  This function produces event notifications for any PN failures, via the
 *  ol_rx_err function.
 *  After the PN check, call the next stage of rx processing (rx --> tx
 *  forwarding check).
 *
 * @param vdev - which virtual device the frames were addressed to
 * @param peer - which peer the rx frames belong to
 * @param tid  - which TID within the peer the rx frames belong to
 * @param msdu_list - NULL-terminated list of MSDUs to perform PN check on
 *      (if PN check is applicable, i.e. PN length > 0)
 */
void
ol_rx_pn_check(
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    qdf_nbuf_t msdu_list);

/**
 * @brief If applicable, check the Packet Number to detect replays.
 * @details
 *  Same as ol_rx_pn_check but return valid rx netbufs
 *  rather than invoking the rx --> tx forwarding check.
 *
 * @param vdev - which virtual device the frames were addressed to
 * @param peer - which peer the rx frames belong to
 * @param tid - which TID within the peer the rx frames belong to
 * @param msdu_list - NULL-terminated list of MSDUs to perform PN check on
 *      (if PN check is applicable, i.e. PN length > 0)
 * @return list of netbufs that didn't fail the PN check
 */
qdf_nbuf_t
ol_rx_pn_check_base(
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    qdf_nbuf_t msdu_list);


#endif /* _OL_RX_PN_H_ */

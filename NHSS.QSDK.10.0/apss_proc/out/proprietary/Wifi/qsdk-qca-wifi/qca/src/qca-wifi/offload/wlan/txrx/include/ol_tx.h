/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/**
 * @file ol_tx.h
 * @brief Internal definitions for the high-level tx module.
 */
#ifndef _OL_TX__H_
#define _OL_TX__H_

#include <qdf_nbuf.h>    /* qdf_nbuf_t */
#include <qdf_lock.h>
#include <cdp_txrx_cmn_struct.h> /* ol_txrx_vdev_handle */

qdf_nbuf_t
ol_tx_ll(void *vdev, qdf_nbuf_t msdu_list);

qdf_nbuf_t
ol_tx_exception(void *vdev, qdf_nbuf_t msdu_list,
                struct cdp_tx_exception_metadata *tx_exc);

qdf_nbuf_t
ol_tx_non_std_ll(
    ol_txrx_vdev_handle data_vdev,
    u_int8_t ext_tid,
    enum ol_txrx_osif_tx_spec tx_spec,
    qdf_nbuf_t msdu_list);

qdf_nbuf_t
ol_tx_hl(void *vdev, qdf_nbuf_t msdu_list);

qdf_nbuf_t
ol_tx_non_std_hl(
    ol_txrx_vdev_handle data_vdev,
    u_int8_t ext_tid,
    enum ol_txrx_osif_tx_spec tx_spec,
    qdf_nbuf_t msdu_list);

#endif /* _OL_TX__H_ */

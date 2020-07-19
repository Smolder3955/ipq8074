/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/**
 * @file ol_tx_send.h
 * @brief API definitions for the tx sendriptor module within the data SW.
 */
#ifndef _OL_TX_SEND__H_
#define _OL_TX_SEND__H_

#include <qdf_nbuf.h>      /* qdf_nbuf_t */
#include <ol_txrx_types.h> /* ol_tx_send_t */

/**
 * @brief Send a tx frame to the target.
 * @details
 *
 * @param vdev - the data virtual device sending the data
 *      (for storing the tx desc in the virtual dev's tx_target_list,
 *      and for accessing the phy dev)
 * @param vdev - the virtual device sending the data
 *      (for specifying the transmitter address for multicast / broadcast data)
 * @param netbuf - the tx frame
 */
void
ol_tx_send(
    struct ol_txrx_vdev_t *vdev,
    struct ol_tx_desc_t *tx_desc,
    qdf_nbuf_t msdu);


#endif /* _OL_TX_SEND__H_ */

/*
 * Copyright (c) 2014 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/**
 * @file ol_rawmode_txrx_api.h
 * @brief Definitions used in external interfaces to Raw Mode specific Tx/Rx
 * routines
 */
#ifndef _OL_RAWMODE_TXRX_API__H_
#define _OL_RAWMODE_TXRX_API__H_

#if QCA_OL_SUPPORT_RAWMODE_TXRX
/**
 * @brief Prepare additional cached header descriptor info for Raw Mode
 *
 * @param pdev - the physical device sending the data
 * @param vdev - the virtual device sending the data
 * @param netbuf - the master network buffer
 * @param tx_desc - the SW Tx descriptor for the Tx frame
 * @param ptotal_len - pointer at which the total length of all network buffers
 *      in chain should be stored
 * @param pnum_frags - pointer at which the total number of network buffers in
 *      chain should be stored.
 *
 * @return 0 on success, -1 on failure. In case of failure, *ptotal_len and
 *      *pnum_frags will not be altered.
 */
extern int8_t
ol_tx_prepare_raw_desc_chdhdr(
        struct ol_txrx_pdev_t *pdev,
        struct ol_txrx_vdev_t *vdev,
        qdf_nbuf_t netbuf,
        struct ol_tx_desc_t *tx_desc,
        u_int16_t *ptotal_len,
        u_int8_t *pnum_frags);

#define OL_TX_PREPARE_RAW_DESC_CHDHDR(_pdev, _vdev, _nbuf, _tx_desc, \
                        _ptotal_len, _pnum_frags) \
        ol_tx_prepare_raw_desc_chdhdr((_pdev), (_vdev), (_nbuf), (_tx_desc),\
                        (_ptotal_len), (_pnum_frags))
#else
#define OL_TX_PREPARE_RAW_DESC_CHDHDR(_pdev, _vdev, _nbuf, _tx_desc, \
                        _ptotal_len, _pnum_frags) \
        (0)
#endif /* QCA_OL_SUPPORT_RAWMODE_TXRX */


#if QCA_OL_SUPPORT_RAWMODE_TXRX
/**
 * @brief Unmap slave Tx nbufs in chain of Raw frame nbufs
 *
 * @param pdev - the physical device receiving the data
 * @param netbuf - the master nbuf in Tx chain
 *
 * @return void
 */
extern void
ol_raw_tx_chained_nbuf_unmap(ol_txrx_pdev_handle pdev, qdf_nbuf_t netbuf);

/**
 * @brief Deliver Raw frames to OS layer
 * @details
 *   Carry out requisite processing of a batch of nbufs, including actions
 *   such as discard if indicated by FW, and deliver to OS layer.
 *   XXX: This function may need to be modified by end platforms depending on
 *   how they design interfacing to external entities like Access Controllers
 *   which are to be passed the Raw frames.
 *
 * @param vdev - the virtual device receiving the data
 * @param peer - the peer from which the data has been received
 * @param tid - the TID
 * @param nbuf_list - linked list of network buffers containing one or more Raw
 *        MPDUs
 *
 * @return void
 */
extern void
ol_rx_deliver_raw(
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    qdf_nbuf_t nbuf_list);

/**
 * @brief Find nbuf corresponding to beginning of next MPDU in a chain of Raw
 * frame nbufs
 *
 * @param pdev - the physical device receiving the data
 * @param mpdu_list - chain of nbufs being delivered
 * @param mpdu_tail - pointer into which address of last nbuf for current MPDU
 *     should be saved
 * @param next_mpdu - pointer into which address of first nbuf for next MPDU
 *     should be saved
 *
 * @return void
 */
extern
#if QCA_RAWMODE_OPTIMIZATION_CONFIG > 0
inline
#endif /* QCA_RAWMODE_OPTIMIZATION_CONFIG */
void
ol_rx_mpdu_list_next_raw(
    struct ol_txrx_pdev_t *pdev,
    void *mpdu_list,
    qdf_nbuf_t *mpdu_tail,
    qdf_nbuf_t *next_mpdu);

/**
 * @brief Calculate the total length of the raw packet
 *
 * @param nbuf - single network buffer or head of chain of network buffers containing a single Raw packet
 *
 * @return total length of Raw packet
 */
extern
#if QCA_RAWMODE_OPTIMIZATION_CONFIG > 0
inline
#endif /* QCA_RAWMODE_OPTIMIZATION_CONFIG */
uint32_t
ol_tx_rawmode_nbuf_len(qdf_nbuf_t nbuf);

/**
 * @brief Help enqueue a raw packet for peer flow control
 * @details
 *   Help with a few raw mode specific queuing manipulations and statistics updates
 *
 * @param pdev - the physical device sending the data
 * @param peer  handle of peer we are sending to
 * @param nbuf - single network buffer or head of chain of network buffers containing a single Raw packet
 * @param peer_id - ID of the peer we are sending to
 * @param tid - 802.11 Traffic ID
 *
 * @return total size of packets enqueued
 */
extern
#if QCA_RAWMODE_OPTIMIZATION_CONFIG > 0
inline
#endif /* QCA_RAWMODE_OPTIMIZATION_CONFIG */
uint32_t
ol_tx_rawmode_enque_pflow_ctl_helper(struct ol_txrx_pdev_t *pdev,
                struct ol_txrx_peer_t *peer, qdf_nbuf_t nbuf,
                uint32_t peer_id, u_int8_t tid);
/**
 * @brief Help de-queue the raw packet for peer flow control
 * @details
 *   Help with a few raw mode specific queuing manipulations and statistics
 *   updates
 *
 * @param pdev - the physical device sending the data
 * @param peer  handle of the peer we are sending to
 * @param nbuf - single network buffer or head of chain of network buffers containing a single Raw packet
 * @param tid - 802.11 Traffic ID
 *
 * @return total size of packets dequeued
 */
extern
#if QCA_RAWMODE_OPTIMIZATION_CONFIG > 0
inline
#endif /* QCA_RAWMODE_OPTIMIZATION_CONFIG */
uint32_t
ol_tx_rawmode_deque_pflow_ctl_helper(struct ol_txrx_pdev_t *pdev,
                struct ol_txrx_peer_t *peer, qdf_nbuf_t nbuf, u_int8_t tid);


#define OL_RAW_TX_CHAINED_NBUF_UNMAP(_pdev, _netbuf) \
        ol_raw_tx_chained_nbuf_unmap((_pdev), (_netbuf))
#define OL_RX_DELIVER_RAW(_vdev, _peer, _tid, _nbuf_list) \
        ol_rx_deliver_raw((_vdev), (_peer), (_tid), (_nbuf_list))
#define  OL_RX_MPDU_LIST_NEXT_RAW(_pdev, _mpdu_list, _mpdu_tail, _next_mpdu) \
        ol_rx_mpdu_list_next_raw((_pdev), (_mpdu_list), (_mpdu_tail), (_next_mpdu))

#else /* QCA_OL_SUPPORT_RAWMODE_TXRX */

#define OL_RAW_TX_CHAINED_NBUF_UNMAP(_pdev, _netbuf) \
        do {} while (0)
#define OL_RX_DELIVER_RAW(_vdev, _peer, _tid, _nbuf_list) \
        do {} while (0)
#define  OL_RX_MPDU_LIST_NEXT_RAW(_pdev, _mpdu_list, _mpdu_tail, _next_mpdu) \
        do {} while(0)
#endif /* QCA_OL_SUPPORT_RAWMODE_TXRX */


#endif /* _OL_RAWMODE_TXRX_API__H_ */

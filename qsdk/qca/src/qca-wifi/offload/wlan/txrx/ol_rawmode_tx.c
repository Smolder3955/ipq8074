/*
 * Copyright (c) 2014 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <qdf_nbuf.h>               /* qdf_nbuf_t, etc. */
#include <ol_txrx_types.h>          /* ol_txrx_pdev_t */
#include <ol_txrx_api.h>            /* ol_txrx_vdev_handle */
#include <ol_rawmode_txrx_api.h>    /* API definitions */
#include <ol_htt_tx_api.h>          /* htt_tx_desc_frag */

#if QCA_OL_SUPPORT_RAWMODE_TXRX

/* Raw Mode specific Tx functionality */


#if QCA_OL_SUPPORT_RAWMODE_AR900B_BUFFCOALESCE

/* Raw mode Tx buffer coalescing
 *
 * This should ideally be disabled in end systems to avoid the if checks and
 * copies.
 * Instead, interfacing with external entities like Access Controller should be
 * designed such that this is never required (e.g. using Jumbo frames).
 *
 * We implement a simple coalescing scheme wherein last few buffers are combined
 * if necessary to help form max sized A-MSDU.
 *
 * For efficiency, we don't check the PHY mode/peer HT capability etc. to see
 * if a lower A-MSDU limit applies. The external entity (e.g. Access Controller)
 * must take care of ensuring it uses the right limit.
 *
 * However, as simple guard steps, we do check if previous buffers have been
 * fully utilized (by examining sub-total so far, assuming non Jumbo 802.3
 * frames), and whether total of all buffers falls within max VHT A-MSDU limit.
 * This is necessary to ensure we demand a new nbuf of reasonable size from the
 * OS.
 */

#define RAW_TX_8023_MTU_MAX                         (1500)
#define RAW_TX_VHT_MPDU_MAX                         (11454)

static int
ol_raw_tx_coalesce_nbufs(struct ol_txrx_pdev_t *pdev,
        qdf_nbuf_t *pnbuf, u_int16_t non_coalesce_len)
{
    u_int16_t coalesce_len = 0;
    qdf_nbuf_t currnbuf = NULL, nextnbuf = NULL, newnbuf = NULL;
    u_int8_t *currnewbufptr = NULL;

    qdf_assert_always(non_coalesce_len >=
            (RAW_TX_8023_MTU_MAX * (MAX_FRAGS_PER_RAW_TX_MPDU - 1)));

    currnbuf = *pnbuf;

    while(currnbuf)
    {
        coalesce_len += qdf_nbuf_len(currnbuf);
        currnbuf = qdf_nbuf_next(currnbuf);
    }

    /* We do not check mode etc. - see previous comments above */
    qdf_assert_always((coalesce_len + non_coalesce_len) <=
                            RAW_TX_VHT_MPDU_MAX);

    newnbuf = qdf_nbuf_alloc(pdev->osdev, coalesce_len, 0, 4, FALSE);

    if (!newnbuf) {
        qdf_print("Could not allocate new buffer for Raw Tx buffer "
                     "coalescing");
        return -1;
    }

    qdf_nbuf_set_pktlen(newnbuf, coalesce_len);

    currnbuf = *pnbuf;
    currnewbufptr = (u_int8_t*)qdf_nbuf_data(newnbuf);

    while(currnbuf)
    {
        qdf_mem_copy(currnewbufptr,
                qdf_nbuf_data(currnbuf), qdf_nbuf_len(currnbuf));
        currnewbufptr += qdf_nbuf_len(currnbuf);
        nextnbuf = qdf_nbuf_next(currnbuf);

        qdf_nbuf_free(currnbuf);

        currnbuf = nextnbuf;
    }

    *pnbuf = newnbuf;

    return 0;
}
#endif /* QCA_OL_SUPPORT_RAWMODE_AR900B_BUFFCOALESCE */


int8_t
ol_tx_prepare_raw_desc_chdhdr(
        struct ol_txrx_pdev_t *pdev,
        struct ol_txrx_vdev_t *vdev,
        qdf_nbuf_t netbuf,
        struct ol_tx_desc_t *tx_desc,
        u_int16_t *total_len,
        u_int8_t *pnum_frags)
{
    qdf_nbuf_t currnbuf = NULL, nextnbuf = NULL;
#if QCA_OL_SUPPORT_RAWMODE_AR900B_BUFFCOALESCE
    qdf_nbuf_t tempnbuf = NULL;
#endif
    u_int16_t extra_len = 0;
    u_int8_t num_extra_frags = 0;

    /* Process the non-master nbufs in the chain, if any.
     * We need to carry out this processing at this point in order to
     * determine total data length. We piggy-back other operations to avoid
     * additional loops in caller later.
     */
    currnbuf = qdf_nbuf_next(netbuf);

    while (currnbuf) {
        qdf_nbuf_num_frags_init(currnbuf);

        if(QDF_STATUS_E_FAILURE ==
            qdf_nbuf_map_single(vdev->osdev,
                currnbuf,
                QDF_DMA_TO_DEVICE)) {
            qdf_print("DMA Mapping error ");
            goto bad;
        }

        num_extra_frags++;

        /* qdf_nbuf_len() is a light-weight inline dereferencing operation
         * hence we do not store it into a local variable for second reuse.
         * Can revisit this if later extensions require more than two
         * accesses.
         */
         extra_len += qdf_nbuf_len(currnbuf);

         htt_tx_desc_frag(pdev->htt_pdev, tx_desc->htt_frag_desc,
                 num_extra_frags, qdf_nbuf_get_frag_paddr(currnbuf, 0),
                 qdf_nbuf_len(currnbuf));

#if QCA_OL_SUPPORT_RAWMODE_AR900B_BUFFCOALESCE
         /* Look-ahead */
         if (pdev->is_ar900b &&
             (num_extra_frags == (MAX_FRAGS_PER_RAW_TX_MPDU - 2)) &&
             ((tempnbuf = qdf_nbuf_next(currnbuf)) != NULL) &&
             qdf_nbuf_next(tempnbuf) != NULL) {
            /* We will be crossing chaining limit in the next 2 passes.
               Try to coalesce.
             */
            if (ol_raw_tx_coalesce_nbufs(pdev,
                        &tempnbuf, extra_len + qdf_nbuf_len(netbuf)) < 0) {
                goto bad;
            } else {
                /* tempnbuf now contains the final, coalesced buffer */
                qdf_nbuf_set_next(currnbuf, tempnbuf);
                qdf_nbuf_set_next(tempnbuf, NULL);
            }
        }
#endif /* QCA_OL_SUPPORT_RAWMODE_AR900B_BUFFCOALESCE */

         currnbuf = qdf_nbuf_next(currnbuf);
    }

    (*total_len) += extra_len;
    (*pnum_frags) += num_extra_frags;

    return 0;

bad:
    /* Free all nbufs in the chain */
    currnbuf = netbuf;
    while (currnbuf) {
        nextnbuf = qdf_nbuf_next(currnbuf);
        if (qdf_nbuf_get_frag_paddr(currnbuf, 0)) {
            qdf_nbuf_unmap_single(vdev->osdev, currnbuf, QDF_DMA_TO_DEVICE);
        }
        qdf_nbuf_free(currnbuf);
        currnbuf = nextnbuf;
    }
    return -1;
}

void
ol_raw_tx_chained_nbuf_unmap(ol_txrx_pdev_handle pdev_handle, qdf_nbuf_t netbuf)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;
    /* It is the responsibility of the caller to ensure that
     * qdf_nbuf_next(netbuf) is non-NULL. This is for overall efficiency.
     */

    qdf_nbuf_t chnbuf = qdf_nbuf_next(netbuf);

    do {
        struct sk_buff *next = qdf_nbuf_next(chnbuf);
        qdf_nbuf_unmap_single(
                pdev->osdev, (qdf_nbuf_t) chnbuf, QDF_DMA_TO_DEVICE);
        qdf_nbuf_free(chnbuf);
        chnbuf = next;
    } while (chnbuf);

    qdf_nbuf_set_next(netbuf, NULL);
}

inline uint32_t ol_tx_rawmode_nbuf_len(qdf_nbuf_t nbuf)
{
    qdf_nbuf_t tmpbuf;
    uint32_t len = 0;

    /* Calcuating the length of Chained Raw SKBs */
    if (qdf_nbuf_is_tx_chfrag_start(nbuf)) {
        for (tmpbuf = nbuf; tmpbuf != NULL; tmpbuf = tmpbuf->next) {
            len += qdf_nbuf_len(tmpbuf);
            if (qdf_nbuf_is_tx_chfrag_end(nbuf)) {
                break;
            }
        }
    } else {
        /* Length of single raw SKB */
        len = qdf_nbuf_len(nbuf);
    }

    return len;
}

#if PEER_FLOW_CONTROL
/*
 *  This function assumes peer lock is already taken
 *  by the caller function
 */
inline uint32_t
ol_tx_rawmode_enque_pflow_ctl_helper(struct ol_txrx_pdev_t *pdev,
                struct ol_txrx_peer_t *peer, qdf_nbuf_t nbuf,
                uint32_t peer_id, u_int8_t tid)
{

    qdf_nbuf_t tmpbuf = NULL;
    qdf_nbuf_t headbuf = nbuf;
    uint32_t len = 0;

    /* Flow control for unchained Raw packets will be handled in a manner
    similar to non-Raw packets.*/
    if (nbuf->next == NULL) {
        len = qdf_nbuf_len(nbuf);
        qdf_nbuf_queue_add(&peer->tidq[tid].nbufq, nbuf);
    } else {

        /* Marking start and end SKBs in chain so that while de-queuing,
        SKBs belonging to the same chain can be retrieved and
        serviced under a common Tx descriptor*/
        qdf_nbuf_set_tx_chfrag_start(nbuf, 1);

        for (; nbuf != NULL; nbuf = nbuf->next) {
            if (nbuf->next == NULL) {
                qdf_nbuf_set_tx_chfrag_end(nbuf, 1);
            }
            tmpbuf = nbuf;
            qdf_nbuf_queue_add(&peer->tidq[tid].nbufq, tmpbuf);
            len += qdf_nbuf_len(tmpbuf);
        }
        /* resetting the nbuf to point the head SKB*/
        nbuf = headbuf;
    }

    /* Queue count is increased by one even for chained packets
     * since all the skbs in the chain correspond to a single packet */
    return len;
}

/*
 *  This function assumes peer lock is already taken
 *  by the caller function
 */
inline uint32_t
ol_tx_rawmode_deque_pflow_ctl_helper(struct ol_txrx_pdev_t *pdev,
                struct ol_txrx_peer_t *peer, qdf_nbuf_t nbuf, u_int8_t tid)
{
    qdf_nbuf_t chainbuf = NULL;
    qdf_nbuf_t tmpbuf = NULL;
    uint32_t pkt_len = 0;

    if (qdf_nbuf_is_tx_chfrag_start(nbuf)) {
        chainbuf = qdf_nbuf_queue_remove(&peer->tidq[tid].nbufq);
        if (chainbuf == NULL)
            return pkt_len;
        do {
            tmpbuf = qdf_nbuf_queue_remove(&peer->tidq[tid].nbufq);
            if (tmpbuf == NULL)
                return pkt_len;
            pkt_len += qdf_nbuf_len(tmpbuf);
            chainbuf->next = tmpbuf;
            chainbuf = chainbuf->next;
        } while(!qdf_nbuf_is_tx_chfrag_end(tmpbuf));
        tmpbuf->next = NULL;

    } else {
        nbuf = qdf_nbuf_queue_remove(&peer->tidq[tid].nbufq);
	if (nbuf == NULL)
	    return pkt_len;
        pkt_len = qdf_nbuf_len(nbuf);
        nbuf->next = NULL;
    }

    return pkt_len;
}
#endif


#endif /* QCA_OL_SUPPORT_RAWMODE_TXRX */

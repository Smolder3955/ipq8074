/*
 *  * Copyright (c) 2011 Qualcomm Atheros, Inc.
 *   * All Rights Reserved.
 *    * Qualcomm Atheros Confidential and Proprietary.
 *     */

#include <qdf_nbuf.h>         /* qdf_nbuf_t, etc. */

#include <ol_txrx_internal.h> /* TXRX_ASSERT1 */
#include <ol_txrx_types.h>    /* pdev stats */
#include <ol_tx_desc.h>       /* ol_tx_desc */
#include <ol_tx_send.h>       /* ol_tx_send */
#include <ol_txrx_api.h>       /* ol_tx_send */
#include <ol_txrx.h>


/* FIXME Need to remove hard-coded
 * header file inclusions and
 * change as a part of convergent driver
 */
#include <AR900B/hw/datastruct/msdu_link_ext.h>
#include <QCA9984/hw/datastruct/msdu_link_ext.h>
#include <QCA9888/v2/hw/datastruct/msdu_link_ext.h>

#define SG_DBG 0

void
ol_tx_sg_desc_display(void *tx_desc)
{
    struct ol_tx_sg_desc_t *tx_sg_desc = (struct ol_tx_sg_desc_t *)tx_desc;
    int count;

    if(!tx_sg_desc) return;
    qdf_print("==========SG Descriptor==============>");
    qdf_print("tx_sg_desc %pK ", tx_sg_desc);
    qdf_print("tx_sg_desc->frag_cnt %d ", tx_sg_desc->frag_cnt);
    qdf_print("tx_sg_desc->total_len %d ", tx_sg_desc->total_len);

    for (count = 0; count < tx_sg_desc->frag_cnt; count++)
        qdf_print("frag addr[%d]: %x, frag_len: %d", count, tx_sg_desc->frag_paddr_lo[count], tx_sg_desc->frag_len[count]);
}


static inline struct ol_tx_sg_desc_t *
ol_tx_sg_desc_alloc(struct ol_txrx_pdev_t *pdev)
{
    struct ol_tx_sg_desc_t *tx_sg_desc = NULL;

    qdf_spin_lock_bh(&pdev->tx_sg_mutex);
    if (qdf_likely(pdev->tx_sg_desc.freelist)) {
        tx_sg_desc = &pdev->tx_sg_desc.freelist->tx_sg_desc;
        pdev->tx_sg_desc.freelist = pdev->tx_sg_desc.freelist->next;
        pdev->pdev_stats.sg_desc_cnt++;
    }
    qdf_spin_unlock_bh(&pdev->tx_sg_mutex);

    return tx_sg_desc;
}
#ifdef QCA_PARTNER_PLATFORM
void
ol_tx_sg_desc_free(struct ol_txrx_pdev_t *pdev,
                    struct ol_tx_sg_desc_t *tx_sg_desc)
{
    __ol_tx_sg_desc_free(pdev, tx_sg_desc);
}
#endif

void
ol_tx_print_sg_stats(
    void *vdev_handle)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    struct ol_txrx_pdev_t *pdev = vdev->pdev;
        qdf_warn("++++++++++ SG STATISTICS +++++++++++\n");
        qdf_warn("sg TX Pkts       : %u\n", pdev->pdev_stats.tx_i.sg.sg_pkt.num);
        qdf_warn("sg TX Bytes      : %llu\n", pdev->pdev_stats.tx_i.sg.sg_pkt.bytes);
        qdf_warn("Non-sg TX Pkts   : %u\n", pdev->pdev_stats.tx_i.sg.non_sg_pkts.num);
        qdf_warn("Non-sg TX Bytes  : %llu\n", pdev->pdev_stats.tx_i.sg.non_sg_pkts.bytes);
        qdf_warn("sg Dropped Pkts  : %u\n", pdev->pdev_stats.tx_i.sg.dropped_host.num);
        qdf_warn("sg Dropped Bytes : %llu\n", pdev->pdev_stats.tx_i.sg.dropped_host.bytes);
        qdf_warn("sg Desc Total %d In Use : %d\n", pdev->tx_sg_desc.pool_size, \
                pdev->pdev_stats.sg_desc_cnt);

}

void
ol_tx_rst_sg_stats(void *vdev_handle)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    struct ol_txrx_pdev_t *pdev = vdev->pdev;

    qdf_print(".....Resetting sg Stats ");
    /* Tx */
    pdev->pdev_stats.tx_i.sg.sg_pkt.num = 0;
    pdev->pdev_stats.tx_i.sg.sg_pkt.bytes = 0;
    pdev->pdev_stats.tx_i.sg.non_sg_pkts.num = 0;
    pdev->pdev_stats.tx_i.sg.non_sg_pkts.bytes = 0;
    pdev->pdev_stats.tx_i.sg.dropped_host.num = 0;
    pdev->pdev_stats.tx_i.sg.dropped_host.bytes = 0;
    qdf_warn("sg_stats %u %llu %u %llu \n", pdev->pdev_stats.tx_i.sg.sg_pkt.num,
        pdev->pdev_stats.tx_i.sg.sg_pkt.bytes, pdev->pdev_stats.tx_i.sg.non_sg_pkts.num,
        pdev->pdev_stats.tx_i.sg.non_sg_pkts.bytes);
}

void
ol_tx_sg_desc_prepare(
    struct ol_txrx_pdev_t *pdev,
    struct ol_tx_desc_t *tx_desc,
    struct ol_tx_sg_desc_t *sw_sg_desc)
{

#if SG_DBG
    ol_tx_sg_desc_display(sw_tso_desc);
#endif

    ol_tx_sg_desc_free(pdev, sw_sg_desc);

#if SG_DBG
    ol_tx_msdu_desc_display(msdu_link_ext_p);
#endif

}

inline int
ol_tx_sg_process_skb(ol_txrx_vdev_handle vdev,qdf_nbuf_t msdu)
{
    unsigned int cur_frag, nr_frags;
    struct ol_txrx_vdev_t *vdev_handle = (struct ol_txrx_vdev_t *)vdev;
    struct ol_txrx_pdev_t *pdev = ((struct ol_txrx_vdev_t *)vdev)->pdev;
    struct skb_shared_info *info = skb_shinfo(msdu);
    struct ol_tx_sg_desc_t * sw_sg_desc;
    int ret = 0;
    TXRX_VDEV_STATS_MSDU_INCR(vdev_handle, tx_i.sg.sg_pkt, msdu);
    /* Allocate SW TSO Descriptor for storing parsed info. */
    sw_sg_desc = ol_tx_sg_desc_alloc(pdev);
    if (qdf_unlikely(!sw_sg_desc)) {
        qdf_print("HOST_SG:Dropping Pkt, SW SG Descriptor allocation failed ");
	    qdf_nbuf_free(msdu);
        return -ENOMEM;
    }

    if(QDF_STATUS_E_FAILURE == qdf_nbuf_map_nbytes_single(pdev->osdev, msdu,
                                                     QDF_DMA_TO_DEVICE,
                                                     qdf_nbuf_headlen(msdu))) {
        qdf_print("dma map error");
	ol_tx_sg_desc_free(pdev, sw_sg_desc);
	qdf_nbuf_free(msdu);
        return -EIO;
    }

    sw_sg_desc->frag_paddr_lo[0] = qdf_nbuf_get_frag_paddr(msdu, 0);
    sw_sg_desc->frag_paddr_hi[0] = 0x0;
    sw_sg_desc->frag_len[0] = qdf_nbuf_headlen(msdu);

    nr_frags = qdf_nbuf_get_nr_frags(msdu);
    for (cur_frag = 0; cur_frag < info->nr_frags; cur_frag++) {

	if (QDF_STATUS_E_FAILURE == qdf_nbuf_frag_map(pdev->osdev,
			                         msdu,
			                         0,
			                         QDF_DMA_TO_DEVICE,
			                         cur_frag)) {
	    qdf_print("frag dma map error");
	    ol_tx_sg_desc_free(pdev, sw_sg_desc);
	    qdf_nbuf_free(msdu);
            return -EIO;
    }
        sw_sg_desc->frag_paddr_lo[cur_frag + 1] = qdf_nbuf_get_tx_frag_paddr(msdu);
        sw_sg_desc->frag_paddr_hi[cur_frag + 1] = 0x0;
        sw_sg_desc->frag_len[cur_frag + 1] = qdf_nbuf_get_frag_size(msdu, cur_frag);
    }

    sw_sg_desc->frag_cnt = cur_frag + 1;
    sw_sg_desc->total_len = qdf_nbuf_len(msdu);

#if SG_DBG
    ol_tx_sg_desc_display(sw_sg_desc);
#endif
    qdf_nbuf_set_tx_fctx_type(msdu, (void *)sw_sg_desc, CB_FTYPE_SG);
    ret = OL_TX_LL_FORWARD_PACKET(vdev, msdu, 1);
#if !PEER_FLOW_CONTROL
    if (qdf_unlikely(ret)) {
        qdf_nbuf_free(msdu);
        return -EIO;
    }
#endif
    return ret;
}


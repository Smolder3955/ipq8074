/*
 * Copyright (c) 2011,2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */


#include <qdf_nbuf.h>      /* qdf_nbuf_t, etc. */
#include <qdf_lock.h>   /* qdf_spinlock */
#include <queue.h>         /* TAILQ */

#include <ol_htt_tx_api.h> /* htt_tx_desc_id */

#include <ol_txrx_types.h> /* ol_txrx_pdev_t */
#include <ol_tx_desc.h>
#include <ol_txrx_internal.h>

//#include <hw/datastruct/msdu_link_ext.h>
#include "reg_struct.h"
#include "regtable.h"

#if MESH_MODE_SUPPORT
extern void os_if_tx_free_batch_ext(struct sk_buff *bufs, int tx_err);
extern void os_if_tx_free_ext(struct sk_buff *skb);
#endif

/* TBD: make this inline in the .h file? */
struct ol_tx_desc_t *
ol_tx_desc_find(struct ol_txrx_pdev_t *pdev, u_int16_t tx_desc_id)
{
    return &pdev->tx_desc.array[tx_desc_id].tx_desc;
}
qdf_export_symbol(ol_tx_desc_find);


#define ol_tx_desc_base_inline(pdev, vdev, netbuf, tx_desc) \
do {                                                        \
    /* allocate the descriptor */                           \
    tx_desc = ol_tx_desc_alloc(pdev);                       \
    if (!tx_desc) return NULL;                              \
                                                            \
    /* initialize the SW tx descriptor */                   \
    tx_desc->netbuf = netbuf;                               \
    tx_desc->pkt_type = ol_tx_frm_std;                      \
                                                            \
    /* initialize the HW tx descriptor */                   \
    htt_tx_desc_init(                                       \
        pdev->htt_pdev, tx_desc->htt_tx_desc,               \
        ol_tx_desc_id(pdev, tx_desc),                       \
        qdf_nbuf_len(netbuf),                               \
        vdev->vdev_id,                                      \
        vdev->tx_encap_type,                                \
        qdf_nbuf_get_tx_cksum(netbuf),                      \
        qdf_nbuf_get_tid(netbuf));                          \
} while (0)

#define  SET_DWORD_BITS(dword, mask, shift, val) (dword = ((dword) & (~(mask))) | (val << (shift)))

#define MSDU_LINK_EXT_TCP_OVER_IPV4_CKSUM_EN_SET(base, val) SET_DWORD_BITS(*((u_int32_t *)base + 3), \
        MSDU_LINK_EXT_3_TCP_OVER_IPV4_CHECKSUM_EN_MASK, \
        MSDU_LINK_EXT_3_TCP_OVER_IPV4_CHECKSUM_EN_LSB, \
        val)
#define MSDU_LINK_EXT_TCP_OVER_IPV6_CKSUM_EN_SET(base, val) SET_DWORD_BITS(*((u_int32_t *)base + 3), \
        MSDU_LINK_EXT_3_TCP_OVER_IPV6_CHECKSUM_EN_MASK, \
        MSDU_LINK_EXT_3_TCP_OVER_IPV6_CHECKSUM_EN_LSB, \
        val)
#define MSDU_LINK_EXT_UDP_OVER_IPV4_CKSUM_EN_SET(base, val) SET_DWORD_BITS(*((u_int32_t *)base + 3), \
        MSDU_LINK_EXT_3_UDP_OVER_IPV4_CHECKSUM_EN_MASK, \
        MSDU_LINK_EXT_3_UDP_OVER_IPV4_CHECKSUM_EN_LSB, \
        val)
#define MSDU_LINK_EXT_UDP_OVER_IPV6_CKSUM_EN_SET(base, val) SET_DWORD_BITS(*((u_int32_t *)base + 3), \
        MSDU_LINK_EXT_3_UDP_OVER_IPV6_CHECKSUM_EN_MASK, \
        MSDU_LINK_EXT_3_UDP_OVER_IPV6_CHECKSUM_EN_LSB, \
        val)

struct ol_tx_desc_t *
ol_tx_desc_ll(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_vdev_t *vdev,
    qdf_nbuf_t netbuf)
{
    struct ol_tx_desc_t *tx_desc;
    int i, num_frags;

    ol_tx_desc_base_inline(pdev, vdev, netbuf, tx_desc);

	if(qdf_nbuf_get_tx_cksum(netbuf)==HTT_TX_L4_CKSUM_OFFLOAD) {
		u_int32_t *msdu_link_ext_p;
		msdu_link_ext_p = (u_int32_t *)tx_desc->htt_frag_desc;
		MSDU_LINK_EXT_TCP_OVER_IPV4_CKSUM_EN_SET(msdu_link_ext_p, 1);
		MSDU_LINK_EXT_UDP_OVER_IPV4_CKSUM_EN_SET(msdu_link_ext_p, 1);
		MSDU_LINK_EXT_TCP_OVER_IPV6_CKSUM_EN_SET(msdu_link_ext_p, 1);
		MSDU_LINK_EXT_UDP_OVER_IPV6_CKSUM_EN_SET(msdu_link_ext_p, 1);
	}

    /* initialize the fragmentation descriptor */
    num_frags = qdf_nbuf_get_num_frags(netbuf);
    htt_tx_desc_num_frags(pdev->htt_pdev, tx_desc->htt_frag_desc, num_frags);
    for (i = 0; i < num_frags; i++) {
        int frag_len;
        u_int32_t frag_paddr;

        frag_len = qdf_nbuf_get_frag_len(netbuf, i);
        frag_paddr = qdf_nbuf_get_frag_paddr(netbuf, i);
        htt_tx_desc_frag(
            pdev->htt_pdev, tx_desc->htt_frag_desc, i, frag_paddr, frag_len);
    }
    return tx_desc;
}

struct ol_tx_desc_t *
ol_tx_desc_hl(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_vdev_t *vdev,
    qdf_nbuf_t netbuf)
{
    struct ol_tx_desc_t *tx_desc;

    ol_tx_desc_base_inline(pdev, vdev, netbuf, tx_desc);

    return tx_desc;
}

void ol_tx_desc_frame_list_free(
    struct ol_txrx_pdev_t *pdev,
    ol_tx_desc_list *tx_descs,
    int had_error)
{
    struct ol_tx_desc_t *tx_desc, *tmp;
    qdf_nbuf_t msdus = NULL;
#if MESH_MODE_SUPPORT
    qdf_nbuf_t msdus_ext = NULL;
    int extfree = 0;
#endif

    TAILQ_FOREACH_SAFE(tx_desc, tx_descs, tx_desc_list_elem, tmp) {
        qdf_nbuf_t msdu = tx_desc->netbuf;
#if MESH_MODE_SUPPORT
        extfree = tx_desc->extnd_desc;
#endif
        qdf_atomic_init(&tx_desc->ref_cnt); /* clear the ref cnt */
        qdf_nbuf_unmap(pdev->osdev, msdu, QDF_DMA_TO_DEVICE);
        /* free the tx desc */
        ol_tx_desc_free(pdev, tx_desc);
        /* link the netbuf into a list to free as a batch */
#if MESH_MODE_SUPPORT
        if (extfree) {
            qdf_nbuf_set_next(msdu, msdus_ext);
            msdus_ext = msdu;
        } else
#endif
        {
            qdf_nbuf_set_next(msdu, msdus);
            msdus = msdu;
        }
    }
    /* free the netbufs as a batch */
    qdf_nbuf_tx_free(msdus, had_error);
#if MESH_MODE_SUPPORT
    os_if_tx_free_batch_ext(msdus_ext, had_error);
#endif
}

void ol_tx_desc_frame_free_nonstd(
    struct ol_txrx_pdev_t *pdev,
    struct ol_tx_desc_t *tx_desc,
    int had_error)
{
    int mgmt_type;
    ol_txrx_mgmt_tx_cb cb;

    qdf_atomic_init(&tx_desc->ref_cnt); /* clear the ref cnt */

    /* check the frame type to see what kind of special steps are needed */
    if (tx_desc->pkt_type == ol_tx_frm_tso) {
#if 0
        /*
         * Free the segment's customized ethernet+IP+TCP header.
         * Fragment 0 added by the WLAN driver is the HTT+HTC tx descriptor.
         * Fragment 1 added by the WLAN driver is the Ethernet+IP+TCP header
         * added for this TSO segment.
         */
        tso_tcp_hdr = qdf_nbuf_get_frag_vaddr(tx_desc->netbuf, 1);
        ol_tx_tso_hdr_free(pdev, tso_tcp_hdr);
#endif
        qdf_nbuf_unmap(pdev->osdev, tx_desc->netbuf, QDF_DMA_TO_DEVICE);
        /* free the netbuf */
        qdf_nbuf_set_next(tx_desc->netbuf, NULL);
#if MESH_MODE_SUPPORT
        if (tx_desc->extnd_desc) {
            os_if_tx_free_batch_ext(tx_desc->netbuf, had_error);
        } else
#endif
        {
            qdf_nbuf_tx_free(tx_desc->netbuf, had_error);
        }
    } else if (tx_desc->pkt_type == ol_tx_frm_meucast) {
        /* nbuf_free decrements usage count. */
        struct ol_tx_me_buf_t *buf = (struct ol_tx_me_buf_t *)
            qdf_nbuf_get_tx_fctx(tx_desc->netbuf);
        u_int32_t paddr;
        u_int16_t len;

        /* In case of mcast enhance packet, first fragment will always point to
         * the header + downlad_len fragment. This needs to be unmapped here */
        htt_tx_desc_get_frag(pdev->htt_pdev, tx_desc->htt_frag_desc, 0, &paddr, &len);
        qdf_mem_unmap_nbytes_single(pdev->osdev, paddr, QDF_DMA_TO_DEVICE, len);

        ol_tx_me_free_buf(pdev, buf);
#if MESH_MODE_SUPPORT
        if (tx_desc->extnd_desc) {
            os_if_tx_free_ext(tx_desc->netbuf);
        } else
#endif
        {
            qdf_nbuf_free(tx_desc->netbuf);
        }
        tx_desc->pkt_type = ol_tx_frm_std; /* this can be prefilled? */
    } else if (tx_desc->pkt_type >= OL_TXRX_MGMT_TYPE_BASE) {
        qdf_nbuf_unmap(pdev->osdev, tx_desc->netbuf, QDF_DMA_TO_DEVICE);
        mgmt_type = tx_desc->pkt_type - OL_TXRX_MGMT_TYPE_BASE;
        cb = pdev->tx_mgmt.callbacks[mgmt_type].ota_ack_cb;
        if (cb) {
            void *ctxt;
            ctxt = pdev->tx_mgmt.callbacks[mgmt_type].ctxt;
            cb(ctxt, tx_desc->netbuf, had_error);
        }
        /* free the netbuf */
#if MESH_MODE_SUPPORT
        if (tx_desc->extnd_desc) {
            os_if_tx_free_ext(tx_desc->netbuf);
        } else
#endif
        {
            qdf_nbuf_free(tx_desc->netbuf);
        }
    } else {
        qdf_nbuf_unmap(pdev->osdev, tx_desc->netbuf, QDF_DMA_TO_DEVICE);
        /* single regular frame */
        qdf_nbuf_set_next(tx_desc->netbuf, NULL);
#if MESH_MODE_SUPPORT
        if (tx_desc->extnd_desc) {
            os_if_tx_free_batch_ext(tx_desc->netbuf, had_error);
        } else
#endif
        {
            qdf_nbuf_tx_free(tx_desc->netbuf, had_error);
        }
    }
    /* free the tx desc */
    ol_tx_desc_free(pdev, tx_desc);
}

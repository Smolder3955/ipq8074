/*
 * Copyright (c) 2012 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#ifndef _OL_TXRX__H_
#define _OL_TXRX__H_

#include <qdf_nbuf.h> /* qdf_nbuf_t */
#include <cdp_txrx_cmn_struct.h> /* ol_txrx_vdev_t, etc. */
#include <ol_txrx_types.h> /* ol_txrx_vdev_t, etc. */
#include <ol_tx_desc.h>


void
ol_txrx_peer_unref_delete(struct ol_txrx_peer_t *peer);

void
ol_txrx_aggregate_vdev_stats(struct ol_txrx_vdev_t *vdev, struct cdp_vdev_stats *stats);

#if PEER_FLOW_CONTROL
void
ol_txrx_classify(void *vdev, qdf_nbuf_t nbuf,
        enum txrx_direction dir, struct ol_txrx_nbuf_classify *nbuf_class);
#endif
void ol_iterate_update_peer_list(void *pdev_hdl);

#if HOST_SW_TSO_ENABLE
extern qdf_nbuf_t
ol_tx_tso_segment(ol_txrx_vdev_handle vdev, qdf_nbuf_t msdu);

struct ol_tx_desc_t;
struct ol_tx_tso_desc_t;
extern void
ol_tx_tso_desc_prepare(
		struct ol_txrx_pdev_t *pdev,
		qdf_nbuf_t tso_header_nbuf,
		qdf_nbuf_t segment,
		struct ol_tx_desc_t *tx_desc,
		struct ol_tx_tso_desc_t *sw_tso_desc);

extern void
ol_tx_tso_failure_cleanup( struct ol_txrx_pdev_t *pdev, qdf_nbuf_t segment_list);
#endif /* HOST_SW_TSO_ENABLE */

#if HOST_SW_TSO_SG_ENABLE

static inline u_int32_t
ol_tx_tso_sg_get_nbuf_len(qdf_nbuf_t netbuf)
{
    struct ol_tx_tso_desc_t *sw_tso_sg_desc = (struct ol_tx_tso_desc_t *) qdf_nbuf_get_tx_fctx(netbuf);
    return (sw_tso_sg_desc->data_len + sw_tso_sg_desc->l2_l3_l4_hdr_size);
}
extern void
ol_tx_tso_sg_desc_prepare(
    struct ol_txrx_pdev_t *pdev,
    struct ol_tx_desc_t *tx_desc,
    struct ol_tx_tso_desc_t *sw_tso_sg_desc);

static inline void
ol_tx_tso_sg_desc_free(struct ol_txrx_pdev_t *pdev,
                    struct ol_tx_tso_desc_t *tx_tso_desc)
{
    qdf_spin_lock_bh(&pdev->tx_tso_mutex);
    ((union ol_tx_tso_desc_list_elem_t *) tx_tso_desc)->next =
                                      pdev->tx_tso_desc.freelist;
    pdev->tx_tso_desc.freelist =
                (union ol_tx_tso_desc_list_elem_t *) tx_tso_desc;
    pdev->pdev_stats.tso_desc_cnt--;
    qdf_spin_unlock_bh(&pdev->tx_tso_mutex);
}

#endif  /* HOST_SW_TSO_SG_ENABLE */

#if HOST_SW_SG_ENABLE

static inline void
__ol_tx_sg_desc_free(struct ol_txrx_pdev_t *pdev,
                    struct ol_tx_sg_desc_t *tx_sg_desc)
{
    qdf_spin_lock_bh(&pdev->tx_sg_mutex);
    ((union ol_tx_sg_desc_list_elem_t *) tx_sg_desc)->next =
                                      pdev->tx_sg_desc.freelist;
    pdev->tx_sg_desc.freelist =
                (union ol_tx_sg_desc_list_elem_t *) tx_sg_desc;
    pdev->pdev_stats.sg_desc_cnt--;
    qdf_spin_unlock_bh(&pdev->tx_sg_mutex);
}

#ifdef QCA_PARTNER_PLATFORM
extern void ol_tx_sg_desc_free(struct ol_txrx_pdev_t *pdev,
                                    struct ol_tx_sg_desc_t *tx_sg_desc);
#else
static inline void
ol_tx_sg_desc_free(struct ol_txrx_pdev_t *pdev,
                    struct ol_tx_sg_desc_t *tx_sg_desc)
{
    __ol_tx_sg_desc_free(pdev, tx_sg_desc);
}
#endif


#endif /* HOST_SW_SG_ENABLE */

static inline void
ol_tx_free_buf_generic(struct ol_txrx_pdev_t *pdev, qdf_nbuf_t netbuf)
{
    if (qdf_nbuf_get_tx_ftype(netbuf) == CB_FTYPE_MCAST2UCAST) {
        ol_tx_me_free_buf(pdev, (struct ol_tx_me_buf_t *)qdf_nbuf_get_tx_fctx(netbuf));
    }

#if HOST_SW_TSO_SG_ENABLE
        if (qdf_nbuf_get_tx_ftype(netbuf) == CB_FTYPE_TSO_SG) {
            ol_tx_tso_sg_desc_free(pdev,(struct ol_tx_tso_desc_t *) qdf_nbuf_get_tx_fctx(netbuf));
        }
#endif

#if HOST_SW_SG_ENABLE
        if (qdf_nbuf_get_tx_ftype(netbuf) == CB_FTYPE_SG) {
            ol_tx_sg_desc_free(pdev, (struct ol_tx_sg_desc_t *) qdf_nbuf_get_tx_fctx(netbuf));
        }
#endif
        qdf_nbuf_free(netbuf);
}

static inline int
ol_tx_get_tid_override_queue_mapping(struct ol_txrx_pdev_t *pdev, qdf_nbuf_t nbuf)
{
    /*
     * Here skb->queue_mapping is checked to find out the access category.
     * Streamboost classification engine will set the skb->queue_mapping.
     * However, when multi-queue nedev is enabled, then kernel may set
     * skb->queue_mapping itself. Need to check with streamboost team what
     * will happen when alloc_netdev_mq() is used.
     * TODO : check with streamboost team
     */
    u_int16_t map;
    map = qdf_nbuf_get_queue_mapping(nbuf);
    if (pdev->tid_override_queue_mapping && map) {
        map = (map - 1) & 0x3;
        /* This will be overridden if dscp override is enabled */
        return WME_AC_TO_TID(map);
    }
    return -1;
}

#endif /* _OL_TXRX__H_ */

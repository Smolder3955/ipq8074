/*
 * Copyright (c) 2011, 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/*=== header file includes ===*/
/* generic utilities */
#include <qdf_nbuf.h>          /* qdf_nbuf_t, etc. */
#include <qdf_mem.h>        /* qdf_mem_malloc */

/* external interfaces */
#include <cdp_txrx_cmn_struct.h>       /* ol_txrx_pdev_handle */
#include <ol_txrx_htt_api.h>   /* ol_rx_addba_handler, etc. */
#include <ol_htt_rx_api.h>     /* htt_rx_desc_frame_free */

/* datapath internal interfaces */
#include <ol_txrx_peer_find.h> /* ol_txrx_peer_find_by_id */
#include <ol_txrx_internal.h>  /* TXRX_ASSERT */
#include <ol_rx_reorder.h>
#include <ol_rx_defrag.h>

#if QCA_PARTNER_DIRECTLINK_RX
#define QCA_PARTNER_DIRECTLINK_OL_RX_REORDER 1
#include "ath_carr_pltfrm.h"
#undef QCA_PARTNER_DIRECTLINK_OL_RX_REORDER
#endif
#include <init_deinit_lmac.h>

/*=== data types and defines ===*/
#define OL_RX_REORDER_ROUND_PWR2(value) g_log2ceil[value]

/*=== global variables ===*/

static char g_log2ceil[] = {
    1, // 0 -> 1
    1, // 1 -> 1
    2, // 2 -> 2
    4, 4, // 3-4 -> 4
    8, 8, 8, 8, // 5-8 -> 8
    16, 16, 16, 16, 16, 16, 16, 16, // 9-16 -> 16
    32, 32, 32, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 32, 32, // 17-32 -> 32
    64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, // 33-64 -> 64
};


/*=== function definitions ===*/

/* functions called by txrx components */

void ol_rx_reorder_init(struct ol_rx_reorder_t *rx_reorder, int tid)
{
    rx_reorder->win_sz_mask = 0;
    rx_reorder->array = &rx_reorder->base;
    rx_reorder->base.head = rx_reorder->base.tail = NULL;
    rx_reorder->tid = tid;
    rx_reorder->defrag_timeout_ms = 0;

    rx_reorder->defrag_waitlist_elem.tqe_next = NULL;
    rx_reorder->defrag_waitlist_elem.tqe_prev = NULL;
}

int
ol_rx_reorder_store(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    unsigned seq_num,
    qdf_nbuf_t head_msdu,
    qdf_nbuf_t tail_msdu)
{
    struct ol_rx_reorder_array_elem_t *rx_reorder_array_elem;
    TXRX_ASSERT2(tid < OL_TXRX_NUM_EXT_TIDS);

    if(peer->tids_rx_reorder[tid].array == NULL) {
        printk("%s:tids_rx_reorder.array for tid %d is NULL for peer %pK,"
                   " (%02x:%02x:%02x:%02x:%02x:%02x)\n"
                 "so skipping flush, This could be due to peer corruption \n",
                                                          __func__,tid, peer,
                                  peer->mac_addr.raw[0], peer->mac_addr.raw[1],
                                  peer->mac_addr.raw[2], peer->mac_addr.raw[3],
                                  peer->mac_addr.raw[4], peer->mac_addr.raw[5]);
                    return -1;
    }

    /*
     * if host duplicate detection is enabled and aggregation is not
     * on, check if the incoming sequence number is a duplication of
     * the one just received.
     */
    if (pdev->rx.flags.dup_check && peer->tids_rx_reorder[tid].win_sz_mask == 0) {
        /*
         * TBDXXX: should we check the retry bit is set or not? A strict
         * duplicate packet should be the one with retry bit set;
         * however, since many implementations do not set the retry bit,
         * ignore the check for now.
         */
        if (seq_num == peer->tids_last_seq[tid]) {
            return -1;
        }

    }

    peer->tids_last_seq[tid] = seq_num;

    seq_num &= peer->tids_rx_reorder[tid].win_sz_mask;
    rx_reorder_array_elem = &peer->tids_rx_reorder[tid].array[seq_num];
    if (rx_reorder_array_elem->head) {
        qdf_nbuf_set_next(rx_reorder_array_elem->tail, head_msdu);
    } else {
        rx_reorder_array_elem->head = head_msdu;
    }
    rx_reorder_array_elem->tail = tail_msdu;

    return 0;
}

void
ol_rx_reorder_release(
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    unsigned seq_num_start,
    unsigned seq_num_end)
{
    unsigned int win_sz_mask;
    struct ol_rx_reorder_array_elem_t *rx_reorder_array_elem;
    qdf_nbuf_t head_msdu = NULL;
    qdf_nbuf_t tail_msdu = NULL;
    unsigned orig_seq_num_start;
    struct ol_txrx_pdev_t *pdev = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;

    TXRX_ASSERT2(tid < OL_TXRX_NUM_EXT_TIDS);
    if (vdev == NULL)
        return;
    pdev = vdev->pdev;
    if (pdev == NULL)
        return;
    scn = (struct ol_ath_softc_net80211 *)
           lmac_get_pdev_feature_ptr((struct wlan_objmgr_pdev *)pdev->ctrl_pdev);
    if (scn == NULL)
        return;

    if(peer->tids_rx_reorder[tid].array == NULL) {
            if ((scn->soc->dbg.err_types.seq_num_errors % scn->soc->dbg.print_rate_limit) == 0) {
                printk("%s:tids_rx_reorder.array for tid %d is NULL for peer %pK,"
                       " (%02x:%02x:%02x:%02x:%02x:%02x)\n"
                     "so skipping flush, This could be due to peer corruption \n",
                                                              __func__,tid, peer,
                                      peer->mac_addr.raw[0], peer->mac_addr.raw[1],
                                      peer->mac_addr.raw[2], peer->mac_addr.raw[3],
                                      peer->mac_addr.raw[4], peer->mac_addr.raw[5]);
                        return;
            }
    }

    win_sz_mask = peer->tids_rx_reorder[tid].win_sz_mask;
    seq_num_start &= win_sz_mask;
    seq_num_end   &= win_sz_mask;

    orig_seq_num_start = seq_num_start;

    do {
        rx_reorder_array_elem =
            &peer->tids_rx_reorder[tid].array[seq_num_start];
        seq_num_start = (seq_num_start + 1) & win_sz_mask;

        if (qdf_likely(rx_reorder_array_elem->head)) {
            if (!head_msdu) {
                head_msdu = rx_reorder_array_elem->head;
                tail_msdu = rx_reorder_array_elem->tail;
                rx_reorder_array_elem->head = NULL;
                rx_reorder_array_elem->tail = NULL;
                continue;
            }
            qdf_assert_always(tail_msdu);
            qdf_nbuf_set_next(tail_msdu, rx_reorder_array_elem->head);
            tail_msdu = rx_reorder_array_elem->tail;
            rx_reorder_array_elem->head = rx_reorder_array_elem->tail = NULL;
        } else {
            (scn->soc->dbg.err_types.seq_num_errors)++;

            if ((scn->soc->dbg.err_types.seq_num_errors % scn->soc->dbg.print_rate_limit) == 0) {
                printk("=== Reducing pace for error prints ==== \n");
                printk("Start seq_num %d End seq_num %d tid %d failed seq %d - error occured %d times\n",
                        orig_seq_num_start, seq_num_end, tid, seq_num_start, scn->soc->dbg.err_types.seq_num_errors);
            }
        }
    } while (seq_num_start != seq_num_end);

    /* rx_opt_proc takes a NULL-terminated list of msdu netbufs */
    if (qdf_likely(head_msdu)) {
        qdf_assert_always(tail_msdu);
        qdf_nbuf_set_next(tail_msdu, NULL);
        peer->rx_opt_proc(vdev, peer, tid, head_msdu);
    }
}

void
ol_rx_reorder_flush(
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    unsigned seq_num_start,
    unsigned seq_num_end,
    enum htt_rx_flush_action action)
{
    struct ol_txrx_pdev_t *pdev;
    unsigned win_sz_mask;
    struct ol_rx_reorder_array_elem_t *rx_reorder_array_elem;
    qdf_nbuf_t head_msdu = NULL;
    qdf_nbuf_t tail_msdu = NULL;

    pdev = vdev->pdev;
    win_sz_mask = peer->tids_rx_reorder[tid].win_sz_mask;
    seq_num_start &= win_sz_mask;
    seq_num_end   &= win_sz_mask;

    if(peer->tids_rx_reorder[tid].array == NULL) {
        printk("%s:tids_rx_reorder.array for tid %d is NULL for peer %pK,"
                   " (%02x:%02x:%02x:%02x:%02x:%02x)\n"
                 "so skipping flush, This could be due to peer corruption \n",
                                                          __func__,tid, peer,
                                  peer->mac_addr.raw[0], peer->mac_addr.raw[1],
                                  peer->mac_addr.raw[2], peer->mac_addr.raw[3],
                                  peer->mac_addr.raw[4], peer->mac_addr.raw[5]);
                    return;
    }

    do {
        rx_reorder_array_elem =
            &peer->tids_rx_reorder[tid].array[seq_num_start];
        seq_num_start = (seq_num_start + 1) & win_sz_mask;

        if (rx_reorder_array_elem->head) {
            if (head_msdu == NULL) {
                head_msdu = rx_reorder_array_elem->head;
                tail_msdu = rx_reorder_array_elem->tail;
                rx_reorder_array_elem->head = NULL;
                rx_reorder_array_elem->tail = NULL;
                continue;
            }
            qdf_nbuf_set_next(tail_msdu, rx_reorder_array_elem->head);
            tail_msdu = rx_reorder_array_elem->tail;
            rx_reorder_array_elem->head = rx_reorder_array_elem->tail = NULL;
        }
    } while (seq_num_start != seq_num_end);

    ol_rx_defrag_waitlist_remove(peer, tid);

    if (head_msdu) {
        /* rx_opt_proc takes a NULL-terminated list of msdu netbufs */
        qdf_nbuf_set_next(tail_msdu, NULL);
        if (action == htt_rx_flush_release) {
            peer->rx_opt_proc(vdev, peer, tid, head_msdu);
        } else {
            do {
                qdf_nbuf_t next;
                next = qdf_nbuf_next(head_msdu);
                htt_rx_desc_frame_free(pdev->htt_pdev, head_msdu);
                head_msdu = next;
            } while (head_msdu);
        }
    }
}

void
ol_non_aggr_re_order_flush(
     struct ol_txrx_vdev_t *vdev,
     struct ol_txrx_peer_t *peer, unsigned tid)
{
    struct ol_txrx_pdev_t *pdev;
    struct ol_rx_reorder_array_elem_t *rx_reorder_array_elem;
    qdf_nbuf_t head_msdu = NULL;
    qdf_nbuf_t tail_msdu = NULL;
    qdf_nbuf_t next;

    pdev = vdev->pdev;
    rx_reorder_array_elem = &peer->tids_rx_reorder[tid].base;

    if (rx_reorder_array_elem->head) {
         head_msdu = rx_reorder_array_elem->head;
         tail_msdu = rx_reorder_array_elem->tail;
    }

    if (head_msdu) {
         qdf_nbuf_set_next(tail_msdu, NULL);
         while(head_msdu)
         {
             next = qdf_nbuf_next(head_msdu);
             qdf_nbuf_free(head_msdu);
             head_msdu = next;
         }
    }
    /*Assing NULL to head and tail */
    rx_reorder_array_elem->head = NULL;
    rx_reorder_array_elem->tail = NULL;
}

void
ol_rx_reorder_peer_cleanup(
    struct ol_txrx_vdev_t *vdev, struct ol_txrx_peer_t *peer)
{
    int tid;
    struct ol_rx_reorder_t *rx_reorder;

    for (tid = 0; tid < OL_TXRX_NUM_EXT_TIDS; tid++) {
        rx_reorder = &peer->tids_rx_reorder[tid];

        ol_rx_reorder_flush(vdev, peer, tid, 0, 0, htt_rx_flush_discard);
        ol_non_aggr_re_order_flush(vdev, peer, tid);
        if (rx_reorder->array != &rx_reorder->base)
            qdf_mem_free(rx_reorder->array);
    }
}

/* functions called by HTT */

void
ol_rx_addba_handler(
    ol_txrx_pdev_handle pdev,
    u_int16_t peer_id,
    u_int8_t tid,
    u_int8_t win_sz)
{
    unsigned round_pwr2_win_sz, array_size;
    struct ol_txrx_peer_t *peer;
    struct ol_rx_reorder_t *rx_reorder;

    peer = ol_txrx_peer_find_by_id(pdev, peer_id);
    if (peer == NULL) {
        return;
    }
    rx_reorder = &peer->tids_rx_reorder[tid];

    TXRX_ASSERT2(win_sz <= 64);
    round_pwr2_win_sz = OL_RX_REORDER_ROUND_PWR2(win_sz);
    array_size = round_pwr2_win_sz * sizeof(struct ol_rx_reorder_array_elem_t);

    /* Free up re-order array with pending non-aggregates */
    ol_non_aggr_re_order_flush(peer->vdev, peer, tid);

    rx_reorder->array = qdf_mem_malloc(array_size);
    if (rx_reorder->array == NULL) {
        printk("%s:Memory alloc failed,"
                " So Rx will fail for this TID(%d), peer-id(%d) "
                 "as Rx store/release will fail \n",__func__, tid, peer_id);
        return;
    }
    qdf_mem_set(rx_reorder->array, array_size, 0x0);

    rx_reorder->win_sz_mask = round_pwr2_win_sz - 1;

#if QCA_PARTNER_DIRECTLINK_RX
    /* provide addba information to partner side */
    if (CE_is_directlink(pdev->ce_tx_hdl)) {
        ol_rx_addba_handler_partner(peer, tid, round_pwr2_win_sz);
    }
#endif /* QCA_PARTNER_DIRECTLINK_RX */
}

void
ol_rx_delba_handler(
    ol_txrx_pdev_handle pdev,
    u_int16_t peer_id,
    u_int8_t tid)
{
    struct ol_txrx_peer_t *peer;
    struct ol_rx_reorder_t *rx_reorder;

    peer = ol_txrx_peer_find_by_id(pdev, peer_id);
    if (peer == NULL) {
        return;
    }
    rx_reorder = &peer->tids_rx_reorder[tid];

    /* deallocate the old rx reorder array if allocated and not deallocate only*/
    if (rx_reorder->array != &rx_reorder->base){
        /* Free up re-order array with pending aggregates */
        ol_rx_reorder_flush(peer->vdev, peer, tid, 0, 0, htt_rx_flush_discard);
        qdf_mem_free(rx_reorder->array);
    }
    else
        qdf_print("Check BA session state.Peer:%pK,id:%d,tid:%d",peer,peer_id,tid);

    /* set up the TID with default parameters (ARQ window size = 1) */
    ol_rx_reorder_init(rx_reorder, tid);

#if QCA_PARTNER_DIRECTLINK_RX
    /* provide delba information to partner side */
    if (CE_is_directlink(pdev->ce_tx_hdl)) {
        ol_rx_delba_handler_partner(peer, tid);
    }
#endif /* QCA_PARTNER_DIRECTLINK_RX */
}

void
ol_rx_flush_handler(
    ol_txrx_pdev_handle pdev,
    u_int16_t peer_id,
    u_int8_t tid,
    int seq_num_start,
    int seq_num_end,
    enum htt_rx_flush_action action)
{
    struct ol_txrx_vdev_t *vdev = NULL;
    void *rx_desc;
    struct ol_txrx_peer_t *peer;
    int seq_num;
    struct ol_rx_reorder_array_elem_t *rx_reorder_array_elem;
    htt_pdev_handle htt_pdev = ((struct ol_txrx_pdev_t *)pdev)->htt_pdev;

    peer = ol_txrx_peer_find_by_id(pdev, peer_id);
    if (peer) {
        vdev = peer->vdev;
    } else {
        return;
    }
    if(peer->tids_rx_reorder[tid].array == NULL) {
        printk("%s:tids_rx_reorder.array for tid %d is NULL for peer %pK,"
                   " (%02x:%02x:%02x:%02x:%02x:%02x)\n"
                 "so skipping flush, This could be due to peer corruption \n",
                                                          __func__,tid, peer,
                                  peer->mac_addr.raw[0], peer->mac_addr.raw[1],
                                  peer->mac_addr.raw[2], peer->mac_addr.raw[3],
                                  peer->mac_addr.raw[4], peer->mac_addr.raw[5]);
                    return;
    }

    seq_num = seq_num_start & peer->tids_rx_reorder[tid].win_sz_mask;
    rx_reorder_array_elem = &peer->tids_rx_reorder[tid].array[seq_num];
    if (rx_reorder_array_elem->head) {
        rx_desc =
            htt_rx_msdu_desc_retrieve(htt_pdev, rx_reorder_array_elem->head);
        if (htt_rx_msdu_is_frag(htt_pdev, rx_desc)) {
            ol_rx_reorder_flush_frag(htt_pdev, peer, tid, seq_num_start);
            /*
             * Assuming flush message sent seperately for frags
             * and for normal frames
             */
            return;
        }
    }
    ol_rx_reorder_flush(
        vdev, peer, tid, seq_num_start, seq_num_end, action);
}

#if defined(ENABLE_RX_REORDER_TRACE)

A_STATUS
ol_rx_reorder_trace_attach(ol_txrx_pdev_handle _pdev)
{
    int num_elems;
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)_pdev;

    num_elems = 1 << TXRX_RX_REORDER_TRACE_SIZE_LOG2;
    pdev->rx_reorder_trace.idx = 0;
    pdev->rx_reorder_trace.cnt = 0;
    pdev->rx_reorder_trace.mask = num_elems - 1;
    pdev->rx_reorder_trace.data = qdf_mem_malloc(
        sizeof(*pdev->rx_reorder_trace.data) * num_elems);
    if (! pdev->rx_reorder_trace.data) {
        return A_ERROR;
    }
    while (--num_elems >= 0) {
        pdev->rx_reorder_trace.data[num_elems].seq_num = 0xffff;
    }

    return A_OK;
}

void
ol_rx_reorder_trace_detach(ol_txrx_pdev_handle pdev)
{
    qdf_mem_free(((struct ol_txrx_pdev_t *)pdev)->rx_reorder_trace.data);
}

void
ol_rx_reorder_trace_add(
    ol_txrx_pdev_handle _pdev, u_int8_t tid, u_int16_t seq_num, int num_mpdus)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)_pdev;
    u_int32_t idx = pdev->rx_reorder_trace.idx;

    pdev->rx_reorder_trace.data[idx].tid = tid;
    pdev->rx_reorder_trace.data[idx].seq_num = seq_num;
    pdev->rx_reorder_trace.data[idx].num_mpdus = num_mpdus;
    pdev->rx_reorder_trace.cnt++;
    idx++;
    pdev->rx_reorder_trace.idx = idx & pdev->rx_reorder_trace.mask;
}

void
ol_rx_reorder_trace_display(ol_txrx_pdev_handle _pdev, int just_once, int limit)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)_pdev;
    static int print_count = 0;
    u_int32_t i, start, end;
    u_int64_t cnt;
    int elems;

    if (print_count != 0 && just_once) {
        return;
    }
    print_count++;

    end = pdev->rx_reorder_trace.idx;
    if (pdev->rx_reorder_trace.data[end].seq_num == 0xffff) {
        /* trace log has not yet wrapped around - start at the top */
        start = 0;
        cnt = 0;
    } else {
        start = end;
        cnt = pdev->rx_reorder_trace.cnt - (pdev->rx_reorder_trace.mask + 1);
    }
    elems = (end - 1 - start) & pdev->rx_reorder_trace.mask;
    if (limit > 0 && elems > limit) {
        int delta;
        delta = elems - limit;
        start += delta;
        start &= pdev->rx_reorder_trace.mask;
        cnt += delta;
    }

    i = start;
    qdf_print("                     seq");
    qdf_print("   count   idx  tid  num (LSBs)");
    do {
        u_int16_t seq_num;
        seq_num = pdev->rx_reorder_trace.data[i].seq_num;
        if (seq_num < (1 << 14)) {
            qdf_print("  %6lld  %4d  %3d %4d (%d)",
                cnt, i, pdev->rx_reorder_trace.data[i].tid,
                seq_num, seq_num & 63);
        } else {
            int err = TXRX_SEQ_NUM_ERR(seq_num);
            qdf_print("  %6lld  %4d err %d (%d MPDUs)",
                cnt, i, err, pdev->rx_reorder_trace.data[i].num_mpdus);
        }
        cnt++;
        i++;
        i &= pdev->rx_reorder_trace.mask;
    } while (i != end);
}

#endif /* ENABLE_RX_REORDER_TRACE */

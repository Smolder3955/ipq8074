/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <ol_htt_api.h>
#include <ol_txrx_api.h>
#include <ol_txrx_htt_api.h>
#include <ol_htt_rx_api.h>
#include <ol_txrx_types.h>
#include <ol_rx_reorder.h>
#include <ol_rx_pn.h>
#include <ol_rx_fwd.h>
#include <ol_rx.h>
#include <ol_txrx_internal.h>
#include <ol_ctrl_txrx_api.h>
#include <ol_txrx_peer_find.h>
#include <qdf_nbuf.h>
#include <ieee80211.h>
#include <qdf_util.h>
#include <athdefs.h>
#include <qdf_mem.h>
#include <ol_rx_defrag.h>
/* #include <qdf_io.h> */
#include <enet.h>
#include <qdf_time.h>      /* qdf_time */

#if QCA_PARTNER_DIRECTLINK_RX
#define QCA_PARTNER_DIRECTLINK_OL_RX_DEFRAG 1
#include "ath_carr_pltfrm.h"
#undef QCA_PARTNER_DIRECTLINK_OL_RX_DEFRAG
#endif

#define	DEFRAG_IEEE80211_ADDR_EQ(a1, a2) \
    (qdf_mem_cmp(a1, a2, IEEE80211_ADDR_LEN) == 0)

#define	DEFRAG_IEEE80211_ADDR_COPY(dst, src) \
    qdf_mem_copy(dst, src, IEEE80211_ADDR_LEN)

#define DEFRAG_IEEE80211_QOS_HAS_SEQ(wh) \
    (((wh)->i_fc[0] & \
      (IEEE80211_FC0_TYPE_MASK | IEEE80211_FC0_SUBTYPE_QOS)) == \
      (IEEE80211_FC0_TYPE_DATA | IEEE80211_FC0_SUBTYPE_QOS))

#define DEFRAG_IEEE80211_QOS_GET_TID(_x) \
    ((_x)->i_qos[0] & IEEE80211_QOS_TID)

const struct ol_rx_defrag_cipher f_ccmp = {
    "AES-CCM",
    IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN + IEEE80211_WEP_EXTIVLEN,
    IEEE80211_WEP_MICLEN,
    0,
};

const struct ol_rx_defrag_cipher f_tkip  = {
    "TKIP",
    IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN + IEEE80211_WEP_EXTIVLEN,
    IEEE80211_WEP_CRCLEN,
    IEEE80211_WEP_MICLEN,
};

const struct ol_rx_defrag_cipher f_wep = {
    "WEP",
    IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN,
    IEEE80211_WEP_CRCLEN,
    0,
};


#define IEEE80211_WPI_SMS4_IVLEN        16  /* 128bit */
#define IEEE80211_WPI_SMS4_KIDLEN       1   /* 1 octet */
#define IEEE80211_WPI_SMS4_PADLEN       1   /* 1 octet */
#define IEEE80211_WPI_SMS4_MICLEN       16  /* trailing MIC */
static const struct ol_rx_defrag_cipher f_wpi_sms4 = {
    "WPI_SMS4",
    IEEE80211_WPI_SMS4_KIDLEN +IEEE80211_WPI_SMS4_PADLEN + IEEE80211_WPI_SMS4_IVLEN,
    IEEE80211_WPI_SMS4_MICLEN,
    0,
};


/*
 * Process incoming fragments
 */
void
ol_rx_frag_indication_handler(
    ol_txrx_pdev_handle _pdev,
    qdf_nbuf_t rx_frag_ind_msg,
    u_int16_t peer_id,
    u_int8_t tid)
{
    int seq_num, seq_num_start, seq_num_end;
    struct ol_txrx_vdev_t *vdev = NULL;
    struct ol_txrx_peer_t *peer;
    htt_pdev_handle htt_pdev;
    qdf_nbuf_t head_msdu = NULL;
    qdf_nbuf_t tail_msdu = NULL;
    void *rx_mpdu_desc;
    u_int32_t npackets = 0;
    int msdu_chaining;

    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)_pdev;
    htt_pdev = pdev->htt_pdev;
    peer = ol_txrx_peer_find_by_id(_pdev, peer_id);
    if (peer) {
        vdev = peer->vdev;
    }

    if (htt_rx_ind_flush(rx_frag_ind_msg) && peer) {
        htt_rx_frag_ind_flush_seq_num_range(
                rx_frag_ind_msg, &seq_num_start, &seq_num_end);
        /*
         * Assuming flush indication for frags sent from target is seperate
         * from normal frames
         */
        ol_rx_reorder_flush_frag(htt_pdev, peer, tid, seq_num_start);
    }
    if (peer) {
        msdu_chaining = htt_rx_amsdu_pop(htt_pdev,
#if QCA_OL_SUPPORT_RAWMODE_TXRX
                vdev,
#endif
                rx_frag_ind_msg, &head_msdu, &tail_msdu, &npackets);
        if(msdu_chaining || (npackets > 1) ) {
            /*qdf_assert(head_msdu == tail_msdu);*/
            /*
             * TBDXXX - to deliver SDU with chaining, we need to
             * stitch those scattered buffers into one single buffer.
             * Just discard it now.
             *
             * Also, discard AMSDUs recieved as frags.
             */
            while (1) {
                qdf_nbuf_t next;
                next = qdf_nbuf_next(head_msdu);
                htt_rx_desc_frame_free(htt_pdev, head_msdu);
                if (head_msdu == tail_msdu) {
                    break;
                }
                head_msdu = next;
            }
        } else {

#if QCA_PARTNER_DIRECTLINK_RX
            /*
             * For Direct Link RX, htt_rx_msdu_desc_retrieve() would finally
             * call partner API for get rx descriptor.
             */
            if (CE_is_directlink(pdev->ce_tx_hdl)) {
                rx_mpdu_desc =
                    htt_rx_msdu_desc_retrieve(htt_pdev, head_msdu);
            } else
#endif /* QCA_PARTNER_DIRECTLINK_RX */
            {
                rx_mpdu_desc = htt_rx_mpdu_desc_list_next(htt_pdev, rx_frag_ind_msg);
            } /* QCA_PARTNER_DIRECTLINK_RX */

            seq_num = htt_rx_mpdu_desc_seq_num(htt_pdev, rx_mpdu_desc);
            if( ol_rx_reorder_store_frag(pdev, peer, tid, seq_num, head_msdu) < 0 ) {
                htt_rx_desc_frame_free(htt_pdev, head_msdu);
            }
        }
    } else {
        /* invalid frame - discard it */
        msdu_chaining = htt_rx_amsdu_pop(htt_pdev,
#if QCA_OL_SUPPORT_RAWMODE_TXRX
                vdev,
#endif
                rx_frag_ind_msg, &head_msdu, &tail_msdu, &npackets);

        if(msdu_chaining || (npackets > 1) ) {
            while (1) {
                qdf_nbuf_t next;
                next = qdf_nbuf_next(head_msdu);
                htt_rx_desc_frame_free(htt_pdev, head_msdu);
                if (head_msdu == tail_msdu) {
                    break;
                }
                head_msdu = next;
            }
        } else {
            htt_rx_desc_frame_free(htt_pdev, head_msdu);
        }
    }
    /* request HTT to provide new rx MSDU buffers for the target to fill. */
#if QCA_PARTNER_DIRECTLINK_RX
    /*
     * For Direct Link RX, replenish already done on partner side so skip it.
     */
    if (!CE_is_directlink(pdev->ce_tx_hdl)) {
        htt_rx_msdu_buff_replenish(htt_pdev);
    }
#else /* QCA_PARTNER_DIRECTLINK_RX */
    htt_rx_msdu_buff_replenish(htt_pdev);
#endif /* QCA_PARTNER_DIRECTLINK_RX */
}

/*
 * Flushing fragments
 */
void
ol_rx_reorder_flush_frag(
    htt_pdev_handle htt_pdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    int seq_num)
{
    struct ol_rx_reorder_array_elem_t *rx_reorder_array_elem;
    int seq;
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
    seq = seq_num & peer->tids_rx_reorder[tid].win_sz_mask;
    rx_reorder_array_elem = &peer->tids_rx_reorder[tid].array[seq];
    if (rx_reorder_array_elem->head) {
        ol_rx_frames_free(htt_pdev, rx_reorder_array_elem->head);
        rx_reorder_array_elem->head = NULL;
        rx_reorder_array_elem->tail = NULL;
    }
}

/*
 * Reorder and store fragments
 */
int
ol_rx_reorder_store_frag(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    unsigned seq_num,
    qdf_nbuf_t frag)
{
    struct ieee80211_frame *fmac_hdr, *mac_hdr;
    u_int8_t fragno, more_frag, all_frag_present = 0;
    struct ol_rx_reorder_array_elem_t *rx_reorder_array_elem;
    u_int16_t frxseq, rxseq, seq;
    htt_pdev_handle htt_pdev = pdev->htt_pdev;


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
    /* If BA session is active for this TID. Then drop the frags */
    if( peer->tids_rx_reorder[tid].win_sz_mask != 0 ) {
        printk("Recieved Frags in BA session - Dropping the frames\n");
        return -1;
    }

    seq = seq_num & peer->tids_rx_reorder[tid].win_sz_mask;
    rx_reorder_array_elem = &peer->tids_rx_reorder[tid].array[seq];

    mac_hdr = (struct ieee80211_frame *) qdf_nbuf_data(frag);
    rxseq = qdf_le16_to_cpu(*(u_int16_t *) mac_hdr->i_seq) >>
        IEEE80211_SEQ_SEQ_SHIFT;
    fragno = qdf_le16_to_cpu(*(u_int16_t *) mac_hdr->i_seq) &
        IEEE80211_SEQ_FRAG_MASK;
    more_frag = mac_hdr->i_fc[1] & IEEE80211_FC1_MORE_FRAG;

    if ((!more_frag) && (!fragno) && (!rx_reorder_array_elem->head)) {
        rx_reorder_array_elem->head = frag;
        rx_reorder_array_elem->tail = frag;
        qdf_nbuf_set_next(frag, NULL);
        ol_rx_defrag(pdev, peer, tid, rx_reorder_array_elem->head);
        rx_reorder_array_elem->head = NULL;
        rx_reorder_array_elem->tail = NULL;
        return 0;
    }
    if (rx_reorder_array_elem->head) {
        fmac_hdr = (struct ieee80211_frame *)
            qdf_nbuf_data(rx_reorder_array_elem->head);
        frxseq = qdf_le16_to_cpu(*(u_int16_t *) fmac_hdr->i_seq) >>
            IEEE80211_SEQ_SEQ_SHIFT;
        if (rxseq != frxseq ||
            !DEFRAG_IEEE80211_ADDR_EQ(mac_hdr->i_addr1, fmac_hdr->i_addr1) ||
            !DEFRAG_IEEE80211_ADDR_EQ(mac_hdr->i_addr2, fmac_hdr->i_addr2))
        {
            ol_rx_frames_free(htt_pdev, rx_reorder_array_elem->head);
            rx_reorder_array_elem->head = NULL;
            rx_reorder_array_elem->tail = NULL;
            TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
                "\n ol_rx_reorder_store:  %s mismatch \n",
                (rxseq == frxseq) ? "address" : "seq number");
        }
    }
    ol_rx_fraglist_insert(htt_pdev, &rx_reorder_array_elem->head,
        &rx_reorder_array_elem->tail, frag, &all_frag_present);

    if (pdev->rx.flags.defrag_timeout_check) {
        ol_rx_defrag_waitlist_remove(peer, tid);
    }

    if (all_frag_present) {
        ol_rx_defrag(pdev, peer, tid, rx_reorder_array_elem->head);
        rx_reorder_array_elem->head = NULL;
        rx_reorder_array_elem->tail = NULL;
        peer->tids_rx_reorder[tid].defrag_timeout_ms = 0;
        peer->tids_last_seq[tid] = seq_num;
    } else if (pdev->rx.flags.defrag_timeout_check) {
        u_int32_t now_ms = qdf_system_ticks_to_msecs(qdf_system_ticks());

        peer->tids_rx_reorder[tid].defrag_timeout_ms = now_ms + pdev->rx.defrag.timeout_ms;
        ol_rx_defrag_waitlist_add(peer, tid);
    }
    return 0;
}

/*
 * Insert and store fragments
 */
void
ol_rx_fraglist_insert(
    htt_pdev_handle htt_pdev,
    qdf_nbuf_t *head_addr,
    qdf_nbuf_t *tail_addr,
    qdf_nbuf_t frag,
    u_int8_t *all_frag_present)
{
    qdf_nbuf_t next, prev = NULL, cur = *head_addr;
    struct ieee80211_frame *mac_hdr, *cmac_hdr, *next_hdr, *lmac_hdr;
    u_int8_t fragno, cur_fragno, lfragno, next_fragno;
    u_int8_t last_morefrag = 1, count = 0;

    qdf_assert(frag);
    mac_hdr = (struct ieee80211_frame *) qdf_nbuf_data(frag);
    fragno = qdf_le16_to_cpu(*(u_int16_t *) mac_hdr->i_seq) &
        IEEE80211_SEQ_FRAG_MASK;

    if (!(*head_addr)) {
        *head_addr = frag;
        *tail_addr = frag;
        qdf_nbuf_set_next(*tail_addr, NULL);
        return;
    }
    /* For efficiency, compare with tail first */
    lmac_hdr = (struct ieee80211_frame *) qdf_nbuf_data(*tail_addr);
    lfragno = qdf_le16_to_cpu(*(u_int16_t *) lmac_hdr->i_seq) &
        IEEE80211_SEQ_FRAG_MASK;
    if (fragno > lfragno) {
        qdf_nbuf_set_next(*tail_addr, frag);
        *tail_addr = frag;
        qdf_nbuf_set_next(*tail_addr, NULL);
    } else {
        do {
            cmac_hdr = (struct ieee80211_frame *) qdf_nbuf_data(cur);
            cur_fragno = qdf_le16_to_cpu(*(u_int16_t *) cmac_hdr->i_seq) &
                IEEE80211_SEQ_FRAG_MASK;
            prev = cur;
            cur = qdf_nbuf_next(cur);
        } while (fragno > cur_fragno);

        if (fragno == cur_fragno) {
            htt_rx_desc_frame_free(htt_pdev, frag);
            *all_frag_present = 0;
            return;
        } else {
            qdf_nbuf_set_next(prev, frag);
            qdf_nbuf_set_next(frag, cur);
        }
    }
    next = qdf_nbuf_next(*head_addr);
    lmac_hdr = (struct ieee80211_frame *) qdf_nbuf_data(*tail_addr);
    last_morefrag = lmac_hdr->i_fc[1] & IEEE80211_FC1_MORE_FRAG;
    if (!last_morefrag) {
        do {
            next_hdr = (struct ieee80211_frame *) qdf_nbuf_data(next);
            next_fragno = qdf_le16_to_cpu(*(u_int16_t *) next_hdr->i_seq) &
                IEEE80211_SEQ_FRAG_MASK;
            count++;
            if (next_fragno != count) {
                break;
            }
            next = qdf_nbuf_next(next);
        } while (next);

        if (!next) {
            *all_frag_present = 1;
            return;
        }
    }
    *all_frag_present = 0;
}

/*
 * add tid to pending fragment wait list
 */
void
ol_rx_defrag_waitlist_add(
    struct ol_txrx_peer_t *peer,
    unsigned tid)
{
    struct ol_txrx_pdev_t *pdev = peer->vdev->pdev;
    struct ol_rx_reorder_t *rx_reorder = &peer->tids_rx_reorder[tid];

    TAILQ_INSERT_TAIL(&pdev->rx.defrag.waitlist, rx_reorder,
            defrag_waitlist_elem);
}

/*
 * remove tid from pending fragment wait list
 */
void
ol_rx_defrag_waitlist_remove(
    struct ol_txrx_peer_t *peer,
    unsigned tid)
{
    struct ol_txrx_pdev_t *pdev = peer->vdev->pdev;
    struct ol_rx_reorder_t *rx_reorder = &peer->tids_rx_reorder[tid];

    if (rx_reorder->defrag_waitlist_elem.tqe_next != NULL ||
        rx_reorder->defrag_waitlist_elem.tqe_prev != NULL) {

        TAILQ_REMOVE(&pdev->rx.defrag.waitlist, rx_reorder,
                defrag_waitlist_elem);

        rx_reorder->defrag_waitlist_elem.tqe_next = NULL;
        rx_reorder->defrag_waitlist_elem.tqe_prev = NULL;
    }
}

#ifndef container_of
#define container_of(ptr, type, member) ((type *)( \
                (char *)(ptr) - (char *)(&((type *)0)->member) ) )
#endif

/*
 * flush stale fragments from the waitlist
 */
void
ol_rx_defrag_waitlist_flush(
    struct ol_txrx_pdev_t *pdev)
{
    struct ol_rx_reorder_t *rx_reorder, *tmp;
    u_int32_t now_ms = qdf_system_ticks_to_msecs(qdf_system_ticks());

    TAILQ_FOREACH_SAFE(rx_reorder, &pdev->rx.defrag.waitlist,
            defrag_waitlist_elem, tmp) {
        struct ol_txrx_peer_t *peer;
        struct ol_rx_reorder_t *rx_reorder_base;
        unsigned tid;

        if (rx_reorder->defrag_timeout_ms > now_ms) {
            break;
        }

        tid = rx_reorder->tid;
        /* get index 0 of the rx_reorder array */
        rx_reorder_base = rx_reorder - tid;
        peer = container_of(rx_reorder_base, struct ol_txrx_peer_t, tids_rx_reorder[0]);

        ol_rx_defrag_waitlist_remove(peer, tid);
        ol_rx_reorder_flush_frag(pdev->htt_pdev, peer, tid, 0 /* fragments always stored at seq 0*/);
    }
}


int
ol_rx_frag_wapi_decap(
    qdf_nbuf_t nbuf,
    u_int16_t hdrlen)
{
    struct ieee80211_frame *wh;
    u_int8_t *ivp, *origHdr;

    origHdr = (u_int8_t *) qdf_nbuf_data(nbuf);
    wh = (struct ieee80211_frame *) origHdr;
    ivp = origHdr + hdrlen;
    qdf_mem_move(origHdr + f_wpi_sms4.ic_header, origHdr, hdrlen);
    qdf_nbuf_pull_head(nbuf, f_wpi_sms4.ic_header);

    return OL_RX_DEFRAG_OK;
}


int
ol_rx_frag_wapi_demic(
    qdf_nbuf_t wbuf,
    u_int16_t hdrlen)
{
    struct ieee80211_frame *wh;
    u_int8_t *ivp, *origHdr;

    origHdr = (u_int8_t *) qdf_nbuf_data(wbuf);
    wh = (struct ieee80211_frame *) origHdr;
    ivp = origHdr + hdrlen;
    qdf_nbuf_trim_tail(wbuf, f_wpi_sms4.ic_trailer);

    return OL_RX_DEFRAG_OK;
}

/*
 * Handling security checking and processing fragments
 */
void
ol_rx_defrag(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    qdf_nbuf_t frag_list)
{
    struct ol_txrx_vdev_t *vdev = NULL;
    qdf_nbuf_t tmp_next, msdu, prev = NULL, cur = frag_list;
    u_int8_t index, tkip_demic = 0;
    u_int16_t hdr_space;
    void *rx_desc;
    struct ieee80211_frame *wh;
    u_int8_t key[DEFRAG_IEEE80211_KEY_LEN];

    htt_pdev_handle htt_pdev = pdev->htt_pdev;
    vdev = peer->vdev;

    /* bypass defrag for safe mode */
    if (vdev->safemode) {
        ol_rx_deliver(vdev, peer, tid, frag_list);
        return;
    }

    while (cur) {
        tmp_next = qdf_nbuf_next(cur);
        qdf_nbuf_set_next(cur, NULL);
        if (!ol_rx_pn_check_base(vdev, peer, tid, cur)) {
            /* PN check failed,discard frags */
            if (prev) {
                qdf_nbuf_set_next(prev, NULL);
                ol_rx_frames_free(htt_pdev, frag_list);
            }
            ol_rx_frames_free(htt_pdev, tmp_next);
            TXRX_PRINT(TXRX_PRINT_LEVEL_ERR, "ol_rx_defrag: PN Check failed\n");
            return;
        }
        /* remove FCS from each fragment */
        qdf_nbuf_trim_tail(cur, DEFRAG_IEEE80211_FCS_LEN);
        prev = cur;
        qdf_nbuf_set_next(cur, tmp_next);
        cur = tmp_next;
    }
    cur = frag_list;
    wh = (struct ieee80211_frame *) qdf_nbuf_data(cur);
    hdr_space = ol_rx_frag_hdrsize(wh);
    rx_desc = htt_rx_msdu_desc_retrieve(htt_pdev, frag_list);
    qdf_assert(htt_rx_msdu_has_wlan_mcast_flag(htt_pdev, rx_desc));
    index = htt_rx_msdu_is_wlan_mcast(htt_pdev, rx_desc) ?
        txrx_sec_mcast : txrx_sec_ucast;

    switch (peer->security[index].sec_type) {
    case htt_sec_type_tkip:
        tkip_demic = 1;
        /* fall-through to rest of tkip ops */
    case htt_sec_type_tkip_nomic:
        while (cur) {
            tmp_next = qdf_nbuf_next(cur);
            if (!ol_rx_frag_tkip_decap(cur, hdr_space)) {
                /* TKIP decap failed, discard frags */
                ol_rx_frames_free(htt_pdev, frag_list);
                TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
                    "\n ol_rx_defrag: TKIP decap failed\n");
                return;
            }
            cur = tmp_next;
        }
        break;

    case htt_sec_type_aes_ccmp:
        while (cur) {
            tmp_next = qdf_nbuf_next(cur);
            if (!ol_rx_frag_ccmp_demic(cur, hdr_space)) {
                /* CCMP demic failed, discard frags */
                ol_rx_frames_free(htt_pdev, frag_list);
                TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
                    "\n ol_rx_defrag: CCMP demic failed\n");
                return;
            }
            if (!ol_rx_frag_ccmp_decap(cur, hdr_space)) {
                /* CCMP decap failed, discard frags */
                ol_rx_frames_free(htt_pdev, frag_list);
                TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
                    "\n ol_rx_defrag: CCMP decap failed\n");
                return;
            }
            cur = tmp_next;
        }
        break;

    case htt_sec_type_wep128:
    case htt_sec_type_wep104:
    case htt_sec_type_wep40:
        while (cur) {
            tmp_next = qdf_nbuf_next(cur);
            if (!ol_rx_frag_wep_decap(cur, hdr_space)) {
                /* WEP decap failed, discard frags */
                ol_rx_frames_free(htt_pdev, frag_list);
                TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
                    "\n ol_rx_defrag: WEP decap failed\n");
                return;
            }
            cur = tmp_next;
        }
        break;

    case htt_sec_type_wapi:
        while (cur) {
            tmp_next = qdf_nbuf_next(cur);
            if (!ol_rx_frag_wapi_demic(cur, hdr_space)) {
                /* WAPI demic failed, discard frags */
                ol_rx_frames_free(htt_pdev, frag_list);
                TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
                    "\n ol_rx_defrag: WAPI demic failed\n");
                return;
            }
            if (!ol_rx_frag_wapi_decap(cur, hdr_space)) {
                /* WAPI decap failed, discard frags */
                ol_rx_frames_free(htt_pdev, frag_list);
                TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
                    "\n ol_rx_defrag: WAPI decap failed\n");
                return;
            }
            cur = tmp_next;
        }
        break;


    default:
        break;
    }

    msdu = ol_rx_defrag_decap_recombine(htt_pdev, frag_list, hdr_space);
    if (!msdu) {
        return;
    }

    if (tkip_demic) {
        qdf_mem_copy(
            key,
            peer->security[index].michael_key,
            sizeof(peer->security[index].michael_key));
        if (!ol_rx_frag_tkip_demic(key, msdu, hdr_space)) {
            ol_rx_err(
                pdev->ctrl_pdev,
                vdev->vdev_id, peer->mac_addr.raw, tid, 0, OL_RX_ERR_TKIP_MIC,
                msdu);
            htt_rx_desc_frame_free(htt_pdev, msdu);
            TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
                "\n ol_rx_defrag: TKIP demic failed\n");
            return;
        }
    }
    wh = (struct ieee80211_frame *)qdf_nbuf_data(msdu);
    if (DEFRAG_IEEE80211_QOS_HAS_SEQ(wh)) {
        ol_rx_defrag_qos_decap(msdu, hdr_space);
    }
    if (!pdev->host_80211_enable) {
        if (ol_cfg_frame_type(pdev->ctrl_pdev) == wlan_frm_fmt_802_3) {
           ol_rx_defrag_nwifi_to_8023(msdu);
        }
    }
    /* This is a WAR due to FW bug. The host expects the FW to set the forward bit.
     * But currently the FW doesn't set for fragmented packets.
     * So call the host function ol_rx_intrabss_fw_check to decide to forward the packet
     * or not, and set the FW descriptor accordingly so that ol_rx_fwd_check
     * will forward the packet
     */
    if (ol_rx_intrabss_fwd_check(pdev->ctrl_pdev,
                vdev->vdev_id,&msdu->data[0]))
    {
#define FW_RX_DESC_DISCARD_M 0x1
#define FW_RX_DESC_FORWARD_M 0x2
        *((u_int8_t *) rx_desc) |=
            (FW_RX_DESC_FORWARD_M | FW_RX_DESC_DISCARD_M); //Set the foward and discard flag;
    }

    ol_rx_fwd_check(vdev, peer, tid, msdu);
}

/*
 * Handling TKIP processing for defragmentation
 */
int
ol_rx_frag_tkip_decap(qdf_nbuf_t msdu, u_int16_t hdrlen)
{
    struct ieee80211_frame *wh;
    u_int8_t *ivp, *origHdr;

    /* Header should have extended IV */
    origHdr = (u_int8_t*) qdf_nbuf_data(msdu);
    wh = (struct ieee80211_frame *) origHdr;
    ivp = origHdr + hdrlen;
    if (!(ivp[IEEE80211_WEP_IVLEN] & IEEE80211_WEP_EXTIV)) {
        return OL_RX_DEFRAG_ERR;
    }
    qdf_mem_move(origHdr + f_tkip.ic_header, origHdr, hdrlen);
    qdf_nbuf_pull_head(msdu, f_tkip.ic_header);
    qdf_nbuf_trim_tail(msdu, f_tkip.ic_trailer);

    return OL_RX_DEFRAG_OK;
}

/*
 * Verify and strip MIC from the frame.
 */
int
ol_rx_frag_tkip_demic(const u_int8_t *key, qdf_nbuf_t msdu, u_int16_t hdrlen)
{
    int status;
    u_int16_t pktlen;
    struct ieee80211_frame *wh;
    u_int8_t mic[IEEE80211_WEP_MICLEN] = {0};
    u_int8_t mic0[IEEE80211_WEP_MICLEN] = {0};

    wh = (struct ieee80211_frame *)qdf_nbuf_data(msdu);
    pktlen = ol_rx_defrag_len(msdu);
    status = ol_rx_defrag_mic(
        key, msdu, hdrlen, pktlen - (hdrlen + f_tkip.ic_miclen), mic);
    if (status != OL_RX_DEFRAG_OK) {
        return OL_RX_DEFRAG_ERR;
    }
    ol_rx_defrag_copydata(
        msdu, pktlen - f_tkip.ic_miclen, f_tkip.ic_miclen, (caddr_t) mic0);
    if (qdf_mem_cmp(mic, mic0, f_tkip.ic_miclen)) {
        return OL_RX_DEFRAG_ERR;
    }
    qdf_nbuf_trim_tail(msdu, f_tkip.ic_miclen);

    return OL_RX_DEFRAG_OK;
}

/*
 * Handling CCMP processing for defragmentation
 */
int
ol_rx_frag_ccmp_decap(
    qdf_nbuf_t nbuf,
    u_int16_t hdrlen)
{
    struct ieee80211_frame *wh;
    u_int8_t *ivp, *origHdr;

    origHdr = (u_int8_t *) qdf_nbuf_data(nbuf);
    wh = (struct ieee80211_frame *) origHdr;
    ivp = origHdr + hdrlen;
    if (!(ivp[IEEE80211_WEP_IVLEN] & IEEE80211_WEP_EXTIV)) {
        return OL_RX_DEFRAG_ERR;
    }
    qdf_mem_move(origHdr + f_ccmp.ic_header, origHdr, hdrlen);
    qdf_nbuf_pull_head(nbuf, f_ccmp.ic_header);

    return OL_RX_DEFRAG_OK;
}

/*
 * Verify and strip MIC from the frame.
 */
int
ol_rx_frag_ccmp_demic(
    qdf_nbuf_t wbuf,
    u_int16_t hdrlen)
{
    struct ieee80211_frame *wh;
    u_int8_t *ivp, *origHdr;

    origHdr = (u_int8_t *) qdf_nbuf_data(wbuf);
    wh = (struct ieee80211_frame *) origHdr;
    ivp = origHdr + hdrlen;
    if (!(ivp[IEEE80211_WEP_IVLEN] & IEEE80211_WEP_EXTIV)) {
        return OL_RX_DEFRAG_ERR;
    }
    qdf_nbuf_trim_tail(wbuf, f_ccmp.ic_trailer);

    return OL_RX_DEFRAG_OK;
}

/*
* Handling WEP processing for defragmentation
*/
int
ol_rx_frag_wep_decap(
    qdf_nbuf_t msdu,
    u_int16_t hdrlen)
{
    u_int8_t *origHdr;

    origHdr = (u_int8_t*) qdf_nbuf_data(msdu);
    qdf_mem_move(origHdr + f_wep.ic_header, origHdr, hdrlen);
    qdf_nbuf_pull_head(msdu, f_wep.ic_header);
    qdf_nbuf_trim_tail(msdu, f_wep.ic_trailer);
    return OL_RX_DEFRAG_OK;
}

/*
 * Craft pseudo header used to calculate the MIC.
 */
void
ol_rx_defrag_michdr(
    const struct ieee80211_frame *wh0,
    u_int8_t hdr[])
{
    const struct ieee80211_frame_addr4 *wh =
        (const struct ieee80211_frame_addr4 *) wh0;

    switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
    case IEEE80211_FC1_DIR_NODS:
        DEFRAG_IEEE80211_ADDR_COPY(hdr, wh->i_addr1); /* DA */
        DEFRAG_IEEE80211_ADDR_COPY(hdr + IEEE80211_ADDR_LEN, wh->i_addr2);
        break;
    case IEEE80211_FC1_DIR_TODS:
        DEFRAG_IEEE80211_ADDR_COPY(hdr, wh->i_addr3); /* DA */
        DEFRAG_IEEE80211_ADDR_COPY(hdr + IEEE80211_ADDR_LEN, wh->i_addr2);
        break;
    case IEEE80211_FC1_DIR_FROMDS:
        DEFRAG_IEEE80211_ADDR_COPY(hdr, wh->i_addr1); /* DA */
        DEFRAG_IEEE80211_ADDR_COPY(hdr + IEEE80211_ADDR_LEN, wh->i_addr3);
        break;
    case IEEE80211_FC1_DIR_DSTODS:
        DEFRAG_IEEE80211_ADDR_COPY(hdr, wh->i_addr3); /* DA */
        DEFRAG_IEEE80211_ADDR_COPY(hdr + IEEE80211_ADDR_LEN, wh->i_addr4);
        break;
    }
    /*
     * Bit 7 is IEEE80211_FC0_SUBTYPE_QOS for data frame, but
     * it could also be set for deauth, disassoc, action, etc. for
     * a mgt type frame. It comes into picture for MFP.
     */
    if (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_QOS) {
        const struct ieee80211_qosframe *qwh =
            (const struct ieee80211_qosframe *) wh;
        hdr[12] = qwh->i_qos[0] & IEEE80211_QOS_TID;
    } else {
        hdr[12] = 0;
    }
    hdr[13] = hdr[14] = hdr[15] = 0; /* reserved */
}

/*
 * Michael_mic for defragmentation
 */
int
ol_rx_defrag_mic(
    const u_int8_t *key,
    qdf_nbuf_t wbuf,
    u_int8_t off,
    u_int16_t data_len,
    u_int8_t mic[])
{
    u_int8_t hdr[16];
    u_int32_t l, r;
    const u_int8_t *data;
    u_int32_t space;

    ol_rx_defrag_michdr((struct ieee80211_frame *) qdf_nbuf_data(wbuf), hdr);
    l = get_le32(key);
    r = get_le32(key + 4);

    /* Michael MIC pseudo header: DA, SA, 3 x 0, Priority */
    l ^= get_le32(hdr);
    michael_block(l, r);
    l ^= get_le32(&hdr[4]);
    michael_block(l, r);
    l ^= get_le32(&hdr[8]);
    michael_block(l, r);
    l ^= get_le32(&hdr[12]);
    michael_block(l, r);

    /* first buffer has special handling */
    data = (u_int8_t *)qdf_nbuf_data(wbuf) + off;
    space = ol_rx_defrag_len(wbuf) - off;
    for (;;) {
        if (space > data_len) {
            space = data_len;
        }
        /* collect 32-bit blocks from current buffer */
        while (space >= sizeof(u_int32_t)) {
            l ^= get_le32(data);
            michael_block(l, r);
            data += sizeof(u_int32_t);
            space -= sizeof(u_int32_t);
            data_len -= sizeof(u_int32_t);
        }
        if (data_len < sizeof(u_int32_t)) {
            break;
        }
        wbuf = qdf_nbuf_next(wbuf);
        if (wbuf == NULL) {
            return OL_RX_DEFRAG_ERR;
        }
        if (space != 0) {
            const u_int8_t *data_next;
            /*
             * Block straddles buffers, split references.
             */
            data_next = (u_int8_t *)qdf_nbuf_data(wbuf);
            if (ol_rx_defrag_len(wbuf) < sizeof(u_int32_t) - space) {
                return OL_RX_DEFRAG_ERR;
            }
            switch (space) {
            case 1:
                l ^= get_le32_split(
                    data[0], data_next[0], data_next[1], data_next[2]);
                data = data_next + 3;
                space = ol_rx_defrag_len(wbuf) - 3;
                break;
            case 2:
                l ^= get_le32_split(
                    data[0], data[1], data_next[0], data_next[1]);
                data = data_next + 2;
                space = ol_rx_defrag_len(wbuf) - 2;
                break;
            case 3:
                l ^= get_le32_split(
                    data[0], data[1], data[2], data_next[0]);
                data = data_next + 1;
                space = ol_rx_defrag_len(wbuf) - 1;
                break;
            }
            michael_block(l, r);
            data_len -= sizeof(u_int32_t);
        } else {
            /*
             * Setup for next buffer.
             */
            data = (u_int8_t*) qdf_nbuf_data(wbuf);
            space = ol_rx_defrag_len(wbuf);
        }
    }
    /* Last block and padding (0x5a, 4..7 x 0) */
    switch (data_len) {
    case 0:
        l ^= get_le32_split(0x5a, 0, 0, 0);
        break;
    case 1:
        l ^= get_le32_split(data[0], 0x5a, 0, 0);
        break;
    case 2:
        l ^= get_le32_split(data[0], data[1], 0x5a, 0);
        break;
    case 3:
        l ^= get_le32_split(data[0], data[1], data[2], 0x5a);
        break;
    }
    michael_block(l, r);
    michael_block(l, r);
    put_le32(mic, l);
    put_le32(mic + 4, r);

    return OL_RX_DEFRAG_OK;
}

/*
 * Calculate headersize
 */
int
ol_rx_frag_hdrsize(const void *data)
{
    const struct ieee80211_frame *wh = (const struct ieee80211_frame *) data;
    int size = sizeof(struct ieee80211_frame);

    if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS) {
        size += IEEE80211_ADDR_LEN;
    }
    if (DEFRAG_IEEE80211_QOS_HAS_SEQ(wh)) {
        size += sizeof(u_int16_t);
        if (wh->i_fc[1] & IEEE80211_FC1_ORDER) {
            size += sizeof(struct ieee80211_htc);
        }
    }
    return size;
}

/*
 * Recombine and decap fragments
 */
qdf_nbuf_t
ol_rx_defrag_decap_recombine(
    htt_pdev_handle htt_pdev,
    qdf_nbuf_t frag_list,
    u_int16_t hdrsize)
{
    qdf_nbuf_t tmp;
    qdf_nbuf_t msdu = frag_list;
    qdf_nbuf_t rx_nbuf = frag_list;
    struct ieee80211_frame* wh;

    msdu = qdf_nbuf_next(msdu);
    qdf_nbuf_set_next(rx_nbuf, NULL);
    while (msdu) {
        htt_rx_msdu_desc_free(htt_pdev, msdu);
        tmp = qdf_nbuf_next(msdu);
        qdf_nbuf_set_next(msdu, NULL);
        qdf_nbuf_pull_head(msdu, hdrsize);
        if (!ol_rx_defrag_concat(rx_nbuf, msdu)) {
            ol_rx_frames_free(htt_pdev, tmp);
            htt_rx_desc_frame_free(htt_pdev, rx_nbuf);
            qdf_nbuf_free(msdu); /* msdu rx desc already freed above */
            return NULL;
        }
        msdu = tmp;
    }
    wh = (struct ieee80211_frame *) qdf_nbuf_data(rx_nbuf);
    wh->i_fc[1] &= ~IEEE80211_FC1_MORE_FRAG;
    *(u_int16_t *) wh->i_seq &= ~IEEE80211_SEQ_FRAG_MASK;

    return rx_nbuf;
}

void
ol_rx_defrag_nwifi_to_8023(qdf_nbuf_t msdu)
{
    struct ieee80211_frame_addr4 wh;
    struct ieee80211_frame *wh1;
    uint8_t type,subtype;
    uint32_t hdrsize;
    struct llc_snap_hdr_t llchdr;
    struct ethernet_hdr_t *eth_hdr;
    wh1 = (struct ieee80211_frame *) qdf_nbuf_data(msdu);
    if ((wh1->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS)
        qdf_mem_copy(&wh, qdf_nbuf_data(msdu), sizeof(wh));
    else
        qdf_mem_copy(&wh, qdf_nbuf_data(msdu), (sizeof(wh)-IEEE80211_ADDR_LEN));

    type = wh.i_fc[0] & IEEE80211_FC0_TYPE_MASK;
    subtype = wh.i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
    /* Native Wifi header is 80211 non-QoS header */
    hdrsize = sizeof(struct ieee80211_frame);
    if((wh.i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS)
       hdrsize += IEEE80211_ADDR_LEN;

    qdf_mem_copy(&llchdr, ((uint8_t *) qdf_nbuf_data(msdu)) + hdrsize,
        sizeof(struct llc_snap_hdr_t));

    /*
     * Now move the data pointer to the beginning of the mac header :
     * new-header = old-hdr + (wifhdrsize + llchdrsize - ethhdrsize)
     */
    qdf_nbuf_pull_head(msdu, (hdrsize +
        sizeof(struct llc_snap_hdr_t) - sizeof(struct ethernet_hdr_t)));
    eth_hdr = (struct ethernet_hdr_t *)(qdf_nbuf_data(msdu));
    switch (wh.i_fc[1] & IEEE80211_FC1_DIR_MASK) {
    case IEEE80211_FC1_DIR_NODS:
        qdf_mem_copy(eth_hdr->dest_addr, wh.i_addr1, IEEE80211_ADDR_LEN);
        qdf_mem_copy(eth_hdr->src_addr, wh.i_addr2, IEEE80211_ADDR_LEN);
        break;
    case IEEE80211_FC1_DIR_TODS:
        qdf_mem_copy(eth_hdr->dest_addr, wh.i_addr3, IEEE80211_ADDR_LEN);
        qdf_mem_copy(eth_hdr->src_addr, wh.i_addr2, IEEE80211_ADDR_LEN);
        break;
    case IEEE80211_FC1_DIR_FROMDS:
        qdf_mem_copy(eth_hdr->dest_addr, wh.i_addr1, IEEE80211_ADDR_LEN);
        qdf_mem_copy(eth_hdr->src_addr, wh.i_addr3, IEEE80211_ADDR_LEN);
        break;
    case IEEE80211_FC1_DIR_DSTODS:
        qdf_mem_copy(eth_hdr->dest_addr, wh.i_addr3, IEEE80211_ADDR_LEN);
        qdf_mem_copy(eth_hdr->src_addr,  wh.i_addr4, IEEE80211_ADDR_LEN);
        break;
    }
    qdf_mem_copy(
        eth_hdr->ethertype, llchdr.ethertype, sizeof(llchdr.ethertype));
}

/*
 * Handling QOS for defragmentation
 */
void
ol_rx_defrag_qos_decap(
    qdf_nbuf_t nbuf,
    u_int16_t hdrlen)
{
    struct ieee80211_frame *wh;
    u_int16_t qoslen;
    u_int8_t *qos;

    wh = (struct ieee80211_frame *) qdf_nbuf_data(nbuf);
    if (DEFRAG_IEEE80211_QOS_HAS_SEQ(wh)) {
        qoslen = sizeof(struct ieee80211_qoscntl);
        /* Qos frame with Order bit set indicates a HTC frame */
        if (wh->i_fc[1] & IEEE80211_FC1_ORDER) {
            qoslen += sizeof(struct ieee80211_htc);
        }
        if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS)
        {
            qos = &((struct ieee80211_qosframe_addr4 *) wh)->i_qos[0];
        } else {
            qos = &((struct ieee80211_qosframe *) wh)->i_qos[0];
        }
        /* remove QoS filed from header */
        hdrlen -= qoslen;
        qdf_mem_move((u_int8_t *) wh + qoslen, wh, hdrlen);
        wh = (struct ieee80211_frame *) qdf_nbuf_pull_head(nbuf, qoslen);
        /* clear QoS bit */
        wh->i_fc[0] &= ~IEEE80211_FC0_SUBTYPE_QOS;
    }
}

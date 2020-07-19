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

#include <qdf_nbuf.h>         /* qdf_nbuf_t */

#include <ol_htt_rx_api.h>    /* htt_rx_pn_t, etc. */
#include <ol_ctrl_txrx_api.h> /* ol_rx_err */

#include <ol_txrx_internal.h> /* ol_rx_mpdu_list_next */
#include <ol_txrx_types.h>    /* ol_txrx_vdev_t, etc. */
#include <ol_rx_pn.h>  /* our own defs */
#include <ol_rx_fwd.h> /* ol_rx_fwd_check */
#include <ol_rawmode_txrx_api.h> /* OL_RX_MPDU_LIST_NEXT_RAW */
#include <ol_if_athvar.h>
#include <init_deinit_lmac.h>

#define     MAX_CCMP_PN_GAP_ERR_CHECK       1000    /* Max. gap in CCMP PN, to suspect it as corrupted PN */
#define     MAX_ALLOWED_CONSECUTIVE_PN_FAILURE       350    /* Max. number of consecutive pn check failure allowed  */


/* add the MSDUs from this MPDU to the list of good frames */
#define ADD_MPDU_TO_LIST(head, tail, mpdu, mpdu_tail) do { 	\
        if (!head) {                                        \
            head = mpdu;                                	\
        } else {                                            \
            qdf_nbuf_set_next(tail, mpdu);    				\
        }                                                   \
        tail = mpdu_tail;                               	\
    } while(0);

int ol_rx_pn_cmp24(
    union htt_rx_pn_t *new_pn,
    union htt_rx_pn_t *old_pn,
    int is_unicast,
    int opmode)
{
    return ((new_pn->pn24 & 0xffffff) <= (old_pn->pn24 & 0xffffff));
}


int ol_rx_pn_cmp48(
    union htt_rx_pn_t *new_pn,
    union htt_rx_pn_t *old_pn,
    int is_unicast,
    int opmode)
{
   return
       ((new_pn->pn48 & 0xffffffffffffULL) <=
        (old_pn->pn48 & 0xffffffffffffULL));
}

int ol_rx_pn_wapi_cmp(
    union htt_rx_pn_t *new_pn,
    union htt_rx_pn_t *old_pn,
    int is_unicast,
    int opmode)
{
    int pn_is_replay = 0;

    if (new_pn->pn128[1] == old_pn->pn128[1]) {
        pn_is_replay =  (new_pn->pn128[0] <= old_pn->pn128[0]);
    } else {
        pn_is_replay =  (new_pn->pn128[1] < old_pn->pn128[1]);
    }

    if (is_unicast) {
        if (opmode == wlan_op_mode_ap) {
            pn_is_replay |= ((new_pn->pn128[0] & 0x1ULL) != 0);
        } else {
            pn_is_replay |= ((new_pn->pn128[0] & 0x1ULL) != 1);
        }
    }
    return pn_is_replay;
}

qdf_nbuf_t
ol_rx_pn_check_base(
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    qdf_nbuf_t msdu_list)
{
    struct ol_txrx_pdev_t *pdev = vdev->pdev;
    union htt_rx_pn_t *last_pn, *global_pn, *suspect_pn;
    qdf_nbuf_t out_list_head = NULL;
    qdf_nbuf_t out_list_tail = NULL;
    qdf_nbuf_t mpdu;
    int index; /* unicast vs. multicast */
    int pn_len;
    void *rx_desc;
    int last_pn_valid;
    uint8_t curr_keyid = 0;
    struct ol_ath_softc_net80211 *scn = NULL;
    scn = (struct ol_ath_softc_net80211 *)
             lmac_get_pdev_feature_ptr((struct wlan_objmgr_pdev *)pdev->ctrl_pdev);

    /* First, check whether the PN check applies */
    rx_desc = htt_rx_msdu_desc_retrieve(pdev->htt_pdev, msdu_list);
    qdf_assert(htt_rx_msdu_has_wlan_mcast_flag(pdev->htt_pdev, rx_desc));
    index = htt_rx_msdu_is_wlan_mcast(pdev->htt_pdev, rx_desc) ?
        txrx_sec_mcast : txrx_sec_ucast;
    pn_len = pdev->rx_pn[peer->security[index].sec_type].len;
    if (pn_len == 0) {
        return msdu_list;
    }

    last_pn_valid = peer->tids_last_pn_valid[tid];
    last_pn = &peer->tids_last_pn[tid];
    global_pn = &peer->global_pn;
    suspect_pn = &peer->tids_suspect_pn[tid];
    mpdu = msdu_list;
    while (mpdu) {
        qdf_nbuf_t mpdu_tail, next_mpdu;
        union htt_rx_pn_t new_pn;
        int pn_is_replay = 0, update_last_pn = 1;
#if ATH_SUPPORT_WAPI
        bool is_mpdu_encrypted = 0;
        bool is_unencrypted_pkt_wai = 0;
#endif

        rx_desc = htt_rx_msdu_desc_retrieve(pdev->htt_pdev, mpdu);

        /*
         * Find the last MSDU within this MPDU, and
         * the find the first MSDU within the next MPDU.
         */
        if (OL_CFG_NONRAW_RX_LIKELINESS(vdev->rx_decap_type
                    != htt_pkt_type_raw)) {
            ol_rx_mpdu_list_next(pdev, mpdu, &mpdu_tail, &next_mpdu);
        } else {
            OL_RX_MPDU_LIST_NEXT_RAW(pdev, mpdu, &mpdu_tail, &next_mpdu);
        }


#if ATH_SUPPORT_WAPI
        /* Don't check the PN replay for non-encrypted frames
           or if this is a WAI packet */
        is_mpdu_encrypted = htt_rx_mpdu_is_encrypted(pdev->htt_pdev, rx_desc);
        is_unencrypted_pkt_wai = is_mpdu_encrypted  ?  false : vdev->osif_check_wai(vdev->osif_vdev, mpdu, mpdu_tail);
        if ((!vdev->drop_unenc && !is_mpdu_encrypted) || is_unencrypted_pkt_wai) {
#else
        /* Don't check the PN replay for non-encrypted frames */
        if (!vdev->drop_unenc && !htt_rx_mpdu_is_encrypted(pdev->htt_pdev, rx_desc)) {
#endif
                ADD_MPDU_TO_LIST(out_list_head, out_list_tail, mpdu, mpdu_tail);
                mpdu = next_mpdu;
                continue;
        }

        /* retrieve PN from rx descriptor */
        htt_rx_mpdu_desc_pn(pdev->htt_pdev, rx_desc, &new_pn, pn_len);

	/* if there was no prior PN, there's nothing to check */
	if (last_pn_valid) {
            pn_is_replay = pdev->rx_pn[peer->security[index].sec_type].cmp(
				&new_pn, last_pn, index == txrx_sec_ucast, vdev->opmode);
        } else if (peer->authorize) {
            last_pn_valid = peer->tids_last_pn_valid[tid] = 1;
            /* For multicast TID remember which key index
             * was used to decrypt last mcast/bcast frame.
             */
            if(index == txrx_sec_mcast) {
                peer->mcast_pn_reset_keyidx =
                    (uint8_t)htt_rx_msdu_key_id_octet(pdev->htt_pdev, rx_desc);
            }
        }

        if (pn_is_replay && (index == txrx_sec_mcast)) {
            curr_keyid = (uint8_t)htt_rx_msdu_key_id_octet(pdev->htt_pdev, rx_desc);
            if(curr_keyid != peer->mcast_pn_reset_keyidx) {
                /* Now AP is using newly negotiated Mcast key.
                 * Treat this as a first frame with new PN sequence
                 * and accept this frame.
                 */
                pn_is_replay = 0;
            }
        }

        if (peer->authorize && peer->security[index].sec_type == htt_sec_type_aes_ccmp) {
            if ((new_pn.pn48 & 0xffffffffffffULL) >
                 ((last_pn->pn48 + MAX_CCMP_PN_GAP_ERR_CHECK) & 0xffffffffffffULL)) {
                /* PN jump wrt last_pn is > MAX_CCMP_PN_GAP_ERR_CHECK - PN of current frame is suspected */
                if (suspect_pn->pn48) {
                    /* Check whether PN of the current frame is following prev PN seq or not */
                    if ((new_pn.pn48 & 0xffffffffffffULL) < (suspect_pn->pn48 & 0xffffffffffffULL)) {
                        /*
                         * PN number of the curr frame < PN no of prev rxed frame
                         * As we are not sure about prev suspect PN, to detect replay,
                         * check the current PN with global PN
                         */
                        if ((new_pn.pn48 & 0xffffffffffffULL) < (global_pn->pn48 & 0xffffffffffffULL)) {
                            /* Replay violation */
                            pn_is_replay = 1;
                        } else {
                            /* Current PN is following global PN, so mark this as suspected PN
                             * Don't update last_pn & global_pn
                             */
                            suspect_pn->pn128[0] = new_pn.pn128[0];
                            suspect_pn->pn128[1] = new_pn.pn128[1];
                            update_last_pn = 0;
                        }
                    } else if ((new_pn.pn48 & 0xffffffffffffULL) <
                                ((suspect_pn->pn48 + MAX_CCMP_PN_GAP_ERR_CHECK) & 0xffffffffffffULL)) {
                        /* Current PN is following prev suspected PN seq
                         * Update last_pn & global_pn (update_last_pn = 1;)
                         */
                    } else {
                        /*
                         * Current PN is neither following prev suspected PN nor last_pn
                         * Mark this as new suspect and don't update last_pn & global_pn
                         */
                        suspect_pn->pn128[0] = new_pn.pn128[0];
                        suspect_pn->pn128[1] = new_pn.pn128[1];
                        update_last_pn = 0;
                    }
                } else {
                    /* New Jump in PN observed
                     * So mark this PN as suspected and don't update last_pn/global_pn
                     */
                    suspect_pn->pn128[0] = new_pn.pn128[0];
                    suspect_pn->pn128[1] = new_pn.pn128[1];
                    update_last_pn = 0;
                }
        } else {
            /* Valid PN, update last_pn & global_pn (update_last_pn = 1;) */
        }
    }

        if (pn_is_replay) {
            qdf_nbuf_t msdu;
            /*
             * This MPDU failed the PN check:
             * 1.  Notify the control SW of the PN failure
             *     (so countermeasures can be taken, if necessary)
             * 2.  Discard all the MSDUs from this MPDU.
             */
            msdu = mpdu;

#if RX_DEBUG
            if (peer->tids_rx_reorder[tid].win_sz_mask) {
                peer->tids_PN_drop_BA[tid]++;
            } else {
                peer->tids_PN_drop_noBA[tid]++;
            }
#endif
            if (scn) {
                (scn->soc->dbg.err_types.pn_errors)++;
            }

            if (scn && (scn->soc->dbg.err_types.pn_errors % scn->soc->dbg.print_rate_limit) == 0) {
#if !(TXRX_PRINT_ENABLE)
                printk("=== Reducing pace for error prints ==== \n");
                printk("PN check failed - TID %d, peer %pK "
                        "(%02x:%02x:%02x:%02x:%02x:%02x) %s\n"
                        "    old PN (u64 x2)= 0x%08llx %08llx (LSBs = %lld)\n"
                        "    new PN (u64 x2)= 0x%08llx %08llx (LSBs = %lld)\n"
                        "    global PN (u64 x2)= 0x%08llx %08llx (LSBs = %lld)\n"
                        "    suspect PN (u64 x2)= 0x%08llx %08llx (LSBs = %lld)\n"
#if RX_DEBUG
                        "    htt_status = %d\n"
                        "    PN in BA = %d  - PN in no BA = %d\n"
#endif
                        "    prev seq num = %d"
                        "    new seq num = %d - Error occured %d times\n"
                        ,tid, peer,
                        peer->mac_addr.raw[0], peer->mac_addr.raw[1],
                        peer->mac_addr.raw[2], peer->mac_addr.raw[3],
                        peer->mac_addr.raw[4], peer->mac_addr.raw[5],
                        (index == txrx_sec_ucast) ? "ucast" : "mcast",
                        last_pn->pn128[1],
                        last_pn->pn128[0],
                        last_pn->pn128[0] & 0xffffffffffffULL,
                        new_pn.pn128[1],
                        new_pn.pn128[0],
                        new_pn.pn128[0] & 0xffffffffffffULL,
                        global_pn->pn128[1],
                        global_pn->pn128[0],
                        global_pn->pn128[0] & 0xffffffffffffULL,
                        suspect_pn->pn128[1],
                        suspect_pn->pn128[0],
                        suspect_pn->pn128[0] & 0xffffffffffffULL,
#if RX_DEBUG
                        htt_rx_mpdu_status(pdev->htt_pdev),
                        peer->tids_PN_drop_BA[tid], peer->tids_PN_drop_noBA[tid],
#endif
                        peer->tids_last_seq[tid],
                        htt_rx_mpdu_desc_seq_num(pdev->htt_pdev, rx_desc), scn->soc->dbg.err_types.pn_errors);

#if RX_DEBUG
                        peer->tids_PN_drop_BA[tid] = 0;
                        peer->tids_PN_drop_noBA[tid] = 0;
#endif
#else
                TXRX_PRINT(TXRX_PRINT_LEVEL_WARN,
                    "PN check failed - TID %d, peer %pK "
                    "(%02x:%02x:%02x:%02x:%02x:%02x) %s\n"
                    "    old PN (u64 x2)= 0x%08llx %08llx (LSBs = %lld)\n"
                    "    new PN (u64 x2)= 0x%08llx %08llx (LSBs = %lld)\n"
                    "    global PN (u64 x2)= 0x%08llx %08llx (LSBs = %lld)\n"
                    "    suspect PN (u64 x2)= 0x%08llx %08llx (LSBs = %lld)\n"
#if RX_DEBUG
                    "    htt_status = %d\n"
                    "    PN in BA = %d  - PN in no BA = %d\n"
#endif
                    "    prev seq num = %d"
                    "    new seq num = %d\n",
                    tid, peer,
                    peer->mac_addr.raw[0], peer->mac_addr.raw[1],
                    peer->mac_addr.raw[2], peer->mac_addr.raw[3],
                    peer->mac_addr.raw[4], peer->mac_addr.raw[5],
                    (index == txrx_sec_ucast) ? "ucast" : "mcast",
                    last_pn->pn128[1],
                    last_pn->pn128[0],
                    last_pn->pn128[0] & 0xffffffffffffULL,
                    new_pn.pn128[1],
                    new_pn.pn128[0],
                    new_pn.pn128[0] & 0xffffffffffffULL,
                    global_pn->pn128[1],
                    global_pn->pn128[0],
                    global_pn->pn128[0] & 0xffffffffffffULL,
                    suspect_pn->pn128[1],
                    suspect_pn->pn128[0],
                    suspect_pn->pn128[0] & 0xffffffffffffULL,
#if RX_DEBUG
                    htt_rx_mpdu_status(pdev->htt_pdev),
                    peer->tids_PN_drop_BA[tid], peer->tids_PN_drop_noBA[tid],
#endif
                    peer->tids_last_seq[tid],
                    htt_rx_mpdu_desc_seq_num(pdev->htt_pdev, rx_desc));
#endif
            }

            peer->pn_replay_counter++;
            ol_rx_pn_trace_display(pdev, 1);
            ol_rx_err(
                pdev->ctrl_pdev,
                vdev->vdev_id, peer->mac_addr.raw, tid,
                htt_rx_mpdu_desc_tsf32(pdev->htt_pdev, rx_desc),
                OL_RX_ERR_PN, mpdu);
            /* free all MSDUs within this MPDU */
            do {
                qdf_nbuf_t next_msdu;
                next_msdu = qdf_nbuf_next(msdu);
                htt_rx_desc_frame_free(pdev->htt_pdev, msdu);
                if (msdu == mpdu_tail) {
                    break;
                } else {
                    msdu = next_msdu;
                }
            } while (1);
        } else {
            ADD_MPDU_TO_LIST(out_list_head, out_list_tail, mpdu, mpdu_tail);
            peer->pn_replay_counter = 0;
            if(peer->authorize) {
                /*
                 * Remember the new PN.
                 * For simplicity, just do 2 64-bit word copies to cover the worst
                 * case (WAPI), regardless of the length of the PN.
                 * This is more efficient than doing a conditional branch to copy
                 * only the relevant portion.
                 */
                if (update_last_pn) {
                    last_pn->pn128[0] = new_pn.pn128[0];
                    last_pn->pn128[1] = new_pn.pn128[1];
                    global_pn->pn128[0] = new_pn.pn128[0];
                    global_pn->pn128[1] = new_pn.pn128[1];
                    suspect_pn->pn128[0] = 0;
                    suspect_pn->pn128[1] = 0;
                }
                OL_RX_PN_TRACE_ADD(pdev, peer, tid, rx_desc);
            }
        }

        mpdu = next_mpdu;
    }
    /* make sure the list is null-terminated */
    if (out_list_tail) {
        qdf_nbuf_set_next(out_list_tail, NULL);
    }
    /* Kick out the station if there are 150 consecutive PN replays */
    if(peer->pn_replay_counter > MAX_ALLOWED_CONSECUTIVE_PN_FAILURE) {
        printk("Kicking out node %x:%x:%x:%x:%x:%x due to excessive PN replays\n", peer->mac_addr.raw[0], peer->mac_addr.raw[1],
                                peer->mac_addr.raw[2], peer->mac_addr.raw[3], peer->mac_addr.raw[4], peer->mac_addr.raw[5]);
        peer_sta_kickout(scn, (A_UINT8 *)&peer->mac_addr.raw);
    }
    return out_list_head;
}

void
ol_rx_pn_check(
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    qdf_nbuf_t msdu_list)
{
    msdu_list = ol_rx_pn_check_base(vdev, peer, tid, msdu_list);
    ol_rx_fwd_check(vdev, peer, tid, msdu_list);
}

#if defined(ENABLE_RX_PN_TRACE)

A_STATUS
ol_rx_pn_trace_attach(ol_txrx_pdev_handle pdev)
{
    int num_elems;

    num_elems = 1 << TXRX_RX_PN_TRACE_SIZE_LOG2;
    pdev->rx_pn_trace.idx = 0;
    pdev->rx_pn_trace.cnt = 0;
    pdev->rx_pn_trace.mask = num_elems - 1;
    pdev->rx_pn_trace.data = qdf_mem_malloc(
        sizeof(*pdev->rx_pn_trace.data) * num_elems);
    if (! pdev->rx_pn_trace.data) {
        return A_ERROR;
    }
    return A_OK;
}

void
ol_rx_pn_trace_detach(ol_txrx_pdev_handle pdev)
{
    qdf_mem_free(pdev->rx_pn_trace.data);
}

void
ol_rx_pn_trace_add(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_peer_t *peer,
    u_int16_t tid,
    void *rx_desc)
{
    u_int32_t idx = pdev->rx_pn_trace.idx;
    union htt_rx_pn_t pn;
    u_int32_t pn32;
    u_int16_t seq_num;
    u_int8_t  unicast;

    htt_rx_mpdu_desc_pn(pdev->htt_pdev, rx_desc, &pn, 48);
    pn32 = pn.pn48 & 0xffffffff;
    seq_num = htt_rx_mpdu_desc_seq_num(pdev->htt_pdev, rx_desc);
    unicast = ! htt_rx_msdu_is_wlan_mcast(pdev->htt_pdev, rx_desc);

    pdev->rx_pn_trace.data[idx].peer = peer;
    pdev->rx_pn_trace.data[idx].tid = tid;
    pdev->rx_pn_trace.data[idx].seq_num = seq_num;
    pdev->rx_pn_trace.data[idx].unicast = unicast;
    pdev->rx_pn_trace.data[idx].pn32 = pn32;
    pdev->rx_pn_trace.cnt++;
    idx++;
    pdev->rx_pn_trace.idx = idx & pdev->rx_pn_trace.mask;
}

void
ol_rx_pn_trace_display(ol_txrx_pdev_handle pdev, int just_once)
{
    static int print_count = 0;
    u_int32_t i, start, end;
    u_int64_t cnt;
    int elems;
    int limit = 0; /* move this to the arg list? */

    if (print_count != 0 && just_once) {
        return;
    }
    print_count++;

    end = pdev->rx_pn_trace.idx;
    if (pdev->rx_pn_trace.cnt <= pdev->rx_pn_trace.mask) {
        /* trace log has not yet wrapped around - start at the top */
        start = 0;
        cnt = 0;
    } else {
        start = end;
        cnt = pdev->rx_pn_trace.cnt - (pdev->rx_pn_trace.mask + 1);
    }
    elems = (end - 1 - start) & pdev->rx_pn_trace.mask;
    if (limit > 0 && elems > limit) {
        int delta;
        delta = elems - limit;
        start += delta;
        start &= pdev->rx_pn_trace.mask;
        cnt += delta;
    }

    i = start;
    qdf_print("                                 seq     PN");
    qdf_print("   count  idx    peer   tid uni  num    LSBs");
    do {
        qdf_print("  %6lld %4d  %pK %2d   %d %4d %8d",
            cnt, i,
            pdev->rx_pn_trace.data[i].peer,
            pdev->rx_pn_trace.data[i].tid,
            pdev->rx_pn_trace.data[i].unicast,
            pdev->rx_pn_trace.data[i].seq_num,
            pdev->rx_pn_trace.data[i].pn32);
        cnt++;
        i++;
        i &= pdev->rx_pn_trace.mask;
    } while (i != end);
}
#endif /* ENABLE_RX_PN_TRACE */

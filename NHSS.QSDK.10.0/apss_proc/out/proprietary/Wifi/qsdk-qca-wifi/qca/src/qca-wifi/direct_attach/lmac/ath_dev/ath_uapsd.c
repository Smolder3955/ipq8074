/*
 * Copyright (c) 2011,2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2009, Atheros Communications Inc.
 * All Rights Reserved.
 */

#include "ath_internal.h"
#include "if_athrate.h"

#ifndef REMOVE_PKT_LOG
#include "pktlog.h"
extern struct ath_pktlog_funcs *g_pktlog_funcs;
#endif





#ifdef ATH_SUPPORT_UAPSD
/*
 ******************************************************************************
 * UAPSD Queuing and Transmission routines.
 ******************************************************************************
 */

/*
 * Returns number for frames queued in software for a given node.
 * Context: Tasklet
 */
u_int32_t
ath_tx_uapsd_depth(ath_node_t node)
{
    struct ath_node *an = ATH_NODE(node);

    return an->an_uapsd_qdepth;
}

/*
 * UAPSD initialization.
 */
int
ath_tx_uapsd_init(struct ath_softc *sc)
{
    int error = 0;
    struct ath_buf *bf;
    wbuf_t wbuf;
    struct ath_desc *ds;
    u_int uapsdq_num;
#define ATH_FRAG_PER_QOSNULL_MSDU                   1

    ATH_UAPSD_LOCK_INIT(sc);

    sc->sc_uapsdqnuldepth = 0;

    /* Setup tx descriptors */
    error = ath_descdma_setup(sc, &sc->sc_uapsdqnuldma, &sc->sc_uapsdqnulbf,
                                  "uapsd_qnull", ATH_QOSNULL_TXDESC,
                                  ATH_TXDESC, 1, ATH_FRAG_PER_QOSNULL_MSDU);
    if (error != 0) {
        printk("failed to allocate UAPSD QoS NULL tx descriptors: %d\n", error);
        return error;
    }

    /* Format QoS NULL frame */
    TAILQ_FOREACH(bf, &sc->sc_uapsdqnulbf, bf_list)
    {

        ATH_TXBUF_RESET(bf, sc->sc_num_txmaps);
        wbuf = sc->sc_ieee_ops->uapsd_allocqosnullframe(sc->sc_pdev);
        if (wbuf == NULL) {
            error = -ENOMEM;
            printk("failed to allocate UAPSD QoS NULL wbuf\n");
            break;
        }

        bf->bf_frmlen = sizeof(struct ieee80211_qosframe) + IEEE80211_CRC_LEN;
        bf->bf_mpdu = wbuf;
        bf->bf_isdata = 1;
        bf->bf_lastfrm = bf;
        bf->bf_lastbf = bf->bf_lastfrm; /* one single frame */
        bf->bf_isaggr  = 0;
        bf->bf_isbar  = bf->bf_isampdu = 0;
        bf->bf_next = NULL;
        bf->bf_qosnulleosp = 1;
        bf->bf_node = NULL;

        bf->bf_buf_addr[0] = wbuf_map_single(sc->sc_osdev, wbuf, BUS_DMA_TODEVICE,
                                          OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));

        /* setup descriptor */
        ds = bf->bf_desc;
        ath_hal_setdesclink(sc->sc_ah, ds, 0);
        bf->bf_buf_len[0] = bf->bf_frmlen;
        uapsdq_num = sc->sc_uapsdq->axq_qnum;
#ifndef REMOVE_PKT_LOG
        //XXX ds->ds_vdata = wbuf_header(wbuf);
#endif

        /*
         * Formulate first tx descriptor with tx controls.
         */
        ath_hal_set11n_txdesc(sc->sc_ah, ds
                              , bf->bf_frmlen           /* frame length */
                              , HAL_PKT_TYPE_NORMAL     /* Atheros packet type */
                              , MIN(sc->sc_curtxpow, 60)/* txpower */
                              , HAL_TXKEYIX_INVALID     /* key cache index */
                              , HAL_KEY_TYPE_CLEAR      /* key type */
                              , HAL_TXDESC_INTREQ
                                | HAL_TXDESC_CLRDMASK   /* flags */
                            );

        ath_hal_filltxdesc(sc->sc_ah, ds
                               , bf->bf_buf_addr    /* buffer address */
                               , bf->bf_buf_len     /* buffer length */
                               , 0                  /* descriptor id */
                               , uapsdq_num         /* QCU number */
                               , HAL_KEY_TYPE_CLEAR /* key type */
                               , AH_TRUE            /* first segment */
                               , AH_TRUE            /* last segment */
                               , ds                 /* first descriptor */
                            );


        /* NB: The desc swap function becomes void,
         * if descriptor swapping is not enabled
         */
        ath_desc_swap(ds);
    }

    if (error) {
        ath_tx_uapsd_cleanup(sc);
    }

    return error;
}

/*
 * Reclaim all UAPSD resources.
 * Context: Tasklet
 */
void
ath_tx_uapsd_cleanup(struct ath_softc *sc)
{
    ieee80211_tx_status_t tx_status;
    struct ath_buf *bf;
    wbuf_t wbuf;

    tx_status.flags = 0;

#ifdef  ATH_SUPPORT_TxBF
    tx_status.txbf_status = 0;
#endif

    TAILQ_FOREACH(bf, &sc->sc_uapsdqnulbf, bf_list)
    {
        wbuf = bf->bf_mpdu;
        
        if (wbuf == NULL) {
            continue;
        }

        /* Unmap this frame */
        wbuf_unmap_sg(sc->sc_osdev, wbuf,
                      OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));

        wbuf_release(sc->sc_osdev, wbuf);
        bf->bf_mpdu = NULL;
    }

    if (sc->sc_uapsdqnuldma.dd_desc_len != 0)
        ath_descdma_cleanup(sc, &sc->sc_uapsdqnuldma, &sc->sc_uapsdqnulbf);

    ATH_UAPSD_LOCK_DESTROY(sc);
}

void
ath_tx_uapsd_draintxq(struct ath_softc *sc)
{
    struct ath_buf *bf, *lastbf;
    ath_bufhead bf_head;
    struct ath_txq *txq = sc->sc_uapsdq;

    TAILQ_INIT(&bf_head);
    /*
     * NB: this assumes output has been stopped and
     *     we do not need to block ath_tx_tasklet
     */
    ath_vap_pause_txq_use_inc(sc);
    ATH_UAPSD_LOCK_IRQ(sc);
    for (;;) {
        if (sc->sc_enhanceddmasupport) {
            bf = TAILQ_FIRST(&txq->axq_fifo[txq->axq_tailindex]);
            if (bf == NULL) {
                if (txq->axq_headindex != txq->axq_tailindex)
                    printk("ath_tx_uapsd_draintxq: ERR head %d tail %d\n",
                           txq->axq_headindex, txq->axq_tailindex);
                txq->axq_headindex = 0;
                txq->axq_tailindex = 0;
                break;
            }
        } else {
            bf = TAILQ_FIRST(&txq->axq_q);

            if (bf == NULL) {
                txq->axq_link = NULL;
                txq->axq_linkbuf = NULL;
                break;
            }

            if (bf->bf_status & ATH_BUFSTATUS_STALE) {
                ATH_TXQ_REMOVE_STALE_HEAD(txq, bf, bf_list);

                if (bf->bf_qosnulleosp) {

                    ath_tx_uapsdqnulbf_complete(sc, bf, true);

                } else {
                    ATH_UAPSD_UNLOCK_IRQ(sc);
                    ATH_TXBUF_LOCK(sc);
		    sc->sc_txbuf_free++;

#if ATH_TX_BUF_FLOW_CNTL
                    txq->axq_num_buf_used--;
#endif
                    TAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
                    
#if TRACE_TX_LEAK                
                    TAILQ_REMOVE(&sc->sc_tx_trace_head,bf,bf_tx_trace_list);
#endif //TRACE_TX_LEAK

                    ATH_TXBUF_UNLOCK(sc);
                    ATH_UAPSD_LOCK_IRQ(sc);

#if ATH_SUPPORT_FLOWMAC_MODULE
                    if (sc->sc_osnetif_flowcntrl) {
                        ath_netif_wake_queue(sc);
                    }
#endif
                }
                continue;
            }
        }

        KASSERT(bf->bf_lastbf, ("bf->bf_lastbf is NULL"));
        lastbf = bf->bf_lastbf;
        lastbf->bf_isswaborted = 1;

        /* remove ath_buf's of the same mpdu from txq */
        if (sc->sc_enhanceddmasupport) {
            ATH_EDMA_MCASTQ_MOVE_HEAD_UNTIL(txq, &bf_head, lastbf, bf_list);
        } else {
            ATH_TXQ_MOVE_HEAD_UNTIL(txq, &bf_head, lastbf, bf_list);
        }

        ATH_UAPSD_UNLOCK_IRQ(sc);

#ifdef ATH_SWRETRY
        if (sc->sc_enhanceddmasupport) {
            if (!bf->bf_isampdu && bf->bf_isdata) {
                struct ath_node *an = bf->bf_node;
                if (an) {
                    struct ath_swretry_info *pInfo = &an->an_swretry_info[txq->axq_qnum];
                    ATH_NODE_SWRETRY_TXBUF_LOCK(an);
                    ASSERT(pInfo->swr_num_eligible_frms);
                    pInfo->swr_num_eligible_frms --;
                    ATH_NODE_SWRETRY_TXBUF_UNLOCK(an);
                }
            }
        }
#endif

#ifdef  ATH_SUPPORT_TxBF
        ath_tx_uapsd_complete(sc, bf->bf_node, bf, &bf_head, 0, 0, 0);
#else
        ath_tx_uapsd_complete(sc, bf->bf_node, bf, &bf_head, 0);
#endif
        ATH_UAPSD_LOCK_IRQ(sc);
    }
    ATH_UAPSD_UNLOCK_IRQ(sc);
    ath_vap_pause_txq_use_dec(sc);
}


/*
 * Reclaim all UAPSD node resources.
 * Context: Tasklet
 */
void
ath_tx_uapsd_node_cleanup(struct ath_softc *sc, struct ath_node *an)
{
    struct ath_buf *bf;
    ath_bufhead bf_head;
    struct ath_txq *txq;

    txq = sc->sc_uapsdq;

    ATH_UAPSD_LOCK_IRQ(sc);
    /*
     * Drain the uapsd software queue.
     */
    for (;;) {
        bf = TAILQ_FIRST(&an->an_uapsd_q);

        if (bf == NULL) {
            an->an_uapsd_last_frm = NULL; 
            break;
        }

        TAILQ_REMOVE_HEAD_UNTIL(&an->an_uapsd_q, &bf_head, bf->bf_lastfrm, bf_list);

        /* complete this sub-frame */
        ATH_UAPSD_UNLOCK_IRQ(sc);
#ifdef  ATH_SUPPORT_TxBF
        ath_tx_complete_buf(sc, bf, &bf_head, 0, 0, 0);
#else
        ath_tx_complete_buf(sc, bf, &bf_head, 0);
#endif
        ATH_UAPSD_LOCK_IRQ(sc);
    }

    /* reset the uapsd queue depth */
    an->an_uapsd_qdepth = 0;

    ATH_UAPSD_UNLOCK_IRQ(sc);
}

/*
 * Add frames to per node UAPSD queue.
 * Frames will be transmitted on receiving trigger.
 * Context: Tasklet
 */
void
ath_tx_queue_uapsd(struct ath_softc *sc, struct ath_txq *txq, ath_bufhead *bf_head, ieee80211_tx_control_t *txctl)
{
    struct ath_buf *bf, *bf_prev;
    struct ath_node *an = txctl->an;

    bf = TAILQ_FIRST(bf_head);

    if (txctl->ht && sc->sc_txaggr) {
        bf->bf_isampdu = 0;
        bf->bf_seqno = txctl->seqno; /* save seqno and tidno in buffer */
        bf->bf_tidno = txctl->tidno;
    }

    bf->bf_nframes = 1;
    ath_hal_setdesclink(sc->sc_ah, bf->bf_lastfrm->bf_desc, 0);
    bf->bf_lastbf = bf->bf_lastfrm; /* one single frame */
    bf->bf_next = NULL;

    /*
     * Lock out interrupts since this queue shall be accessed
     * in interrupt context.
     */
    ATH_UAPSD_LOCK_IRQ(sc);
    if (an->an_uapsd_last_frm)  {
        /* bf_prev: last buffer in the queue */
        bf_prev = TAILQ_LAST(&an->an_uapsd_q, ath_bufhead_s);
        ath_hal_setdesclink(sc->sc_ah, bf_prev->bf_desc, bf->bf_daddr);

        /* 
         * for EDMA, treat each frame as a separate unit and tx tasklet will complete each frame individually 
         * and call ath_tx_uapsd_complete for each frame. 
         */
        if (! sc->sc_enhanceddmasupport) {
            an->an_uapsd_last_frm->bf_next = bf;
        }
    }
    an->an_uapsd_last_frm = bf; 

    TAILQ_CONCAT(&an->an_uapsd_q, bf_head, bf_list);
    an->an_uapsd_qdepth++;

    DPRINTF(sc, ATH_DEBUG_UAPSD, "%s: depth %d \n", __func__, an->an_uapsd_qdepth);

    sc->sc_stats.ast_uapsddataqueued++;

    ATH_UAPSD_UNLOCK_IRQ(sc);
}


/*
 * Complete and Reclaim used QoS NULL buffer, requeue in free pool.
 * Context: Tasklet
 * Parameter held_uapsd_lock indicates whether the ATH_UAPSD_LOCK_IRQ is held
 * before calling this routine.
 */
void
ath_tx_uapsdqnulbf_complete(struct ath_softc *sc, struct ath_buf *bf, bool held_uapsd_lock)
{

    /*
     * Return back the QosNull frame that was gotten in the previous 
     * sc->sc_ieee_ops->uapsd_getqosnullframe() call.
     */
    if (bf->bf_node == NULL) {
        DPRINTF(sc, ATH_DEBUG_UAPSD, "%s: Error: bf_node is null. bf=%pK\n", __func__, bf);
    }
    else {
        struct ath_node *an;

        if (held_uapsd_lock) {
            /* Note: do not call uapsd_retqosnullframe() within the lock */
            ATH_UAPSD_UNLOCK_IRQ(sc);
        }

        an = bf->bf_node;
        sc->sc_ieee_ops->uapsd_retqosnullframe(an->an_node, bf->bf_mpdu);

        if (held_uapsd_lock) {
            /* Revert the earlier unlock */
            ATH_UAPSD_LOCK_IRQ(sc);
        }
    }

    if (!held_uapsd_lock) {
        ATH_UAPSD_LOCK_IRQ(sc);
    }

    TAILQ_INSERT_TAIL(&sc->sc_uapsdqnulbf, bf, bf_list);
    sc->sc_uapsdqnuldepth--;

    if (!held_uapsd_lock) {
        ATH_UAPSD_UNLOCK_IRQ(sc);
    }
}

/*
 * Handle transmit completion for UAPSD frames.
 * tx tasklet hand it over the UAPSD burst as a single unit.
 * Context: Tx tasklet
 */
#ifdef ATH_SUPPORT_TxBF
void
ath_tx_uapsd_complete(struct ath_softc *sc, struct ath_node *an, struct ath_buf *bf, ath_bufhead *bf_q, int txok, u_int8_t txbf_status, u_int32_t tstamp)
#else
void
ath_tx_uapsd_complete(struct ath_softc *sc, struct ath_node *an, struct ath_buf *bf, ath_bufhead *bf_q, int txok)
#endif
{
    ath_bufhead bf_head;
    struct ath_buf *bf_next, *bf_lastq = NULL;

    if(!an) {
        DPRINTF(sc, ATH_DEBUG_UAPSD, "%s: ath_node is null in bf %pK\n", __func__, bf);
        return;
    }

    TAILQ_INIT(&bf_head);

    while (bf) {

        bf_next = bf->bf_next;

        /*
         * Indicate EOSP to upper layer for appropriate handling.
         */
        ATH_UAPSD_LOCK_IRQ(sc);
        sc->sc_ieee_ops->uapsd_eospindicate(an->an_node, bf->bf_mpdu, txok, 0);
        ATH_UAPSD_UNLOCK_IRQ(sc);

        /*
         * Is this the last ath_buf in the txq
         */
        if (bf_next == NULL && !sc->sc_enhanceddmasupport) {
            KASSERT(bf->bf_lastfrm == bf->bf_lastbf, ("bf_lastfrm != bf->bf_lastbf"));

            bf_lastq = TAILQ_LAST(bf_q, ath_bufhead_s);
            if (bf_lastq) {
                TAILQ_REMOVE_HEAD_UNTIL(bf_q, &bf_head, bf_lastq, bf_list);
            } else {
                KASSERT(TAILQ_EMPTY(bf_q), ("bf_q NOT EMPTY"));
                TAILQ_INIT(&bf_head);
            }
        } else {
            KASSERT(!TAILQ_EMPTY(bf_q), ("bf_q EMPTY"));
            TAILQ_REMOVE_HEAD_UNTIL(bf_q, &bf_head, bf->bf_lastfrm, bf_list);
        }

        if (bf->bf_qosnulleosp) {
            if (!TAILQ_EMPTY(&bf_head)) {
                /* Assumed that bf_head contains a single bf */
                ASSERT(bf->bf_lastfrm == bf);
                ath_tx_uapsdqnulbf_complete(sc, bf, false);
            }
            sc->sc_stats.ast_uapsdqnulcomp++;
        } else {
            /* complete this frame */
#ifdef  ATH_SUPPORT_TxBF
            ath_tx_complete_buf(sc, bf, &bf_head, txok,  txbf_status, tstamp);
#else
            ath_tx_complete_buf(sc, bf, &bf_head, txok);
#endif
            sc->sc_stats.ast_uapsddatacomp++;
        }

        bf = bf_next;
    }
}

/*
 * Send a QoS NULL frame with EOSP set.  Used when we want to terminate the service
 * period because we have no data frames to send to this node.
 * Context: Interrupt
 */
static void
ath_uapsd_sendqosnull(struct ath_softc *sc, struct ath_node *an, u_int8_t ac)
{
    struct ath_buf *bf;
    ath_bufhead bf_head;
    int prate;

    if (!TAILQ_EMPTY(&sc->sc_uapsdqnulbf)) {
        bf = TAILQ_FIRST(&sc->sc_uapsdqnulbf);
        TAILQ_REMOVE(&sc->sc_uapsdqnulbf, bf, bf_list);
    } else {
        sc->sc_stats.ast_uapsdqnulbf_unavail++;
        DPRINTF(sc, ATH_DEBUG_UAPSD, "%s: qosnul buffers unavailable \n",__func__);
        return;
    }

    sc->sc_uapsdqnuldepth++;

    DPRINTF(sc, ATH_DEBUG_UAPSD, "%s: bf %pK ds %pK depth %d\n",
            __func__, bf, bf->bf_desc, sc->sc_uapsdqnuldepth);
    TAILQ_INIT(&bf_head);
    TAILQ_INSERT_TAIL(&bf_head, bf, bf_list);

    ath_rate_findrate(sc, an, AH_TRUE, 0, ATH_11N_TXMAXTRY,
                      ATH_RC_PROBE_ALLOWED, ac, bf->bf_rcs, &prate, AH_FALSE,
                      bf->bf_flags, NULL);

    bf->bf_node = an;
    ath_buf_set_rate(sc, bf);

    /*
     * Format a QoS NULL frame for this node and ac.
     * Make sure that sc->sc_ieee_ops->uapsd_retqosnullframe() is called when
     * this QosNull frame is no longer needed.
     */
    bf->bf_mpdu = sc->sc_ieee_ops->uapsd_getqosnullframe(an->an_node, bf->bf_mpdu, ac);

    ath_hal_setdesclink(sc->sc_ah, bf->bf_desc, 0);

#ifdef ATH_SWRETRY
    an->an_uapsd_num_addbuf = 1;
    DPRINTF(sc, ATH_DEBUG_SWR, "%s: qosnull frame sent to sc_uapsdq\n", __func__);
#endif
    /*
     * This buffer needs cache sync because we modified it here.
     */
    OS_SYNC_SINGLE(sc->sc_osdev, bf->bf_buf_addr[0], wbuf_get_pktlen(bf->bf_mpdu),
                    BUS_DMA_TODEVICE, OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));

    if (ath_tx_txqaddbuf(sc, sc->sc_uapsdq, &bf_head) != 0) {
        KASSERT(0, ("ath_uapsd_sendqosnull: HW Q Full - Not expected here\n"));
    }
    sc->sc_stats.ast_uapsdqnul_pkts++;
}

/*
 * This function will send UAPSD frames out to a destination node.
 * Context: Interrupt
 */
int
ath_process_uapsd_trigger(ath_dev_t dev, ath_node_t node, u_int8_t maxsp, u_int8_t ac, u_int8_t flush, bool *sent_eosp, u_int8_t maxqdepth)
{
    struct ath_node *an = ATH_NODE(node);
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_buf *bf, *first_bf, *last_bf = NULL;
    struct ath_txq *txq;
    int count;
    struct ieee80211_qosframe *whqos;
    ath_bufhead bf_q, bf_head;
    void *last_ds;
    struct ath_rc_series rcs[4];
    int prate;
    int max_count = maxsp;

    sc->sc_stats.ast_uapsdtriggers++;

    if (!(an->an_flags & ATH_NODE_UAPSD)) {
        sc->sc_stats.ast_uapsdnodeinvalid++;
        return -1;
    }

    if (sc->sc_enhanceddmasupport) {
        if (sc->sc_uapsdq->axq_depth == HAL_TXFIFO_DEPTH) {
            DPRINTF(sc, ATH_DEBUG_UAPSD, "%s: HW queue size %d SW queue size %d\n",
                    __func__, ath_hal_numtxpending(sc->sc_ah, sc->sc_uapsdq->axq_qnum),
                    sc->sc_uapsdq->axq_depth);
            sc->sc_stats.ast_uapsdedmafifofull++;
            return -1;
        }
    }

    ATH_UAPSD_LOCK_IRQ(sc);
    if (sc->sc_uapsd_pause_in_progress) {
        ATH_UAPSD_UNLOCK_IRQ(sc);
        return -1;
    }
    sc->sc_uapsd_trigger_in_progress = true;

    if (sent_eosp != NULL) {
        *sent_eosp = false;
    }

    /*
     * UAPSD queue is empty. Send QoS NULL if this is
     * not a flush operation.
     */
    if (TAILQ_EMPTY(&an->an_uapsd_q)) {
        if (!flush) {
            if (sent_eosp != NULL) {
                *sent_eosp = true;
            }
            ath_uapsd_sendqosnull(sc, an, ac);
        }
        sc->sc_uapsd_trigger_in_progress = false;
        ATH_UAPSD_UNLOCK_IRQ(sc);
        return 0;
    }

    TAILQ_INIT(&bf_q);
    TAILQ_INIT(&bf_head);

    /*
     * Send all frames
     */
#if 0
    if (max_count == maxqdepth)
        max_count = an->an_uapsd_qdepth;
#endif

    txq = sc->sc_uapsdq;

    ath_rate_findrate(sc, an, AH_TRUE, 0, ATH_11N_TXMAXTRY,
                      ATH_RC_PROBE_ALLOWED, ac, rcs, &prate, AH_FALSE,0, NULL);

    /*
     * create a UAPSD frame bust.
     * all the frames are collected into bf_q.
     * they are all linked with bf_next (like sub frames in an aggregate).
     * and only the last descriptor(last frame) have the interrupt bit
     * enabled. note that the tx tasklet complete the burst as a single 
     * unit (it treats this bust as a single unit) 
     */

#ifdef ATH_SWRETRY
	an->an_uapsd_num_addbuf=0;
#endif 
    for (count = 0; count < max_count; count++) {
        bf = TAILQ_FIRST(&an->an_uapsd_q);

        if (bf == NULL)
            break;

        /* Copy the rates into each buffer */
        memcpy(bf->bf_rcs, rcs, sizeof(rcs));
        ath_buf_set_rate(sc, bf);

        last_bf = bf;

        TAILQ_REMOVE_HEAD_UNTIL(&an->an_uapsd_q, &bf_head, bf->bf_lastfrm, bf_list);

        TAILQ_CONCAT(&bf_q, &bf_head, bf_list);

#ifdef ATH_SWRETRY
		if (!bf->bf_isampdu && bf->bf_isdata) 
			an->an_uapsd_num_addbuf++;
#endif 
    }
    if (TAILQ_EMPTY(&an->an_uapsd_q)) {
        an->an_uapsd_last_frm = NULL; 
    }
    first_bf = TAILQ_FIRST(&bf_q);
    /*
     * For non EDMA(osprey) create the UAPSD burst frame by linking
     * lastbf pointer of fisr buf to the last frame (lastbf).
     * For EDMA, treat each frame as a separate unit 
     */ 
    if (!sc->sc_enhanceddmasupport) {
        first_bf->bf_lastbf = last_bf->bf_lastbf;
        last_bf->bf_next = NULL;
    }
    an->an_uapsd_qdepth -= count;
#ifdef ATH_SWRETRY
    DPRINTF(sc, ATH_DEBUG_SWR, "%s: %d swr eligible frames sent to sc_uapsdq actual %d\n", __func__, an->an_uapsd_num_addbuf,count);
#endif
    /*
     * Request Tx Interrupt for last descriptor
     */
    last_ds = last_bf->bf_lastbf->bf_desc;
    ath_hal_txreqintrdesc(sc->sc_ah, last_ds);
    ath_hal_setdesclink(sc->sc_ah, last_ds, 0);

    /*
     * Mark EOSP flag at the last frame in this SP
     */
    whqos = (struct ieee80211_qosframe *)wbuf_header(last_bf->bf_mpdu);

    /* i_qos is available only for QOS data frame */
    if ((whqos->i_fc[0] & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_DATA) {
        if (!flush) {
#if ATH_SUPPORT_WIFIPOS        
            if (!sc->sc_ieee_ops->isthere_wakeup_request(an->an_node)) { 
#endif  
                if ((whqos->i_fc[0] & IEEE80211_FC0_TYPE_MASK) != IEEE80211_FC0_TYPE_MGT) {
                    whqos->i_qos[0] |= IEEE80211_QOS_EOSP;
                    sc->sc_stats.ast_uapsdeospdata++;
                }
#if ATH_SUPPORT_WIFIPOS            
            } else {
                whqos->i_fc[1] |= IEEE80211_FC1_MORE_DATA;
            }
#endif 
        }
	} else {
        /* TX post will not be able to find out the EOSP, force to clear it now */
        /* May be we can add a QOS NULL frame with the EOSP */
        sc->sc_ieee_ops->uapsd_eospindicate(an->an_node, last_bf->bf_mpdu, 1, 1);
    }

    sc->sc_stats.ast_uapsddata_pkts += count;

    /*
     * Clear More data bit for EOSP frame if we have
     * no pending frames
     */
    if (TAILQ_EMPTY(&an->an_uapsd_q)) {
#if ATH_SUPPORT_WIFIPOS        
        if (!sc->sc_ieee_ops->isthere_wakeup_request(an->an_node))
#endif  
            whqos->i_fc[1] &= ~IEEE80211_FC1_MORE_DATA;
    }

    /* Any changes to the QOS Data Header must be reflected to the physical buffer */
    wbuf_uapsd_update(last_bf->bf_mpdu);

    /*
     * The last buffer needs cache sync because we modified it here.
     */
    OS_SYNC_SINGLE(sc->sc_osdev, last_bf->bf_buf_addr[0], wbuf_get_pktlen(last_bf->bf_mpdu),
                    BUS_DMA_TODEVICE, OS_GET_DMA_MEM_CONTEXT(last_bf, bf_dmacontext));

    if (ath_tx_txqaddbuf(sc, txq, &bf_q) != 0) {
        KASSERT(0,
            ("ath_process_uapsd_trigger: HW Q Full - Not expected here\n"));
    }
    
    sc->sc_uapsd_trigger_in_progress = false;
    ATH_UAPSD_UNLOCK_IRQ(sc);
    DPRINTF(sc, ATH_DEBUG_UAPSD, "%s: HW queued %d SW queue depth %d \n", __func__, count, an->an_uapsd_qdepth);
    return (an->an_uapsd_qdepth);
}


#if ATH_VAP_PAUSE_SUPPORT
/*
 * queue the frame to the uapsd staging queue.
 * this is called from vap pause module ath_vap_pause.c.
 * part of vap pause operation , uapsd HW queue is stopped.
 * each of the UAPSD frame burst will be sent to this function.
 * this function will go through the burst and check the completion status
 * of the frame. if the frame is completed (success or failure) it will
 * completed and if the status is in progress then it will put it back to the
 * staging queue. Note that staging queue is use to collect all the frames from
 * uapsd queue before they can be prepended back to uapsd queue. The frames need
 * to  be prepended to keep the freame sequence in order.  
 * context: tasklet.
 */
void
ath_tx_stage_queue_uapsd(struct ath_softc *sc, struct ath_node *an, ath_bufhead *bf_head)
{
    struct ath_buf *bf, *bf_prev,*bf_next, *lastbf;
    struct ieee80211_qosframe *whqos;
    bool need_sync;
    struct ath_desc *ds;
    HAL_STATUS status;
    ath_bufhead bf_tmp_head;
    int stage_queued=0,completed=0, qosnull=0;

    bf = TAILQ_FIRST(bf_head);

    if (!bf ) {
        return;
    }
    if (bf->bf_lastbf) {
        ath_hal_setdesclink(sc->sc_ah, bf->bf_lastbf->bf_desc, 0);
    }
    while (bf) {

        need_sync = false;
        bf_next = bf->bf_next;

        bf->bf_status &= ~ATH_BUFSTATUS_STALE; 
        lastbf = bf->bf_lastfrm;
        ds = lastbf->bf_desc;    /* NB: last decriptor */

        status = ath_hal_txprocdesc(sc->sc_ah, ds);

        /* if it is a qos null frame , reclaim it regardless of the completion status */
        if (bf->bf_qosnulleosp) {
            TAILQ_REMOVE_HEAD_UNTIL(bf_head, &bf_tmp_head, bf->bf_lastfrm, bf_list);
            if (!TAILQ_EMPTY(&bf_tmp_head)) {
                /* Assumed that bf_tmp_head contains a single bf */
                ASSERT(bf->bf_lastfrm == bf);
                ath_tx_uapsdqnulbf_complete(sc, bf, false);
            }
            if (status != HAL_EINPROGRESS) {
                sc->sc_stats.ast_uapsdqnulcomp++;
            }
            ++qosnull;
            bf = bf_next;
            continue;
        } 

        if (status == HAL_EINPROGRESS) {
            bf->bf_lastbf = bf->bf_lastfrm; /* one single frame */
            bf->bf_next = NULL;

            whqos = (struct ieee80211_qosframe *)wbuf_header(bf->bf_mpdu);

            if (whqos->i_qos[0] & IEEE80211_QOS_EOSP) {
                whqos->i_qos[0] &= ~IEEE80211_QOS_EOSP;
                need_sync=true;
            }
            if ( ( whqos->i_fc[1] & IEEE80211_FC1_MORE_DATA ) == 0) {
                whqos->i_fc[1] |= IEEE80211_FC1_MORE_DATA;
                need_sync=true;
            }
            bf_prev = TAILQ_LAST(&an->an_uapsd_stage_q, ath_bufhead_s);
            if (bf_prev) {
                ath_hal_setdesclink(sc->sc_ah, bf_prev->bf_desc, bf->bf_daddr);
                if (!sc->sc_enhanceddmasupport) {
                    an->an_uapsd_last_stage_frm->bf_next = bf;
                }
            } 
            an->an_uapsd_last_stage_frm = bf;
            TAILQ_INIT(&bf_tmp_head);
            TAILQ_REMOVE_HEAD_UNTIL(bf_head, &bf_tmp_head, bf->bf_lastfrm, bf_list);
            TAILQ_CONCAT(&an->an_uapsd_stage_q,&bf_tmp_head, bf_list);
            an->an_uapsd_stage_qdepth++;
            ++stage_queued;
            /*
             * The buffer needs cache sync if we modified it.
             */
            if (need_sync) {
                /* Any changes to the QOS Data Header must be reflected to the physical buffer */
                wbuf_uapsd_update(bf->bf_mpdu);

                OS_SYNC_SINGLE(sc->sc_osdev, bf->bf_buf_addr[0], wbuf_get_pktlen(bf->bf_mpdu),
                               BUS_DMA_TODEVICE, OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
            }
        } else {
            int txok;
            /*
             * Indicate EOSP to upper layer for appropriate handling.
             */
            txok = (ds->ds_txstat.ts_status == 0);
            bf->bf_tx_ratecode = ds->ds_txstat.ts_ratecode;

            ATH_UAPSD_LOCK_IRQ(sc);
            sc->sc_ieee_ops->uapsd_eospindicate(an->an_node, bf->bf_mpdu, txok, 0);
            ATH_UAPSD_UNLOCK_IRQ(sc);
            TAILQ_INIT(&bf_tmp_head);
            TAILQ_REMOVE_HEAD_UNTIL(bf_head, &bf_tmp_head, bf->bf_lastfrm, bf_list);
            /* complete this frame */
#ifdef  ATH_SUPPORT_TxBF
            ath_tx_complete_buf(sc, bf, &bf_tmp_head, txok, ds->ds_txstat.ts_status, ds->ds_txstat.ts_tstamp);
#else
            ath_tx_complete_buf(sc, bf, &bf_tmp_head, txok);
#endif
            sc->sc_stats.ast_uapsddatacomp++;
            ++completed;
        }
        bf = bf_next;
    }
    DPRINTF(sc, ATH_DEBUG_UAPSD,"%s : staging queued %d completed %d qosnull %d\n",__func__,stage_queued, completed,qosnull);
}

/*
 * cleanup staging queue.
 */
void
ath_tx_uapsd_node_stageq_cleanup(struct ath_softc *sc, struct ath_node *an)
{
    struct ath_buf *bf,*bf_next;
    ath_bufhead bf_head;


    bf = TAILQ_FIRST(&an->an_uapsd_stage_q);
    /*
     * Drain the uapsd software staging queue.
     */
    while(bf) {

        TAILQ_REMOVE_HEAD_UNTIL(&an->an_uapsd_stage_q, &bf_head, bf->bf_lastfrm, bf_list);

        bf_next = TAILQ_FIRST(&an->an_uapsd_stage_q);
        /* complete this sub-frame */
#ifdef  ATH_SUPPORT_TxBF
        ath_tx_complete_buf(sc, bf, &bf_head, 0, 0, 0);
#else
        ath_tx_complete_buf(sc, bf, &bf_head, 0);
#endif
        bf=bf_next;
    }
}

/*
 * prepend the staging queue into the main queue.
 * caled from vap pause module(ath_vap_pause.c).
 * context: tasklet.
 */
void
ath_tx_prepend_uapsd_stage_queue(struct ath_softc *sc, struct ath_node *an)
{
    struct ath_buf *bf, *bf_prev;
    int count;
    ATH_UAPSD_LOCK_IRQ(sc);
    if (TAILQ_EMPTY(&an->an_uapsd_stage_q)) {
        ATH_UAPSD_UNLOCK_IRQ(sc);
        return;
    }
    if (an->an_flags & ATH_NODE_CLEAN) {
        /* if node has already been cleaned up, then clean up the stage queue frames as well */
        ATH_UAPSD_UNLOCK_IRQ(sc);
        ath_tx_uapsd_node_stageq_cleanup(sc,an);
        return;
    }
    bf = TAILQ_FIRST(&an->an_uapsd_q);
    bf_prev = TAILQ_LAST(&an->an_uapsd_stage_q, ath_bufhead_s);
    /* bf_prev can not be NULL since the staging queue is not empty */
    if (bf ) {
        ath_hal_setdesclink(sc->sc_ah, bf_prev->bf_desc, bf->bf_daddr);
        if (!sc->sc_enhanceddmasupport) {
            an->an_uapsd_last_stage_frm->bf_next = bf;
        }
    } else {
        /* if no frames on uapsd queue the last staging frame will become last frame */
        an->an_uapsd_last_stage_frm->bf_next = NULL;
        an->an_uapsd_last_frm = an->an_uapsd_last_stage_frm;;
    }
    
    TAILQ_CONCAT(&an->an_uapsd_stage_q, &an->an_uapsd_q, bf_list);
    TAILQ_INIT(&an->an_uapsd_q);
    TAILQ_CONCAT(&an->an_uapsd_q, &an->an_uapsd_stage_q, bf_list);
    TAILQ_INIT(&an->an_uapsd_stage_q);
    count = an->an_uapsd_stage_qdepth;
    an->an_uapsd_qdepth += an->an_uapsd_stage_qdepth;
    an->an_uapsd_stage_qdepth=0;
    an->an_uapsd_last_stage_frm = NULL;
    ATH_UAPSD_UNLOCK_IRQ(sc);
	DPRINTF(sc, ATH_DEBUG_UAPSD, "%s: requeued %d depth %d \n",__func__,count,an->an_uapsd_qdepth);
}


/* 
 * to mark begining of pause operation and
 * also to synchronize with the in progress
 * trigger operation.Wait for the uapsd trigger processing
 * to complete.
 */
void
ath_tx_uapsd_pause_begin(struct ath_softc *sc)
{
    ATH_UAPSD_LOCK_IRQ(sc);
    sc->sc_uapsd_pause_in_progress = AH_TRUE;
    while (sc->sc_uapsd_trigger_in_progress) {
        ATH_UAPSD_UNLOCK_IRQ(sc);
        OS_DELAY(10);
        ATH_UAPSD_LOCK_IRQ(sc);
    }
    ATH_UAPSD_UNLOCK_IRQ(sc);
}

void
ath_tx_uapsd_pause_end(struct ath_softc *sc)
{
    ATH_UAPSD_LOCK_IRQ(sc);
    sc->sc_uapsd_pause_in_progress = AH_FALSE;
    ATH_UAPSD_UNLOCK_IRQ(sc);

}
#endif /* ATH_VAP_PAUSE_SUPPORT */

#endif /* ATH_SUPPORT_UAPSD */






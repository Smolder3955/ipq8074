/*
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*
 *  Copyright (c) 2005 Atheros Communications Inc.  All rights reserved.
 */
/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include "ath_internal.h"
#ifndef REMOVE_PKT_LOG
#include "pktlog.h"
extern struct ath_pktlog_funcs *g_pktlog_funcs;
#endif

#if ATH_VAP_PAUSE_SUPPORT

/*
 * set vap pause in progress flag .
 */
static inline void 
ath_vap_pause_set_in_progress(struct ath_softc *sc)
{
    int iter_count=0;
    systime_t           start_timestamp  = OS_GET_TIMESTAMP(); 
    systime_t           cur_timestamp ; 
    
    while(atomic_read(&sc->sc_txq_use_cnt)) {
        OS_DELAY(10);                            
        ++ iter_count;                           
        if ((iter_count % 1000) == 0) {          
            cur_timestamp  = OS_GET_TIMESTAMP();    
            if (CONVERT_SYSTEM_TIME_TO_MS(cur_timestamp - start_timestamp) >= VAP_PAUSE_SYNCHRONIZATION_TIMEOUT) { 
                printk("WARNING: Timeout in reading sc_txq_use_cnt, iter_count %d \n", iter_count);
                ASSERT(0);                         
            }                                      
        }                                        
    }
    atomic_set(&sc->sc_vap_pause_in_progress, 1);
}
 
/*
 * clear vap pause in progress flag. 
 */
static inline void 
ath_vap_pause_clear_in_progress(struct ath_softc *sc)
{
    atomic_set(&sc->sc_vap_pause_in_progress, 0);
}
static void ath_tx_vap_pause_txq(struct ath_softc *sc, struct ath_txq *txq, struct ath_vap *avp);
static bool ath_tx_vap_check_txq_needs_pause(struct ath_softc *sc, struct ath_txq *txq, struct ath_vap *avp);


/*
 * pause the xmit traffic of a specified vap
 * and requeue the traffic back on to the 
 * corresponding nodes (tid queues of nodes).
 * if vap is null then pause all the vaps.
 * handle both aggregates and non aggregate
 * frames. in the case of management frames
 * the frames are dropped (completed with error).
 * the caller should have paused all the nodes (tids of the nodes)
 * before pausing the vap.
 */ 
static void ath_tx_vap_pause_txqs(struct ath_softc *sc,  struct ath_vap *avp )
{
    u_int32_t i;
    int npend = 0;
    struct ath_hal *ah = sc->sc_ah;
    u_int8_t q_needs_pause[HAL_NUM_TX_QUEUES];
    int restart_after_reset=0;

    ATH_INTR_DISABLE(sc);
    spin_lock_dpc(&(sc)->sc_vap_pause_lock); // This lock is only used for escalate irql
restart:
    npend = 0;

    ath_vap_pause_set_in_progress(sc);

    OS_MEMZERO(q_needs_pause, sizeof(q_needs_pause));
    /* 
     * stop all the HAL data queues
     */
    if (!sc->sc_invalid) {
        struct ath_txq *txq=NULL;
        if (avp == NULL && sc->sc_fastabortenabled) {
           (void) ath_hal_aborttxdma(ah);
            for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {
                if (ATH_TXQ_SETUP(sc, i)) {
                    int n_q_pending=0;
                    txq = &sc->sc_txq[i];
                    /* The TxDMA may not really be stopped.
                     * Double check the hal tx pending count
                     */
                     n_q_pending = ath_hal_numtxpending(ah, sc->sc_txq[i].axq_qnum);
                     if (n_q_pending) {
                        (void) ath_hal_stoptxdma(ah, txq->axq_qnum, 0);
                        npend += ath_hal_numtxpending(ah, sc->sc_txq[i].axq_qnum);
                     }
                }
                /*
                 * at this point all Data queues are paused
                 * all the queues need to processed and restarted.
                 */
                q_needs_pause[i] = AH_TRUE; 
            }
        } else {
            for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {
                if (ATH_TXQ_SETUP(sc, i)) {
                    txq = &sc->sc_txq[i];
                    /* check if the queue needs to be paused */
                    q_needs_pause[i] = ath_tx_vap_check_txq_needs_pause(sc,txq,avp);
                }
            }

            (void) ath_hal_aborttxdma(ah);

            for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {
                if (ATH_TXQ_SETUP(sc, i)) {
                    int n_q_pending=0;
                    txq = &sc->sc_txq[i];
                     n_q_pending = ath_hal_numtxpending(ah, sc->sc_txq[i].axq_qnum);
                     if (n_q_pending) {
                        npend += ath_hal_numtxpending(ah, sc->sc_txq[i].axq_qnum);
                     }
                }
            }
        }
    }

    if (npend && !restart_after_reset) {
        ath_vap_pause_clear_in_progress(sc);
        spin_unlock_dpc(&(sc)->sc_vap_pause_lock);
#ifdef AR_DEBUG		
       ath_dump_descriptors(sc);
#endif
        /* TxDMA not stopped, reset the hal */
        DPRINTF(sc, ATH_DEBUG_RESET, "%s: Unable to stop TxDMA. Reset HAL!\n", __func__);

#if ATH_DATABUS_ERROR_RESET_LOCK_3PP
        ATH_INTERNAL_RESET_LOCK(sc);
#endif
        ath_reset_start(sc, 0, 0, 0);
        ath_reset(sc);
        ath_reset_end(sc, 0);
#if ATH_DATABUS_ERROR_RESET_LOCK_3PP
        ATH_INTERNAL_RESET_UNLOCK(sc);
#endif
        restart_after_reset=1;
        spin_lock_dpc(&(sc)->sc_vap_pause_lock);
        goto restart;
    }

    if (npend && restart_after_reset) {
        /* TxDMA not stopped, reset the hal */
        DPRINTF(sc, ATH_DEBUG_RESET, "%s: Unable to stop TxDMA Even after Reset, ignore and continue \n", __func__);
    }
    /* TO-DO need to handle cab queue */

    /* at this point the HW xmit should have been completely stopped. */
    if (sc->sc_enhanceddmasupport) {
        ath_tx_edma_process(sc);
    }
    for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {
        if (q_needs_pause[i]) {
#ifdef ATH_TX_BUF_FLOW_CNTL
            struct ath_txq *txq = &sc->sc_txq[i];
#if ATH_DEBUG == 0
            /* Fix the compile error when ATH_DEBUG = 0 */
            txq = txq;
#endif 
            DPRINTF(sc, ATH_DEBUG_ANY,"#####%s : %d  qnum %d buf_used %d \n",
                            __func__,__LINE__,txq->axq_qnum, txq->axq_num_buf_used );
#endif
            if (!sc->sc_enhanceddmasupport) {
                ath_tx_processq(sc, &sc->sc_txq[i]); /* process any frames that are completed */
            }
            ath_tx_vap_pause_txq(sc, &sc->sc_txq[i], avp);
#ifdef ATH_TX_BUF_FLOW_CNTL
            DPRINTF(sc, ATH_DEBUG_ANY,"#####%s : %d  qnum %d buf_used %d  \n",
                            __func__,__LINE__,txq->axq_qnum, txq->axq_num_buf_used);
#endif
        }
    }
    ath_vap_pause_clear_in_progress(sc);
    spin_unlock_dpc(&(sc)->sc_vap_pause_lock);
    ATH_INTR_ENABLE(sc);
}

/*
 * unpause the xmit traffic of a specified vap
 * handle both aggregates and non aggregate
 * frames. 
 */ 
static void ath_tx_vap_unpause_txqs(struct ath_softc *sc,  struct ath_vap *avp)
{

}

/*
 * pause traffic of a vap from a specified queue.
 * if vap is null all the traffic will be paused.
 */ 
static void ath_tx_vap_pause_txq(struct ath_softc *sc, struct ath_txq *txq, struct ath_vap *avp)
{
   struct ath_buf *bf, *lastbf;
   ath_bufhead bf_head, bf_stage;
   struct ath_node *an,*an_uapsd_head;
   ath_bufhead    vap_mcast_stage_q[ATH_VAPSIZE];         /* temprorary per vap staging queue for cabq traffic */
   struct ath_vap        *mcast_vap_q[ATH_VAPSIZE] = {NULL,};
   u_int32_t i;
   struct ieee80211_frame *wh;

   if (txq == sc->sc_cabq) {
       for (i=0;i<ATH_VAPSIZE;++i) {
          TAILQ_INIT(&vap_mcast_stage_q[i]);
       }
   }

   an_uapsd_head=NULL;

   TAILQ_INIT(&bf_stage);
    /*
     * NB: this assumes output has been stopped and
     *     we do not need to block ath_tx_tasklet
     */
    for (;;) {
        ATH_TXQ_LOCK(txq);
        if (sc->sc_enhanceddmasupport) {
            bf = TAILQ_FIRST(&txq->axq_fifo[txq->axq_tailindex]);
            if (bf == NULL) {
                if (txq->axq_headindex != txq->axq_tailindex)
                    printk("ath_tx_draintxq: ERR head %d tail %d\n",
                           txq->axq_headindex, txq->axq_tailindex);
                txq->axq_headindex = 0;
                txq->axq_tailindex = 0;
                ATH_TXQ_UNLOCK(txq);
                break;
            }
        } else {
            bf = TAILQ_FIRST(&txq->axq_q);
            if (bf == NULL) {
                txq->axq_link = NULL;
                txq->axq_linkbuf = NULL;
                ATH_TXQ_UNLOCK(txq);
                break;
            }

            if (bf->bf_status & ATH_BUFSTATUS_STALE) {
                ATH_TXQ_REMOVE_STALE_HEAD(txq, bf, bf_list);
                ATH_TXQ_UNLOCK(txq);
#ifdef ATH_SUPPORT_UAPSD
                if (bf->bf_qosnulleosp) {

                    ath_tx_uapsdqnulbf_complete(sc, bf, false);

                } else
#endif
                {
                    ATH_TXBUF_LOCK(sc);
                    sc->sc_txbuf_free++;
#if ATH_TX_BUF_FLOW_CNTL
					if(bf) {
                    txq->axq_num_buf_used--;
					}
#endif
                    TAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
                    
#if TRACE_TX_LEAK                
                    TAILQ_REMOVE(&sc->sc_tx_trace_head,bf,bf_tx_trace_list);
#endif //TRACE_TX_LEAK

                    ATH_TXBUF_UNLOCK(sc);
#if ATH_SUPPORT_FLOWMAC_MODULE
                    if (sc->sc_osnetif_flowcntrl) {
                        ath_netif_wake_queue(sc);
                    }
#endif
                }
                continue;
            }
        }

        lastbf = bf->bf_lastbf;
 
        TAILQ_INIT(&bf_head);

        /* remove ath_buf's of the same mpdu from txq */
        if (sc->sc_enhanceddmasupport) {
             if (txq == sc->sc_cabq || txq == sc->sc_uapsdq) {
                 ATH_EDMA_MCASTQ_MOVE_HEAD_UNTIL(txq, &bf_head, lastbf, bf_list);
             } else {
                 ATH_EDMA_TXQ_MOVE_HEAD_UNTIL(txq, &bf_head, lastbf, bf_list);
             }
        } else {
            ATH_TXQ_MOVE_HEAD_UNTIL(txq, &bf_head, lastbf, bf_list);
        }

        txq->axq_totalqueued --;                                      

        if (bf->bf_isaggr) {
            txq->axq_aggr_depth--;
        }
#if ATH_SUPPORT_CFEND
        if (txq == sc->sc_cfendq) {
            /* process CF End packet */
            if (bf->bf_state.bfs_iscfend) {
                ath_tx_cfend_complete (sc, bf, &bf_head);
                ATH_TXQ_UNLOCK(txq);
                continue; /* process rest of the buffers */
            }
        }
#endif


        ATH_TXQ_UNLOCK(txq);

#ifdef AR_DEBUG
        if (!sc->sc_enhanceddmasupport && CHK_SC_DEBUG(sc, ATH_DEBUG_RESET))
            /* Legacy only as the enhanced DMA txprocdesc() 
             * will move the tx status ring tail pointer.
             */
            ath_printtxbuf(bf, ath_hal_txprocdesc(sc->sc_ah, bf->bf_desc) == HAL_OK);
#endif /* AR_DEBUG */

        an = bf->bf_node;
        /*
         * if the node belongs to the vap being paused (or) if the request
         * is to pause all vaps (avp is NULL)
         * then put it back in to the nodes queue.
         */
        if (!avp || (avp && an->an_avp == avp) ) {
#ifdef ATH_SUPPORT_UAPSD
            if (txq == sc->sc_uapsdq) {
                /* 
                 * if the node is not on the UAPSD node list then put it on the list.
                 * alwasys put it on the head of the list.
                 */
                if (!an->an_temp_next && (an != an_uapsd_head)) {
                    if(an_uapsd_head){
                        an->an_temp_next = an_uapsd_head;
                    }
                    an_uapsd_head = an;
                }
                if (TAILQ_FIRST(&bf_head) == NULL ) {
                    DPRINTF(sc, ATH_DEBUG_ANY,"#####%s : %d  bf_head is empty \n",__func__, __LINE__);
                } else {
                    ath_tx_stage_queue_uapsd(sc,an, &bf_head);
                }
                continue;
            }

#endif
            if (txq == sc->sc_cabq) {
                ath_bufhead    *mcast_stage_q = NULL;
                /* 
                 * get the mcast staging queue for this vap and 
                 * add the frame to the mcast staging queue.
                 */

                for (i=0;i<ATH_VAPSIZE;++i) {
                   if (mcast_vap_q[i] == avp) {
                       mcast_stage_q =  &vap_mcast_stage_q[i]; 
                   } else if (mcast_vap_q[i] == NULL) {
                       mcast_stage_q =  &vap_mcast_stage_q[i]; 
                       mcast_vap_q[i] = avp;
                   }
                   if (mcast_stage_q ) {
                       break;
                   }
                }

                if (mcast_stage_q == NULL) {
                   DPRINTF(sc, ATH_DEBUG_ANY, "%s: mcat_stage_q is NULL \n", __func__);
                   continue; 
                }

                TAILQ_CONCAT(mcast_stage_q, &bf_head, bf_list);                   
                continue;
            }

            if (bf->bf_isampdu) {
                if (!bf->bf_isaggr) {
                    __11nstats(sc,tx_unaggr_comperror);
                }
                ath_tx_mark_aggr_rifs_done(sc, txq, bf, &bf_head,
                                          &((struct ath_desc *)(lastbf->bf_desc))->ds_txstat, 0);
            } else {
#ifdef ATH_SWRETRY
                if (sc->sc_enhanceddmasupport) {
                    /*
                     * Decrement of swr_num_eligible_frms for AMPDU is done
                     * above in ath_tx-complete_aggr_rifs.
                     */
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
                if (bf->bf_isbar) {
                    DPRINTF(sc, ATH_DEBUG_RESET, "*****%s: BAR frame \n", __func__);
#ifdef ATH_SUPPORT_TxBF
                    ath_tx_complete_buf(sc, bf, &bf_head, 0, 0, 0);
#else
                    ath_tx_complete_buf(sc, bf, &bf_head, 0);
#endif
                } else {
                    /*
                     *  Non Aggregates, put them at the head of the tid queue (if node is still avail,)
                     */

                    atomic_inc(&an->an_active_tx_cnt);
                    /* Make sure that Node is still alive and not temporary node */
                    if ((an->an_flags & (ATH_NODE_TEMP | ATH_NODE_CLEAN)) == 0) {
                        struct ath_atx_tid *tid = ATH_AN_2_TID(an, bf->bf_tidno);
                        TAILQ_INSERTQ_HEAD(&tid->buf_q, &bf_head, bf_list);
                        atomic_dec(&an->an_active_tx_cnt);
                    }
                    else {
                        if ((an->an_flags & ATH_NODE_TEMP) != 0) {
                            DPRINTF(sc, ATH_DEBUG_ANY, "%s: an=0x%pK is Temp-node.\n", __func__, an);
                        }
                        if ((an->an_flags & ATH_NODE_CLEAN) != 0) {
                            DPRINTF(sc, ATH_DEBUG_ANY, "%s: an=0x%pK is already CLEAN.\n", __func__, an);
                        }

                        atomic_dec(&an->an_active_tx_cnt);
                        
                        // Free these buffers.
#ifdef ATH_SUPPORT_TxBF
                        ath_tx_complete_buf(sc, bf, &bf_head, 0, 0, 0);
#else
                        ath_tx_complete_buf(sc, bf, &bf_head, 0);
#endif
                    }
                }
            }
        } else {
            /*
             * if the frame does not need to be paused
             * then put it on to a staging queue.
             */
            TAILQ_CONCAT(&bf_stage, &bf_head, bf_list);                   
        }
    }

#ifdef ATH_SUPPORT_UAPSD
    while(an_uapsd_head) {
        an=an_uapsd_head;
        an_uapsd_head = an->an_temp_next;
        an->an_temp_next=NULL;
        ath_tx_prepend_uapsd_stage_queue(sc,an);
    }
#endif
    /* prepend the staging queue back to vap mcast queue */
    if (txq == sc->sc_cabq) {
        ath_bufhead    *mcast_stage_q = NULL;

        for (i=0;i<ATH_VAPSIZE;++i) {
           mcast_stage_q =  &vap_mcast_stage_q[i]; 
           /*
            * prepend only if the mcast staging queue is not empty
            */ 
           if (TAILQ_FIRST(mcast_stage_q))   { 
              /*
               * need to prepend the frames from staging queue to the vap mcast queue.
               * do it in 2 steps.
               * move the frames from the vap mcast queue to the
               * end of the staging queue and move all the frames from staging queue 
               * to the vaps mcast queue.
               */ 
              TAILQ_CONCAT(mcast_stage_q, &mcast_vap_q[i]->av_mcastq.axq_q, bf_list);
              mcast_vap_q[i]->av_mcastq.axq_depth=0; 
              mcast_vap_q[i]->av_mcastq.axq_totalqueued = 0;                        
              mcast_vap_q[i]->av_mcastq.axq_linkbuf = 0;                        
              mcast_vap_q[i]->av_mcastq.axq_link = NULL;                        

              bf = TAILQ_FIRST(mcast_stage_q);
              while (bf) {
                  /*
                   * Remove a single ath_buf from the staging  queue and add it to
                   * the mcast queue.
                   */
                  lastbf = bf->bf_lastbf;

                   wh = (struct ieee80211_frame *)wbuf_header(bf->bf_mpdu);
                  
                   DPRINTF(sc, ATH_DEBUG_ANY, "%s: queue mcast frame back seq # %d \n", __func__,
                        le16toh(*(u_int16_t *)wh->i_seq) >> IEEE80211_SEQ_SEQ_SHIFT);
                  TAILQ_REMOVE_HEAD_UNTIL(mcast_stage_q, &bf_head, lastbf, bf_list);
                  if (ath_tx_mcastqaddbuf(sc, mcast_vap_q[i], &bf_head) != EOK) {
                      /* failed to queue the buf, complete it with an error */
#ifdef ATH_SUPPORT_TxBF
                      ath_tx_complete_buf(sc,bf,&bf_head,0,0, 0);
#else
                      ath_tx_complete_buf(sc,bf,&bf_head,0);
#endif
                  }

                  bf = TAILQ_FIRST(mcast_stage_q);
              }
           }
        }
    }

    /*
     * put the frames from the stage list back on to the HW queue
     * and restart the HW queue.
     */
    for (;;) {
        bf = TAILQ_FIRST(&bf_stage);
        if (bf == NULL) {
            break;
        }
#ifdef ATH_SWRETRY
        /*
         * Decrement of swr_num_eligible_frms for frms in bf_stage.
         * Frms in bf_stage already incr the counter once.
         * Hence we decr counter first before giving to HW (incr counter again)
         */
        if (bf->bf_isdata) {
            struct ath_node *an = bf->bf_node;
            struct ath_swretry_info *pInfo = &an->an_swretry_info[txq->axq_qnum];
            ATH_NODE_SWRETRY_TXBUF_LOCK(an);
            ASSERT(pInfo->swr_num_eligible_frms);
            pInfo->swr_num_eligible_frms --;
            ATH_NODE_SWRETRY_TXBUF_UNLOCK(an);
        }
#endif 
        lastbf = bf->bf_lastbf;
        TAILQ_REMOVE_HEAD_UNTIL(&bf_stage, &bf_head, lastbf, bf_list);
        ATH_TXQ_LOCK(txq);
        if (ath_tx_txqaddbuf(sc, txq, &bf_head) != 0) {
            DPRINTF(sc, ATH_DEBUG_ANY, "++++====%s[%d]:"
                    " ath_tx_txqaddbuf failed \n", __func__, __LINE__);
        }
        ATH_TXQ_UNLOCK(txq);
    }

}

/**
 * check if the txq needs pause .
 * txq needs to pause
 *  if the avp is null (paus all vaps) and txq has some frames  OR
 *  if the avp is non ull (paus all vaps) and txq has some frames for tha vap.
 */ 
static bool ath_tx_vap_check_txq_needs_pause(struct ath_softc *sc, struct ath_txq *txq, struct ath_vap *avp)
{
    struct ath_buf *bf;
    struct ath_node *an;
    u_int32_t tailindex=0;


    ATH_TXQ_LOCK(txq);

#if ATH_SUPPORT_CFEND
    /* no need to pause cfend queue, let the cf end frames go out */
    if (txq == sc->sc_cfendq) {
        ATH_TXQ_UNLOCK(txq);
        return false;
    }
#endif

    if (sc->sc_enhanceddmasupport) {
        bf = TAILQ_FIRST(&txq->axq_fifo[txq->axq_tailindex]);
        tailindex = txq->axq_tailindex;
    }  else {
        bf = TAILQ_FIRST(&txq->axq_q);
    }

    /* 
     * if no frames then no need to pause.
     */
    if (bf == NULL) {
        ATH_TXQ_UNLOCK(txq);
        return false;
    }

    /* 
     * there is  atleast one frame and the request is to pausu all
     * vaps so we need to pause the queue.
     */
    if (avp == NULL) {
        ATH_TXQ_UNLOCK(txq);
        return true;
    }
    while(bf) {
        /*
         * ignore stale buffers when checking .
         * stale buffer has valid bf_node pointers but
         * no real data.
         */
        if (!(bf->bf_status & ATH_BUFSTATUS_STALE)) {
            an = bf->bf_node;
            /*
             * if the node belongs to the vap being paused.
             * then we need to pause.
             */ 
            if (an->an_avp == avp) {
                ATH_TXQ_UNLOCK(txq);
                return true;
            }
        }
        /* move to the next frame */
        if (sc->sc_enhanceddmasupport) {
            tailindex = (tailindex+1) & (HAL_TXFIFO_DEPTH-1);  
            /* bail out if reached end of the circular list */
            if (tailindex == txq->axq_tailindex) {
               bf=NULL;
            } else {
               bf = TAILQ_FIRST(&txq->axq_fifo[tailindex]);
            }
        } else {
           
            /*
             * a stale buffer marked with the status ATH_BUFSTATUS_STALE.
             * will not have the lastbf pointer set (weird !).
             * just move on to the next frame if we run into stale buffer.
             */
            
            if (bf->bf_lastbf) {
                bf = TAILQ_NEXT(bf->bf_lastbf,bf_list);
            }  else {
                bf = TAILQ_NEXT(bf,bf_list);
            }
        }
    }

    ATH_TXQ_UNLOCK(txq);
    /* no frame on the txq belongs to the vap , so no need to stop */
    return false;
}

void ath_tx_vap_pause_control(struct ath_softc *sc,  struct ath_vap *avp, bool pause )
{

    if (pause) {
        ath_tx_uapsd_pause_begin(sc);
        ath_tx_vap_pause_txqs(sc,avp);
    } else {
        ath_tx_vap_unpause_txqs(sc,avp);
        ath_tx_uapsd_pause_end(sc);
    }
}


#endif /*  ATH_VAP_PAUSE_SUPPORT */

/*
 * Copyright (c) 2005, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2013, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/*! \file ath_aponly.c
**
** export lmac symbols for the aponly code
**
*/

#include "osdep.h"
#include "ath_internal.h"
#include "ath_hwtimer.h"
#include "if_athvar.h"

#if UMAC_SUPPORT_APONLY
#if ATH_TX_COMPACT
extern void
ath_bar_tx(struct ath_softc *sc, struct ath_node *an, struct ath_atx_tid *tid);

/* Cleaning of the buffers belonged to the Node/tid which has been marked cleanup */
void ath_node_leftovercleanup( struct ath_softc *sc, struct ath_txq * txq, ath_bufhead *bf_pending,
        ath_bufhead *bf_headfree,struct ath_atx_tid *tid)
{
    struct ath_buf *bf = NULL;

    ATH_TXQ_LOCK(txq);
    bf = TAILQ_FIRST(bf_pending);
    while (bf != NULL){
        ath_tx_update_baw(sc, tid, bf->bf_seqno);
        TAILQ_REMOVE(bf_pending,bf,bf_list);
        TAILQ_INSERT_TAIL(bf_headfree,bf,bf_list);
        bf = TAILQ_FIRST(bf_pending);
    }

    if (tid->baw_head == tid->baw_tail) {
        tid->addba_exchangecomplete = 0;
        tid->addba_exchangeattempts = 0;
        tid->addba_exchangestatuscode = IEEE80211_STATUS_UNSPECIFIED;
        /* resume the tid */
        qdf_atomic_dec(&tid->paused);
        tid->cleanup_inprogress = AH_FALSE;
    }

    ATH_TXQ_UNLOCK(txq);
}


#if ATH_SUPPORT_IQUE
int
_modify_sw_retry_limit(struct ath_softc *sc,struct ath_buf *bf)
{
    struct ath_node *tan;
    int sw_retry_limit = ATH_MAX_SW_RETRIES;


#if ATH_SUPPORT_VOWEXT && ATH_SUPPORT_IQUE
    if((TID_TO_WME_AC(bf->bf_tidno) == WME_AC_VI) && ((ATH_NODE_ATHEROS( (struct ath_node *)bf->bf_node))->txRateCtrlViVo.consecRtsFailCount))
    {
        /* Increase the sw retry limit for video packets in case
           we are seeing consecutive RTS failures with this node */
        /* This is to minimize the packet drops at Tx side(PLR)
           because of STA side reset (triggerd by BB panic) */
        sw_retry_limit = (ATH_MAX_SW_RETRIES * 2);
    }
#endif

#if ATH_SUPPORT_IQUE
    /* For the frames to be droped who block the headline of the AC_VI queue,
     * these frames should not be sw-retried. So mark them as already xretried.
     */
    tan = ATH_NODE(bf->bf_node);
    if (sc->sc_ieee_ops->get_hbr_block_state(tan->an_node) &&
            TID_TO_WME_AC(bf->bf_tidno) == WME_AC_VI) {
        bf->bf_retries = sw_retry_limit;
    }
#endif
    return sw_retry_limit;
}

#endif

int
_txcomp_handle_subframe_retry ( struct ath_softc *sc,
                         struct ath_buf *bf ,
                         struct ath_atx_tid *tid  ,
                         int sw_retry_limit,
                         int *sendbar,
                         ath_bufhead *bf_pending,
                         u_int32_t currts)
{
#if ATH_SUPPORT_IQUE && ATH_SUPPORT_IQUE_EXT
    u_int32_t lapsed_td, qin_ts, fxmit_ts;
#endif


#if ATH_SUPPORT_VOWEXT && ATH_SUPPORT_IQUE
           if((bf->bf_retries > sw_retry_limit ) &&  !(atomic_read(&sc->sc_in_reset))){

#else
            if((bf->bf_retries > ATH_MAX_SW_RETRIES ) &&  !(atomic_read(&sc->sc_in_reset))){
#endif
                struct ath_node *an = bf->bf_node;
                struct ath_vap *avp = an->an_avp;

                avp->av_stats.av_tx_xretries++;
                bf->bf_isxretried = 1;
               *sendbar = tid->addba_exchangecomplete;
                return 1; /*xretried*/
            }
            ath_tx_set_retry(sc, bf);
#if ATH_SUPPORT_IQUE && ATH_SUPPORT_IQUE_EXT
            /*
             *   Note: This reset-in-progress could cause the retry timeout to be exceeded
             *   since we are requeuing this packet regardless of the duration time. See above
             *    check for sc->sc_in_reset.
             */
            qin_ts = wbuf_get_qin_timestamp(bf->bf_mpdu);
            fxmit_ts = wbuf_get_firstxmitts(bf->bf_mpdu);
            lapsed_td = (currts>=qin_ts) ? (currts - qin_ts) :
                ((0xffffffff - qin_ts) + currts);

            bf->bf_txduration = (currts>=fxmit_ts) ? (currts - fxmit_ts) :
                ((0xffffffff - fxmit_ts) + currts);

            if (((bf->bf_txduration >= sc->sc_retry_duration) && (sc->sc_retry_duration > 0) && (!atomic_read(&sc->sc_in_reset))) ||
                    ((lapsed_td >= sc->total_delay_timeout) && (sc->total_delay_timeout > 0) && (!atomic_read(&sc->sc_in_reset))))
            {
                __11nstats(sc, tx_xretries);
                bf->bf_isxretried = 1;
                *sendbar = tid->addba_exchangecomplete;
                return 1; /*xretried */
            } else {
                TAILQ_INSERT_HEAD(bf_pending, bf, bf_list);
            }
#else
            TAILQ_INSERT_HEAD(bf_pending, bf, bf_list);
#endif

    return 0;
}

#if ATH_SUPPORT_IQUE && ATH_SUPPORT_IQUE_EXT
#define _IQUE_EXT_GETCURRTS(_sc, _currts) \
    if(_sc->sc_retry_duration>0 || _sc->total_delay_timeout>0) {\
      _currts = ath_hal_gettsf32(sc->sc_ah);\
    }\
    else {\
      _currts = 0;\
    }
#else
#define _IQUE_EXT_GETCURRTS(_sc,_currts)
#endif

#if ATH_SUPPORT_VOWEXT
void
_txcomp_vow_handle_sorting(struct ath_softc *sc,struct ath_atx_tid *tid , ath_bufhead *bf_pending)
{
    int8_t is_sorted = 1;


    if ((ATH_IS_VOWEXT_BUFFREORDER_ENABLED(sc)))
    {
        if (TAILQ_EMPTY(&tid->buf_q)) {
            is_sorted = 0;
        } else if( ath_insertq_inorder(sc, tid, bf_pending) < 0) {
            is_sorted= 0;
        }
    } else {
        is_sorted = 0;
    }

    /* either not VI, or not VOW enabled */
    if (!is_sorted) {
        TAILQ_INSERTQ_HEAD(&tid->buf_q, bf_pending, bf_list);
    }

}

#endif

#ifdef ATH_SWRETRY
void _ath_swretry_comp_handle_tim(struct ath_softc *sc ,struct ath_atx_tid *tid, struct ath_node *an)
{
    if (!tid->an->an_tim_set
            && (tid->an->an_flags & ATH_NODE_PWRSAVE)
            && !tid->cleanup_inprogress && !(an->an_flags & ATH_NODE_CLEAN))
    {
#if LMAC_SUPPORT_POWERSAVE_QUEUE
        if (sc->sc_ath_ops.get_pwrsaveq_len(tid->an, 0)==0)
#else
        if (sc->sc_ieee_ops->get_pwrsaveq_len(tid->an->an_node)==0)
#endif

        {
            ATH_NODE_SWRETRY_TXBUF_LOCK(tid->an);
            sc->sc_ieee_ops->set_tim(tid->an->an_node,1);
            tid->an->an_tim_set = AH_TRUE;
            ATH_NODE_SWRETRY_TXBUF_UNLOCK(tid->an);
        }
    }
}

static INLINE void
_ath_swretry_comp_aggr_dec_eligible_frms(struct ath_softc *sc, struct ath_node *an,
                                         struct ath_txq *txq, struct ath_tx_status *ts)
{
    /*
     * Decrement the "swr_num_eligible_frms".
     * This is incremented once per aggregate (in ath_txq_txqaddbuf).
     */
    struct ath_swretry_info *pInfo;
    pInfo = &an->an_swretry_info[txq->axq_qnum];
    ATH_NODE_SWRETRY_TXBUF_LOCK(an);
    pInfo->swr_num_eligible_frms --;
    if (ts->ts_status & HAL_TXERR_XRETRY) {
        DPRINTF(sc, ATH_DEBUG_SWR, "%s: AMPDU XRETRY swr_num_eligible_frms = %d\n",
                                    __func__, pInfo->swr_num_eligible_frms);
        /*
         * Note : If it was completed with XRetry, no packets of the aggregate were sent out.
         * Maybe the STA is asleep ?
         */
        pInfo->swr_state_filtering = AH_TRUE;
    }
    else if (ts->ts_status & HAL_TXERR_FILT) {
        DPRINTF(sc, ATH_DEBUG_SWR, "%s: AMPDU FILT swr_num_eligible_frms = %d\n",
                                    __func__, pInfo->swr_num_eligible_frms);
    }
    ATH_NODE_SWRETRY_TXBUF_UNLOCK(an);
}

#else
#define _ath_swretry_comp_handle_tim(_sc,_tid,_an)
#define _ath_swretry_comp_aggr_dec_eligible_frms(_sc, _an, _txq, _ts)

#endif
#ifndef REMOVE_PKT_LOG
#include "pktlog.h"
extern struct ath_pktlog_funcs *g_pktlog_funcs;


void    _pktlog_update_txcomp(struct ath_softc *sc,struct ath_buf *bf, struct ath_buf *bf_last)
{
    struct log_tx log_data = {0};

    log_data.firstds = bf->bf_desc;
    log_data.bf = bf;
    ath_log_txctl(sc, &log_data, 0);



    if (bf->bf_next == NULL &&
            bf_last->bf_status & ATH_BUFSTATUS_STALE) {
        log_data.firstds = bf_last->bf_desc;
        log_data.bf = bf_last;
        ath_log_txctl(sc, &log_data, 0);
    }


}
#endif

int
ath_tx_complete_aggr_compact(struct ath_softc *sc, struct ath_txq *txq, struct ath_buf *bf,
        ath_bufhead *bf_head, struct ath_tx_status *ts, int txok,ath_bufhead *bf_headfree)
{
    struct ath_node *an = bf->bf_node;
    struct ath_atx_tid *tid = ATH_AN_2_TID(an, bf->bf_tidno);
    struct ath_vap *avp = an->an_avp;
    struct ath_buf *bf_next = NULL;

    u_int16_t seq_st = 0;
    u_int32_t ba[WME_BA_BMP_SIZE >> 5];
    int nbad=0;
    ath_bufhead bf_pending;
    int sendbar = 0;
    u_int32_t currts =0;

#ifndef REMOVE_PKT_LOG
    struct ath_buf *bf_last = bf->bf_lastbf;
#endif

    u_int sw_retry_limit = ATH_MAX_SW_RETRIES;
    TAILQ_INIT(&bf_pending);
    TAILQ_INIT(bf_headfree);

    _IQUE_EXT_GETCURRTS(sc, currts);

    _ath_swretry_comp_aggr_dec_eligible_frms(sc, an, txq, ts);

#if ATH_SUPPORT_IQUE
    if( TID_TO_WME_AC(bf->bf_tidno)>= WME_AC_VI)
         sw_retry_limit = _modify_sw_retry_limit(sc, bf);
#endif



    if( !bf->bf_isaggr){
#ifndef REMOVE_PKT_LOG
        if(!sc->sc_nodebug)
            _pktlog_update_txcomp(sc, bf, bf_last);
#endif

#define RETRY_SUCCESS 0
#define RETRY_FAIL 1

        if(qdf_unlikely(!txok)){
            if(_txcomp_handle_subframe_retry(sc, bf ,tid ,sw_retry_limit,&sendbar, &bf_pending,currts) == RETRY_SUCCESS)
                goto end0;
        } else {
            if (bf->bf_retries || ts->ts_longretry) {
                avp->av_stats.av_tx_retries++;
                if ((bf->bf_retries + ts->ts_longretry) > 1)
                    avp->av_stats.av_tx_mretries++;
            }
        }
        ATH_TXQ_LOCK(txq);
        ath_tx_update_baw(sc, tid, bf->bf_seqno);
        ATH_TXQ_UNLOCK(txq);
        TAILQ_INSERT_TAIL(bf_headfree,bf,bf_list);
    }else{

        OS_MEMZERO(ba, WME_BA_BMP_SIZE >> 3);
        if (qdf_likely(txok)) {
            if (ATH_DS_TX_BA(ts)) {
                /*  extract starting sequence and block-ack bitmap */
                seq_st = ATH_DS_BA_SEQ(ts);
                OS_MEMCPY(ba, ATH_DS_BA_BITMAP(ts), WME_BA_BMP_SIZE >> 3);
            }
        }
        while (bf)
        {
#ifndef REMOVE_PKT_LOG
            if(!sc->sc_nodebug)
                _pktlog_update_txcomp(sc, bf,bf_last);
#endif

            avp->av_stats.av_aggr_pkt_count++;

            bf_next = bf->bf_next;
            if (qdf_unlikely(!txok) || !(ATH_BA_ISSET(ba, ATH_BA_INDEX(seq_st, bf->bf_seqno)))){
                nbad++;
                __11nstats(sc, txaggr_compretries);
                if(_txcomp_handle_subframe_retry(sc, bf ,tid ,sw_retry_limit,&sendbar, &bf_pending,currts) == RETRY_SUCCESS){
                    bf = bf_next;
                    continue;
                }
            } else {
                if (bf->bf_retries || ts->ts_longretry) {
                    avp->av_stats.av_tx_retries++;
                    if (bf->bf_retries > 1)
                        avp->av_stats.av_tx_mretries++;                        
                }
            }
            /* subframe succes */
            ATH_TXQ_LOCK(txq);
            ath_tx_update_baw(sc, tid, bf->bf_seqno);
            ATH_TXQ_UNLOCK(txq);
            TAILQ_INSERT_TAIL(bf_headfree,bf,bf_list);
            bf = bf_next;
        }
    }
end0 :   
    if (sendbar && !ath_vap_pause_in_progress(sc)) {
        ath_bar_tx(sc, an, tid);
    } else if (ath_vap_pause_in_progress(sc)) {
        printk("%s:txq pause is in progress\n",__func__);
    }

    /*
     *  node is already gone. no more assocication
     *  with the node. the node might have been freed
     *  any  node acces can result in panic.note tid
     *  is part of the node.
     */
    /* Node clean check over here for panic in legacy complete_aggr_rifs is itself bug as we already
       have done update_baw whcih should have crashed before coming over here
       So I have removed the node clean check in this new compact function
       and there is no proper hanlding of bf_pending also in the legacy complete_aggr_rifs
       In the new desigs  Node cannot be freed unless all the buffers are freed in ath_edma_free_complete_buf
       which called at the end of edma_tasklet_compact, Hence the woraround of defercomplete_bf also has been removed.
       */

    /*if (an->an_flags & ATH_NODE_CLEAN) goto out0;*/

    /*
     * prepend un-acked frames to the beginning of the pending frame queue
     */
    if (!TAILQ_EMPTY(&bf_pending)) {
        if ( an->an_flags & ATH_NODE_CLEAN || tid->cleanup_inprogress ) {
           ath_node_leftovercleanup(sc,txq, &bf_pending,bf_headfree,tid);
           goto out0;
        }
        ATH_TXQ_LOCK(txq);
#if ATH_SUPPORT_VOWEXT
        _txcomp_vow_handle_sorting(sc,tid, &bf_pending);
#else
        TAILQ_INSERTQ_HEAD(&tid->buf_q, &bf_pending, bf_list);
#endif

        ath_tx_queue_tid(txq, tid);
        ATH_TXQ_UNLOCK(txq);

        _ath_swretry_comp_handle_tim(sc,tid,an);
    }

out0:
    return nbad;

}

static INLINE int
ath_frame_type(wbuf_t wbuf)
{
    struct ieee80211_frame *wh;
    int type;

    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;

    return type;
}

static INLINE int
ath_frame_subtype(wbuf_t wbuf)
{
    struct ieee80211_frame *wh;
    int type;

    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    type = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

    return type;
}

static INLINE void
_ath_tx_free_buf(struct ath_softc *sc, struct ath_buf *bf,struct ath_node *an, struct ath_atx_tid *tid, struct ath_txq *txq)
{
        wbuf_unmap_sg(sc->sc_osdev, bf->bf_mpdu,
        OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));

        if(an !=NULL){
#ifdef ATH_SUPPORT_QUICK_KICKOUT
            if (bf->bf_isxretried) {
                an->an_consecutive_xretries++ ;
                sc->sc_ieee_ops->tx_node_kick_event((ieee80211_node_t)((an)->an_node),&an->an_consecutive_xretries,bf->bf_mpdu);
            }
            else
                an->an_consecutive_xretries=0;
#endif

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
            if (bf->bf_isxretried)
                ath_ald_update_excretry_stats(tid->ac);
#endif

            if (bf->bf_isbar && tid->bar_paused) {
                tid->bar_paused--;
                ATH_TX_RESUME_TID(sc, tid);

            }

            if (ath_frame_type(bf->bf_mpdu) == IEEE80211_FC0_TYPE_MGT || ath_frame_type(bf->bf_mpdu) == IEEE80211_FC0_TYPE_CTL || ( (ath_frame_type(bf->bf_mpdu) == IEEE80211_FC0_TYPE_DATA) && (ath_frame_subtype(bf->bf_mpdu) == IEEE80211_FC0_SUBTYPE_NODATA || ath_frame_subtype(bf->bf_mpdu) == IEEE80211_FC0_SUBTYPE_QOS_NULL))) {
                sc->sc_ieee_ops->tx_mgmt_complete((ieee80211_node_t)((an)->an_node), bf->bf_mpdu, bf->bf_isxretried);
            } else {
                sc->sc_ieee_ops->tx_complete_compact((ieee80211_node_t)((an)->an_node), bf->bf_mpdu);
            }
            sc->sc_ieee_ops->tx_free_node( (ieee80211_node_t)an->an_node,bf->bf_isxretried);
        }
#if ATH_TX_COMPACT
        if (bf->bf_isdata  && sc->sc_pn_checknupdate_war)
            sc->sc_ieee_ops->check_and_update_pn(bf->bf_mpdu);
#endif

        wbuf_complete_any(bf->bf_mpdu);
        bf->bf_mpdu =NULL;


        ATH_TXBUF_LOCK(sc);
#if ATH_TX_BUF_FLOW_CNTL
        txq =  &sc->sc_txq[bf->bf_qnum];
        txq->axq_num_buf_used --;
#endif
        sc->sc_txbuf_free ++;
#if QCA_AIRTIME_FAIRNESS
        if (bf->bf_atf_accounting == 1) {
            if (an) {
                ATH_NODE_ATF_TOKEN_LOCK(an);
                if (sc->sc_ieee_ops->atf_subgroup_free_buf) {
                        sc->sc_ieee_ops->atf_subgroup_free_buf(an->an_node,
                                     bf->bf_atf_accounting_size, bf->bf_atf_sg);
                }
                ATH_NODE_ATF_TOKEN_UNLOCK(an);
            } else {
                qdf_print("%s: buffer marked for atf accounting has no an reference", __func__);
            }
        }
        bf->bf_atf_accounting = 0;
#endif
        TAILQ_INSERT_TAIL(&sc->sc_txbuf, bf, bf_list);
        ATH_TXBUF_UNLOCK(sc);
}

static void _tx_update_ieeestats(struct ath_softc *sc, struct ath_buf *bf, int txok)
{
    ieee80211_tx_status_t tx_status;
    tx_status.flags =0;
    if(!txok)
        tx_status.flags |= ATH_TX_ERROR;
    tx_status.retries = bf->bf_retries;
    tx_status.rateKbps = ath_ratecode_to_ratekbps(sc, bf->bf_tx_ratecode);
    sc->sc_ieee_ops->tx_update_stats(sc->sc_ieee, bf->bf_mpdu, &tx_status);
}

#define NODEMIN_KICK_OUTLIMIT  10
void  ath_edma_free_complete_buf( struct ath_softc *sc , struct ath_txq *txq, ath_bufhead * bf_headfree,int txok )
{
    struct ath_buf *bf;
    struct ath_node *an ;
    struct ath_atx_tid *tid;
    bf = TAILQ_FIRST(bf_headfree);
    if(bf == NULL)
        return;
    an = bf->bf_node;
    tid = ATH_AN_2_TID(an, bf->bf_tidno);

    ATH_TXQ_LOCK(txq);
    if (tid->cleanup_inprogress) {
        /* check to see if we're done with cleaning the h/w queue */

        if (tid->baw_head == tid->baw_tail) {
            tid->addba_exchangecomplete = 0;
            tid->addba_exchangeattempts = 0;
            tid->addba_exchangestatuscode = IEEE80211_STATUS_UNSPECIFIED;
            tid->cleanup_inprogress = AH_FALSE;

            ATH_TXQ_UNLOCK(txq);
            /* send buffered frames as singles */
            ATH_TX_RESUME_TID(sc, tid);
        } else {
            ATH_TXQ_UNLOCK(txq);
        }
    }else
        ATH_TXQ_UNLOCK(txq);

    while(bf != NULL){
        TAILQ_REMOVE(bf_headfree, bf, bf_list);
        if(unlikely(!sc->sc_nodebug))
            _tx_update_ieeestats(sc, bf, txok);
        _ath_tx_free_buf(sc, bf, an, tid, txq);
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
		if(txok)
		{
			ath_ald_update_pktcnt(tid->ac);
		}
#endif
        bf = TAILQ_FIRST(bf_headfree);
    }

}

/* For Compact path this function is called from exception path like ath_txqaddbuf failed
 * or detach  Non AMPDU case  */

#ifdef ATH_SUPPORT_TxBF
void
ath_tx_complete_buf(struct ath_softc *sc, struct ath_buf *bf, ath_bufhead *bf_q, int txok, u_int8_t txbf_status, u_int32_t tstamp)
#else
void
ath_tx_complete_buf(struct ath_softc *sc, struct ath_buf *bf, ath_bufhead *bf_q, int txok)
#endif
{
    if(bf == NULL)
        return;

    if(!txok) {
        __11nstats(sc, txunaggr_compretries);
    }

    if(sc->sc_enhanceddmasupport){

        struct ath_txq *txq      = &sc->sc_txq[bf->bf_qnum];
        struct ath_node *an      =  bf->bf_node;
        struct ath_atx_tid *tid  = ATH_AN_2_TID(an, bf->bf_tidno);

        if (bf->bf_state.bfs_ispaprd) {
            ath_tx_paprd_complete(sc, bf, bf_q);
            return ;
        }

        _ath_tx_free_buf(sc,bf,an,tid,txq);
    } else {
#if ATH_SUPPORT_TxBF
        _ath_tx_complete_buf(sc, bf, bf_q, txok, txbf_status, tstamp);
#else
        _ath_tx_complete_buf(sc, bf, bf_q, txok);
#endif
    }
}

void
ath_tx_mark_aggr_rifs_done(struct ath_softc *sc, struct ath_txq *txq, struct ath_buf *bf,
                ath_bufhead *bf_head, struct ath_tx_status *ts, int txok)
{
    if(sc->sc_enhanceddmasupport){
        ath_bufhead  bf_headfree;
        TAILQ_INIT(&bf_headfree);

        ath_tx_complete_aggr_compact(sc, txq, bf, bf_head, ts, txok ,&bf_headfree);
        ath_edma_free_complete_buf(sc,txq,&bf_headfree, txok);
    }else
        ath_tx_complete_aggr_rifs(sc, txq, bf, bf_head, ts ,txok);
}
#endif // ATH_TX_COMPACT
#endif // UMAC_SUPPORT_APONLY

#if ATH_DEBUG
OS_EXPORT_SYMBOL(dprintf);
#endif  /* ATH_DEBUG */

OS_EXPORT_SYMBOL(ath_tx_pause_tid);
OS_EXPORT_SYMBOL(ath_tx_resume_tid);
OS_EXPORT_SYMBOL(ath_tx_send_normal);
OS_EXPORT_SYMBOL(ath_tx_send_ampdu);
OS_EXPORT_SYMBOL(ath_txq_schedule);
OS_EXPORT_SYMBOL(ath_tx_num_badfrms);
OS_EXPORT_SYMBOL(ath_tx_complete_aggr_rifs);


OS_EXPORT_SYMBOL(_ath_tx_complete_buf);

#if ATH_TX_COMPACT
OS_EXPORT_SYMBOL( ath_tx_complete_aggr_compact);
OS_EXPORT_SYMBOL(ath_tx_complete_buf);
OS_EXPORT_SYMBOL( ath_edma_free_complete_buf);
#endif

OS_EXPORT_SYMBOL(ath_tx_update_stats);
OS_EXPORT_SYMBOL(ath_txq_depth);
OS_EXPORT_SYMBOL(ath_buf_set_rate);
OS_EXPORT_SYMBOL(ath_txto_tasklet);
OS_EXPORT_SYMBOL(ath_tx_get_rts_retrylimit);
OS_EXPORT_SYMBOL(ath_tx_update_minimal_stats);

OS_EXPORT_SYMBOL(ath_tx_uapsd_complete);
OS_EXPORT_SYMBOL(ath_tx_queue_uapsd);

OS_EXPORT_SYMBOL(ath_pwrsave_set_state);
#if ATH_SUPPORT_FLOWMAC_MODULE
//ATH_SUPPORT_VOWEXT
OS_EXPORT_SYMBOL(ath_netif_stop_queue);
#endif
OS_EXPORT_SYMBOL(ath_edmaAllocRxbufsForFreeList);
OS_EXPORT_SYMBOL(ath_rx_edma_intr);
OS_EXPORT_SYMBOL(ath_hw_hang_check);
OS_EXPORT_SYMBOL(ath_beacon_tasklet);
OS_EXPORT_SYMBOL(ath_bstuck_tasklet);
OS_EXPORT_SYMBOL(ath_beacon_config);
OS_EXPORT_SYMBOL(ath_bmiss_tasklet);

OS_EXPORT_SYMBOL(ath_allocRxbufsForFreeList);
#if ATH_SUPPORT_DESCFAST
OS_EXPORT_SYMBOL(ath_rx_proc_descfast);
#endif

#ifdef ATH_GEN_TIMER
OS_EXPORT_SYMBOL(ath_gen_timer_isr);
OS_EXPORT_SYMBOL(ath_gen_timer_tsfoor_isr);
#if ATH_RX_LOOPLIMIT_TIMER
OS_EXPORT_SYMBOL(ath_gen_timer_start);
OS_EXPORT_SYMBOL(ath_gen_timer_stop);
OS_EXPORT_SYMBOL(ath_gen_timer_gettsf32);

extern int ath_rx_looplimit_handler(void *arg);
OS_EXPORT_SYMBOL(ath_rx_looplimit_handler);
#endif
#endif

OS_EXPORT_SYMBOL(ath_handle_rx_intr);
#if ATH_SUPPORT_PAPRD
OS_EXPORT_SYMBOL(ath_tx_paprd_complete);
#endif
OS_EXPORT_SYMBOL(ath_tx_tasklet);

OS_EXPORT_SYMBOL(ath_rx_process_uapsd);
#ifdef ATH_SUPPORT_TxBF
OS_EXPORT_SYMBOL(ath_rx_bf_handler);
OS_EXPORT_SYMBOL(ath_rx_bf_process);
OS_EXPORT_SYMBOL(ath_txbf_chk_rpt_frm);
#endif
#if ATH_SUPPORT_LED
OS_EXPORT_SYMBOL(ath_led_report_data_flow);
#endif
OS_EXPORT_SYMBOL(ath_rx_requeue);
OS_EXPORT_SYMBOL(ath_bar_rx);
OS_EXPORT_SYMBOL(ath_ampdu_input);
OS_EXPORT_SYMBOL(ath_setdefantenna);
#ifdef ATH_SWRETRY
OS_EXPORT_SYMBOL(ath_form_swretry_frm);
OS_EXPORT_SYMBOL(ath_check_swretry_req);
#if ATH_SWRETRY_MODIFY_DSTMASK
OS_EXPORT_SYMBOL(ath_tx_modify_cleardestmask);
#endif
OS_EXPORT_SYMBOL(ath_tx_mpdu_requeue);
OS_EXPORT_SYMBOL(ath_tx_mpdu_resend);
OS_EXPORT_SYMBOL(ath_tx_dec_eligible_frms);
#endif
#if LMAC_SUPPORT_POWERSAVE_QUEUE
OS_EXPORT_SYMBOL(ath_node_pwrsaveq_queue);
#endif //LMAC_SUPPORT_POWERSAVE_QUEUE

#if ATH_SUPPORT_VOW_DCS
OS_EXPORT_SYMBOL(ath_rx_duration);
#endif


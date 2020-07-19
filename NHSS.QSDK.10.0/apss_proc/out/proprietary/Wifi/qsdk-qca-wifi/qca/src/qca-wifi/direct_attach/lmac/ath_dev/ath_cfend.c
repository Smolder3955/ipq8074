/*
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

/*! \file ath_cfend.c
**  \brief ATH CF-END Processing
*/

#include "ath_internal.h"
#ifndef REMOVE_PKT_LOG
#include "pktlog.h"
extern struct ath_pktlog_funcs *g_pktlog_funcs;
#endif

#if ATH_SUPPORT_CFEND
 /*
  * CF-END initialization.
  */
int
ath_tx_cfend_init(struct ath_softc *sc)
{
    int error = 0;
    struct ath_buf *bf;
    wbuf_t wbuf;
    struct ath_desc *ds;
    u_int32_t result = 0;

    ath_hal_getcapability(sc->sc_ah, HAL_CAP_CFENDFIX, 0, &result);

    /* initialize only ar5416 chips */
    if (result == AH_FALSE) {
        DPRINTF(sc, ATH_DEBUG_XMIT, "%s No support for CFEND\n",
                __func__);
        return 0;
    }

    /* we should check if cfend flag installed here, but by default, this is
     * disable at init time, so not doing init would break the dynamic
     * enabling process, so do not check any enabled flag to do
     * initialization
     */


    error = ath_descdma_setup(sc, &sc->sc_cfenddma, &sc->sc_cfendbuf,
            "cfend", 1, 1, 1, 1);

    if (error != 0) {
        DPRINTF(sc, ATH_DEBUG_XMIT,
                "%s failed to allocate CF-END tx descriptors: %d\n",
                __func__, error);
        goto bad;
    }

    /* initialize the spinlock */
    ATH_CFEND_LOCK_INIT(sc);

    /* Format frame */
    bf = TAILQ_FIRST(&sc->sc_cfendbuf);
	/* Modify for static analysis, prevent bf is NULL */
	if (bf == NULL) {
        return -1;
    }
    ATH_TXBUF_RESET(bf, sc->sc_num_txmaps);

    wbuf = sc->sc_ieee_ops->cfend_alloc(sc->sc_ieee);
    if (wbuf == NULL) {
        error = -ENOMEM;
        DPRINTF(sc, ATH_DEBUG_XMIT,
                "%s failed to allocate CF END wbuf\n",
                __func__);
        ath_tx_cfend_cleanup(sc);
        goto free_desc;
    } else {
        /* on windows platform, it seems like MPDUs for management frame
         * come out of different pool, and wb_type is set to WBUF_TX_MGMT
         * which would cause different wb_action table to use. For cfend we
         * send only control frame, it would be too much to rewrite whole
         * lot of code for one single frame, so get a buffer from management
         * pool and change the wb_type to WBUF_TX_CTL. When actually freeing
         * make sure that you reset it back.
         */
        wbuf_set_type(wbuf, WBUF_TX_CTL);

    }


    if (bf) {
        DPRINTF(sc, ATH_DEBUG_XMIT,
                "%s * allocated cfend pointer at %pK \n", __func__, bf);
    }

    bf->bf_frmlen = sizeof(struct ieee80211_ctlframe_addr2);
    bf->bf_mpdu = wbuf;
    bf->bf_isdata = 0;
    bf->bf_lastfrm = bf;
    bf->bf_lastbf = bf->bf_lastfrm; /* one single frame */
    bf->bf_isaggr  = 0;
    bf->bf_isbar  = bf->bf_isampdu = 0;
    bf->bf_next = NULL;
    bf->bf_state.bfs_iscfend = 0;

    bf->bf_buf_addr[0] = wbuf_map_single(
                        sc->sc_osdev,
                        wbuf,
                        BUS_DMA_TODEVICE,
                        OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));

    /* setup descriptor */
    ds = bf->bf_desc;
    ds->ds_link = 0;
    ds->ds_data = bf->bf_buf_addr[0];
#ifndef REMOVE_PKT_LOG
    ds->ds_vdata = wbuf_header(wbuf);
#endif

    bf->bf_flags = (HAL_TXDESC_NOACK
                   | HAL_TXDESC_INTREQ
                   | HAL_TXDESC_EXT_AND_CTL
                   | HAL_TXDESC_CLRDMASK);

    return 0;

    /* free all the resources and then clear the enabled flag, so that no
     * one uses
     */
free_desc:
    if (sc->sc_cfenddma.dd_desc_len != 0)
        ath_descdma_cleanup(sc, &sc->sc_cfenddma, &sc->sc_cfendbuf);
    ATH_CFEND_LOCK_DESTROY(sc);
bad:
    /* make sure that feature itself disbaled by default when not able to
     * initialize
     */
    return error;
}
/*
 * Avoid taking any locks while in interrupt context. 
 * sc_cfendbuf contains one single buffer that, should 
 * provide locking
 */
int
ath_tx_txqaddbuf_nolock(struct ath_softc *sc, struct ath_txq *txq,
        ath_bufhead *head)
{
#define DESC2PA(_sc, _va) \
    ((caddr_t)(_va) - (caddr_t)((_sc)->sc_cfenddma.dd_desc) + \
     (_sc)->sc_cfenddma.dd_desc_paddr)

    struct ath_hal *ah = sc->sc_ah;
    struct ath_buf *bf;

    /*
     * Insert the frame on the outbound list and
     * pass it on to the hardware.
     */

    bf = TAILQ_FIRST(head);
    if (bf == NULL) {
        return 0;
    }

    if (atomic_read(&sc->sc_in_reset)) {
        return -1;
    }

    ATH_TXQ_CONCAT(txq, head);

    DPRINTF(sc, ATH_DEBUG_TX_PROC,
            "%s: txq depth = %d\n", __func__, txq->axq_depth);

    if (txq->axq_link == NULL) {
        ath_hal_puttxbuf(ah, txq->axq_qnum, bf->bf_daddr);
        DPRINTF(sc, ATH_DEBUG_XMIT,
                "%s: TXDP[%u] = %llx (%pK)\n", __func__,
                txq->axq_qnum, ito64(bf->bf_daddr), bf->bf_desc);
    } else {
#ifdef AH_NEED_DESC_SWAP
        *txq->axq_link = cpu_to_le32(bf->bf_daddr);
#else
        *txq->axq_link = bf->bf_daddr;
#endif
        OS_SYNC_SINGLE(sc->sc_osdev, (dma_addr_t)(DESC2PA(sc, txq->axq_link)), 
                       sizeof(*sc->sc_rxlink), BUS_DMA_TODEVICE, NULL);

        DPRINTF(sc, ATH_DEBUG_XMIT, "%s: link[%u] (%pK)=%llx (%pK)\n",
            __func__, txq->axq_qnum, txq->axq_link,
            ito64(bf->bf_daddr), bf->bf_desc);
    }
    ath_hal_getdesclinkptr(ah, bf->bf_lastbf->bf_desc, &(txq->axq_link));
    ath_hal_txstart(ah, txq->axq_qnum);
    return 0;
#undef DESC2PA
}
/*
 * Send CF-End Frame.
 * Context: Interrupt
 */

void
ath_sendcfend(struct ath_softc *sc, u_int8_t *bssid)
{
    struct ath_buf *bf;

    u_int8_t rix, rate;
    const HAL_RATE_TABLE *rt;
    struct ath_desc *ds;
    HAL_11N_RATE_SERIES  series[4];
    ath_bufhead bf_head;
    struct ieee80211_frame *hdr=NULL;
    u_int32_t smartAntenna = 0;
#if UNIFIED_SMARTANTENNA
    u_int32_t antenna_array[4]= {0,0,0,0}; /* initilize to zero */
    u_int8_t i = 0;
#endif   

    ATH_CFEND_LOCK(sc);
    bf = TAILQ_FIRST(&sc->sc_cfendbuf);
    if(bf) {
        TAILQ_REMOVE(&sc->sc_cfendbuf, bf, bf_list);
    }
    ATH_CFEND_UNLOCK(sc);

    if (NULL == bf) {
        DPRINTF(sc, ATH_DEBUG_XMIT,
                "%s CFEND buffer seems NULL, last free has some bug \n",
                __func__);
        return ;
    }

    if(bf->bf_state.bfs_iscfend ){
        DPRINTF(sc, ATH_DEBUG_XMIT,
                "%s CF-End packet is in CFEND queue", __func__);
        return;
    } 
     
    

    TAILQ_INIT(&bf_head);
    TAILQ_INSERT_TAIL(&bf_head, bf, bf_list);
    bf->bf_node = NULL;

    ds = bf->bf_desc;
    rix = sc->sc_minrateix;
    rt = sc->sc_currates;
    rate = rt->info[rix].rate_code;

#ifdef ATH_CFEND_DEBUG
    DPRINTF(sc,
            ATH_DEBUG_XMIT,
            "%s bf->bf_frmlen %d wbuf_get_len(%d), bf->flags %x "
            "que num %d  bf->bf_buf_len %d\n",
           __func__, bf->bf_frmlen,
           wbuf_get_pktlen(bf->bf_mpdu),
           bf->bf_flags, sc->sc_cfendq->axq_qnum, bf->bf_buf_len[0]);
#endif
    /* fill the bssid */
    hdr = wbuf_raw_data(bf->bf_mpdu);
    if (hdr) {
        ATH_ADDR_COPY(hdr->i_addr2, bssid);
    }
    bf->bf_buf_len[0] = roundup(wbuf_get_pktlen(bf->bf_mpdu), 4);


    ath_hal_set11n_txdesc(sc->sc_ah
            , ds
            , wbuf_get_pktlen(bf->bf_mpdu) + IEEE80211_CRC_LEN
            , HAL_PKT_TYPE_NORMAL
            , MIN(sc->sc_curtxpow, 60)
            , HAL_TXKEYIX_INVALID
            , HAL_KEY_TYPE_CLEAR
            , bf->bf_flags);


    ath_hal_filltxdesc(sc->sc_ah
            , ds
            , bf->bf_buf_addr
            , bf->bf_buf_len
            , 0
            , sc->sc_cfendq->axq_qnum
            , HAL_KEY_TYPE_CLEAR
            , AH_TRUE
            , AH_TRUE
            , ds);

    OS_MEMZERO(series, sizeof(HAL_11N_RATE_SERIES) * 4);

    series[0].Tries = 1;
    series[0].Rate = rate;
    series[0].ch_sel = ath_txchainmask_reduction(sc,
                        sc->sc_tx_chainmask, rate);
    series[0].RateFlags =  0;

    smartAntenna = SMARTANT_INVALID;

#if UNIFIED_SMARTANTENNA
    smartAntenna = 0;
    for (i =0 ;i <= sc->max_fallback_rates; i++) {
        antenna_array[i] = sc->smart_ant_tx_default;
    }   
    ath_hal_set11n_ratescenario(sc->sc_ah, ds, ds, 0, 0, 0 ,
            series, 4, 0, smartAntenna, antenna_array);
#else
    ath_hal_set11n_ratescenario(sc->sc_ah, ds, ds, 0, 0, 0 ,
            series, 4, 0, smartAntenna);
#endif    
    ath_desc_swap(ds);

    bf->bf_state.bfs_iscfend = 1;
#ifdef QCA_SUPPORT_CP_STATS
    pdev_lmac_cp_stats_ast_cfend_sent_inc(sc->sc_pdev, 1);
#else
    sc->sc_stats.ast_cfend_sent++;
#endif
    /* frame is sent in interrupt context, so do not try to take
     * any interrupts. Having single buffer for CFEND provides
     * implicit protection. Either frame is there in hardware
     * queue or in s/w queue. So no corruption issue shows up.
     */
    if (ath_tx_txqaddbuf_nolock(sc, sc->sc_cfendq, &bf_head)) {
        ath_tx_cfend_complete(sc, bf, &bf_head);
    }
} /* end ath_sendcfend */

void
ath_tx_cfend_complete(struct ath_softc *sc, struct ath_buf *bf,
        ath_bufhead *bf_q)
{
    ath_bufhead bf_head;
    if (!bf) {
        DPRINTF(sc, ATH_DEBUG_XMIT,
                "%s CFEND buffer is NULL \n", __func__);
        return;
    }else{
        DPRINTF(sc, ATH_DEBUG_XMIT,
                "%s freeing cfend buffer bf %pK, wbuf %pK n",
                __func__, bf, bf->bf_mpdu);
    }

    /* clear the flag for re-use */
    bf->bf_state.bfs_iscfend = 0;
#ifndef REMOVE_PKT_LOG
    {
        struct log_tx log_data;
        log_data.firstds = bf->bf_desc;
        log_data.bf = bf;
        ath_log_txctl(sc, &log_data, 0);
        log_data.lastds = bf->bf_desc;
        ath_log_txstatus(sc, &log_data, 0);
    }
#endif

    TAILQ_INIT(&bf_head);
    TAILQ_REMOVE_HEAD_UNTIL(bf_q, &bf_head, bf, bf_list);
    TAILQ_CONCAT(&sc->sc_cfendbuf, &bf_head, bf_list);
}
/*
 * Reclaim CF-END resources.
 * Context: Tasklet
 *    */
void
ath_tx_cfend_cleanup(struct ath_softc *sc)
{
    ieee80211_tx_status_t tx_status;
    struct ath_buf *bf;
    wbuf_t wbuf;
    uint32_t result=0;
    tx_status.flags = 0;

    ath_hal_getcapability(sc->sc_ah, HAL_CAP_CFENDFIX, 0, &result);
    if (result == AH_FALSE) return;

    ATH_CFEND_LOCK(sc);

    TAILQ_FOREACH(bf, &sc->sc_cfendbuf, bf_list) {
        /* Unmap this frame */
#if 0
        wbuf_unmap_sg(sc->sc_osdev, wbuf,
                OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
#endif
        TAILQ_REMOVE(&sc->sc_cfendbuf, bf, bf_list);
        if (bf) {
            if (bf->bf_mpdu != NULL) {
                wbuf = bf->bf_mpdu;
                wbuf_set_type(wbuf, WBUF_TX_MGMT);
                wbuf_free(wbuf);
                bf->bf_mpdu=NULL;
            }
        }
    }

    if (sc->sc_cfenddma.dd_desc_len != 0) {
        ath_descdma_cleanup(sc, &sc->sc_cfenddma, &sc->sc_cfendbuf);
    }
    ATH_CFEND_UNLOCK(sc);

    ATH_CFEND_LOCK_DESTROY(sc);
}

void
ath_tx_cfend_draintxq(struct ath_softc *sc)
{
    struct ath_buf *bf;
    ath_bufhead bf_head;
    struct ath_txq *txq = sc->sc_cfendq;

    TAILQ_INIT(&bf_head);
    /*
     * NB: this assumes output has been stopped and
     *     we do not need to block ath_tx_tasklet
     */

    ATH_CFEND_LOCK(sc);

    for (;;) {
        bf = TAILQ_FIRST(&txq->axq_q);
        if (bf == NULL) {
            txq->axq_link = NULL;
            txq->axq_linkbuf = NULL;
            break;
        }
        /* process CF End packet */
        if (bf && bf->bf_state.bfs_iscfend) {

            ((struct ath_desc*)(bf->bf_desc))->ds_txstat.ts_flags =
                                                HAL_TX_SW_ABORTED;

            ATH_TXQ_MOVE_HEAD_UNTIL(txq, &bf_head, bf, bf_list);
            ath_tx_cfend_complete (sc, bf, &bf_head);
            TAILQ_INIT(&bf_head);
            continue;
        }
    }
    ATH_CFEND_UNLOCK(sc);
}

int
ath_cfendq_update(struct ath_softc *sc)
{
    HAL_TXQ_INFO qi;
    int qnum = sc->sc_cfendq->axq_qnum;

    ath_hal_gettxqueueprops(sc->sc_ah, qnum, &qi);
    ath_txq_update(sc, qnum, &qi);
    return 0;
}


/*
 * @brief, check whether cfend support needed, if so configure it
*/
int
ath_cfendq_config(struct ath_softc *sc)
{
   u_int32_t result=0;

    ath_hal_getcapability(sc->sc_ah, HAL_CAP_CFENDFIX,0, &result);

    if (result == AH_TRUE) {
        sc->sc_cfendq = ath_txq_setup(sc, HAL_TX_QUEUE_CFEND, 0);
        if (sc->sc_cfendq == NULL) {
            DPRINTF(sc, ATH_DEBUG_XMIT,
                    "%s unable to setup CFEND queue !\n", __func__);
            return -EIO;
        }
        ath_cfendq_update(sc);
    }
    return 0;
}

/*
 * @brief - check the current aggregate status, and clear channel busy
 * condition
*/

void
ath_check_n_send_cfend(struct ath_softc *sc, struct ath_rx_status *rxs,
        int proc_desc_status, struct ath_buf *bf, u_int8_t *bssid)
{
    u_int32_t result = 0;
    u_int16_t framedur = 0;
    u_int8_t  aggr;
    struct ieee80211_frame *wh=NULL;
    wbuf_t wbuf;

#define IS_AGGR_LAST_SUBFRAME(_rxs) (((_rxs)->rs_isaggr) &&\
                               !((_rxs)->rs_moreaggr))
#define CFEND_OFFSET 500

    ath_hal_getcapability(sc->sc_ah, HAL_CAP_CFENDFIX, 0, &result);

    if (result == AH_FALSE || !bf ) {
        return;
    }
    /* if aggregate has good frames and last subframe has crc error
     * check to  see, if cfend need to be sent
     */
    aggr = ath_hal_get_rx_cur_aggr_n_reset(sc->sc_ah);
    if ((HAL_OK == proc_desc_status)
            && (aggr)
            && IS_AGGR_LAST_SUBFRAME(rxs)) {

        if (rxs->rs_status  & HAL_RXERR_CRC){

            wbuf = bf->bf_mpdu;
            /* Sync before using rx buffer */
            if (!(bf->bf_status & ATH_BUFSTATUS_SYNCED)) {
                OS_SYNC_SINGLE(sc->sc_osdev,
                    bf->bf_buf_addr[0],
                    wbuf_get_len(wbuf),
                    BUS_DMA_FROMDEVICE,
                    OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
                bf->bf_status |= ATH_BUFSTATUS_SYNCED;
            }

            wh = (struct ieee80211_frame *) (wbuf_raw_data(wbuf));
            if (wh == NULL) {
                return;
            }

            framedur = (wh->i_dur[1] << 8) | (wh->i_dur[0]);
            if ( (wh->i_dur[1] !=0 )) {
                if ((framedur + CFEND_OFFSET) >
                        ((ath_hal_gettsf32(sc->sc_ah)) -
                                rxs->rs_tstamp)) {
#ifdef QCA_SUPPORT_CP_STATS
                    pdev_lmac_cp_stats_ast_cfend_sched_inc(sc->sc_pdev, 1);
#else
                    sc->sc_stats.ast_cfend_sched++;
#endif
                    ath_sendcfend(sc, bssid);
                }
            }
        }
    }
}
#endif /* ATH_SUPPORT_CFEND */

/*
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

/*
 * Recv Aggregation code.
 */

#include "ath_internal.h"


static void ath_rx_flush_tid(struct ath_softc *sc, struct ath_arx_tid *rxtid, int drop);

/*
 * Set a user defined ADDBA response status code.
 */
void
ath_set_addbaresponse(ath_dev_t dev, ath_node_t node,
                      u_int8_t tidno, u_int16_t statuscode)
{
    struct ath_node *an = ATH_NODE(node);
    struct ath_arx_tid *rxtid = &an->an_aggr.rx.tid[tidno];

    /*
     * Set the user defined ADDBA response for this TID
     */
    rxtid->userstatuscode = statuscode;
}

/*
 * Clear the user defined ADDBA response status code.
 */
void
ath_clear_addbaresponsestatus(ath_dev_t dev, ath_node_t node)
{
#define    N(a)    (sizeof(a)/sizeof(a[0]))
    struct ath_node *an = ATH_NODE(node);
    struct ath_arx_tid *rxtid;
    int i;

    for (i = 0; i < N(an->an_rx_tid); i++) {
        rxtid = &an->an_rx_tid[i];
        rxtid->userstatuscode = IEEE80211_STATUS_SUCCESS;
    }
#undef N
}

/*
 * Process ADDBA request and save response information in per-TID data structure
 */
int
ath_addba_requestprocess(
    ath_dev_t dev, ath_node_t node,
    u_int8_t dialogtoken,
    struct ieee80211_ba_parameterset *baparamset,
    u_int16_t batimeout,
    struct ieee80211_ba_seqctrl basequencectrl
    )
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_node *an = ATH_NODE(node);
    u_int16_t tidno = baparamset->tid;
    struct ath_arx_tid *rxtid  = &an->an_aggr.rx.tid[tidno];

    if (!sc->sc_rxaggr) /* drop addba request */
        return -EINVAL;

    ATH_RXTID_LOCK(rxtid);

    if (rxtid->userstatuscode != IEEE80211_STATUS_SUCCESS) {
        /*
         * XXX: we imply it's manual ADDBA mode if userstatuscode
         * is set to non-success code
         */
        rxtid->statuscode = rxtid->userstatuscode;
    } else {                    /* Allow aggregation reception */
        if (rxtid->addba_exchangecomplete && rxtid->rxbuf != NULL) {
            ATH_CANCEL_RXTIMER(&rxtid->timer, CANCEL_NO_SLEEP);
            ath_rx_flush_tid(sc, rxtid, 0);
            rxtid->addba_exchangecomplete = 0;
        }

        /*
         * Adjust rx BA window size. Peer might indicate a zero buffer size for
         * a _dont_care_ condition.
         */
        if (baparamset->buffersize)
            rxtid->baw_size = MIN(baparamset->buffersize, rxtid->baw_size);
        if (sc->sc_config.ampdu_rx_bsize) {
            rxtid->baw_size = MIN(rxtid->baw_size, sc->sc_config.ampdu_rx_bsize);
            DPRINTF(sc, ATH_DEBUG_AGGR_MEM, "%s: req: %d user: %d -> baw: %d\n",
                __func__, baparamset->buffersize, sc->sc_config.ampdu_rx_bsize, rxtid->baw_size);
        }

        /* set rx sequence number */
        rxtid->seq_next = basequencectrl.startseqnum;

        /* save ADDBA response parameters in rx TID */
        rxtid->statuscode               = IEEE80211_STATUS_SUCCESS;
        rxtid->baparamset.bapolicy      = IEEE80211_BA_POLICY_IMMEDIATE;
        rxtid->baparamset.buffersize    = rxtid->baw_size;
        rxtid->batimeout                = 0;
        /*
        ** Allocate the receive buffers for this TID
        */

        DPRINTF(sc,ATH_DEBUG_AGGR_MEM,"%s: Allcating rxbuffer for TID %d\n",__func__,tidno);

        if (rxtid->rxbuf == NULL) {
            /*
            ** If the rxbuff is not NULL at this point, we *probably* already allocated the
            ** buffer on a previous ADDBA, and this is a subsequent ADDBA that got through.
            ** Don't allocate, but use the value in the pointer (we zero it out when we de-allocate)
            */

            rxtid->rxbuf = (struct ath_rxbuf *)OS_MALLOC(sc->sc_osdev ,
                                                         ATH_TID_MAX_BUFS*sizeof(struct ath_rxbuf),
                                                         GFP_ATOMIC);
        }

        if (rxtid->rxbuf == NULL) {
            /*
            ** If malloc is unsuccessful, treat this as an ADDBA Reject
            */
            rxtid->statuscode = IEEE80211_STATUS_REFUSED;
            DPRINTF(sc,ATH_DEBUG_AGGR_MEM,"%s: Unable to allocate RX buffer, refusing ADDBA\n",__func__);

        } 
        else {

                /*
                ** Ensure the memory is zeroed out (all internal pointers are null)
                */

                OS_MEMZERO(rxtid->rxbuf, ATH_TID_MAX_BUFS*sizeof(struct ath_rxbuf));

                DPRINTF(sc,ATH_DEBUG_AGGR_MEM, "%s: Allocated @%pK\n",__func__,
                                                                    rxtid->rxbuf);

            /* Allow aggregation reception */
            rxtid->addba_exchangecomplete = 1;
        }
    }

    rxtid->dialogtoken              = dialogtoken;
    rxtid->baparamset.amsdusupported = IEEE80211_BA_AMSDU_SUPPORTED;
    rxtid->baparamset.tid           = tidno;
#if ATH_SUPPORT_TIDSTUCK_WAR
    rxtid->rxtid_lastdata           = 0;
#endif	

    ATH_RXTID_UNLOCK(rxtid);
    return 0;
}

/*
 * Setup ADDBA response
 *
 * Output: status code, BA parameter set and BA timeout
 *         for response
 */
void
ath_addba_responsesetup(
    ath_dev_t dev, ath_node_t node,
    u_int8_t tidno,
    u_int8_t *dialogtoken,
    u_int16_t *statuscode,
    struct ieee80211_ba_parameterset *baparamset,
    u_int16_t *batimeout
    )
{
    struct ath_node *an = ATH_NODE(node);
    struct ath_arx_tid *rxtid  = &an->an_aggr.rx.tid[tidno];

    /* setup ADDBA response paramters */
    *dialogtoken = rxtid->dialogtoken;
    *statuscode = rxtid->statuscode;
    *baparamset = rxtid->baparamset;
    *batimeout  = rxtid->batimeout;
}

/*
 * Process DELBA
 */
void
ath_delba_process(
    ath_dev_t dev, ath_node_t node,
    struct ieee80211_delba_parameterset *delbaparamset,
    u_int16_t reasoncode
    )
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_node *an = ATH_NODE(node);
    u_int16_t tidno = delbaparamset->tid;

    if (delbaparamset->initiator)
        ath_rx_aggr_teardown(sc, an, tidno);
    else
        ath_tx_aggr_teardown(sc, an, tidno);
}

/*
 * Process received BAR frame
 */
int
ath_bar_rx(struct ath_softc *sc, struct ath_node *an, wbuf_t wbuf)
{
    struct ieee80211_frame_bar *bar;
    int tidno, tidno_oldway;
    u_int16_t seqno;
    struct ath_arx_tid *rxtid;
    int index, cindex;
    wbuf_t twbuf;
    ieee80211_rx_status_t *rx_status;
    u_int8_t comba = 0;

    __11nstats(sc, rx_bars);

    /*
     * look at BAR contents
     */
    bar = (struct ieee80211_frame_bar *) wbuf_header(wbuf);
    tidno = (le16toh(bar->i_ctl) & IEEE80211_BAR_CTL_TID_M) >> IEEE80211_BAR_CTL_TID_S;

    comba = le16toh(bar->i_ctl) & IEEE80211_BAR_CTL_COMBA;
    /* WAR for EV#69958
     * in case of old station sending the bar,
     *      (a) tid would get zero
     *      (b) compressed bitmap flag would be zero AND
     *      (c) reserved bits gets some non-zero value. get tid_oldway from this reserved bits.
     * If tid_oldway is a non-zero value, there is pretty good chance that it is sent by old
     * Atheros driver. In such a case choose the tid_oldway as tid.
     * If any non-Atheros station sends reserved bits as non-zero we might end-up choosing wrong tid, 
     * only when (a) and (b) of above matches, so still we will be safe side, not 100% though.
     *
     * In our RX filter we need to enable to receive the uncompressed block
     * ack req, but we should never announse that in beacon.
     */
    tidno_oldway = (bar->i_ctl & IEEE80211_BAR_CTL_TID_M) >> IEEE80211_BAR_CTL_TID_S;
    if ((tidno == 0) && (tidno_oldway != 0) && (comba == 0)) {
        tidno = tidno_oldway;
    }
    seqno = le16toh(bar->i_seq) >> IEEE80211_SEQ_SEQ_SHIFT;

    /*
     * process BAR - indicate all pending RX frames till the BAR seqno
     */
    rxtid = &an->an_aggr.rx.tid[tidno];

    ATH_RXTID_LOCK(rxtid);

    /*
     * get relative index
     */
    index = ATH_BA_INDEX(rxtid->seq_next, seqno);

    /*
     * drop BAR if old sequence (index is too large)
     */
    if ((index > rxtid->baw_size) &&
        (index > (IEEE80211_SEQ_MAX - (rxtid->baw_size << 2)))) {
        /*
         * discard frame, ieee layer may not treat frame as a dup
         */
        ATH_RXTID_UNLOCK(rxtid);
        __11nstats(sc, rx_bardiscard);
        wbuf_free(wbuf);

        DPRINTF(sc, ATH_DEBUG_AGGR_MEM,
                "%s[%d]: DROPPING old sequence: tid %d, baw_head %d, baw_tail %d, seq_next %d\n",
                __func__, __LINE__, 
            tidno, rxtid->baw_head, rxtid->baw_tail, rxtid->seq_next); 

        return IEEE80211_FC0_TYPE_CTL;
    }

    /*
     * complete receive processing for all pending frames upto BAR seqno
     */
    cindex = (rxtid->baw_head + index) & (ATH_TID_MAX_BUFS - 1);

    DPRINTF(sc, ATH_DEBUG_AGGR_MEM,
            "%s[%d]: dst[%02x:%02x:%02x:%02x:%02x:%02x] src[%02x:%02x:%02x:%02x:%02x:%02x] seq[%d], "
            "BAR RX frame tid %d, baw_head %d, baw_tail %d, seq_next %d, cindex %d\n",
            __func__, __LINE__,
            bar->i_ra[0], bar->i_ra[1], bar->i_ra[2], bar->i_ra[3], bar->i_ra[4], bar->i_ra[5],
            bar->i_ta[0], bar->i_ta[1], bar->i_ta[2], bar->i_ta[3], bar->i_ta[4], bar->i_ta[5],
            seqno, tidno, rxtid->baw_head, rxtid->baw_tail, rxtid->seq_next, cindex);

    while ((rxtid->baw_head != rxtid->baw_tail) &&
           (rxtid->baw_head != cindex)) {
        twbuf = rxtid->rxbuf[rxtid->baw_head].rx_wbuf;
        rx_status = &rxtid->rxbuf[rxtid->baw_head].rx_status;
        rxtid->rxbuf[rxtid->baw_head].rx_wbuf = NULL;

        INCR(rxtid->baw_head, ATH_TID_MAX_BUFS);
        INCR(rxtid->seq_next, IEEE80211_SEQ_MAX);

        if (twbuf != NULL) {
            __11nstats(sc, rx_barcomps);
            sc->sc_ieee_ops->rx_subframe(an->an_node, twbuf, rx_status);
        }
    }

    /*
     * ... and indicate rest of the frames in-order
     */
    while (rxtid->baw_head != rxtid->baw_tail &&
           rxtid->rxbuf[rxtid->baw_head].rx_wbuf != NULL) {
        twbuf = rxtid->rxbuf[rxtid->baw_head].rx_wbuf;
        rx_status = &rxtid->rxbuf[rxtid->baw_head].rx_status;
        rxtid->rxbuf[rxtid->baw_head].rx_wbuf = NULL;

        INCR(rxtid->baw_head, ATH_TID_MAX_BUFS);
        INCR(rxtid->seq_next, IEEE80211_SEQ_MAX);

        __11nstats(sc, rx_barrecvs);
        sc->sc_ieee_ops->rx_subframe(an->an_node, twbuf, rx_status);
    }

    ATH_RXTID_UNLOCK(rxtid);

    /*
     * free bar itself
     */
    wbuf_free(wbuf);
    return IEEE80211_FC0_TYPE_CTL;
}

/*
 * Function to handle a subframe of aggregation when HT is enabled
 */
int
ath_ampdu_input(struct ath_softc *sc, struct ath_node *an, wbuf_t wbuf, ieee80211_rx_status_t *rx_status)
{
    struct ieee80211_frame             *wh;
    struct ieee80211_qosframe          *whqos;
    struct ieee80211_qosframe_addr4    *whqos_4addr;
    u_int8_t                           type, subtype;
    int                                ismcast;
    int                                tid;
    struct ath_arx_tid                 *rxtid;
    int                                index, cindex, rxdiff;
    u_int16_t                          rxseq;
    struct ath_rxbuf                   *rxbuf;
    int                                is4addr;
    wbuf_t                             wbuf_to_indicate;

    wh = (struct ieee80211_frame *) wbuf_header(wbuf);
    is4addr = (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS;

    __11nstats(sc, rx_aggr);
    /*
     * collect stats of frames with non-zero version
     */
    if ((wh->i_fc[0] & IEEE80211_FC0_VERSION_MASK) != IEEE80211_FC0_VERSION_0) {
        __11nstats(sc, rx_aggrbadver);
        wbuf_free(wbuf);
        DPRINTF(sc, ATH_DEBUG_AGGR_MEM, "%s[%d]: DROPPING bad version\n", __func__, __LINE__);
        return -1;
    }

    if (rx_status->flags & ATH_RX_DECRYPT_ERROR) {
        __11nstats(sc, rx_aggrbadver);
        wbuf_free(wbuf);
        DPRINTF(sc, ATH_DEBUG_AGGR_MEM, "%s[%d]: DROPPING rx decrypt error\n", __func__, __LINE__);
        return -1;
    }

    type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
    subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
    ismcast = IEEE80211_IS_MULTICAST(wh->i_addr1);

    /*
     * lookup rx tid state
     */
    if (is4addr) { /* special qos check for 4 address frames */
        whqos_4addr = (struct ieee80211_qosframe_addr4 *) wh;
        tid = whqos_4addr->i_qos[0] & IEEE80211_QOS_TID;
    } else {
        whqos = (struct ieee80211_qosframe *) wh;
        tid = whqos->i_qos[0] & IEEE80211_QOS_TID;
    }

    rxtid = &an->an_aggr.rx.tid[tid];

    /*
     * Drop the frame not belonging to me if ProxySTA is not enabled.
     * Refer to Bug 34218.
     */ 
    if (!sc->sc_enableproxysta) {
        /*
         * We are comparing the last 5 bytes first. If matches, then check the
         * first octet talking into account the BSSID mask.
         */
#if ATH_SUPPORT_AP_WDS_COMBO
        if (OS_MEMCMP(&(wh->i_addr1[1]), &(sc->sc_myaddr[1]), IEEE80211_ADDR_LEN - 2)) {
            if (((wh->i_addr1[0] & sc->sc_bssidmask[0]) == (sc->sc_myaddr[0] & sc->sc_bssidmask[0])) &&
                ((wh->i_addr1[IEEE80211_ADDR_LEN - 1] & sc->sc_bssidmask[IEEE80211_ADDR_LEN - 1]) == 
                (sc->sc_myaddr[IEEE80211_ADDR_LEN - 1] & sc->sc_bssidmask[IEEE80211_ADDR_LEN - 1]))) {
#else
        if (!ismcast &&
            (OS_MEMCMP(&(wh->i_addr1[1]), &(sc->sc_myaddr[1]), IEEE80211_ADDR_LEN - 1) ||
             ((wh->i_addr1[0] & sc->sc_bssidmask[0]) != (sc->sc_myaddr[0] & sc->sc_bssidmask[0])))) {
#endif
            DPRINTF(sc, ATH_DEBUG_AGGR_MEM,
                "%s[%d]: dst[%02x:%02x:%02x:%02x:%02x:%02x] src[%02x:%02x:%02x:%02x:%02x:%02x] seq[%d], "
                "DROPPING frame tid %d, baw_head %d, baw_tail %d, seq_next %d\n",
                __func__, __LINE__,
                wh->i_addr1[0], wh->i_addr1[1], wh->i_addr1[2], wh->i_addr1[3], wh->i_addr1[4], wh->i_addr1[5],
                wh->i_addr2[0], wh->i_addr2[1], wh->i_addr2[2], wh->i_addr2[3], wh->i_addr2[4], wh->i_addr2[5],
                (le16toh(*(u_int16_t *)wh->i_seq) >> IEEE80211_SEQ_SEQ_SHIFT),
                tid, rxtid->baw_head, rxtid->baw_tail, rxtid->seq_next); 
            wbuf_free(wbuf);
            return -1;
        }
    }

    if ((type == IEEE80211_FC0_TYPE_CTL) &&
        (subtype == IEEE80211_FC0_SUBTYPE_BAR)) {
        return ath_bar_rx(sc, an, wbuf);
    }

    /*
     * special aggregate processing is ONLY for qos unicast data frames
     */
    if (type != IEEE80211_FC0_TYPE_DATA ||
        subtype != IEEE80211_FC0_SUBTYPE_QOS || (ismcast)) {
        __11nstats(sc, rx_nonqos);
        return sc->sc_ieee_ops->rx_subframe(an->an_node, wbuf, rx_status);
    }

    ATH_RXTID_LOCK(rxtid);


    /*
     * If the ADDBA exchange has not been completed by the source,
     * process via legacy path (i.e. no reordering buffer is needed)
     */
    if (!rxtid->addba_exchangecomplete) {
        ATH_RXTID_UNLOCK(rxtid);
        __11nstats(sc, rx_nonqos);
        return sc->sc_ieee_ops->rx_subframe(an->an_node, wbuf, rx_status);
    }
#if ATH_SUPPORT_TIDSTUCK_WAR
    rxtid->rxtid_lastdata = OS_GET_TIMESTAMP();
#endif	

    /*
     * extract sequence number from recvd frame
     */
    rxseq = le16toh(*(u_int16_t *)wh->i_seq) >> IEEE80211_SEQ_SEQ_SHIFT;

    if (rxtid->seq_reset) {
        __11nstats(sc, rx_seqreset);
        rxtid->seq_reset = 0;
        rxtid->seq_next = rxseq;
    }

    index = ATH_BA_INDEX(rxtid->seq_next, rxseq);

    /*
     * drop frame if old sequence (index is too large)
     */
    if (index > (IEEE80211_SEQ_MAX - (rxtid->baw_size << 2))) {
        /*
         * discard frame, ieee layer may not treat frame as a dup
         */
        ATH_RXTID_UNLOCK(rxtid);
        __11nstats(sc, rx_oldseq);
        DPRINTF(sc, ATH_DEBUG_AGGR_MEM,
                "%s[%d]: DROPPING frame tid %d, baw_head %d, baw_tail %d, seq_next %d, index(%d) > %d, seq[%d]\n",
                __func__, __LINE__, 
            tid, rxtid->baw_head, rxtid->baw_tail, rxtid->seq_next, index,
            (IEEE80211_SEQ_MAX - (rxtid->baw_size << 2)),
            (le16toh(*(u_int16_t *)wh->i_seq) >> IEEE80211_SEQ_SEQ_SHIFT));
        wbuf_free(wbuf);
        return IEEE80211_FC0_TYPE_DATA;
    }

    /*
     * sequence number is beyond block-ack window
     */
    if (index >= rxtid->baw_size) {

        __11nstats(sc, rx_bareset);

        /*
         * complete receive processing for all pending frames
         */
        while (index >= rxtid->baw_size) {

            rxbuf = rxtid->rxbuf + rxtid->baw_head;
            
            // Increment ahead, in case there is a flush tid from within rx_subframe.
            INCR(rxtid->baw_head, ATH_TID_MAX_BUFS);
            INCR(rxtid->seq_next, IEEE80211_SEQ_MAX);

            if (rxbuf->rx_wbuf != NULL) {
                wbuf_to_indicate = rxbuf->rx_wbuf;
                rxbuf->rx_wbuf = NULL; 
                __11nstats(sc, rx_baresetpkts);
                sc->sc_ieee_ops->rx_subframe(an->an_node, wbuf_to_indicate,
                                             &rxbuf->rx_status);
                __11nstats(sc, rx_recvcomp);                             
            }

            index --;
        }
    }

    /*
     * add buffer to the recv ba window
     */
    cindex = (rxtid->baw_head + index) & (ATH_TID_MAX_BUFS - 1);
    rxbuf = rxtid->rxbuf + cindex;
    
    DPRINTF(sc, ATH_DEBUG_AGGR_MEM,
            "%s[%d]: dst[%02x:%02x:%02x:%02x:%02x:%02x] src[%02x:%02x:%02x:%02x:%02x:%02x] seq[%d], "
            "DROPPING frame tid %d, baw_head %d, baw_tail %d, seq_next %d cindex %d\n",
            __func__, __LINE__,
            wh->i_addr1[0], wh->i_addr1[1], wh->i_addr1[2], wh->i_addr1[3], wh->i_addr1[4], wh->i_addr1[5],
            wh->i_addr2[0], wh->i_addr2[1], wh->i_addr2[2], wh->i_addr2[3], wh->i_addr2[4], wh->i_addr2[5],
            (le16toh(*(u_int16_t *)wh->i_seq) >> IEEE80211_SEQ_SEQ_SHIFT),
            tid, rxtid->baw_head, rxtid->baw_tail, rxtid->seq_next, cindex);

    if (rxbuf->rx_wbuf != NULL) {
        /*
         *duplicate frame
         */
        DPRINTF(sc, ATH_DEBUG_AGGR_MEM, "%s[%d]:Dup frame tid %d, baw_head %d, baw_tail %d, seq_next %d, cindex %d\n",
                __func__, __LINE__, tid, rxtid->baw_head, rxtid->baw_tail, rxtid->seq_next, cindex);
        ATH_RXTID_UNLOCK(rxtid);
        __11nstats(sc, rx_dup);
        wbuf_free(wbuf);
        return IEEE80211_FC0_TYPE_DATA;
    }

    rxbuf->rx_wbuf = wbuf;
    rxbuf->rx_time = OS_GET_TIMESTAMP();
    rxbuf->rx_status = *rx_status;

    rxdiff = (rxtid->baw_tail - rxtid->baw_head) &
             (ATH_TID_MAX_BUFS - 1);

    /*
     * advance tail if sequence received is newer than any received so far
     */
    if (index >= rxdiff) {
        __11nstats(sc, rx_baadvance);
        rxtid->baw_tail = cindex;
        INCR(rxtid->baw_tail, ATH_TID_MAX_BUFS);
    }

    /*
     * indicate all in-order received frames
     */
    while (rxtid->baw_head != rxtid->baw_tail) {
        rxbuf = rxtid->rxbuf + rxtid->baw_head;
        if (!rxbuf->rx_wbuf)
            break;

        __11nstats(sc, rx_recvcomp);

        INCR(rxtid->baw_head, ATH_TID_MAX_BUFS);
        INCR(rxtid->seq_next, IEEE80211_SEQ_MAX);
        
        wbuf_to_indicate = rxbuf->rx_wbuf;
        rxbuf->rx_wbuf = NULL;
        sc->sc_ieee_ops->rx_subframe(an->an_node, wbuf_to_indicate, &rxbuf->rx_status);
    }

    /*
     * start a timer to flush all received frames if there are pending
     * receive frames
     */
    if (rxtid->baw_head != rxtid->baw_tail) {
        if (!ATH_RXTIMER_IS_ACTIVE(&rxtid->timer)) {
            __11nstats(sc,rx_timer_starts);
                ATH_SET_RXTIMER_PERIOD(&rxtid->timer, sc->sc_rxtimeout[TID_TO_WME_AC(tid)]);
                ATH_START_RXTIMER(&rxtid->timer);
        }
    } else {
        if (ATH_RXTIMER_IS_ACTIVE(&rxtid->timer)) {
            __11nstats(sc,rx_timer_stops);
        }
        ATH_CANCEL_RXTIMER(&rxtid->timer, CANCEL_NO_SLEEP);
    }
    
    ATH_RXTID_UNLOCK(rxtid);
    return IEEE80211_FC0_TYPE_DATA;
}

/*
 * Timer to flush all received sub-frames
 */
static int
ath_rx_timer(void *context)
{
    struct ath_arx_tid *rxtid = (struct ath_arx_tid *) context;
    struct ath_node *an = rxtid->an;
    struct ath_softc *sc = an->an_sc;
    struct ath_rxbuf *rxbuf;
    int count = 0;
    int ret = 1 ; /* This default return value will set 
                   * the state of this timer to inactive.
                  */

#ifndef ATH_HTC_REMOVE_RXTIMER_EARLYTIMEOUTFIX
    systime_t diff;
    struct ath_arx_tid *rxtid0 = &an->an_rx_tid[0];
    int tidno = (rxtid - rxtid0);
    u_int8_t rxtimeout = sc->sc_rxtimeout[TID_TO_WME_AC(tidno)];
#endif    

    int baw_head;
    wbuf_t wbuf_to_indicate;
    int old_addba_state, lc_debug=0;
    ATH_PS_WAKEUP(sc);

    __11nstats(sc, rx_timer_run);
    ATH_RXTID_LOCK(rxtid);
    baw_head = rxtid->baw_head;
    old_addba_state = rxtid->addba_exchangecomplete;
    while (baw_head != rxtid->baw_tail) {
        lc_debug++;
        if (!rxtid->rxbuf) {
#if 0
            KASSERT(rxtid->rxbuf, ("%s[%d] rxtid timer active = %d, baw_head = %d, baw_tail = %d loop_count %d exchange_state old, new (%d %d)\n",
                    __func__, __LINE__, ath_timer_is_active(&rxtid->timer), baw_head,
                    rxtid->baw_tail), lc_debug, old_addba_state, rxtid->addba_exchangecomplete);
#endif
            printk("%s[%d] rxtid timer active = %d, baw_head = %d, baw_tail = %d"
                    "loop_count %d exchange_state old, new (%d %d)\n",
                    __func__, __LINE__, ath_timer_is_active(&rxtid->timer), baw_head,
                    rxtid->baw_tail, lc_debug, old_addba_state, rxtid->addba_exchangecomplete);
            break;
        }
        rxbuf = rxtid->rxbuf + baw_head;
        if (!rxbuf->rx_wbuf) {
            count++;
            INCR(baw_head, ATH_TID_MAX_BUFS);
            continue;
        }
#ifndef ATH_HTC_REMOVE_RXTIMER_EARLYTIMEOUTFIX
            /*
             * Stop if the next one is a very recent frame.
             *
             * Call OS_GET_TIMESTAMP in every iteration to protect against the
             * case in which a new frame is received while we are executing this
             * function. Using a timestamp obtained before entering the loop could
             * lead to a very large time interval (a negative value typecast to
             * unsigned), breaking the function's logic.
             */
            diff = CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP() - rxbuf->rx_time);
            if (diff < rxtimeout) {
                ATH_SET_RXTIMER_PERIOD(&rxtid->timer, rxtimeout - diff);
                break;
            }
#endif 
        __11nstats(sc, rx_recvcomp);
        __11nstats(sc, rx_comp_to);
        __11nstatsn(sc, rx_skipped, count);

        count++;

        INCR(baw_head, ATH_TID_MAX_BUFS);
        ADD(rxtid->baw_head, count, ATH_TID_MAX_BUFS);
        ADD(rxtid->seq_next, count, IEEE80211_SEQ_MAX);
        wbuf_to_indicate = rxbuf->rx_wbuf;
        rxbuf->rx_wbuf = NULL;
        sc->sc_ieee_ops->rx_subframe(an->an_node, wbuf_to_indicate,
                                     &rxbuf->rx_status);
        count = 0;
    }

    /*
     * start a timer to flush all received frames if there are pending
     * receive frames
     */
    if (rxtid->baw_head != rxtid->baw_tail) {
        __11nstats(sc, rx_timer_more);
        ret = 0;    /* Start this timer */
    } 

    ATH_RXTID_UNLOCK(rxtid);

    ATH_PS_SLEEP(sc);

    return ret;
}

/*
 * Free all pending sub-frames in the re-ordering buffer
 * ATH_RXTID_LOCK must be held
 */
static void
ath_rx_flush_tid(struct ath_softc *sc, struct ath_arx_tid *rxtid, int drop)
{
    struct ath_rxbuf *rxbuf;

    while (rxtid->baw_head != rxtid->baw_tail) {
        rxbuf = rxtid->rxbuf + rxtid->baw_head;
        if (!rxbuf->rx_wbuf) {
            INCR(rxtid->baw_head, ATH_TID_MAX_BUFS);
            INCR(rxtid->seq_next, IEEE80211_SEQ_MAX);
            __11nstats(sc, rx_skipped);
            continue;
        }

        __11nstats(sc, rx_recvcomp);

        if (drop) {
            DPRINTF(sc, ATH_DEBUG_AGGR_MEM, "%s[%d]:drop baw_head %d, baw_tail %d, seq_next %d\n",
                    __func__, __LINE__, rxtid->baw_head, rxtid->baw_tail, rxtid->seq_next); 
            wbuf_free(rxbuf->rx_wbuf);
        } else {
            __11nstats(sc, rx_comp_to);
            sc->sc_ieee_ops->rx_subframe(rxtid->an->an_node, rxbuf->rx_wbuf,
                                         &rxbuf->rx_status);
        }
        rxbuf->rx_wbuf = NULL;

        INCR(rxtid->baw_head, ATH_TID_MAX_BUFS);
        INCR(rxtid->seq_next, IEEE80211_SEQ_MAX);
    }
}

/*
 * Rx aggregation tear down
 */
void
ath_rx_aggr_teardown(struct ath_softc *sc, struct ath_node *an, u_int8_t tidno)
{
    struct ath_arx_tid *rxtid = &an->an_rx_tid[tidno];

    if (!rxtid->addba_exchangecomplete)
        return;

    ATH_CANCEL_RXTIMER(&rxtid->timer, CANCEL_NO_SLEEP);
    ATH_RXTID_LOCK(rxtid);
    ath_rx_flush_tid(sc, rxtid, 0);
    rxtid->addba_exchangecomplete = 0;
    
    /* 
    ** De-allocate the receive buffer array allocated when addba 
    ** started 
    */ 
    
    if (rxtid->rxbuf != NULL) {
        DPRINTF(sc,ATH_DEBUG_AGGR_MEM, "%s: Deallocating TID %d rxbuff @%pK\n", 
                   __func__, tidno, rxtid->rxbuf); 
        OS_FREE( rxtid->rxbuf );
        
        /*
        ** Set pointer to null to avoid reuse
        */
        
        rxtid->rxbuf = NULL; 
    }
    ATH_RXTID_UNLOCK(rxtid);
}

/*
 * Initialize per-node receive state
 */
void
ath_rx_node_init(struct ath_softc *sc, struct ath_node *an)
{
    if (sc->sc_rxaggr) {
        struct ath_arx_tid *rxtid;
        int tidno;

        /* Init per tid rx state */
        for (tidno = 0, rxtid = &an->an_rx_tid[tidno]; tidno < WME_NUM_TID;
             tidno++, rxtid++) {
            rxtid->an        = an;
            rxtid->seq_reset = 1;
            rxtid->seq_next  = 0;
            rxtid->baw_size  = WME_MAX_BA;
            rxtid->baw_head  = rxtid->baw_tail = 0;

            /*
            ** Ensure the buffer pointer is null at this point (needs to be allocated
            ** when addba is received)
            */

            rxtid->rxbuf     = NULL;

            ATH_RXTIMER_INIT(sc->sc_osdev, &rxtid->timer, sc->sc_rxtimeout[TID_TO_WME_AC(tidno)],
                                                     ath_rx_timer, rxtid);

            ATH_RXTID_LOCK_INIT(rxtid);

            /* ADDBA state */
            rxtid->addba_exchangecomplete = 0;
            rxtid->userstatuscode = IEEE80211_STATUS_SUCCESS;
#if ATH_SUPPORT_TIDSTUCK_WAR
            rxtid->rxtid_lastdata = 0;
#endif			
        }
    }
}

void ath_rx_node_cleanup(struct ath_softc *sc, struct ath_node *an)
{
    if (sc->sc_rxaggr) {
        struct ath_arx_tid *rxtid;
        int tidno,i;

        /* Init per tid rx state */
        for (tidno = 0, rxtid = &an->an_rx_tid[tidno]; tidno < WME_NUM_TID;
             tidno++, rxtid++) {
            int old_head = 0;

            /*
             * Make the old_head variable appear to be used outside the
             * KASSERT call, so the compiler won't complain of an unused
             * variable if the KASSERT call is compiled out.
             */
            old_head = rxtid->baw_head; //pre ath_rx_flush_tid

            /* must cancel timer first */
            ATH_CANCEL_RXTIMER(&rxtid->timer, CANCEL_NO_SLEEP);
            
            if (!rxtid->addba_exchangecomplete)
             continue;

            /* drop any pending sub-frames */
            ATH_RXTID_LOCK(rxtid);
            ath_rx_flush_tid(sc, rxtid, 1);
            
            if(rxtid->rxbuf) {
                for (i = 0; i < ATH_TID_MAX_BUFS; i++){
                    KASSERT((rxtid->rxbuf[i].rx_wbuf == NULL),
                            ("tid[%d]->rxbuf[%d] != NULL (baw"
                             " head %d tail %d", tidno, i,
                             old_head, rxtid->baw_tail));
                }

                OS_FREE(rxtid->rxbuf);
                rxtid->rxbuf = NULL;
            }
            rxtid->addba_exchangecomplete = 0;
#if ATH_SUPPORT_TIDSTUCK_WAR
            rxtid->rxtid_lastdata = 0;
#endif			
            ATH_RXTID_UNLOCK(rxtid);
        }
    }

}




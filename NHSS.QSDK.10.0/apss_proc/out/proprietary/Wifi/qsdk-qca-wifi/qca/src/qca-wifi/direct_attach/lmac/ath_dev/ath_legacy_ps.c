/*
 * Copyright (c) 2010, Atheros Communications Inc.
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

#include "ath_internal.h"

#if LMAC_SUPPORT_POWERSAVE_QUEUE

static void ath_node_pwrsaveq_complete_athwbuf(struct ath_node *an, ath_wbuf_t athwbuf)
{
    struct ath_softc *sc = an->an_sc;
    ieee80211_tx_status_t ts;

    ts.flags = ATH_TX_ERROR;
    ts.retries = 0;
    if (athwbuf) {
        sc->sc_ieee_ops->tx_complete(athwbuf->wbuf, &ts, 0);
        OS_FREE_PS(athwbuf);
    }
}

/*
 * handle queueing of frames with PS bit set.
 * if a new frame gets queued with no PS bit set then dump any
 * existing (queued) frames with PS bit set.
 * if a new frame gets queued with  PS bit set then
 * increment the ps frame count.
 */
 
static void ath_node_pwrsaveq_handle_ps_frames(struct ath_node *an,
                                            ath_wbuf_t athwbuf, u_int8_t frame_type)
{
    struct ath_softc *sc = an->an_sc;
    struct ath_node_pwrsaveq *mgtq;
    ath_wbuf_t tmpathwbuf;

    mgtq  = ATH_NODE_PWRSAVEQ_MGMTQ(an); 

    if (wbuf_is_pwrsaveframe(athwbuf->wbuf))  {
        /* ps frame (null (or) pspoll frame) */
        ++mgtq->nsq_num_ps_frames;
    } else {
        /* non ps frame*/
        if (mgtq->nsq_num_ps_frames) {
            struct ath_node_pwrsaveq *tmpq, temp_q;
            tmpq = &temp_q;

            ATH_NODE_PWRSAVEQ_INIT(tmpq);
            /*
             * go through the mgt queue and dump the frames with PS=1(pspoll,null).
             * accummulate the remaining frames into temp queue.
             */
            for (;;) {
                ATH_NODE_PWRSAVEQ_DEQUEUE(mgtq, tmpathwbuf);
                if (tmpathwbuf == NULL) {
                    break;
                }
                if (wbuf_is_pwrsaveframe(tmpathwbuf->wbuf))  {
                    /* complete the wbuf */
                    DPRINTF(sc, ATH_DEBUG_PWR_SAVE, "%s: send error, comple the wbuf\n", __func__);
                    ath_node_pwrsaveq_complete_athwbuf(an, tmpathwbuf);
                } else {
                    ATH_NODE_PWRSAVEQ_ADD(tmpq,tmpathwbuf);
                }
            }

            mgtq->nsq_num_ps_frames = 0;
            /*
             * move all the frames from temp queue to mgmt queue.
             */
            for (;;) {
                ATH_NODE_PWRSAVEQ_DEQUEUE(tmpq, tmpathwbuf);
                if (tmpathwbuf == NULL) {
                    break;
                }
                ATH_NODE_PWRSAVEQ_ADD(mgtq, tmpathwbuf);
            }
        }
    }
}

/*
 * Save an outbound packet for a node in power-save sleep state.
 * The new packet is placed on the node's saved queue, and the TIM
 * is changed by UMAC caller.
 */
void
ath_node_pwrsaveq_queue(ath_node_t node, ath_wbuf_t athwbuf, u_int8_t frame_type)
{
    struct ath_node *an = ATH_NODE(node);
    struct ath_softc *sc = an->an_sc;
    int qlen, age;
    struct ath_node_pwrsaveq *dataq,*mgtq,*psq;

    dataq = ATH_NODE_PWRSAVEQ_DATAQ(an);
    mgtq  = ATH_NODE_PWRSAVEQ_MGMTQ(an);


    ATH_NODE_PWRSAVEQ_LOCK(dataq);
    ATH_NODE_PWRSAVEQ_LOCK(mgtq);

    if (frame_type == IEEE80211_FC0_TYPE_MGT) {
        psq = mgtq;
    } else {
        psq = dataq;
    }

    if (ATH_NODE_PWRSAVEQ_FULL(psq)) {
        ATH_NODE_PWRSAVEQ_UNLOCK(mgtq);
        ATH_NODE_PWRSAVEQ_UNLOCK(dataq);
        DPRINTF(sc, ATH_DEBUG_ANY,
                "%s pwr save q: overflow, drops (size %d) \n",
                (psq == dataq) ? "data" : "mgt",
                ATH_NODE_PWRSAVEQ_MAX_LEN);

        ath_node_pwrsaveq_complete_athwbuf(an, athwbuf);

        return;

    }

    /*
     * special handling of frames with PS = 1.
     */
    ath_node_pwrsaveq_handle_ps_frames(an, athwbuf, frame_type);


    /* By default use the 4 * beacon intval as the aging time */
    age = (ATH_NODE_AGE_INTVAL << 2) >> 10; /* TU -> secs */

    /*
     * Note: our aging algorithm is not working well. In fact, due to the long interval
     * when the aging algorithm is called (ATH_NODE_INACT_WAIT is 150 secs), we depend on
     * the associated station node to be disassociated to clear its stale frames. However,
     * as a temporary fix, I will make sure that the age is at least greater than 
     * ATH_NODE_INACT_WAIT. Otherwise, we will discard all frames in ps queue even though
     * it is just queued.
    */
    if (age < ATH_NODE_INACT_WAIT) {
        DPRINTF(sc, ATH_DEBUG_PWR_SAVE,
                "%s Note: increased age from %d to %d secs.\n",
                __func__, age, ATH_NODE_INACT_WAIT);
        age = ATH_NODE_INACT_WAIT;
    }

    ATH_NODE_PWRSAVEQ_ENQUEUE(psq, athwbuf, qlen, age);

    /*
     * calculate the combined queue length of management and data queues.
     */
    qlen = ATH_NODE_PWRSAVEQ_QLEN(dataq);
    qlen += ATH_NODE_PWRSAVEQ_QLEN(mgtq);

    ATH_NODE_PWRSAVEQ_UNLOCK(mgtq);
    ATH_NODE_PWRSAVEQ_UNLOCK(dataq);

    DPRINTF(sc, ATH_DEBUG_PWR_SAVE,
            "%s pwr queue:save frame, %u now queued \n",
            (psq == dataq) ? "data" : "mgt" ,qlen);
    /* TIM bit will be set by UMAC caller */
}

/*
 * send one frme out of power save queue .
 * called in reponse to PS poll.
 * returns 1 if succesfully sends a frame. 0 other wise.
 */
int
ath_node_pwrsaveq_send(ath_node_t node, u_int8_t frame_type)
{
    struct ath_node *an = ATH_NODE(node);
    struct ath_softc *sc = an->an_sc;
    ath_wbuf_t athwbuf;
    wbuf_t wbuf = NULL;
    int qlen;
    struct ath_node_pwrsaveq *dataq, *mgtq;
    ieee80211_tx_control_t *txctl;
    struct ieee80211_frame *wh;
    u_int8_t more_frag;

    dataq = ATH_NODE_PWRSAVEQ_DATAQ(an); 
    mgtq  = ATH_NODE_PWRSAVEQ_MGMTQ(an); 

next_frag:

    ATH_NODE_PWRSAVEQ_LOCK(dataq);
    ATH_NODE_PWRSAVEQ_LOCK(mgtq);

    if (frame_type == IEEE80211_FC0_TYPE_MGT) {
        ATH_NODE_PWRSAVEQ_DEQUEUE(mgtq, athwbuf);
        if (athwbuf)
            wbuf = athwbuf->wbuf;
        if (wbuf && wbuf_is_pwrsaveframe(wbuf))  {
            /* ps frame (null (or) pspoll frame) */
            --mgtq->nsq_num_ps_frames;
        } 
    } else {
        ATH_NODE_PWRSAVEQ_DEQUEUE(dataq, athwbuf);
        if (athwbuf)
            wbuf = athwbuf->wbuf;
    }

    if (athwbuf == NULL || wbuf == NULL) {
        ATH_NODE_PWRSAVEQ_UNLOCK(mgtq);
        ATH_NODE_PWRSAVEQ_UNLOCK(dataq);
        return 0;
    }
        
    qlen = ATH_NODE_PWRSAVEQ_QLEN(dataq);
    qlen += ATH_NODE_PWRSAVEQ_QLEN(mgtq);

    ATH_NODE_PWRSAVEQ_UNLOCK(mgtq);
    ATH_NODE_PWRSAVEQ_UNLOCK(dataq);

    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    more_frag = wh->i_fc[1] & IEEE80211_FC1_MORE_FRAG;

    /* 
     * If there are more packets, set the more packets bit
     * in the packet dispatched to the station; otherwise
     * turn off the TIM bit.
     */
    if (qlen != 0) {
        DPRINTF(sc, ATH_DEBUG_PWR_SAVE, "send packet, %u still queued \n", qlen);
        /* 
         * encap will set more data bit.
         */
        wbuf_set_moredata(wbuf);
        if (wbuf_is_moredata(wbuf)) {
             wh->i_fc[1] |= IEEE80211_FC1_MORE_DATA;
        } 
    } else {
        DPRINTF(sc, ATH_DEBUG_PWR_SAVE, "%s", "send packet, queue empty \n");
        /* TIM bit will be set by UMAC caller */
    }

    wbuf_clear_legacy_ps(wbuf);
    txctl = &athwbuf->txctl;

    if (sc->sc_ath_ops.tx(sc, wbuf, txctl) != 0) {
        DPRINTF(sc, ATH_DEBUG_PWR_SAVE, "%s: send error, comple the wbuf\n", __func__);
        ath_node_pwrsaveq_complete_athwbuf(an, athwbuf);
        /* process all fragment together */
        if (more_frag)
            goto next_frag;

        return 0;
    }

    if (athwbuf) {
        OS_FREE_PS(athwbuf);
    }

    /* process all fragments together */
    if (more_frag)
        goto next_frag;

    return 1;
}

/*
 * node saveq flush.
 */
void
ath_node_pwrsaveq_flush(ath_node_t node)
{
    struct ath_node *an = ATH_NODE(node);
    struct ath_softc *sc = an->an_sc;
    ath_wbuf_t athwbuf;
    wbuf_t wbuf;
    struct ath_node_pwrsaveq *dataq,*mgtq;
    int dqlen, mqlen;
    ieee80211_tx_control_t *txctl;

    dataq = ATH_NODE_PWRSAVEQ_DATAQ(an); 
    mgtq  = ATH_NODE_PWRSAVEQ_MGMTQ(an); 

    /* XXX if no stations in ps mode, flush mc frames */
    dqlen = ATH_NODE_PWRSAVEQ_QLEN(dataq);

    /*
     * Flush queued mgmt frames.
     */
    ATH_NODE_PWRSAVEQ_LOCK(mgtq);
    if (ATH_NODE_PWRSAVEQ_QLEN(mgtq) != 0) {
        DPRINTF(sc, ATH_DEBUG_PWR_SAVE, "flush mgt ps queue, %u packets queued \n",
                                         ATH_NODE_PWRSAVEQ_QLEN(mgtq));
        for (;;) {

            ATH_NODE_PWRSAVEQ_DEQUEUE(mgtq, athwbuf);
            if (athwbuf == NULL)
                break;
            wbuf = athwbuf->wbuf;
            ASSERT(wbuf);
            mqlen = ATH_NODE_PWRSAVEQ_QLEN(mgtq);
            DPRINTF(sc, ATH_DEBUG_PWR_SAVE, "sendmgmt ps queue, %u packets remaining \n", mqlen);
            /*
             * For a Station node (non bss node) If this is the last packet, turn off the TIM bit,
             * Set the PWR_SAV bit on every frame but the last one
             * to allow encap to test for
             * adding more MORE_DATA bit to wh.
             */
            if (mqlen || dqlen) {
                wbuf_set_moredata(wbuf);
                if (wbuf_is_moredata(wbuf)) {
                    struct ieee80211_frame *wh = (struct ieee80211_frame *)wbuf_header(wbuf);
                    wh->i_fc[1] |= IEEE80211_FC1_MORE_DATA;
                } 
            }

            wbuf_clear_legacy_ps(wbuf);
            txctl = &athwbuf->txctl;

            if (sc->sc_ath_ops.tx(sc, wbuf, txctl) != 0) {
                DPRINTF(sc, ATH_DEBUG_PWR_SAVE, "%s: send error, comple the wbuf\n", __func__);
                ath_node_pwrsaveq_complete_athwbuf(an, athwbuf);
            } else {
                if (athwbuf) {
                    OS_FREE_PS(athwbuf);
                }
            }
        }
    }
    mgtq->nsq_num_ps_frames = 0;
    ATH_NODE_PWRSAVEQ_UNLOCK(mgtq);

    /*
     * Flush queued unicast frames.
     */
    ATH_NODE_PWRSAVEQ_LOCK(dataq);
    if (ATH_NODE_PWRSAVEQ_QLEN(dataq) != 0) {
        DPRINTF(sc, ATH_DEBUG_PWR_SAVE, "flush data ps queue, %u packets queued \n",
            ATH_NODE_PWRSAVEQ_QLEN(dataq));
        for (;;) {

            ATH_NODE_PWRSAVEQ_DEQUEUE(dataq, athwbuf);
            if (athwbuf == NULL)
                break;
            wbuf = athwbuf->wbuf;
            ASSERT(wbuf);
            dqlen = ATH_NODE_PWRSAVEQ_QLEN(dataq);
            DPRINTF(sc, ATH_DEBUG_PWR_SAVE, "senddata ps queue, %u packets remaining \n", dqlen);
            /*
             * For a Station node (non bss node) If this is the last packet, turn off the TIM bit,
             * Set the PWR_SAV bit on every frame but the last one
             * to allow encap to test for
             * adding more MORE_DATA bit to wh.
             */
            if (dqlen) {
                wbuf_set_moredata(wbuf);
                if (wbuf_is_moredata(wbuf)) {
                    struct ieee80211_frame *wh = (struct ieee80211_frame *)wbuf_header(wbuf);
                    wh->i_fc[1] |= IEEE80211_FC1_MORE_DATA;
                } 
            }
            
            wbuf_clear_legacy_ps(wbuf);
            txctl = &athwbuf->txctl;

            if (sc->sc_ath_ops.tx(sc, wbuf, txctl) != 0) {
                DPRINTF(sc, ATH_DEBUG_PWR_SAVE, "%s: send error, comple the wbuf\n", __func__);
                ath_node_pwrsaveq_complete_athwbuf(an, athwbuf);
            } else {
                if (athwbuf) {
                    OS_FREE_PS(athwbuf);
                }
            }
        }
    }
    ATH_NODE_PWRSAVEQ_UNLOCK(dataq);

    /* TIM will be set by UMAC caller */
}

/*
 * Clear any frames queued on a node's power save queue.
 * The number of frames that were present is returned.
 */
int
ath_node_pwrsaveq_drain(ath_node_t node)
{
    struct ath_node *an = ATH_NODE(node);
    int qlen;
    struct ath_node_pwrsaveq *dataq,*mgtq;
    ath_wbuf_t athwbuf;

    dataq = ATH_NODE_PWRSAVEQ_DATAQ(an); 
    mgtq  = ATH_NODE_PWRSAVEQ_MGMTQ(an); 

    qlen = ATH_NODE_PWRSAVEQ_QLEN(dataq);
    qlen += ATH_NODE_PWRSAVEQ_QLEN(mgtq);

    /*
     * free all the frames.
     */

    for (;;) {
        ATH_NODE_PWRSAVEQ_LOCK(dataq);
        ATH_NODE_PWRSAVEQ_POLL(dataq, athwbuf);
        if (athwbuf == NULL) {
            ATH_NODE_PWRSAVEQ_UNLOCK(dataq);
            break;
        }
        ATH_NODE_PWRSAVEQ_DEQUEUE(dataq, athwbuf);
        ATH_NODE_PWRSAVEQ_UNLOCK(dataq);
        ath_node_pwrsaveq_complete_athwbuf(an, athwbuf);
    }

    for (;;) {
        ATH_NODE_PWRSAVEQ_LOCK(mgtq);
        ATH_NODE_PWRSAVEQ_POLL(mgtq, athwbuf);
        if (athwbuf == NULL) {
            ATH_NODE_PWRSAVEQ_UNLOCK(mgtq);
            break;
        }
        ATH_NODE_PWRSAVEQ_DEQUEUE(mgtq, athwbuf);
        ATH_NODE_PWRSAVEQ_UNLOCK(mgtq);
        ath_node_pwrsaveq_complete_athwbuf(an, athwbuf);
    }

    return qlen;
}

/*
 * Age frames on the power save queue. The aging interval is
 * 4 times the listen interval specified by the station.  This
 * number is factored into the age calculations when the frame
 * is placed on the queue.  We store ages as time differences
 * so we can check and/or adjust only the head of the list.
 * If a frame's age exceeds the threshold then discard it.
 * The number of frames discarded is returned so the caller
 * can check if it needs to adjust the tim.
 */
int ath_node_pwrsaveq_age(ath_node_t node)
{
    struct ath_node *an = ATH_NODE(node);
    struct ath_softc *sc = an->an_sc;
    int discard = 0;
    struct ath_node_pwrsaveq *dataq,*mgtq;

    dataq = ATH_NODE_PWRSAVEQ_DATAQ(an);
    mgtq  = ATH_NODE_PWRSAVEQ_MGMTQ(an);

    if ((ATH_NODE_PWRSAVEQ_QLEN(dataq) != 0) ||
        (ATH_NODE_PWRSAVEQ_QLEN(mgtq) != 0)) {
        ath_wbuf_t athwbuf;

        for (;;) {
            ATH_NODE_PWRSAVEQ_LOCK(dataq);
            ATH_NODE_PWRSAVEQ_POLL(dataq, athwbuf);
            if((athwbuf == NULL) || (wbuf_get_age(athwbuf->wbuf) >= ATH_NODE_INACT_WAIT)) {
                if (athwbuf != NULL)
                    wbuf_set_age(athwbuf->wbuf, wbuf_get_age(athwbuf->wbuf) - ATH_NODE_INACT_WAIT);
                ATH_NODE_PWRSAVEQ_UNLOCK(dataq);
                break;
            }
            DPRINTF(sc, ATH_DEBUG_PWR_SAVE,
                    "discard data frame, age %u \n", wbuf_get_age(athwbuf->wbuf));
            ATH_NODE_PWRSAVEQ_DEQUEUE(dataq, athwbuf);
            ATH_NODE_PWRSAVEQ_UNLOCK(dataq);
            ath_node_pwrsaveq_complete_athwbuf(an, athwbuf);
            discard++;
        }

        for (;;) {
            ATH_NODE_PWRSAVEQ_LOCK(mgtq);
            ATH_NODE_PWRSAVEQ_POLL(mgtq, athwbuf);
            if((athwbuf == NULL) || (wbuf_get_age(athwbuf->wbuf) >= ATH_NODE_INACT_WAIT)) {
                if (athwbuf != NULL)
                    wbuf_set_age(athwbuf->wbuf, wbuf_get_age(athwbuf->wbuf) - ATH_NODE_INACT_WAIT);
                ATH_NODE_PWRSAVEQ_UNLOCK(mgtq);
                break;
            }
            DPRINTF(sc, ATH_DEBUG_PWR_SAVE,
                    "discard mgt frame, age %u \n", wbuf_get_age(athwbuf->wbuf));
            ATH_NODE_PWRSAVEQ_DEQUEUE(mgtq, athwbuf);
            ATH_NODE_PWRSAVEQ_UNLOCK(mgtq);
            ath_node_pwrsaveq_complete_athwbuf(an, athwbuf);
            discard++;
        }

        DPRINTF(sc, ATH_DEBUG_PWR_SAVE, "discard %u frames for age \n", discard);
    }
    return discard;
}

void ath_node_pwrsaveq_get_info(ath_node_t node, void *info)
{
    struct ath_node *an = ATH_NODE(node);
    struct ath_node_pwrsaveq *dataq,*mgtq;

    dataq = ATH_NODE_PWRSAVEQ_DATAQ(an);
    mgtq  = ATH_NODE_PWRSAVEQ_MGMTQ(an);

    ATH_NODE_PWRSAVEQ_LOCK(dataq);
    ATH_NODE_PWRSAVEQ_LOCK(mgtq);
    ((ath_node_pwrsaveq_info *)info)->data_count = ATH_NODE_PWRSAVEQ_QLEN(dataq);
    ((ath_node_pwrsaveq_info *)info)->mgt_count = ATH_NODE_PWRSAVEQ_QLEN(mgtq);
    ((ath_node_pwrsaveq_info *)info)->data_len = ATH_NODE_PWRSAVEQ_BYTES(dataq);
    ((ath_node_pwrsaveq_info *)info)->mgt_len = ATH_NODE_PWRSAVEQ_BYTES(mgtq);
    ((ath_node_pwrsaveq_info *)info)->ps_frame_count = mgtq->nsq_num_ps_frames;
    ATH_NODE_PWRSAVEQ_UNLOCK(mgtq);
    ATH_NODE_PWRSAVEQ_UNLOCK(dataq);
}


void ath_node_pwrsaveq_attach(struct ath_node *an)
{
    ATH_NODE_PWRSAVEQ_INIT(&(an->an_dataq));
    ATH_NODE_PWRSAVEQ_INIT(&(an->an_mgmtq));
}

void ath_node_pwrsaveq_detach(struct ath_node *an)
{
    ATH_NODE_PWRSAVEQ_DESTROY(&(an->an_dataq));
    ATH_NODE_PWRSAVEQ_DESTROY(&(an->an_mgmtq));
}

void ath_node_pwrsaveq_set_param(ath_node_t node, u_int8_t param, u_int32_t val)
{
    struct ath_node *an = ATH_NODE(node);

    switch((enum ath_node_pwrsaveq_param)param) {
    case ATH_NODE_PWRSAVEQ_DATA_Q_LEN:
        an->an_dataq.nsq_max_len = val;
        break;
    case ATH_NODE_PWRSAVEQ_MGMT_Q_LEN:
        an->an_mgmtq.nsq_max_len = val;
        break;
    default:
        break;
    }

}

#endif /* LMAC_SUPPORT_POWERSAVE_QUEUE */


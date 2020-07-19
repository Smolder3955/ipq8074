/*
 * Copyright (c) 2011,2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.
 */
#include <ieee80211_power_priv.h>
#include <wlan_mlme_dp_dispatcher.h>

/* Rather than using this default value, customer platforms can provide a custom value for this constant. 
   Coustomer platform will use the different define value by themself */
#ifndef IEEE80211_NODEQ_MAX_LEN
#define IEEE80211_NODEQ_MAX_LEN 50
#endif

#define IEEE80211_NODE_SAVEQ_INIT(_nsq) do {             \
        spin_lock_init(&((_nsq)->nsq_lock));             \
        (_nsq)->nsq_len=0;                               \
        (_nsq)->nsq_whead=0;                             \
        (_nsq)->nsq_wtail=0;                             \
        (_nsq)->nsq_bytes = 0;                           \
        (_nsq)->nsq_max_len = IEEE80211_NODEQ_MAX_LEN;   \
        (_nsq)->nsq_num_ps_frames = 0;                   \
    } while (0)

#define IEEE80211_NODE_SAVEQ_DESTROY(_nsq) do {      \
        spin_lock_destroy(&(_nsq)->nsq_lock);        \
        (_nsq)->nsq_len=0;                                              \
        KASSERT(((_nsq)->nsq_whead == NULL), ("node powersave queue is not empty")); \
    } while (0)

#define IEEE80211_NODE_SAVEQ_DATAQ(_ni)     (&_ni->ni_dataq)
#define IEEE80211_NODE_SAVEQ_MGMTQ(_ni)     (&_ni->ni_mgmtq)
#define IEEE80211_NODE_SAVEQ_QLEN(_nsq)     (_nsq->nsq_len)
#define IEEE80211_NODE_SAVEQ_BYTES(_nsq)    (_nsq->nsq_bytes)
#define IEEE80211_NODE_SAVEQ_LOCK(_nsq)      spin_lock(&_nsq->nsq_lock)
#define IEEE80211_NODE_SAVEQ_UNLOCK(_nsq)    spin_unlock(&_nsq->nsq_lock)
#define IEEE80211_NODE_SAVEQ_FULL(_nsq)     ((_nsq)->nsq_len >= (_nsq)->nsq_max_len)

#define IEEE80211_NODE_SAVEQ_DEQUEUE(_nsq, _w) do {   \
    _w = _nsq->nsq_whead;                       \
    if (_w) {                                    \
        _nsq->nsq_whead =  wbuf_next(_w);        \
        wbuf_set_next(_w, NULL);                 \
        if ( _nsq->nsq_whead ==  NULL)           \
            _nsq->nsq_wtail =  NULL;             \
        --_nsq->nsq_len;                         \
        _nsq->nsq_bytes -= wbuf_get_pktlen(_w);   \
    }                                            \
} while (0)

#define IEEE80211_NODE_SAVEQ_ENQUEUE(_nsq, _w, _qlen, _age) do {\
    wbuf_set_next(_w, NULL); 			                 \
    if (_nsq->nsq_wtail != NULL) {                       \
        _age -= wbuf_get_age(_nsq->nsq_whead);           \
        wbuf_set_next(_nsq->nsq_wtail, _w);              \
        _nsq->nsq_wtail = _w;                            \
    } else {                                             \
        _nsq->nsq_whead =  _nsq->nsq_wtail =  _w;        \
    }                                                    \
    _qlen=++_nsq->nsq_len;                               \
    _nsq->nsq_bytes +=  wbuf_get_pktlen(_w);              \
    wbuf_set_age(_w, _age);                              \
} while (0)

/* queue frame without changing the age */
#define IEEE80211_NODE_SAVEQ_ADD(_nsq, _w) do {\
    wbuf_set_next(_w, NULL); 			                 \
    if (_nsq->nsq_wtail != NULL) {                       \
        wbuf_set_next(_nsq->nsq_wtail, _w);              \
        _nsq->nsq_wtail = _w;                            \
    } else {                                             \
        _nsq->nsq_whead =  _nsq->nsq_wtail =  _w;        \
    }                                                    \
    ++_nsq->nsq_len;                                     \
    _nsq->nsq_bytes +=  wbuf_get_pktlen(_w);              \
} while (0)

#define IEEE80211_NODE_SAVEQ_POLL(_nsq, _w)  (_w = _nsq->nsq_whead)

/*
 * Clear any frames queued on a node's power save queue.
 * The number of frames that were present is returned.
 */
int
ieee80211_node_saveq_drain(struct ieee80211_node *ni)
{
    int qlen;
    struct node_powersave_queue *dataq,*mgtq;
    wbuf_t wbuf;
    struct ieee80211_tx_status ts;

    ts.ts_flags = IEEE80211_TX_ERROR;
#ifdef ATH_SUPPORT_TxBF
    ts.ts_tstamp = 0;
    ts.ts_txbfstatus = 0;
#endif
    dataq = IEEE80211_NODE_SAVEQ_DATAQ(ni); 
    mgtq  = IEEE80211_NODE_SAVEQ_MGMTQ(ni); 

    qlen = IEEE80211_NODE_SAVEQ_QLEN(dataq);
	qlen += IEEE80211_NODE_SAVEQ_QLEN(mgtq);

    /*
     * free all the frames.
     */
    for (;;) {
        IEEE80211_NODE_SAVEQ_LOCK(dataq);
        IEEE80211_NODE_SAVEQ_POLL(dataq, wbuf);
        if (wbuf == NULL) {
            IEEE80211_NODE_SAVEQ_UNLOCK(dataq);
            break;
        }
        IEEE80211_NODE_SAVEQ_DEQUEUE(dataq, wbuf);
        IEEE80211_NODE_SAVEQ_UNLOCK(dataq);
        ieee80211_release_wbuf(ni,wbuf, &ts);
    }
    for (;;) {
        IEEE80211_NODE_SAVEQ_LOCK(mgtq);
        IEEE80211_NODE_SAVEQ_POLL(mgtq, wbuf);
        if (wbuf == NULL) {
            IEEE80211_NODE_SAVEQ_UNLOCK(mgtq);
            break;
        }
        IEEE80211_NODE_SAVEQ_DEQUEUE(mgtq, wbuf);
        IEEE80211_NODE_SAVEQ_UNLOCK(mgtq);
        ieee80211_release_wbuf(ni,wbuf, &ts);
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
int
ieee80211_node_saveq_age(struct ieee80211_node *ni)
{
    int discard = 0;
    struct node_powersave_queue *dataq,*mgtq;
    struct ieee80211vap *vap=ni->ni_vap;

#if LMAC_SUPPORT_POWERSAVE_QUEUE
    struct ieee80211com *ic = ni->ni_ic;

    if ( ic->ic_node_pwrsaveq_age && ic->ic_get_lmac_pwrsaveq_len ) {
        ic->ic_node_pwrsaveq_age(ic, ni);
        if (ic->ic_get_lmac_pwrsaveq_len(ic, ni, 0) == 0
#ifdef ATH_SWRETRY
        && ic->ic_exist_pendingfrm_tidq(ni->ni_ic, ni) != 0
#endif
        && vap->iv_set_tim != NULL) {
            vap->iv_set_tim(ni, 0, false);
        }

        return 0;
    }
#endif

    dataq = IEEE80211_NODE_SAVEQ_DATAQ(ni); 
    mgtq  = IEEE80211_NODE_SAVEQ_MGMTQ(ni); 

    /* XXX racey but good 'nuf? */
	if ((IEEE80211_NODE_SAVEQ_QLEN(dataq) != 0) ||
	    (IEEE80211_NODE_SAVEQ_QLEN(mgtq) != 0)) {
        wbuf_t wbuf;

        for (;;) {
            struct ieee80211_tx_status ts;
            ts.ts_flags = IEEE80211_TX_ERROR;
#ifdef ATH_SUPPORT_TxBF
            ts.ts_tstamp = 0;
            ts.ts_txbfstatus = 0;
#endif
            IEEE80211_NODE_SAVEQ_LOCK(dataq);
            IEEE80211_NODE_SAVEQ_POLL(dataq, wbuf);
            if ((wbuf == NULL) || (wbuf_get_age(wbuf) >= IEEE80211_INACT_WAIT)) {
                if (wbuf != NULL)
                    wbuf_set_age(wbuf, wbuf_get_age(wbuf) - IEEE80211_INACT_WAIT);
                IEEE80211_NODE_SAVEQ_UNLOCK(dataq);
                break;
            }
            IEEE80211_NOTE(vap, IEEE80211_MSG_POWER, ni,
                "discard frame, age %u \n", wbuf_get_age(wbuf));

            IEEE80211_NODE_SAVEQ_DEQUEUE(dataq, wbuf);
            IEEE80211_NODE_SAVEQ_UNLOCK(dataq);
            ieee80211_release_wbuf(ni,wbuf, &ts);
            discard++;
        }

        for (;;) {
            struct ieee80211_tx_status ts;
            ts.ts_flags = IEEE80211_TX_ERROR;
            IEEE80211_NODE_SAVEQ_LOCK(mgtq);
            IEEE80211_NODE_SAVEQ_POLL(mgtq, wbuf);
            if ((wbuf == NULL) || (wbuf_get_age(wbuf) >= IEEE80211_INACT_WAIT)) {
                if (wbuf != NULL)
                    wbuf_set_age(wbuf, wbuf_get_age(wbuf) - IEEE80211_INACT_WAIT);
                IEEE80211_NODE_SAVEQ_UNLOCK(mgtq);
                break;
            }
            IEEE80211_NOTE(vap, IEEE80211_MSG_POWER, ni,
                           "discard mgt frame, age %u \n", wbuf_get_age(wbuf));

            IEEE80211_NODE_SAVEQ_DEQUEUE(mgtq, wbuf);
            IEEE80211_NODE_SAVEQ_UNLOCK(mgtq);
            ieee80211_release_wbuf(ni,wbuf, &ts);
			discard++;
        }

        IEEE80211_NOTE(vap, IEEE80211_MSG_POWER, ni,
            "discard %u frames for age \n", discard);
#ifdef QCA_SUPPORT_CP_STATS
        WLAN_PEER_CP_STAT_ADD(ni, ps_discard, discard);
#endif
    }
    if ((IEEE80211_NODE_SAVEQ_QLEN(dataq) == 0) && 
        (IEEE80211_NODE_SAVEQ_QLEN(mgtq) == 0)) {
#ifdef ATH_SWRETRY
        /* also check whether there is frame in tid q */
        if (vap->iv_set_tim != NULL && (ni->ni_ic)->ic_exist_pendingfrm_tidq(ni->ni_ic, ni) !=0)
#else
        if (vap->iv_set_tim != NULL)
#endif
            vap->iv_set_tim(ni, 0, false);
    }
    return discard;
}

/*
 * handle queueing of frames with PS bit set.
 * if a new frame gets queued with no PS bit set then dump any
 * existing (queued) frames with PS bit set.
 * if a new frame gets queued with  PS bit set then
 * increment the ps frame count.
 */
 
static void ieee80211_node_saveq_handle_ps_frames(struct ieee80211_node *ni, wbuf_t wbuf, u_int8_t frame_type)
{
    struct node_powersave_queue *mgtq;
    wbuf_t tmpwbuf;

    mgtq  = IEEE80211_NODE_SAVEQ_MGMTQ(ni); 

    if (wbuf_is_pwrsaveframe(wbuf))  {
        /* ps frame (null (or) pspoll frame) */
        ++mgtq->nsq_num_ps_frames;
    } else {
        /* non ps frame*/
        if (mgtq->nsq_num_ps_frames) {
            struct ieee80211_tx_status ts;
            struct node_powersave_queue *tmpq, temp_q;
            tmpq = &temp_q;

            IEEE80211_NODE_SAVEQ_INIT(tmpq);
            /*
             * go through the mgt queue and dump the frames with PS=1(pspoll,null).
             * accummulate the remaining frames into temp queue.
             */
            for (;;) {
                IEEE80211_NODE_SAVEQ_DEQUEUE(mgtq, tmpwbuf);
                if (tmpwbuf == NULL) {
                    break;
                }
                if (wbuf_is_pwrsaveframe(tmpwbuf))  {
                    IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_ANY, ni,
                                   "%s pwr save q: complete the PS frame with error \n", __func__);
                    ts.ts_flags = IEEE80211_TX_ERROR;
#ifdef ATH_SUPPORT_TxBF
                    ts.ts_tstamp = 0;
                    ts.ts_txbfstatus = 0;
#endif
                    ieee80211_release_wbuf(ni,tmpwbuf, &ts);
                } else {
                    IEEE80211_NODE_SAVEQ_ADD(tmpq,tmpwbuf);
                }
            }

            mgtq->nsq_num_ps_frames = 0;
            /*
             * move all the frames from temp queue to mgmt queue.
             */
            for (;;) {
                IEEE80211_NODE_SAVEQ_DEQUEUE(tmpq, tmpwbuf);
                if (tmpwbuf == NULL) {
                    break;
                }
                IEEE80211_NODE_SAVEQ_ADD(mgtq, tmpwbuf);
            }
        }
    }
     
}
/*
 * Save an outbound packet for a node in power-save sleep state.
 * The new packet is placed on the node's saved queue, and the TIM
 * is changed, if necessary.
 */
void
ieee80211_node_saveq_queue(struct ieee80211_node *ni, wbuf_t wbuf, u_int8_t frame_type)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    int qlen, age;
    struct node_powersave_queue *dataq,*mgtq,*psq;
    struct ieee80211_tx_status ts;

#if LMAC_SUPPORT_POWERSAVE_QUEUE
    if ( ic->ic_get_lmac_pwrsaveq_len ) {
        /* mark the frame and will give to ath layer */
        wbuf_set_legacy_ps(wbuf);
        if (ic->ic_get_lmac_pwrsaveq_len(ic, ni, 0) == 0 && vap->iv_set_tim != NULL) {
            vap->iv_set_tim(ni, 1, false);
        }

        return;
    }
#endif

    dataq = IEEE80211_NODE_SAVEQ_DATAQ(ni); 
    mgtq  = IEEE80211_NODE_SAVEQ_MGMTQ(ni); 
    

    IEEE80211_NODE_SAVEQ_LOCK(dataq);
    IEEE80211_NODE_SAVEQ_LOCK(mgtq);

    if (frame_type == IEEE80211_FC0_TYPE_MGT) {
        psq = mgtq;
    } else {
        psq = dataq;
    }

    if (IEEE80211_NODE_SAVEQ_FULL(psq)) {
        IEEE80211_NODE_SAVEQ_UNLOCK(mgtq);
        IEEE80211_NODE_SAVEQ_UNLOCK(dataq);
        IEEE80211_NOTE(vap, IEEE80211_MSG_ANY, ni,
                       "%s pwr save q: overflow, drops (size %d) \n",
                       (psq == dataq) ? "data" : "mgt", 
            IEEE80211_PS_MAX_QUEUE);
#ifdef QCA_SUPPORT_CP_STATS
        WLAN_PEER_CP_STAT(ni,psq_drops);
#endif
#ifdef IEEE80211_DEBUG
        if (ieee80211_msg_dumppkts(vap))
            ieee80211_dump_pkt(ni->ni_ic->ic_pdev_obj, wtod(wbuf, caddr_t), wbuf_get_len(wbuf), -1, -1);
#endif
        ts.ts_flags = IEEE80211_TX_ERROR;
#ifdef ATH_SUPPORT_TxBF
        ts.ts_tstamp = 0;
        ts.ts_txbfstatus = 0;
#endif
        ieee80211_release_wbuf(ni,wbuf, &ts);
        return;

    }
    /*
     * special handling of frames with PS = 1.
     */
    ieee80211_node_saveq_handle_ps_frames(ni,wbuf,frame_type);

    /*
     * Tag the frame with it's expiry time and insert
     * it in the queue.  The aging interval is 4 times
     * the listen interval specified by the station. 
     * Frames that sit around too long are reclaimed
     * using this information.
     */
    age = ((ni->ni_intval * ic->ic_lintval) << 2) >> 10; /* TU -> secs */
    /*
     * Note: our aging algorithm is not working well. In fact, due to the long interval
     * when the aging algorithm is called (IEEE80211_INACT_WAIT is 15 secs), we depend on
     * the associated station node to be disassociated to clear its stale frames. However,
     * as a temporary fix, I will make sure that the age is at least greater than 
     * IEEE80211_INACT_WAIT. Otherwise, we will discard all frames in ps queue even though
     * it is just queued.
    */
    if (age < IEEE80211_INACT_WAIT) {
        IEEE80211_NOTE(vap, IEEE80211_MSG_POWER, ni,
                       "%s Note: increased age from %d to %d secs.\n",
                       __func__, age, IEEE80211_INACT_WAIT);
        age = IEEE80211_INACT_WAIT;
    }
    IEEE80211_NODE_SAVEQ_ENQUEUE(psq, wbuf, qlen, age);
    /*
     * calculate the combined queue length of management and data queues.
     */
    qlen = IEEE80211_NODE_SAVEQ_QLEN(dataq);
	qlen += IEEE80211_NODE_SAVEQ_QLEN(mgtq);

    IEEE80211_NODE_SAVEQ_UNLOCK(mgtq);
    IEEE80211_NODE_SAVEQ_UNLOCK(dataq);

    IEEE80211_NOTE(vap, IEEE80211_MSG_POWER, ni,
                   "%s pwr queue:save frame, %u now queued \n", (psq == dataq) ? "data" : "mgt" ,qlen);
    if (qlen == 1 && vap->iv_set_tim != NULL)
        vap->iv_set_tim(ni, 1, false);
}

/*
 * node saveq flush.
 */
void
ieee80211_node_saveq_flush(struct ieee80211_node *ni)
{
    struct ieee80211vap *vap = ni->ni_vap;
    wbuf_t wbuf;
    struct node_powersave_queue *dataq,*mgtq;
    int dqlen, mqlen;

#if LMAC_SUPPORT_POWERSAVE_QUEUE
    struct ieee80211com *ic = ni->ni_ic;
    if(ic->ic_node_pwrsaveq_flush) {
        ic->ic_node_pwrsaveq_flush(ic, ni);
        if (vap->iv_set_tim != NULL)
            vap->iv_set_tim(ni, 0, false);
        return;
    }
#endif

    dataq = IEEE80211_NODE_SAVEQ_DATAQ(ni); 
    mgtq  = IEEE80211_NODE_SAVEQ_MGMTQ(ni); 

    /* XXX if no stations in ps mode, flush mc frames */
    dqlen = IEEE80211_NODE_SAVEQ_QLEN(dataq);

    /*
	 * Flush queued mgmt frames.
	 */
    IEEE80211_NODE_SAVEQ_LOCK(mgtq);
	if (IEEE80211_NODE_SAVEQ_QLEN(mgtq) != 0) {
		IEEE80211_NOTE(vap, IEEE80211_MSG_POWER, ni,
		    "flush mgt ps queue, %u packets queued \n",
                       IEEE80211_NODE_SAVEQ_QLEN(mgtq));
		for (;;) {

			IEEE80211_NODE_SAVEQ_DEQUEUE(mgtq, wbuf);
			if (wbuf == NULL)
				break;
            mqlen = IEEE80211_NODE_SAVEQ_QLEN(mgtq);
			/*
			 * For a Station node (non bss node) If this is the last packet, turn off the TIM bit,
             * Set the PWR_SAV bit on every frame but the last one
             * to allow encap to test for
             * adding more MORE_DATA bit to wh.
			 */
            if ((ni != ni->ni_bss_node) && (mqlen || dqlen)) {
                wbuf_set_moredata(wbuf);
            }
            ieee80211_send_mgmt(ni->ni_vap,ni,wbuf,false);
		}
	}
    mgtq->nsq_num_ps_frames = 0;
    IEEE80211_NODE_SAVEQ_UNLOCK(mgtq);

	/*
     * Flush queued unicast frames.
     */
    IEEE80211_NODE_SAVEQ_LOCK(dataq);
    if (IEEE80211_NODE_SAVEQ_QLEN(dataq) != 0) {
        IEEE80211_NOTE(vap, IEEE80211_MSG_POWER, ni,
            "flush data ps queue, %u packets queued \n",
            IEEE80211_NODE_SAVEQ_QLEN(dataq));
        for (;;) {

            IEEE80211_NODE_SAVEQ_DEQUEUE(dataq, wbuf);
            if (wbuf == NULL)
                break;
            dqlen = IEEE80211_NODE_SAVEQ_QLEN(dataq);
            IEEE80211_NOTE(vap, IEEE80211_MSG_POWER, ni,
                           "senddata ps queue, %u packets remaining \n", dqlen);
            /*
             * For a Station node (non bss node) If this is the last packet, turn off the TIM bit,
             * Set the PWR_SAV bit on every frame but the last one
             * to allow encap to test for
             * adding more MORE_DATA bit to wh.
             */
            if ((ni != ni->ni_bss_node) && dqlen) {
                wbuf_set_moredata(wbuf);
            }
            
            wlan_vdev_send_wbuf(vap->vdev_obj, ni->peer_obj, wbuf);
        }
    }
    IEEE80211_NODE_SAVEQ_UNLOCK(dataq);

    if (vap->iv_set_tim != NULL)
	    vap->iv_set_tim(ni, 0, false);
}


/*
 * send one frme out of power save queue .
 * called in reponse to PS poll.
 * returns 1 if succesfully sends a frame. 0 other wise.
 */
int
ieee80211_node_saveq_send(struct ieee80211_node *ni, int frame_type)
{
    struct ieee80211vap *vap = ni->ni_vap;
    wbuf_t wbuf;
    int qlen;
    struct node_powersave_queue *dataq,*mgtq;

    /*
     * do not send any frames if the vap is paused.
     * wait until the vap is unpaused.
     */
    if (ieee80211_vap_is_paused(vap)) return 0;

    dataq = IEEE80211_NODE_SAVEQ_DATAQ(ni); 
    mgtq  = IEEE80211_NODE_SAVEQ_MGMTQ(ni); 

    IEEE80211_NODE_SAVEQ_LOCK(dataq);
    IEEE80211_NODE_SAVEQ_LOCK(mgtq);

    if (frame_type == IEEE80211_FC0_TYPE_MGT) {
        IEEE80211_NODE_SAVEQ_DEQUEUE(mgtq, wbuf);
        if (wbuf && wbuf_is_pwrsaveframe(wbuf))  {
            /* ps frame (null (or) pspoll frame) */
            --mgtq->nsq_num_ps_frames;
        } 
    } else {
        IEEE80211_NODE_SAVEQ_DEQUEUE(dataq, wbuf);
    }

	if (wbuf == NULL) {
        IEEE80211_NODE_SAVEQ_UNLOCK(mgtq);
        IEEE80211_NODE_SAVEQ_UNLOCK(dataq);
		return 0;
	}
        
    qlen = IEEE80211_NODE_SAVEQ_QLEN(dataq);
	qlen += IEEE80211_NODE_SAVEQ_QLEN(mgtq);

    IEEE80211_NODE_SAVEQ_UNLOCK(mgtq);
    IEEE80211_NODE_SAVEQ_UNLOCK(dataq);
	/* 
	 * If there are more packets, set the more packets bit
	 * in the packet dispatched to the station; otherwise
	 * turn off the TIM bit.
	 */
	if (qlen != 0) {
		IEEE80211_NOTE(vap, IEEE80211_MSG_POWER, ni,
		    "send packet, %u still queued \n", qlen);
        /* 
         * encap will set more data bit.
         */
         wbuf_set_moredata(wbuf);
	} else {
		IEEE80211_NOTE(vap, IEEE80211_MSG_POWER, ni,
		    "%s", "send packet, queue empty \n");
#ifdef ATH_SWRETRY
        /* also check whether there is frame in tid q */
        if (vap->iv_set_tim != NULL && (ni->ni_ic)->ic_exist_pendingfrm_tidq(ni->ni_ic, ni) !=0)
#else
		if (vap->iv_set_tim != NULL)
#endif
			vap->iv_set_tim(ni, 0, false);
	}

    if (frame_type == IEEE80211_FC0_TYPE_MGT) {
        /* send the management frame down to the ath  */
        ieee80211_send_mgmt(vap,ni,wbuf,true);
    } else {
        /* send the data down to the ath  */
        wlan_vdev_send_wbuf(vap->vdev_obj, ni->peer_obj, wbuf);
    }

    return 1;

}

/*
 * Handle station leaving bss.
 */
void
ieee80211_node_saveq_cleanup(struct ieee80211_node *ni)
{
    struct ieee80211vap *vap = ni->ni_vap;
#if LMAC_SUPPORT_POWERSAVE_QUEUE
    struct ieee80211com *ic = ni->ni_ic;
#endif

    /* vap is already deleted */
    if (vap == NULL) return;

    if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
        ieee80211_mlme_node_leave(ni);
    }
    else {
        ieee80211node_clear_flag(ni, IEEE80211_NODE_PWR_MGT);
    }

#if LMAC_SUPPORT_POWERSAVE_QUEUE
    if(ic->ic_node_pwrsaveq_drain == NULL ) {
        if ((ieee80211_node_saveq_drain(ni) != 0) && (vap->iv_set_tim != NULL))
            vap->iv_set_tim(ni, 0, false);
    } else  if ((ic->ic_node_pwrsaveq_drain(ic, ni) != 0) && (vap->iv_set_tim != NULL)) {
            vap->iv_set_tim(ni, 0, false);
    }
#else
    if ((ieee80211_node_saveq_drain(ni) != 0) && (vap->iv_set_tim != NULL))
        vap->iv_set_tim(ni, 0, false);
#endif

    IEEE80211_NOTE(vap, IEEE80211_MSG_POWER, ni,
                       "%s %u sta's in ps mode \n", __func__,vap->iv_ps_sta);
}

void ieee80211_node_saveq_get_info(struct ieee80211_node *ni, ieee80211_node_saveq_info *info)
{
    struct node_powersave_queue *dataq,*mgtq;
#if LMAC_SUPPORT_POWERSAVE_QUEUE
    struct ieee80211com *ic = ni->ni_ic;
    if( ic->ic_node_pwrsaveq_get_info ) {
        ic->ic_node_pwrsaveq_get_info(ic, ni, info);
        return;
    }
#endif

    dataq = IEEE80211_NODE_SAVEQ_DATAQ(ni); 
    mgtq  = IEEE80211_NODE_SAVEQ_MGMTQ(ni); 

    IEEE80211_NODE_SAVEQ_LOCK(dataq);
    IEEE80211_NODE_SAVEQ_LOCK(mgtq);
    info->data_count = IEEE80211_NODE_SAVEQ_QLEN(dataq);
    info->mgt_count = IEEE80211_NODE_SAVEQ_QLEN(mgtq);
    info->data_len = IEEE80211_NODE_SAVEQ_BYTES(dataq);
    info->mgt_len = IEEE80211_NODE_SAVEQ_BYTES(mgtq);
    info->ps_frame_count = mgtq->nsq_num_ps_frames; 
    IEEE80211_NODE_SAVEQ_UNLOCK(mgtq);
    IEEE80211_NODE_SAVEQ_UNLOCK(dataq);
} 

void ieee80211_node_saveq_attach(struct ieee80211_node *ni)
{
    IEEE80211_NODE_SAVEQ_INIT(&(ni->ni_dataq));
    IEEE80211_NODE_SAVEQ_INIT(&(ni->ni_mgmtq));
}

void ieee80211_node_saveq_detach(struct ieee80211_node *ni)
{
    IEEE80211_NODE_SAVEQ_DESTROY(&(ni->ni_dataq));
    IEEE80211_NODE_SAVEQ_DESTROY(&(ni->ni_mgmtq));
}

void ieee80211_node_saveq_set_param(struct ieee80211_node *ni, enum ieee80211_node_saveq_param param, u_int32_t val)
{
#if LMAC_SUPPORT_POWERSAVE_QUEUE
    struct ieee80211com *ic = ni->ni_ic;
    if(ic->ic_node_pwrsaveq_set_param) {
        ic->ic_node_pwrsaveq_set_param(ic, ni, param, val);
        return;
    }
#endif
    switch(param) {
    case IEEE80211_NODE_SAVEQ_DATA_Q_LEN:
        ni->ni_dataq.nsq_max_len = val;
        break;
    case IEEE80211_NODE_SAVEQ_MGMT_Q_LEN:
        ni->ni_mgmtq.nsq_max_len = val;
        break;
    default:
        break;
    }

}

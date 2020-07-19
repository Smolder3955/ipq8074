/*
 *
 * Copyright (c) 2011-2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 *  Copyright (c) 2008 Atheros Communications Inc.  All rights reserved.
 */

#include <wbuf.h>
#include <ieee80211_data.h>
#include <ieee80211_var.h>
#include <ieee80211_objmgr_priv.h>
#include <wlan_son_pub.h>
#include <wlan_mlme_dp_dispatcher.h>
#if 0
#include "if_upperproto.h"
#include <if_llc.h>
#include <if_athproto.h>
#include <osdep.h>
#include <ieee80211_ald.h>
#include <ieee80211_crypto.h>
#include <ieee80211_node.h>
#include <ieee80211_objmgr_priv.h>
#include "wlan_mgmt_txrx_utils_api.h"
#endif

void
ieee80211_tx_bf_completion_handler(struct ieee80211_node *ni,  struct ieee80211_tx_status *ts);
int ieee80211_vap_set_complete_buf_handler(wbuf_t wbuf,
                   ieee80211_vap_complete_buf_handler handler, void *arg)
{
    wbuf_set_complete_handler(wbuf,(void *) handler,arg);
    return 0;
}

static INLINE void
ieee80211_release_wbuf_internal(struct ieee80211_node *ni, wbuf_t wbuf, struct ieee80211_tx_status *ts )
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_frame *wh;
    int type, subtype;
    uint32_t delayed_cleanup = 0;

    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
    subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
    delayed_cleanup = (ts->ts_flags & IEEE80211_TX_ERROR_NO_SKB_FREE);
    /* Clear cleanup flag as event handlers don't understand this flag */
    ts->ts_flags &= (~IEEE80211_TX_ERROR_NO_SKB_FREE);

	if (ts->ts_flags == 0) {
	    /* Incase of udp downlink only traffic,
    	 * reload the ni_inact every time when a
	     * frame is successfully acked by station.
    	 */
	    ni->ni_inact = ni->ni_inact_reload;
	}
    /*
     * Tx post processing for non beacon frames.
     * check if the vap is valid , it could have been deleted.
     */
    if (vap) {
        ieee80211_vap_complete_buf_handler handler;
        void *arg;

        if (!(type == IEEE80211_FC0_TYPE_MGT && subtype == IEEE80211_FC0_SUBTYPE_BEACON)) {
			wlan_vdev_deliver_tx_event(ni->peer_obj, wbuf, wh, ts);
            /*
             * beamforming status indicates that a CV update is required
             */
#ifdef ATH_SUPPORT_TxBF
            /* should check status validation, not zero and TxBF mode first to avoid EV#82661*/
            if (!(ts->ts_txbfstatus & ~(TxBF_Valid_Status)) && (ts->ts_txbfstatus != 0))
                 ieee80211_tx_bf_completion_handler(ni, ts);
#endif
        }
        if (vap->iv_evtable && vap->iv_evtable->wlan_xmit_update_status) {
                vap->iv_evtable->wlan_xmit_update_status(vap->iv_ifp, wbuf, ts);
        }
        /*
         * Created callback function to be called upon completion of transmission of Channel Switch
         * Request/Response frames. Any frame type now can have a completion callback function
         * instead of only Management or NULL Data frames.
         */
         /* All the frames have the handler function */
         wbuf_get_complete_handler(wbuf,(void **)&handler,&arg);
         if (handler) {
             handler(vap,wbuf,arg,wh->i_addr1,wh->i_addr2,wh->i_addr3,ts);
         }
    } else {
		/* Modify for static analysis, if vap is NULL, don't use IEEE80211_DPRINTF */
		printk("%s vap is null node ni 0x%pK\n", __func__, ni);
    }


    /*
     * Removing this printout because under bad channel conditions it results
     * in continual printouts. If/when there's a new DPRINTF category that's
     * suitable for this printout, it can be restored and changed from MSG_ANY
     * to the new category, so it won't always print.
     */
    /*
    if (ts->ts_flags) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                      "%s Tx Status flags 0x%x \n",__func__, ts->ts_flags);
    }
    */

    /* Skip wbuf free if delayed cleanup is set. */
    if (!delayed_cleanup) {
        if(ts->ts_flags & IEEE80211_TX_FLUSH){
            wbuf_complete_any(wbuf);
        }
        else {
            wbuf_complete(wbuf);
        }
    }
}

/*
 * to be used by the UMAC internally.
 * for wbufs which do not have a reference yet and have
 * not been sent down for transmission to ath layer yet and
 * being released from internal buffers.
 */
void
ieee80211_release_wbuf(struct ieee80211_node *ni, wbuf_t wbuf, struct ieee80211_tx_status *ts)
{
    ieee80211_release_wbuf_internal(ni,wbuf,ts );
}
#ifdef ATH_SUPPORT_QUICK_KICKOUT

void ieee80211_kick_node(struct ieee80211_node *ni)
{
    struct ieee80211com *ic = ni->ni_ic;
    u_int16_t associd = 0;
    bool delayed_cleanup = false;

    /* follow what timeout_station() does */
    IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_INACT, ni,
        "station kicked out due to excessive retries (refcnt %u) associd %d\n",
        ieee80211_node_refcnt(ni), IEEE80211_AID(ni->ni_associd));

   if (ni->ni_associd != 0) {
       if(!ic->ic_is_mode_offload(ic))
           ieee80211node_set_flag(ni, IEEE80211_NODE_KICK_OUT_DEAUTH);
#if QCA_SUPPORT_SON
       else if (!son_is_steer_in_prog(ni->peer_obj)) {
           /* Node is not being steered so it is safe to send the deauth.
            * When a node is being steered, sending a deauth can interrupt
            * the association/EAPOL handshake on the new BSS as it seems some
            * STAs do not verify whether it came from the old or new BSS.
            */
#else
       else {
#endif
               delayed_cleanup = ieee80211_try_mark_node_for_delayed_cleanup(ni);
               ieee80211_send_deauth(ni, IEEE80211_REASON_DISASSOC_LOW_ACK);
            }
       }

    if (!ieee80211_try_ref_node(ni))
        return;
    associd = ni->ni_associd;
    if ( ni->ni_vap->iv_opmode == IEEE80211_M_IBSS || ni->ni_vap->iv_opmode == IEEE80211_M_STA) {
        ieee80211_sta_leave(ni);
    } else {
        if (!delayed_cleanup ||
            ieee80211node_has_extflag(ni, IEEE80211_NODE_DELAYED_CLEANUP_FAIL))
            IEEE80211_NODE_LEAVE(ni);
    }

    if(ic->ic_is_mode_offload(ic)) {
        IEEE80211_DELIVER_EVENT_MLME_DEAUTH_INDICATION(ni->ni_vap, ni->ni_macaddr, associd, IEEE80211_REASON_DISASSOC_LOW_ACK);
    }

    ieee80211_free_node(ni);

}

#endif

#if ATH_DEBUG
#define OFFCHAN_EXT_TID_NONPAUSE    19
#endif

void
ieee80211_complete_wbuf(wbuf_t wbuf, struct ieee80211_tx_status *ts )
{
    struct ieee80211_node *ni = wlan_wbuf_get_peer_node(wbuf);
    struct ieee80211_frame *wh;
    struct ieee80211_action_ba_addbaresponse *addbaresponse;
    struct ieee80211_action_ba_delba *delba_sent;
    int type, subtype;
    struct ieee80211com *ic = NULL;
    struct ieee80211_action *ia;
    u_int8_t tidno = wbuf_get_tid(wbuf);
    u_int8_t *frm;
#if UMAC_SUPPORT_WNM
    u_int32_t secured = 0 ;
#endif
#if ATH_DEBUG
    ieee80211_vap_complete_buf_handler handler;
    void *arg;
    int result = QDF_STATUS_SUCCESS;
#endif
    KASSERT((ni != NULL),("ni can not be null"));
    if(ni == NULL)
    {
       printk("%s: Error wbuf node ni is NULL, so exiting. wbuf: 0x%pK \n",__func__, wbuf);
       return;
    }

    KASSERT((ni->ni_vap != NULL),("vap can not be null"));

    ic = ni->ni_ic;

    if (ic == NULL) {
        qdf_print("%s: ic cannot be null", __func__);
        qdf_assert_always(0);
        return;
    }

    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
    subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
#if UMAC_SUPPORT_WNM
    secured = wh->i_fc[1] & IEEE80211_FC1_WEP;
#endif
#if ATH_DEBUG

    /* offchan data frame of offload path need not go through regular wbuf completion,
       since the data frames completion of offload radios will use different path
       offload mgmt completion alone use this path */
    if (ic && ic->ic_is_mode_offload(ic) && (type == IEEE80211_FC0_TYPE_DATA) && (tidno == OFFCHAN_EXT_TID_NONPAUSE)) {
        uint32_t delayed_cleanup = 0;
        delayed_cleanup = (ts->ts_flags & IEEE80211_TX_ERROR_NO_SKB_FREE);

       /*
        * Created callback function to be called upon completion of transmission of Channel Switch
        * Request/Response frames. Any frame type now can have a completion callback function
        * instead of only Management or NULL Data frames.
        */
        /* All the frames have the handler function */
        wbuf_get_complete_handler(wbuf,(void **)&handler,&arg);
        if (handler) {
            handler(ni->ni_vap,wbuf,arg,wh->i_addr1,wh->i_addr2,wh->i_addr3,ts);
        }
        if (!delayed_cleanup) {
            wbuf_complete(wbuf);
        }
        ieee80211_free_node(ni);
        return;
    }
#endif
#if ATH_SUPPORT_WIFIPOS
    if (wbuf_get_cts_frame(wbuf))
        ieee80211_cts_done(ts->ts_flags==0);

    if ((ni->ni_flags & IEEE80211_NODE_WAKEUP) && (ni->ni_flags & IEEE80211_NODE_UAPSD))
    	ni->ni_consecutive_xretries = 0 ;
#endif
    if (type == IEEE80211_FC0_TYPE_MGT && subtype == IEEE80211_FC0_SUBTYPE_ACTION) {
        frm = (u_int8_t *)&wh[1];
        ia = (struct ieee80211_action *) frm;
        if ((ia->ia_category == IEEE80211_ACTION_CAT_BA) &&
            (ia->ia_action == IEEE80211_ACTION_BA_ADDBA_RESPONSE)) {
            addbaresponse = (struct ieee80211_action_ba_addbaresponse *) frm;
            result = ic->ic_addba_resp_tx_completion(ni, addbaresponse->rs_baparamset.tid,
                                             ts->ts_flags);
            if (result)
                qdf_print("%s: Reo queue update failed", __func__);
        }
        if ((ia->ia_category == IEEE80211_ACTION_CAT_BA) &&
            (ia->ia_action == IEEE80211_ACTION_BA_DELBA)) {
            delba_sent = (struct ieee80211_action_ba_delba *) frm;
            result = ic->ic_delba_tx_completion(ni, delba_sent->dl_delbaparamset.tid,
            ts->ts_flags);
        }
    }
    /* Check SmartNet feature. Only support Windows and STA mode from now on */
    if ((ni->ni_vap) && IEEE80211_VAP_IS_SEND_80211_ENABLED(ni->ni_vap) &&
        (ni->ni_vap->iv_opmode == IEEE80211_M_STA)) {
        if (ieee80211_vap_smartnet_enable_is_set(ni->ni_vap)) {
            wbuf_tx_recovery_8021q(wbuf);
        }
    }

    ieee80211_release_wbuf_internal(ni,wbuf,ts);

#if defined(ATH_SUPPORT_QUICK_KICKOUT) || UMAC_SUPPORT_NAWDS
    /* if not bss node, check the successive tx failed counts */
    if ((ni->ni_vap) &&
        (ni->ni_vap->iv_opmode == IEEE80211_M_HOSTAP) &&
        (ni != ni->ni_vap->iv_bss)) {
        if (ts->ts_flags & IEEE80211_TX_XRETRY) {
            ni->ni_consecutive_xretries++;
#ifdef ATH_SUPPORT_QUICK_KICKOUT
            /* if the node is not a NAWDS repeater and failed count reaches
             * a pre-defined limit, kick out the node
             */
            if (((ni->ni_flags & IEEE80211_NODE_NAWDS) == 0) &&
                (ni->ni_consecutive_xretries >= ni->ni_vap->iv_sko_th) &&
				(!ieee80211_vap_wnm_is_set(ni->ni_vap))) {
                if (ni->ni_vap->iv_sko_th != 0) {
                    ni->ni_consecutive_xretries = 0;
                   ieee80211_kick_node(ni);
                }
            }
#endif
            /*
             * Force decrease the inactivity mark count of NAWDS node if
             * consecutive tx fail count reaches a pre-defined limit.
             * Stop tx to this NAWDS node until it activate again.
             */
            if (ni->ni_flags & IEEE80211_NODE_NAWDS &&
                (ni->ni_consecutive_xretries) >= 5) {
                ni->ni_inact = 1;
            }
        } else {
            if ((ni->ni_flags & IEEE80211_NODE_NAWDS) &&
                (ni->ni_flags & IEEE80211_NODE_HT)) {
                u_int16_t status;
                /*
                 * NAWDS repeater just brought up so reset the ADDBA status
                 * so that AGGR could be enabled to enhance throughput
                 * 10 is taken from ADDBA_EXCHANGE_ATTEMPTS
                 */
                if(ic && ic->ic_addba_status) {
                    ic->ic_addba_status(ni, tidno, &status);
                    if (status && (ni->ni_consecutive_xretries) >= 10) {
                        ic->ic_addba_clear(ni);
                    }
                }//addba_status NULL check
            }

            if (!(ts->ts_flags & IEEE80211_TX_ERROR)) {
                /* node alive so reset the counts */
#if UMAC_SUPPORT_WNM
                if(ni->ni_wnm != NULL) {
                    ieee80211_wnm_bssmax_updaterx(ni, secured);
                }
#endif
                ni->ni_consecutive_xretries = 0;
            }
        }
    }
#endif  /* defined(ATH_SUPPORT_QUICK_KICKOUT) || UMAC_SUPPORT_NAWDS */

#if ATH_SUPPORT_FLOWMAC_MODULE
    /* if this frame is getting completed due to lack of resources, do not
     * try to wake the queue again.
     */
    if ((ni->ni_vap) && (ts->ts_flowmac_flags & IEEE80211_TX_FLOWMAC_DONE)
            && ni->ni_vap->iv_dev_stopped
            && ni->ni_vap->iv_flowmac) {
        if (ni->ni_vap->iv_evtable->wlan_pause_queue) {
            ni->ni_vap->iv_evtable->wlan_pause_queue(
                    ni->ni_vap->iv_ifp, 0, ni->ni_vap->iv_flowmac);
            ni->ni_vap->iv_dev_stopped = 0;

        }
    }
#endif

#ifdef ATH_SUPPORT_QUICK_KICKOUT
		if ((ni->ni_vap) && (ni->ni_flags & IEEE80211_NODE_KICK_OUT_DEAUTH)
                && ieee80211_node_refcnt(ni)==1)
            /* checking node count to one to make sure that no more packets
               are buffered in hardware queue*/
        {
            struct ieee80211_node *tempni;
            u_int16_t associd;
            ieee80211node_clear_flag(ni, IEEE80211_NODE_KICK_OUT_DEAUTH);
            tempni=ieee80211_tmp_node(ni->ni_vap, ni->ni_macaddr);
            if (tempni != NULL) {
                associd = ni->ni_associd;
                IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_AUTH, "%s: sending DEAUTH to %s, sta kickout reason %d\n",
                        __func__, ether_sprintf(tempni->ni_macaddr), IEEE80211_REASON_KICK_OUT);
                ieee80211_send_deauth(tempni, IEEE80211_REASON_KICK_OUT);
                /* claim node immediately */
                wlan_objmgr_delete_node(tempni);
                IEEE80211_DELIVER_EVENT_MLME_DEAUTH_INDICATION(ni->ni_vap, ni->ni_macaddr,
                        associd, IEEE80211_REASON_KICK_OUT);
            }
        }
#endif
    ieee80211_free_node(ni);
}

/*
 * This API gets invoked from the mgmt_txrx layer
 * to inform the upper layer about Tx completion
 */
QDF_STATUS
ieee80211_mgmt_complete_wbuf(void *ctx, wbuf_t wbuf, uint32_t status, void *ts_status)
{
    struct ieee80211_tx_status *ts;
    struct ieee80211_node *ni;
    struct ieee80211vap *vap = NULL;
    struct wlan_objmgr_peer *peer;
    struct ieee80211com *ic;

    ts = (struct ieee80211_tx_status *)ts_status;

    /* mgmt txrx has passed peer as ctx */
    peer = (struct wlan_objmgr_peer *)ctx;
    if (peer) {
        ni = wlan_peer_get_mlme_ext_obj(peer);
    } else {
        qdf_print("%s[%d] peer is NULL", __func__, __LINE__);
        return QDF_STATUS_E_NULL_VALUE;
    }

    if (ni) {
        ic = ni->ni_ic;
    } else {
        qdf_print("%s[%d] ni is NULL", __func__, __LINE__);
        return QDF_STATUS_E_NULL_VALUE;
    }

    vap = ni->ni_vap;
    if (ic) {
        if (ic->ic_is_mode_offload(ic)) {
            ieee80211_complete_wbuf(wbuf, ts);
        } else {
            struct ieee80211_frame *wh;
            ieee80211_vap_complete_buf_handler handler;
            void *arg;
            wh = (struct ieee80211_frame *)wbuf_header(wbuf);
            wbuf_get_complete_handler(wbuf,(void **)&handler,&arg);
            if (handler) {
                handler(ni->ni_vap,wbuf,arg,wh->i_addr1,wh->i_addr2,wh->i_addr3,NULL);
            }
        }
    } else {
       qdf_print("%s: null ic", __func__);
        return QDF_STATUS_E_NULL_VALUE;
    }
    return QDF_STATUS_SUCCESS;
}

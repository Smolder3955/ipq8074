/*
 *
 * Copyright (c) 2011-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.
 *
 */

#include "if_athvar.h"
#include <da_uapsd.h>
#include <da_dp.h>

void wlan_prepare_qosnulldata(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
			      int ac);
#ifdef ATH_SUPPORT_UAPSD
#define UAPSD_AC_CAN_TRIGGER(_ac, _dp_peer)  \
             ((((_dp_peer)->peer_uapsd_dyn_trigena[_ac] == -1) ? ((_dp_peer)->peer_uapsd_ac_trigena[_ac]) \
                            :((_dp_peer)->peer_uapsd_dyn_trigena[_ac])) &&  wlan_peer_mlme_flag_get(_dp_peer->peer, WLAN_PEER_F_UAPSD_TRIG))
wbuf_t ath_net80211_uapsd_allocqosnullframe(struct wlan_objmgr_pdev *pdev)
{
	wbuf_t wbuf;
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);

	if (!dp_pdev) {
		qdf_print("%s:dp_pdev is NULL ", __func__);
		return NULL;
	}

	wbuf =
	    wbuf_alloc(dp_pdev->osdev, WBUF_TX_MGMT,
		       sizeof(struct ieee80211_qosframe));
	if (wbuf != NULL)
		wbuf_push(wbuf, sizeof(struct ieee80211_qosframe));

	return wbuf;
}

wbuf_t
ath_net80211_uapsd_getqosnullframe(struct wlan_objmgr_peer * peer, wbuf_t wbuf,
				   int ac)
{
	wlan_prepare_qosnulldata(peer, wbuf, ac);

	/* Add a refcnt to the node so that it remains until this QosNull frame
	   completion in ath_net80211_uapsd_retqosnullframe() */
	if (wlan_objmgr_peer_try_get_ref(peer, WLAN_MLME_SB_ID) != QDF_STATUS_SUCCESS)
		return NULL;

	return wbuf;
}

void ath_net80211_uapsd_retqosnullframe(struct wlan_objmgr_peer *peer,
					wbuf_t wbuf)
{
	/* Release the node refcnt that was acquired in ath_net80211_uapsd_getqosnullframe() */
	wlan_objmgr_peer_release_ref(peer, WLAN_MLME_SB_ID);
}

/*
 * This function is called when we have successsfully transmitted EOSP.
 * It clears the SP flag so that we are ready to accept more triggers
 * from this node.
 */
void
ath_net80211_uapsd_eospindicate(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
				int txok, int force_eosp)
{
	struct ieee80211_qosframe *qwh;
	struct dadp_peer *dp_peer = NULL;

	if (peer == NULL)
		return;
	dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer) {
		qdf_print("%s:dp_peer is NULL ", __func__);
		return;
	}

	qwh = (struct ieee80211_qosframe *)wbuf_header(wbuf);

	if ((qwh->i_fc[0] ==
	     (IEEE80211_FC0_SUBTYPE_QOS | IEEE80211_FC0_TYPE_DATA))
	    || (qwh->i_fc[0] ==
		(IEEE80211_FC0_SUBTYPE_QOS_NULL | IEEE80211_FC0_TYPE_DATA))
	    || force_eosp) {
		if (
#if ATH_SUPPORT_WIFIPOS
			   (wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_WAKEUP))
			   ||
#endif
			   (qwh->i_qos[0] & IEEE80211_QOS_EOSP) || force_eosp) {
			wlan_peer_mlme_flag_clear(peer, WLAN_PEER_F_UAPSD_SP);
			QDF_TRACE(
				       QDF_MODULE_ID_OUTPUT,
				       QDF_TRACE_LEVEL_DEBUG, "%s : End SP\n",
				       __func__);
			if (!txok)
				dp_peer->peer_stats.ps_tx_eosplost++;
		}
	}
	if ((qwh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_MGT) {
		/* clear the SP for node */
		wlan_peer_mlme_flag_clear(peer, WLAN_PEER_F_UAPSD_SP);
	}

	return;
}

/*
 * This function determines whether the received frame is a valid UAPSD trigger.
 * Called from interrupt context or DPC context depending on parameter isr_context.
 */
bool
ath_net80211_check_uapsdtrigger(struct wlan_objmgr_pdev * pdev,
				struct ieee80211_qosframe * qwh,
				u_int16_t keyix, bool isr_context)
{
	struct ath_softc_net80211 *scn = NULL;
	struct dadp_pdev *dp_pdev = NULL;
	struct wlan_objmgr_peer *peer = NULL;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct dadp_peer *dp_peer = NULL;
	int tid, ac;
	u_int16_t frame_seq;
	int queue_depth;
	bool sent_eosp = false;
	bool isapsd = false;

	dp_pdev = wlan_get_dp_pdev(pdev);
	if (!dp_pdev) {
		qdf_print("%s:dp_pdev is NULL ", __func__);
		return isapsd;
	}
	scn = dp_pdev->scn;
	/*
	 * Locate the node for sender
	 */
	IEEE80211_KEYMAP_LOCK(scn);
	peer = (keyix != HAL_RXKEYIX_INVALID) ? scn->sc_keyixmap[keyix] : NULL;
	if (peer == NULL) {
		/*
		 * All associated nodes have keyix<->ni mapping established
		 * even in OPEN/WEP modes.
		 */
		IEEE80211_KEYMAP_UNLOCK(scn);
		return isapsd;
	} else {
		if (wlan_objmgr_peer_try_get_ref(peer, WLAN_MLME_SB_ID) != QDF_STATUS_SUCCESS) {
			IEEE80211_KEYMAP_UNLOCK(scn);
			return isapsd;
		}
		IEEE80211_KEYMAP_UNLOCK(scn);
	}

	dp_peer = wlan_get_dp_peer(peer);
	vdev = wlan_peer_get_vdev(peer);

	if (!(wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_UAPSD)))
		goto end;

	if ((wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_UAPSD_SP)))
		goto end;

	/* Ignore probe request frame */
	if (((qwh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_MGT)
	    && ((qwh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
		IEEE80211_FC0_SUBTYPE_PROBE_REQ))
		goto end;
	/*
	 * Must deal with change of state here, since otherwise there would
	 * be a race (on two quick frames from STA) between this code and the
	 * tasklet where we would:
	 *   - miss a trigger on entry to PS if we're already trigger hunting
	 *   - generate spurious SP on exit (due to frame following exit frame)
	 */
	if ((((qwh->i_fc[1] & IEEE80211_FC1_PWR_MGT) == IEEE80211_FC1_PWR_MGT) ^
	     (wlan_peer_mlme_flag_get(dp_peer->peer, WLAN_PEER_F_UAPSD_TRIG))))
	{
		wlan_peer_mlme_flag_clear(peer, WLAN_PEER_F_UAPSD_SP);

		if (qwh->i_fc[1] & IEEE80211_FC1_PWR_MGT) {
			WME_UAPSD_PEER_TRIGSEQINIT(dp_peer);
			dp_peer->peer_stats.ps_uapsd_triggerenabled++;
			wlan_peer_mlme_flag_set(peer, WLAN_PEER_F_UAPSD_TRIG);
		} else {
			/*
			 * Node transitioned from UAPSD -> Active state. Flush out UAPSD frames
			 */
			dp_peer->peer_stats.ps_uapsd_active++;
			wlan_peer_mlme_flag_clear(peer, WLAN_PEER_F_UAPSD_TRIG);
			queue_depth =
			    scn->sc_ops->process_uapsd_trigger(scn->sc_dev,
							       dp_peer->an,
							       WME_UAPSD_NODE_MAXQDEPTH,
							       0, 1, &sent_eosp,
							       WME_UAPSD_NODE_MAXQDEPTH);

			if (!queue_depth &&
			    IEEE80211_PEER_UAPSD_USETIM(dp_peer)) {
				VDEV_SET_TIM(peer, 0, isr_context);
			}

		}

		goto end;
	}

	/*
	 * Check for a valid trigger frame i.e. QoS Data or QoS NULL
	 */
	if (((qwh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) !=
	     IEEE80211_FC0_TYPE_DATA) ||
	    !(qwh->i_fc[0] & IEEE80211_FC0_SUBTYPE_QOS)) {
		goto end;
	}

	if (((qwh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
	     IEEE80211_FC0_TYPE_DATA)
	    &&
	    (((qwh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
	      IEEE80211_FC0_SUBTYPE_QOS)
	     || ((qwh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) ==
		 IEEE80211_FC0_SUBTYPE_QOS_NULL))) {
		tid = qwh->i_qos[0] & IEEE80211_QOS_TID;
		ac = TID_TO_WME_AC(tid);

		isapsd = true;

		if (UAPSD_AC_CAN_TRIGGER(ac, dp_peer)) {
			/*
			 * Detect duplicate triggers and drop if so.
			 */
			frame_seq = le16toh(*(u_int16_t *) qwh->i_seq);
			if ((qwh->i_fc[1] & IEEE80211_FC1_RETRY) &&
			    frame_seq == dp_peer->ni_uapsd_trigseq[ac]) {
				dp_peer->peer_stats.ps_uapsd_duptriggers++;
				DPRINTF(scn, ATH_DEBUG_UAPSD,
					"%s : Drop duplicate trigger\n",
					__func__);
				goto end;
			}

			/*
			 * SP in progress for this node, discard trigger.
			 */
			if ((wlan_peer_mlme_flag_get
			     (peer, WLAN_PEER_F_UAPSD_SP))) {
				dp_peer->peer_stats.ps_uapsd_ignoretriggers++;
				DPRINTF(scn, ATH_DEBUG_UAPSD,
					"%s : SP in-progress; ignore trigger\n",
					__func__);
				goto end;
			}

			dp_peer->peer_stats.ps_uapsd_triggers++;

			queue_depth =
			    scn->sc_ops->process_uapsd_trigger(scn->sc_dev,
							       dp_peer->an,
							       dp_peer->
							       peer_uapsd_maxsp,
							       ac, 0,
							       &sent_eosp,
							       WME_UAPSD_NODE_MAXQDEPTH);

			if (queue_depth == -1)
				goto end;

			/* start the SP */
			wlan_peer_mlme_flag_set(peer, WLAN_PEER_F_UAPSD_SP);
			dp_peer->ni_uapsd_trigseq[ac] = frame_seq;

			DPRINTF(scn, ATH_DEBUG_UAPSD, "%s : Start SP\n",
				__func__);
#if ATH_SUPPORT_WIFIPOS
			if (wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_WAKEUP)) {
				VDEV_SET_TIM(peer, 1, isr_context);
			}
#endif

			if (!queue_depth &&
			    IEEE80211_PEER_UAPSD_USETIM(dp_peer)) {
				VDEV_SET_TIM(peer, 0, isr_context);
			}
		}
	}
end:
	wlan_objmgr_peer_release_ref(peer, WLAN_MLME_SB_ID);
	return isapsd;
}

/*
 * This function is called for each frame received on the high priority queue.
 * If the hardware has classified this frame as a UAPSD trigger, we locate the node
 * and deliver data.
 * For non-trigger frames, we check for PM transition.
 * Called from interrupt context.
 */
void
ath_net80211_uapsd_deliverdata(ieee80211_handle_t ieee,
			       struct ieee80211_qosframe *qwh,
			       u_int16_t keyix,
			       u_int8_t is_trig, bool isr_context)
{
#if 0				/*TBD_DADP: None of the DA chips support HW UAPSD detection */
	struct ieee80211com *ic = NET80211_HANDLE(ieee);
	struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
	struct ieee80211_node *ni;
	int tid, ac;
	u_int16_t frame_seq;
	int queue_depth;
	bool sent_eosp = false;

	UNREFERENCED_PARAMETER(isr_context);

	/*
	 * Locate the node for sender
	 */
	IEEE80211_KEYMAP_LOCK(scn);
	ni = (keyix != HAL_RXKEYIX_INVALID) ? scn->sc_keyixmap[keyix] : NULL;
	IEEE80211_KEYMAP_UNLOCK(scn);
	if (ni == NULL) {
		/*
		 * No key index or no entry, do a lookup
		 */
		ni = ieee80211_find_rxnode(ic,
					   (struct ieee80211_frame_min *)qwh);
		if (ni == NULL) {
			return;
		}
	} else {
		ieee80211_ref_node(ni);
	}

	if (!(ni->ni_flags & IEEE80211_NODE_UAPSD)
	    || (ni->ni_flags & IEEE80211_NODE_ATH_PAUSED))
		goto end;

	/* We cannot have a PM state change if this is a trigger frame */
	if (!is_trig) {
		/*
		 * Must deal with change of state here, since otherwise there would
		 * be a race (on two quick frames from STA) between this code and the
		 * tasklet where we would:
		 *   - miss a trigger on entry to PS if we're already trigger hunting
		 *   - generate spurious SP on exit (due to frame following exit frame)
		 */
		if ((((qwh->i_fc[1] & IEEE80211_FC1_PWR_MGT) ==
		      IEEE80211_FC1_PWR_MGT) ^ ((ni->
						 ni_flags &
						 IEEE80211_NODE_UAPSD_TRIG) ==
						IEEE80211_NODE_UAPSD_TRIG))) {
			ieee80211node_clear_flag(ni, IEEE80211_NODE_UAPSD_SP);

			if (qwh->i_fc[1] & IEEE80211_FC1_PWR_MGT) {
				WME_UAPSD_NODE_TRIGSEQINIT(ni);
				ni->ni_stats.ns_uapsd_triggerenabled++;
				ieee80211node_set_flag(ni,
						       IEEE80211_NODE_UAPSD_TRIG);
			} else {
				/*
				 * Node transitioned from UAPSD -> Active state. Flush out UAPSD frames
				 */
				ni->ni_stats.ns_uapsd_active++;
				ieee80211node_clear_flag(ni,
							 IEEE80211_NODE_UAPSD_TRIG);
				scn->sc_ops->process_uapsd_trigger(scn->sc_dev,
								   dp_peer->an,
								   WME_UAPSD_NODE_MAXQDEPTH,
								   0, 1,
								   &sent_eosp,
								   WME_UAPSD_NODE_MAXQDEPTH);
			}

			goto end;
		}
	} else {		/* Is UAPSD trigger */
		tid = qwh->i_qos[0] & IEEE80211_QOS_TID;
		ac = TID_TO_WME_AC(tid);

		if (WME_UAPSD_AC_CAN_TRIGGER(ac, ni)) {
			/*
			 * Detect duplicate triggers and drop if so.
			 */
			frame_seq = le16toh(*(u_int16_t *) qwh->i_seq);
			if ((qwh->i_fc[1] & IEEE80211_FC1_RETRY) &&
			    frame_seq == ni->ni_uapsd_trigseq[ac]) {
				ni->ni_stats.ns_uapsd_duptriggers++;
				DPRINTF(scn, ATH_DEBUG_UAPSD,
					"%s : Drop duplicate trigger\n",
					__func__);
				goto end;
			}

			/*
			 * SP in progress for this node, discard trigger.
			 */
			if (ni->ni_flags & IEEE80211_NODE_UAPSD_SP) {
				ni->ni_stats.ns_uapsd_ignoretriggers++;
				DPRINTF(scn, ATH_DEBUG_UAPSD,
					"%s : SP in-progress; ignore trigger\n",
					__func__);
				goto end;
			}

			ni->ni_stats.ns_uapsd_triggers++;

			DPRINTF(scn, ATH_DEBUG_UAPSD, "%s : Start SP\n",
				__func__);

			queue_depth =
			    scn->sc_ops->process_uapsd_trigger(scn->sc_dev,
							       dp_peer->an,
							       ni->
							       ni_uapsd_maxsp,
							       ac, 0,
							       &sent_eosp,
							       WME_UAPSD_NODE_MAXQDEPTH);
			if (queue_depth == -1)
				goto end;

			/* start the SP */
			ieee80211node_set_flag(ni, IEEE80211_NODE_UAPSD_SP);
			ni->ni_uapsd_trigseq[ac] = frame_seq;

			if (!queue_depth &&
			    (ni->ni_vap->iv_set_tim != NULL) &&
			    IEEE80211_NODE_UAPSD_USETIM(ni)) {
				ni->ni_vap->iv_set_tim(ni, 0, isr_context);
			}
		}
	}
end:
	ieee80211_free_node(ni);
	return;
#endif
}

/*
 * This function pauses/unpauses uapsd operation used by p2p protocol when the GO enters absent period.
 * Called from tasket context.
 */
void ath_net80211_uapsd_pause_control(struct wlan_objmgr_peer *peer, bool pause)
{
	/* need to disable irq */
	if (!(wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_UAPSD)))
		return;
	if (pause) {
		wlan_peer_mlme_flag_clear(peer, WLAN_PEER_F_UAPSD_SP);
	}
}

void
ath_net80211_uapsd_process_uapsd_trigger(ieee80211_handle_t ieee,
					 struct ieee80211_node *ni,
					 bool enforce_max_sp, bool * sent_eosp)
{
#if 0				/* Unused - Is this for triggering UAPSD from SW - testing purpose ??? */
	struct ieee80211com *ic = NET80211_HANDLE(ieee);
	struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

	if (enforce_max_sp) {
		scn->sc_ops->process_uapsd_trigger(scn->sc_dev,
						   dp_peer->an,
						   ni->ni_uapsd_maxsp, 0, 0,
						   sent_eosp,
						   WME_UAPSD_NODE_MAXQDEPTH);
	} else {
		scn->sc_ops->process_uapsd_trigger(scn->sc_dev,
						   dp_peer->an,
						   WME_UAPSD_NODE_MAXQDEPTH, 0,
						   1, sent_eosp,
						   WME_UAPSD_NODE_MAXQDEPTH);
	}

	return;
#endif
}

#endif /*ATH_SUPPORT_UAPSD */

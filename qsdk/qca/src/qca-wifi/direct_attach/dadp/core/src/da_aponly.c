/*
* Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
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

#include <ah.h>
#include "ath_internal.h"
#include "if_athrate.h"
#include "ratectrl.h"
#include <osdep.h>
#include <wbuf.h>
#if WLAN_SPECTRAL_ENABLE
#include "../direct_attach/lmac/spectral/spectral.h"
#endif /* WLAN_SPECTRAL_ENABLE */

#if UMAC_SUPPORT_VI_DBG
#include <ieee80211_vi_dbg.h>
#endif

#include "../direct_attach/lmac/ratectrl/ratectrl11n.h"
#include "if_athvar.h"
#include "if_athproto.h"
#include "ath_cwm.h"
#include <da_amsdu.h>
#include <da_uapsd.h>
#include "if_ath_htc.h"
#include "if_llc.h"

#include "asf_print.h"		/* asf_print_setup */

#include "qdf_mem.h"		/* qdf_mem_alloc,free */
#include "qdf_lock.h"		/* qdf_spinlock_* */
#include "qdf_types.h"		/* qdf_vprint */
#if QCA_SUPPORT_SON
#include <wlan_son_pub.h>
#endif
#include "qdf_perf.h"

#include "ath_internal.h"	/* */

#include "osif_private.h"

#if QCA_AIRTIME_FAIRNESS
#include <target_if_atf.h>
#include "ath_airtime_fairness.h"
#endif
#include "wlan_lmac_if_api.h"

#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_utils_api.h>
#include <wlan_cfg80211_ic_cp_stats.h>
#endif
#include <wlan_utility.h>
#include <da_dp.h>
#include <da_aponly.h>
#include <da_frag.h>

#if UMAC_SUPPORT_APONLY

#if ATH_SUPPORT_KEYPLUMB_WAR
#define MAX_DECRYPT_ERR_KEYPLUMB 15
#endif
#if ATH_DEBUG
extern unsigned long ath_rtscts_enable;	/* defined in ah_osdep.c  */
extern int min_buf_resv;
#endif

#if UMAC_SUPPORT_PROXY_ARP
int wlan_proxy_arp(wlan_if_t vap, wbuf_t wbuf);
int do_proxy_arp(wlan_if_t vap, qdf_nbuf_t netbuf);
#endif

#if UNIFIED_SMARTANTENNA
static inline uint32_t ath_smart_ant_txfeedback_aponly(struct ath_softc *sc,
						       struct ath_node *an,
						       struct ath_buf *bf,
						       int nBad,
						       struct ath_tx_status
						       *ts);
static inline uint32_t ath_smart_ant_rxfeedback_aponly(struct ath_softc *sc,
						       wbuf_t wbuf,
						       struct ath_rx_status
						       *rxs, uint32_t pkts);
#else
static inline uint32_t ath_smart_ant_txfeedback_aponly(struct ath_softc *sc,
						       struct ath_node *an,
						       struct ath_buf *bf,
						       int nBad,
						       struct ath_tx_status *ts)
{
	return 0;
}
#endif

#if ATH_SUPPORT_VOWEXT
#include "ratectrl11n.h"
#endif

#ifndef REMOVE_PKT_LOG
#include "pktlog.h"
extern struct ath_pktlog_funcs *g_pktlog_funcs;
#endif

#ifndef ARRAY_LEN
#define ARRAY_LEN(x) (sizeof(x)/sizeof((x)[0]))
#endif
extern void bus_dma_sync_single(void *hwdev,
				dma_addr_t dma_handle,
				size_t size, int direction);

#if ATH_SUPPORT_KEYPLUMB_WAR
extern void ath_keycache_print(ath_dev_t);
extern int ath_checkandplumb_key(ath_dev_t, ath_node_t, u_int16_t);
#endif
/*
 * To be included by ath_xmit.c so that  we can inlise some of the functions
 * for performance reasons.
 */
//copied from another file, _ht.c; we need create a common header
#define ADDBA_EXCHANGE_ATTEMPTS     10
#define ADDBA_TIMEOUT               200	/* 200 milliseconds */

/* Typical MPDU Length, used for all the rate control computations */
#define MPDU_LENGTH                 1544

#if ATH_VOW_EXT_STATS
void ath_add_ext_stats(struct ath_rx_status *rxs, wbuf_t wbuf,
		       struct ath_softc *sc, struct ath_phy_stats *phy_stats,
		       ieee80211_rx_status_t * rx_status);
#endif

static void
ieee80211_input_update_data_stats_aponly(struct wlan_objmgr_peer *peer,
					 struct wlan_vdev_mac_stats *mac_stats,
					 wbuf_t wbuf,
					 struct ieee80211_rx_status *rs,
					 u_int16_t realhdrsize);

#if ATH_WDS_SUPPORT_APONLY
static inline struct ieee80211_node *ieee80211_find_wds_node_aponly(struct
								    ieee80211_node_table
								    *nt,
								    const
								    u_int8_t *
								    macaddr);
#endif

#ifdef ATH_AMSDU

extern void
ath_amsdu_stageq_flush(struct wlan_objmgr_pdev *pdev,
		       struct ath_amsdu_tx *amsdutx);

inline int
ath_get_amsdusupported_aponly(ath_dev_t dev, ath_node_t node, int tidno)
{
	struct ath_softc *sc = ATH_DEV_TO_SC(dev);
	struct ath_node *an = ATH_NODE(node);
	ath_atx_tid_t *tid = ATH_AN_2_TID(an, tidno);

	if (sc->sc_txamsdu && an->an_avp->av_config.av_amsdu && tid->addba_exchangecomplete) {
		return (tid->addba_amsdusupported);
	}

	return (FALSE);
}

#define ATH_RATE_OUT(x)            (((x) != ATH_RATE_DUMMY_MARKER) ? (ATH_EP_RND((x), ATH_RATE_EP_MULTIPLIER)) : ATH_RATE_DUMMY_MARKER)
/*
 * If the receive phy rate is lower than the threshold or our transmit queue has at least one
 * frame to work on, we keep building the AMSDU.
 */
static inline int
ath_amsdu_sched_check_aponly(struct ath_softc_net80211 *scn, void *anode,
			     int priority)
{
	struct ath_node *an = (struct ath_node *)anode;
	if ((ATH_RATE_OUT(an->an_avgrxrate) <= 162000) ||
	    (scn->sc_ops->txq_depth(scn->sc_dev, scn->sc_ac2q[priority]) >= 1))
	{
		return 1;
	}
	return 0;
}

static inline int
ieee80211_amsdu_check_aponly(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf)
{
	if (wbuf_is_uapsd(wbuf) || wbuf_is_moredata(wbuf)) {
		return 1;
	}

	return ieee80211_8023frm_amsdu_check(wbuf);
}

static inline wbuf_t ath_amsdu_send_aponly(wbuf_t wbuf)
{
	struct wlan_objmgr_peer *peer = wbuf_get_peer(wbuf);
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct ath_softc_net80211 *scn = NULL;
	struct dadp_pdev *dp_pdev = NULL;
	struct dadp_vdev *dp_vdev = NULL;
	struct dadp_peer *dp_peer = NULL;
	wbuf_t amsdu_wbuf;
	u_int8_t tidno = wbuf_get_tid(wbuf);
	u_int32_t framelen;
	struct ath_amsdu_tx *amsdutx;
	int amsdu_deny;
	struct ieee80211_tx_status ts;

	dp_pdev = wlan_get_dp_pdev(pdev);
	if (!dp_pdev) {
		qdf_print("%s:dp_pdev is NULL ", __func__);
		return wbuf;
	}

	scn = dp_pdev->scn;

	dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		return wbuf;
	}

	dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer) {
		qdf_print("%s:dp_peer is NULL ", __func__);
		return wbuf;
	}

	/* AMSDU handling not initialized */
	if (unlikely(dp_peer->an_amsdu == NULL)) {
		return wbuf;
	}
	if (dp_vdev->vdev_fragthreshold < 2346 ||
	    !(wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE ||
	      wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE)) {
		return wbuf;
	}
	framelen = roundup(wbuf_get_pktlen(wbuf), 4);
	amsdu_deny = ieee80211_amsdu_check_aponly(vdev, wbuf);
	/* Set the tx status flags */
	ts.ts_flags = 0;
	ts.ts_retries = 0;
#ifdef ATH_SUPPORT_TxBF
	ts.ts_txbfstatus = 0;
#endif
	ATH_AMSDU_TXQ_LOCK(dp_pdev);
	amsdutx = &(dp_peer->an_amsdu->amsdutx[tidno]);
	if (amsdutx->amsdu_tx_buf) {
		/* If AMSDU staging buffer exists, we need to add this wbuf
		 * to it.
		 */
		amsdu_wbuf = amsdutx->amsdu_tx_buf;
		/*
		 * Is there enough room in the AMSDU staging buffer?
		 * Is the newly arrived wbuf larger than our amsdu limit?
		 * If not, dispatch the AMSDU and return the current wbuf.
		 */
		if ((framelen > AMSDU_MAX_SUBFRM_LEN) || amsdu_deny) {
			/* Flush the amsdu q */
			ATH_AMSDU_TXQ_UNLOCK(dp_pdev);
			ath_amsdu_stageq_flush(pdev, amsdutx);
			return wbuf;
		}
		if (ath_amsdu_sched_check_aponly
		    (scn, dp_peer->an, wbuf_get_priority(amsdu_wbuf))
		    && (AMSDU_MAX_SUBFRM_LEN + framelen +
			wbuf_get_pktlen(amsdu_wbuf)) <
		    dp_pdev->pdev_amsdu_max_size) {
			/* We are still building the AMSDU */
			ieee80211_amsdu_encap(vdev, amsdu_wbuf, wbuf, framelen,
					      0);
			ATH_AMSDU_TXQ_UNLOCK(dp_pdev);
			/* Free the tx buffer */
			wlan_complete_wbuf(wbuf, &ts);
			return NULL;
		} else {
			/*
			 * This is the last wbuf to be added to the AMSDU
			 * No pad for this frame.
			 * Return the AMSDU wbuf back.
			 */
			ieee80211_amsdu_encap(vdev, amsdu_wbuf, wbuf,
					      wbuf_get_pktlen(wbuf), 0);
			ATH_AMSDU_TXQ_UNLOCK(dp_pdev);
			/* Free the tx buffer */
			ieee80211_complete_wbuf(wbuf, &ts);
			ath_amsdu_stageq_flush(pdev, amsdutx);
			return NULL;
		}
	} else {
		/* Begin building the AMSDU */
		/* AMSDU for small frames only */
		if ((framelen > AMSDU_MAX_SUBFRM_LEN) || amsdu_deny) {
			ATH_AMSDU_TXQ_UNLOCK(dp_pdev);
			return wbuf;
		}
		amsdu_wbuf =
		    wbuf_alloc(dp_pdev->osdev, WBUF_TX_DATA,
			       AMSDU_MAX_BUFFER_SIZE);
		/* No AMSDU buffer available */
		if (amsdu_wbuf == NULL) {
			ATH_AMSDU_TXQ_UNLOCK(dp_pdev);
			return wbuf;
		}
		/* Perform 802.11 AMSDU encapsulation */
		ieee80211_amsdu_encap(vdev, amsdu_wbuf, wbuf, framelen, 1);
		/*
		 * Copy information from buffer
		 * Bump reference count for the node.
		 */
		wbuf_set_priority(amsdu_wbuf, wbuf_get_priority(wbuf));
		wbuf_set_tid(amsdu_wbuf, tidno);

		if (wlan_objmgr_peer_try_get_ref(peer, WLAN_MLME_SB_ID) != QDF_STATUS_SUCCESS) {
			wbuf_release(dp_pdev->osdev, amsdu_wbuf);
			ATH_AMSDU_TXQ_UNLOCK(dp_pdev);
			return NULL;
		}

		wbuf_set_peer(amsdu_wbuf, peer);
		wbuf_set_amsdu(amsdu_wbuf);
		amsdutx->amsdu_tx_buf = amsdu_wbuf;
		if (!amsdutx->sched) {
			amsdutx->sched = 1;
			TAILQ_INSERT_TAIL(&dp_pdev->sc_amsdu_txq, amsdutx,
					  amsdu_qelem);
		}
		ATH_AMSDU_TXQ_UNLOCK(dp_pdev);
		/* Free the tx buffer */
		wlan_complete_wbuf(wbuf, &ts);
		if (!ath_timer_is_active(&dp_pdev->sc_amsdu_flush_timer))
			ath_start_timer(&dp_pdev->sc_amsdu_flush_timer);
		return NULL;
	}
}
#endif

static INLINE void
ath_tx_set_retry_aponly(struct ath_softc *sc, struct ath_buf *bf)
{
	wbuf_t wbuf;
	struct ieee80211_frame *wh;

	__11nstats(sc, tx_retries);

	bf->bf_isretried = 1;
	bf->bf_retries++;

	wbuf = bf->bf_mpdu;
	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	wh->i_fc[1] |= IEEE80211_FC1_RETRY;
}

static inline u_int32_t ath_get_retries_num_aponly(struct ath_buf *bf,
						   u_int32_t rateindex,
						   u_int32_t retries)
{
	u_int32_t totalretries = 0;
	switch (rateindex) {
	case 0:
		totalretries = retries;
		break;
	case 1:
		totalretries = retries + bf->bf_rcs[0].tries;
		break;
	case 2:
		totalretries = retries + bf->bf_rcs[1].tries +
		    bf->bf_rcs[0].tries;
		break;
	case 3:
		totalretries = retries + bf->bf_rcs[2].tries +
		    bf->bf_rcs[1].tries + bf->bf_rcs[0].tries;
		break;
	default:
		printk("Invalid rateindex \n\r");
	}
	return totalretries;
}

/*
 * Update block ack window
 */
static inline void
ath_tx_update_baw_aponly(struct ath_softc *sc, struct ath_atx_tid *tid,
			 int seqno)
{
	int index, cindex;

	__11nstats(sc, tx_bawupdates);

	index = ATH_BA_INDEX(tid->seq_start, seqno);
	cindex = (tid->baw_head + index) & (ATH_TID_MAX_BUFS - 1);

	TX_BUF_BITMAP_CLR(tid->tx_buf_bitmap, cindex);

	while (tid->baw_head != tid->baw_tail &&
	       !TX_BUF_BITMAP_IS_SET(tid->tx_buf_bitmap, tid->baw_head)) {
		__11nstats(sc, tx_bawupdtadv);
		INCR(tid->seq_start, IEEE80211_SEQ_MAX);
		INCR(tid->baw_head, ATH_TID_MAX_BUFS);
	}
}

/*
 * queue up a dest/ac pair for tx scheduling
 * NB: must be called with txq lock held
 */
static INLINE void
ath_tx_queue_tid_aponly(struct ath_txq *txq, struct ath_atx_tid *tid)
{
	struct ath_atx_ac *ac = tid->ac;

	/*
	 * if tid is paused, hold off
	 */
	if (qdf_atomic_read(&tid->paused))
		return;

	/*
	 * add tid to ac atmost once
	 */
	if (tid->sched)
		return;

	tid->sched = AH_TRUE;
	TAILQ_INSERT_TAIL(&ac->tid_q, tid, tid_qelem);

	/*
	 * add node ac to txq atmost once
	 */
	if (ac->sched)
		return;

	ac->sched = AH_TRUE;
	TAILQ_INSERT_TAIL(&txq->axq_acq, ac, ac_qelem);
}

static void
ath_bar_tx_aponly(struct ath_softc *sc, struct ath_node *an,
		  struct ath_atx_tid *tid)
{
	__11nstats(sc, tx_bars);

	/* pause TID until BAR completes */
	ATH_TX_PAUSE_TID(sc, tid);

	if (sc->sc_ieee_ops->send_bar) {
		if (sc->sc_ieee_ops->send_bar(an->an_node, tid->tidno, tid->seq_start)) {
			/* resume tid if send bar failed. */
			ATH_TX_RESUME_TID(sc, tid);
		}
	}
}

/*
 * Completion routine of an aggregate
 */
static inline void
ath_tx_complete_aggr_rifs_aponly(struct ath_softc *sc, struct ath_txq *txq,
				 struct ath_buf *bf, ath_bufhead * bf_q,
				 struct ath_tx_status *ts, int txok)
{
	struct ath_node *an = NULL;
	struct ath_atx_tid *tid = NULL;
	struct ath_buf *bf_last = NULL;
#if ATH_SUPPORT_IQUE
	struct ath_node *tan;
#endif
	struct ath_buf *bf_next, *bf_lastq = NULL;
	ath_bufhead bf_head, bf_pending;
	u_int16_t seq_st = 0;
	u_int32_t ba[WME_BA_BMP_SIZE >> 5];
	int isaggr, txfail, txpending, sendbar = 0, needreset = 0;
	int isnodegone;
	u_int sw_retry_limit = ATH_MAX_SW_RETRIES;

#ifdef ATH_RIFS
	int isrifs = 0;
	struct ath_buf *bar_bf = NULL;
#endif

	/* defer completeion on  atleast one buffer   */
	struct _defer_completion {
		struct ath_buf *bf;
		ath_bufhead bf_head;
		u_int8_t txfail;
	} defer_completion;

	if (bf == NULL) {
		return;
	}

	OS_MEMZERO(&defer_completion, sizeof(defer_completion));

	OS_MEMZERO(ba, WME_BA_BMP_SIZE >> 3);
	an = bf->bf_node;
	isnodegone = (an->an_flags & ATH_NODE_CLEAN);
	tid = ATH_AN_2_TID(an, bf->bf_tidno);
	bf_last = bf->bf_lastbf;
	isaggr = bf->bf_isaggr;
#ifdef ATH_RIFS
	isrifs = (ATH_RIFS_SUBFRAME_FIRST == bf->bf_rifsburst_elem) ? 1 : 0;

	if (isrifs) {
		bar_bf = bf->bf_lastbf;
		ASSERT(ATH_RIFS_BAR == bar_bf->bf_rifsburst_elem);
	}

	if (likely(isaggr || isrifs))
#else
	if (likely(isaggr))
#endif
	{
#ifdef ATH_RIFS
		isrifs ? __11nstats(sc, tx_comprifs) : __11nstats(sc,
								  tx_compaggr);
#else
		__11nstats(sc, tx_compaggr);
#endif
		if (likely(txok)) {
			if (likely(ATH_DS_TX_BA(ts))) {
				/*
				 * extract starting sequence and block-ack bitmap
				 */
				seq_st = ATH_DS_BA_SEQ(ts);
				OS_MEMCPY(ba, ATH_DS_BA_BITMAP(ts),
					  WME_BA_BMP_SIZE >> 3);
			} else {
#ifdef ATH_RIFS
				isrifs ? __11nstats(sc, txrifs_babug) :
				    __11nstats(sc, txaggr_babug);
#else
				__11nstats(sc, txaggr_babug);
#endif
				DPRINTF(sc, ATH_DEBUG_TX_PROC,
					"%s: BA bit not set.\n", __func__);

				/*
				 * Owl can become deaf/mute when BA bug happens.
				 * Chip needs to be reset. See bug 32789.
				 */
				needreset = 1;
			}
		}
	}
#if ATH_SWRETRY
	/*
	 * Decrement the "swr_num_eligible_frms".
	 * This is incremented once per aggregate (in ath_txq_txqaddbuf).
	 */
	{
		struct ath_swretry_info *pInfo;
		pInfo = &an->an_swretry_info[txq->axq_qnum];
		ATH_NODE_SWRETRY_TXBUF_LOCK(an);
		pInfo->swr_num_eligible_frms--;
		if (ts->ts_status & HAL_TXERR_XRETRY) {
			DPRINTF(sc, ATH_DEBUG_SWR,
				"%s: AMPDU XRETRY swr_num_eligible_frms = %d\n",
				__func__, pInfo->swr_num_eligible_frms);
			/*
			 * Note : If it was completed with XRetry, no packets of the aggregate were sent out.
			 * Maybe the STA is asleep ?
			 */
			pInfo->swr_state_filtering = AH_TRUE;
		} else if (ts->ts_status & HAL_TXERR_FILT) {
			DPRINTF(sc, ATH_DEBUG_SWR,
				"%s: AMPDU FILT swr_num_eligible_frms = %d\n",
				__func__, pInfo->swr_num_eligible_frms);
		}
		ATH_NODE_SWRETRY_TXBUF_UNLOCK(an);
	}
#endif

	TAILQ_INIT(&bf_pending);

#ifdef ATH_RIFS
	while (bf && bf->bf_rifsburst_elem != ATH_RIFS_BAR)
#else
	while (bf)
#endif
	{
		txfail = txpending = 0;
		bf_next = bf->bf_next;

		if (ATH_BA_ISSET(ba, ATH_BA_INDEX(seq_st, bf->bf_seqno))) {
			/*
			 * transmit completion, subframe is acked by block ack
			 */
#ifdef ATH_RIFS
			isrifs ? __11nstats(sc, txrifs_compgood) :
			    __11nstats(sc, txaggr_compgood);
#else
			__11nstats(sc, txaggr_compgood);

#endif
		}
#ifdef ATH_RIFS
		else if ((!isaggr && !isrifs) && txok)
#else
		else if (!isaggr && txok)
#endif
		{
			/*
			 * transmit completion
			 */
#ifdef ATH_RIFS
			isrifs ? __11nstats(sc, tx_compnorifs) :
			    __11nstats(sc, tx_compunaggr);
#else
			__11nstats(sc, tx_compunaggr);
#endif
		} else {
			/*
			 * retry the un-acked ones
			 */
			if (ts->ts_flags & HAL_TXERR_XRETRY) {
				__11nstats(sc, tx_sf_hw_xretries);
			}
#ifdef ATH_RIFS
			isrifs ? __11nstats(sc, txrifs_compretries) :
			    __11nstats(sc, txaggr_compretries);
#else
			__11nstats(sc, txaggr_compretries);
#endif

#if ATH_SUPPORT_IQUE
			/* For the frames to be droped who block the headline of the AC_VI queue,
			 *              * these frames should not be sw-retried. So mark them as already xretried.
			 *                           */
			tan = ATH_NODE(bf->bf_node);
			if (sc->sc_ieee_ops->get_hbr_block_state(tan->an_node)
			    && TID_TO_WME_AC(bf->bf_tidno) == WME_AC_VI) {
				bf->bf_retries = sw_retry_limit;
			}
#endif

			if (likely(!tid->cleanup_inprogress && !isnodegone &&
				   !bf_last->bf_isswaborted)) {

				if ((bf->bf_retries < sw_retry_limit)
				    || (atomic_read(&sc->sc_in_reset))) {
					ath_tx_set_retry_aponly(sc, bf);

					txpending = 1;
				} else {
					__11nstats(sc, tx_xretries);
					bf->bf_isxretried = 1;
					txfail = 1;
					sendbar = tid->addba_exchangecomplete;
					DPRINTF(sc, ATH_DEBUG_TX_PROC,
						"%s drop tx frame tid %d bf_seqno %d\n",
						__func__, tid->tidno,
						bf->bf_seqno);
				}
			} else {
				/*
				 * the entire aggregate is aborted by software due to
				 * reset, channel change, node left and etc.
				 */
				if (bf_last->bf_isswaborted) {
					__11nstats(sc, txaggr_comperror);
				}

				/*
				 * cleanup in progress, just fail
				 * the un-acked sub-frames
				 */
				txfail = 1;
			}
		}

		/*
		 * Remove ath_buf's of this sub-frame from aggregate queue.
		 */
		if (unlikely(bf_next == NULL && !sc->sc_enhanceddmasupport)) {	/* last subframe in the aggregate */
			ASSERT(bf->bf_lastfrm == bf_last);

			/*
			 * The last descriptor of the last sub frame could be a holding descriptor
			 * for h/w. If that's the case, bf->bf_lastfrm won't be in the bf_q.
			 * Make sure we handle bf_q properly here.
			 */
			bf_lastq = TAILQ_LAST(bf_q, ath_bufhead_s);
			if (bf_lastq) {
				TAILQ_REMOVE_HEAD_UNTIL(bf_q, &bf_head,
							bf_lastq, bf_list);
			} else {
				/*
				 * XXX: if the last subframe only has one descriptor which is also being used as
				 * a holding descriptor. Then the ath_buf is not in the bf_q at all.
				 */
				ASSERT(TAILQ_EMPTY(bf_q));
				TAILQ_INIT(&bf_head);
			}
		} else {
			ASSERT(!TAILQ_EMPTY(bf_q));
			TAILQ_REMOVE_HEAD_UNTIL(bf_q, &bf_head, bf->bf_lastfrm,
						bf_list);
		}

#ifndef REMOVE_PKT_LOG
		/* do pktlog */
		{
			struct log_tx log_data = { 0 };
			struct ath_buf *tbf;

			TAILQ_FOREACH(tbf, &bf_head, bf_list) {
				log_data.firstds = tbf->bf_desc;
				log_data.bf = tbf;
				ath_log_txctl(sc, &log_data, 0);
			}

			if (bf->bf_next == NULL &&
			    bf_last->bf_status & ATH_BUFSTATUS_STALE) {
				log_data.firstds = bf_last->bf_desc;
				log_data.bf = bf_last;
				ath_log_txctl(sc, &log_data, 0);
			}
		}
#endif

		if (!txpending) {
			/*
			 * complete the acked-ones/xretried ones; update block-ack window
			 */
			ATH_TXQ_LOCK(txq);
			ath_tx_update_baw_aponly(sc, tid, bf->bf_seqno);
			if (unlikely((isnodegone) && (tid->cleanup_inprogress))) {
				if (tid->baw_head == tid->baw_tail) {
					tid->addba_exchangecomplete = 0;
					tid->addba_exchangeattempts = 0;
					tid->addba_exchangestatuscode =
					    IEEE80211_STATUS_UNSPECIFIED;
					/* resume the tid */
					qdf_atomic_dec(&tid->paused);
					__11nstats(sc, tx_tidresumed);
					tid->cleanup_inprogress = AH_FALSE;
				}
			}
			ATH_TXQ_UNLOCK(txq);

			if (defer_completion.bf) {
#ifdef ATH_SUPPORT_TxBF
				ath_tx_complete_buf(sc, defer_completion.bf,
						    &defer_completion.bf_head,
						    !defer_completion.txfail,
						    ts->ts_txbfstatus,
						    ts->ts_tstamp);
#else
				ath_tx_complete_buf(sc, defer_completion.bf,
						    &defer_completion.bf_head,
						    !defer_completion.txfail);
#endif
			}
			/*
			 * save this sub-frame to be completed at the end. this
			 * will keep the node referenced till the end of the function
			 * and prevent acces to the node memory after it is freed (note tid is part of node).
			 */
			defer_completion.bf = bf;
			defer_completion.txfail = txfail;
			if (!TAILQ_EMPTY(&bf_head)) {
				defer_completion.bf_head = bf_head;
				TAILQ_INIT(&bf_head);
			} else {
				TAILQ_INIT(&defer_completion.bf_head);
			}
		} else {
			/*
			 * retry the un-acked ones
			 */
			if (unlikely(!sc->sc_enhanceddmasupport)) {	/* holding descriptor support for legacy */
				/*
				 * XXX: if the last descriptor is holding descriptor, in order to requeue
				 * the frame to software queue, we need to allocate a new descriptor and
				 * copy the content of holding descriptor to it.
				 */
				if (bf->bf_next == NULL &&
				    bf_last->bf_status & ATH_BUFSTATUS_STALE) {
					struct ath_buf *tbf;
					int nmaps;

					/* allocate new descriptor */
					ATH_TXBUF_LOCK(sc);
					tbf = TAILQ_FIRST(&sc->sc_txbuf);
					if (tbf == NULL) {
						/*
						 * We are short on memory, release the wbuf
						 * and bail out.
						 * Complete the packet with status *Not* OK.
						 */
						ATH_TXBUF_UNLOCK(sc);

						ATH_TXQ_LOCK(txq);
						ath_tx_update_baw_aponly(sc,
									 tid,
									 bf->bf_seqno);
						ATH_TXQ_UNLOCK(txq);

						if (defer_completion.bf) {
#ifdef ATH_SUPPORT_TxBF
							ath_tx_complete_buf(sc,
									    defer_completion.bf,
									    &defer_completion.bf_head,
									    !defer_completion.txfail,
									    ts->ts_txbfstatus,
									    ts->ts_tstamp);
#else
							ath_tx_complete_buf(sc,
									    defer_completion.
									    bf,
									    &defer_completion.
									    bf_head,
									    !defer_completion.
									    txfail);
#endif
						}
						/*
						 * save this sub-frame to be completed later
						 * this is a  holding buffer, we do not want  to return this to
						 * the free list yet. clear the bf_head so that the ath_tx_complete_buf  will
						 * not return any thing to the sc_txbuf.
						 * also mark this subframe as an error.
						 * since the bf_head in cleared, ath_tx_complete_buf will
						 * just complete the wbuf for this subframe and will not return any
						 * ath bufs to free list.
						 */
						defer_completion.bf = bf;
						defer_completion.txfail = 1;
						TAILQ_INIT(&defer_completion.
							   bf_head);

						// At this point, bf_next is NULL: We are done with this aggregate.
						break;
					}
					TAILQ_REMOVE(&sc->sc_txbuf, tbf,
						     bf_list);
					if (tbf) {
#if ATH_TX_BUF_FLOW_CNTL
						txq->axq_num_buf_used++;
#endif
					}
					sc->sc_txbuf_free--;
					ATH_TXBUF_UNLOCK(sc);

					ATH_TXBUF_RESET(tbf, sc->sc_num_txmaps);

					/* copy descriptor content */
					tbf->bf_mpdu = bf_last->bf_mpdu;
					tbf->bf_node = bf_last->bf_node;
#ifndef REMOVE_PKT_LOG
					tbf->bf_vdata = bf_last->bf_vdata;
#endif
					for (nmaps = 0;
					     nmaps < sc->sc_num_txmaps;
					     nmaps++) {
						tbf->bf_buf_addr[nmaps] =
						    bf_last->bf_buf_addr[nmaps];
						tbf->bf_buf_len[nmaps] =
						    bf_last->bf_buf_len[nmaps];
						tbf->bf_avail_buf--;
					}
					memcpy(tbf->bf_desc, bf_last->bf_desc,
					       sc->sc_txdesclen);

					/* link it to the frame */
					if (bf_lastq) {
						ath_hal_setdesclink(sc->sc_ah,
								    bf_lastq->
								    bf_desc,
								    tbf->
								    bf_daddr);
						bf->bf_lastfrm = tbf;
						ath_hal_cleartxdesc(sc->sc_ah,
								    bf->
								    bf_lastfrm->
								    bf_desc);
					} else {
						tbf->bf_state =
						    bf_last->bf_state;
						tbf->bf_lastfrm = tbf;
						ath_hal_cleartxdesc(sc->sc_ah,
								    tbf->
								    bf_lastfrm->
								    bf_desc);

						/* copy the DMA context */
						OS_COPY_DMA_MEM_CONTEXT
						    (OS_GET_DMA_MEM_CONTEXT
						     (tbf, bf_dmacontext),
						     OS_GET_DMA_MEM_CONTEXT
						     (bf_last, bf_dmacontext));
					}
					TAILQ_INSERT_TAIL(&bf_head, tbf,
							  bf_list);
				} else {
					/*
					 * Clear descriptor status words for software retry
					 */
					ath_hal_cleartxdesc(sc->sc_ah,
							    bf->bf_lastfrm->
							    bf_desc);
				}
			}

			/*
			 * Put this buffer to the temporary pending queue to retain ordering
			 */
			TAILQ_CONCAT(&bf_pending, &bf_head, bf_list);
		}

		bf = bf_next;
	}

	/*
	 * node is already gone. no more assocication
	 * with the node. the node might have been freed
	 * any  node acces can result in panic.note tid
	 * is part of the node.
	 */
	if (isnodegone)
		goto done;

	if (unlikely(tid->cleanup_inprogress)) {
		/* check to see if we're done with cleaning the h/w queue */
		ATH_TXQ_LOCK(txq);

		if (tid->baw_head == tid->baw_tail) {
			tid->addba_exchangecomplete = 0;
			tid->addba_exchangeattempts = 0;
			tid->addba_exchangestatuscode =
			    IEEE80211_STATUS_UNSPECIFIED;

			ath_wmi_aggr_enable((ath_dev_t) sc, an, tid->tidno, 0);

			ATH_TXQ_UNLOCK(txq);

			tid->cleanup_inprogress = AH_FALSE;

			/* send buffered frames as singles */
			ATH_TX_RESUME_TID(sc, tid);
		} else {
			ATH_TXQ_UNLOCK(txq);
		}

		goto done;
	}

	if (unlikely(sendbar && !ath_vap_pause_in_progress(sc))) {
		ath_bar_tx_aponly(sc, an, tid);
	} else if (ath_vap_pause_in_progress(sc)) {
		printk("%s:txq pause is in progress\n", __func__);
	}
#ifdef ATH_RIFS
	if (unlikely(isrifs))
		ath_rifsburst_bar_buf_free(sc, bar_bf);
#endif
	/*
	 * prepend un-acked frames to the beginning of the pending frame queue
	 */
	if (!TAILQ_EMPTY(&bf_pending)) {

#ifdef ATH_RIFS
		isrifs ? __11nstats(sc, txrifs_prepends) :
		    __11nstats(sc, txaggr_prepends);
#else
		__11nstats(sc, txaggr_prepends);
#endif

		ATH_TXQ_LOCK(txq);

		TAILQ_INSERTQ_HEAD(&tid->buf_q, &bf_pending, bf_list);

		ath_tx_queue_tid_aponly(txq, tid);
		ATH_TXQ_UNLOCK(txq);

#ifdef ATH_SWRETRY
		if (!tid->an->an_tim_set &&
		    (tid->an->an_flags & ATH_NODE_PWRSAVE) &&
#if LMAC_SUPPORT_POWERSAVE_QUEUE
		    sc->sc_ath_ops.get_pwrsaveq_len(tid->an, 0) == 0 &&
#else
		    sc->sc_ieee_ops->get_pwrsaveq_len(tid->an->an_node) == 0 &&
#endif
		    !tid->cleanup_inprogress &&
		    !(an->an_flags & ATH_NODE_CLEAN)) {
			ATH_NODE_SWRETRY_TXBUF_LOCK(tid->an);
			sc->sc_ieee_ops->set_tim(tid->an->an_node, 1);
			tid->an->an_tim_set = AH_TRUE;
			ATH_NODE_SWRETRY_TXBUF_UNLOCK(tid->an);
		}
#endif
	}

done:
	/*
	 * complete the defrred buffer.
	 * at this point the associated node could be freed.
	 */
	if (defer_completion.bf) {
#ifdef ATH_SUPPORT_TxBF
		ath_tx_complete_buf(sc, defer_completion.bf,
				    &defer_completion.bf_head,
				    !defer_completion.txfail, ts->ts_txbfstatus,
				    ts->ts_tstamp);
#else
		ath_tx_complete_buf(sc, defer_completion.bf,
				    &defer_completion.bf_head,
				    !defer_completion.txfail);
#endif
	}

	return;
}

/*
 * Process completed xmit descriptors from the specified queue.
 */
int ath_tx_processq_aponly(struct ath_softc *sc, struct ath_txq *txq)
{
#define PA2DESC(_sc, _pa)	\
		((struct ath_desc*)((caddr_t)(_sc)->sc_txdma.dd_desc +	\
				((_pa) - (_sc)->sc_txdma.dd_desc_paddr)))
	struct ath_hal *ah = sc->sc_ah;
	struct ath_buf *bf, *lastbf, *bf_held = NULL;
	ath_bufhead bf_head;
	struct ath_desc *ds;
	struct ath_node *an;
	HAL_STATUS status;
	struct ath_tx_status txstat;
#ifdef ATH_SUPPORT_UAPSD
	int uapsdq = 0;
#endif
	int nacked;
	int txok, nbad = 0;
	int isrifs = 0;
#ifdef ATH_SWRETRY
	struct ieee80211_frame *wh;
#endif
#if ATH_SUPPORT_VOWEXT
	u_int8_t n_head_fail = 0;
	u_int8_t n_tail_fail = 0;
#endif

	if (unlikely(txq == sc->sc_uapsdq)) {
		DPRINTF(sc, ATH_DEBUG_UAPSD, "%s: reaping U-APSD txq\n",
			__func__);
#ifdef ATH_SUPPORT_UAPSD
		uapsdq = 1;
#endif
	}

	nacked = 0;
	for (;;) {
		ATH_TXQ_LOCK(txq);
		txq->axq_intrcnt = 0;	/* reset periodic desc intr count */
		bf = TAILQ_FIRST(&txq->axq_q);
		if (bf == NULL) {
			txq->axq_link = NULL;
			txq->axq_linkbuf = NULL;
			ATH_TXQ_UNLOCK(txq);
			break;
		}

		/*
		 * There is a race condition that DPC gets scheduled after sw writes TxE
		 * and before hw re-load the last descriptor to get the newly chained one.
		 * Software must keep the last DONE descriptor as a holding descriptor -
		 * software does so by marking it with the STALE flag.
		 */
		bf_held = NULL;
		if (bf->bf_status & ATH_BUFSTATUS_STALE) {
			bf_held = bf;
			bf = TAILQ_NEXT(bf_held, bf_list);
			if (bf == NULL) {
#if defined(ATH_SWRETRY) && defined(ATH_SWRETRY_MODIFY_DSTMASK)
				if (sc->sc_swRetryEnabled)
					txq->axq_destmask = AH_TRUE;
#endif
				ATH_TXQ_UNLOCK(txq);
				break;
			}
		}

		isrifs =
		    (ATH_RIFS_SUBFRAME_FIRST == bf->bf_rifsburst_elem) ? 1 : 0;
		lastbf = bf->bf_lastbf;
		ds = lastbf->bf_desc;	/* NB: last decriptor */
		OS_SYNC_SINGLE(sc->sc_osdev, lastbf->bf_daddr, sc->sc_txdesclen,
			       BUS_DMA_FROMDEVICE, NULL);

		status = ath_hal_txprocdesc(ah, ds);

		if (status == HAL_EINPROGRESS) {
			ATH_TXQ_UNLOCK(txq);
			break;
		}
		if (bf->bf_desc == txq->axq_lastdsWithCTS) {
			txq->axq_lastdsWithCTS = NULL;
		}
		if (ds == txq->axq_gatingds) {
			txq->axq_gatingds = NULL;
		}

		txstat = ds->ds_txstat;	/* use a local cacheable copy to improce d-cache efficiency */
		/*
		 * Remove ath_buf's of the same transmit unit from txq,
		 * however leave the last descriptor back as the holding
		 * descriptor for hw.
		 */
		lastbf->bf_status |= ATH_BUFSTATUS_STALE;
		ATH_TXQ_MOVE_HEAD_BEFORE(txq, &bf_head, lastbf, bf_list);

		if (bf->bf_isaggr) {
			txq->axq_aggr_depth--;
		}

		txok = (txstat.ts_status == 0);

		/* workaround for Hardware corrupted TX TID
		 * There are two ways we can handle this situation, either we
		 * go over a ath_reset_internal path OR
		 * corrupt the tx status in such a way that entire aggregate gets
		 * re-transmitted, taking the second approach here.
		 * Corrupt both tx_status and clear the ba bitmap.
		 */

		if (unlikely
		    (bf->bf_isaggr && txok && bf->bf_tidno != txstat.tid)) {
			txstat.ts_status |= HAL_TXERR_BADTID;
			txok = !txok;
			txstat.ba_low = txstat.ba_high = 0x0;
			DPRINTF(sc, ATH_DEBUG_XMIT,
				"%s:%d identified bad tid status (buf:desc  %d:%d)\n",
				__func__, __LINE__, bf->bf_tidno, txstat.tid);
		}
		bf->bf_tx_ratecode = ds->ds_txstat.ts_ratecode;	/* Ratecode retrieved in bf */

		ATH_TXQ_UNLOCK(txq);

#ifdef ATH_SWRETRY
		if (txok || (ath_check_swretry_req(sc, bf) == AH_FALSE)
		    || txq == sc->sc_uapsdq) {
			/* Change the status of the frame and complete
			 * this frame as normal frame
			 */
			bf->bf_status &= ~ATH_BUFSTATUS_MARKEDSWRETRY;

		} else {
			/* This frame is going through SW retry mechanism
			 */
			bf->bf_status |= ATH_BUFSTATUS_MARKEDSWRETRY;

			bf = ath_form_swretry_frm(sc, txq, &bf_head, bf);
			/* Here bf will be changed only when there is single
			 * buffer for the current frame.
			 */
			lastbf = bf->bf_lastfrm;
			ds = lastbf->bf_desc;
			OS_SYNC_SINGLE(sc->sc_osdev, lastbf->bf_daddr,
				       sc->sc_txdesclen, BUS_DMA_FROMDEVICE,
				       NULL);

			txstat = ds->ds_txstat;
		}
#endif

		/* Put the old holding descriptor to the free queue */
		if (bf_held) {
			TAILQ_REMOVE(&bf_head, bf_held, bf_list);
#ifdef ATH_SUPPORT_UAPSD
			if (bf_held->bf_qosnulleosp) {
				ATH_UAPSD_LOCK_IRQ(sc);
				TAILQ_INSERT_TAIL(&sc->sc_uapsdqnulbf, bf_held,
						  bf_list);
				sc->sc_uapsdqnuldepth--;
				ATH_UAPSD_UNLOCK_IRQ(sc);
			} else
#endif
			{
				ATH_TXBUF_LOCK(sc);
#if ATH_TX_BUF_FLOW_CNTL
				txq->axq_num_buf_used--;
#endif
				sc->sc_txbuf_free++;
				TAILQ_INSERT_TAIL(&sc->sc_txbuf, bf_held,
						  bf_list);
				ATH_TXBUF_UNLOCK(sc);
			}
		}

		an = bf->bf_node;
		if (likely(an != NULL)) {
			int noratectrl;
			noratectrl =
			    an->an_flags & (ATH_NODE_CLEAN | ATH_NODE_PWRSAVE);
#ifdef ATH_SWRETRY
			ath_tx_dec_eligible_frms(sc, bf, txq->axq_qnum,
						 &txstat);
#endif

			ath_tx_update_stats(sc, bf, txq->axq_qnum, &txstat);

			/*
			 * Hand the descriptor to the rate control algorithm
			 * if the frame wasn't dropped for filtering or sent
			 * w/o waiting for an ack.  In those cases the rssi
			 * and retry counts will be meaningless.
			 */
			if (unlikely(!bf->bf_isampdu)) {
				/*
				 * This frame is sent out as a single frame. Use hardware retry
				 * status for this frame.
				 */
				bf->bf_retries = txstat.ts_longretry;
				if (txstat.ts_status & HAL_TXERR_XRETRY) {
					__11nstats(sc, tx_sf_hw_xretries);
					bf->bf_isxretried = 1;
				}
				nbad = 0;
#if ATH_SUPPORT_VOWEXT
				n_head_fail = n_tail_fail = 0;
#endif
			} else {
				nbad =
				    ath_tx_num_badfrms(sc, bf, &txstat, txok);
#if ATH_SUPPORT_VOWEXT
				n_tail_fail = (nbad & 0xFF);
				n_head_fail = ((nbad >> 8) & 0xFF);
				nbad = ((nbad >> 16) & 0xFF);
#endif
			}

			if (likely((txstat.ts_status & HAL_TXERR_FILT) == 0 &&
				   (bf->bf_flags & HAL_TXDESC_NOACK) == 0)) {
				/*
				 * If frame was ack'd update the last rx time
				 * used to workaround phantom bmiss interrupts.
				 */
				if (txstat.ts_status == 0)
					nacked++;

				if (likely
				    (bf->bf_isdata && !noratectrl
				     && !bf->bf_useminrate)) {
					if (isrifs)
						OS_SYNC_SINGLE(sc->sc_osdev,
							       bf->bf_rifslast->
							       bf_daddr,
							       sc->sc_txdesclen,
							       BUS_DMA_FROMDEVICE,
							       NULL);

#if ATH_SUPPORT_VOWEXT
					ath_rate_tx_complete(sc, an,
							     isrifs ? bf->
							     bf_rifslast->
							     bf_desc : ds,
							     bf->bf_rcs,
							     TID_TO_WME_AC(bf->
									   bf_tidno),
							     bf->bf_nframes,
							     nbad, n_head_fail,
							     n_tail_fail,
							     ath_tx_get_rts_retrylimit
							     (sc, txq),
							     &bf->bf_pp_rcs);
#else
					ath_rate_tx_complete(sc,
							     an,
							     isrifs ? bf->
							     bf_rifslast->
							     bf_desc : ds,
							     bf->bf_rcs,
							     TID_TO_WME_AC(bf->
									   bf_tidno),
							     bf->bf_nframes,
							     nbad,
							     ath_tx_get_rts_retrylimit
							     (sc, txq),
							     &bf->bf_pp_rcs);
#endif
				}
			}
#ifdef ATH_SWRETRY
			if (CHK_SC_DEBUG(sc, ATH_DEBUG_SWR)
			    && (ds->ds_txstat.ts_status || bf->bf_isswretry)) {
				wh = (struct ieee80211_frame *)wbuf_header(bf->
									   bf_mpdu);
				DPRINTF(sc, ATH_DEBUG_SWR,
					"%s: SeqNo%d --> rate %02X, swretry %d retrycnt %d totaltries %d\n",
					__func__,
					(*(u_int16_t *) & wh->i_seq[0]) >> 4,
					ds->ds_txstat.ts_ratecode,
					(bf->bf_isswretry) ? 1 : 0,
					bf->bf_swretries,
					bf->bf_totaltries +
					ds->ds_txstat.ts_longretry +
					ds->ds_txstat.ts_shortretry);
				DPRINTF(sc, ATH_DEBUG_SWR,
					"%s: SeqNo%d --> status %08X\n",
					__func__,
					(*(u_int16_t *) & wh->i_seq[0]) >> 4,
					ds->ds_txstat.ts_status);
			}

			if (bf->bf_status & ATH_BUFSTATUS_MARKEDSWRETRY) {
				ath_tx_mpdu_resend(sc, txq, &bf_head, txstat);
				/* We have completed the buf in resend in case of
				 * failure and hence not needed and will be fatal
				 * if we fall through this
				 */
				//XXX TBD txFFDrain();
				continue;
			}
#endif

			/*
			 * Complete this transmit unit
			 *
			 * Node cannot be referenced past this point since it can be freed
			 * here.
			 */
			if (likely(bf->bf_isampdu)) {
				if (unlikely
				    (txstat.ts_flags & HAL_TX_DESC_CFG_ERR))
					__11nstats(sc, txaggr_desc_cfgerr);
				if (unlikely
				    (txstat.ts_flags & HAL_TX_DATA_UNDERRUN)) {
					__11nstats(sc, txaggr_data_urun);
				}
				if (unlikely
				    (txstat.ts_flags & HAL_TX_DELIM_UNDERRUN)) {
					__11nstats(sc, txaggr_delim_urun);

				}

				ath_tx_complete_aggr_rifs_aponly(sc, txq, bf,
								 &bf_head,
								 &txstat, txok);
			} else {
#ifndef REMOVE_PKT_LOG
				/* do pktlog */
				{
					struct log_tx log_data = { 0 };
					struct ath_buf *tbf;

					TAILQ_FOREACH(tbf, &bf_head, bf_list) {
						log_data.firstds = tbf->bf_desc;
						log_data.bf = tbf;
						ath_log_txctl(sc, &log_data, 0);
					}

					/* log the last descriptor. */
					log_data.firstds = lastbf->bf_desc;
					log_data.bf = lastbf;
					ath_log_txctl(sc, &log_data, 0);
				}
#endif

#ifdef ATH_SUPPORT_UAPSD
				if (uapsdq) {
#ifdef ATH_SUPPORT_TxBF
					ath_tx_uapsd_complete(sc, an, bf,
							      &bf_head, txok, 0,
							      0);
#else
					ath_tx_uapsd_complete(sc, an, bf,
							      &bf_head, txok);
#endif
				} else {
#ifdef ATH_SUPPORT_TxBF
					ath_tx_complete_buf(sc, bf, &bf_head,
							    txok, 0, 0);
#else
					ath_tx_complete_buf(sc, bf, &bf_head,
							    txok);
#endif

				}
#else
#ifdef ATH_SUPPORT_TxBF
				ath_tx_complete_buf(sc, bf, &bf_head, txok, 0,
						    0);
#else
				ath_tx_complete_buf(sc, bf, &bf_head, txok);
#endif
#endif
			}

#ifndef REMOVE_PKT_LOG
			/* do pktlog */
			{
				struct log_tx log_data = { 0 };
				log_data.lastds = ds;
				log_data.bf = bf;
				log_data.nbad = nbad;
				ath_log_txstatus(sc, &log_data, 0);
			}
#endif
		}

		/*
		 * schedule any pending packets if aggregation is enabled
		 */
		{
			ATH_TXQ_LOCK(txq);
			ath_txq_schedule(sc, txq);
			ATH_TXQ_UNLOCK(txq);
		}
		ds->ds_txstat = txstat;
	}
	return nacked;
}

/*
 * Deferred processing of transmit interrupt.
 */
static inline void ath_tx_tasklet_aponly(ath_dev_t dev)
{
	struct ath_softc *sc = ATH_DEV_TO_SC(dev);
	int i, nacked = 0, qdepth = 0;
	u_int32_t qcumask = ((1 << HAL_NUM_TX_QUEUES) - 1);
#if ATH_TX_POLL
	u_int32_t q_time = 0;
	int ticks = OS_GET_TICKS();
#endif
#if ATH_TX_TO_RESET_ENABLE
	u_int32_t max_q_time = 0;
#endif

	ath_hal_gettxintrtxqs(sc->sc_ah, &qcumask);

	/* Do the Beacon completion callback (if enabled) */
	if (unlikely
	    ((atomic_read(&sc->sc_has_tx_bcn_notify))
	     && (qcumask & (1 << sc->sc_bhalq)))) {
		/* Notify that a beacon has completed */
		ath_tx_bcn_notify(sc);
		/*
		 * beacon queue is not setup like a data queue and hence
		 * so for beacon queue the ATH_TXQ_SETUP will be false and
		 * ath_tx_processq will not be called fro beacon queue.
		 */
	}

	/*
	 * Process each active queue.
	 */
	ath_vap_pause_txq_use_inc(sc);
	for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {
		if (ATH_TXQ_SETUP(sc, i)) {
			if (((qcumask & (1 << i))
#if ATH_TX_POLL
			     || (sc->sc_txq[i].axq_depth &&
				 (q_time =
				  ATH_DIFF(sc->sc_txq[i].axq_lastq_tick,
					   ticks)) >
				 MSEC_TO_TICKS(ATH_TX_POLL_TIMER))
#endif
			    )) {
				nacked +=
				    ath_tx_processq_aponly(sc, &sc->sc_txq[i]);
			}
#if ATH_TX_TO_RESET_ENABLE
			if (q_time > max_q_time) {
				max_q_time = q_time;
			}
#endif
			qdepth += ath_txq_depth(sc, i);
		}
	}

	ath_vap_pause_txq_use_dec(sc);

#if ATH_TX_TO_RESET_ENABLE
	if (unlikely(max_q_time > MSEC_TO_TICKS(ATH_TXQ_MAX_TIME))) {
		DPRINTF(sc, ATH_DEBUG_RESET, "%s: timed out on TXQ \n",
			__func__);
#ifdef AR_DEBUG
		ath_dump_descriptors(sc);
#endif
		ath_internal_reset(sc);
	}
#endif
}

/*
 * To complete a chain of buffers associated a frame
 */
#ifdef ATH_SUPPORT_TxBF
static inline void
ath_tx_complete_buf_aponly(struct ath_softc *sc, struct ath_buf *bf,
			   ath_bufhead * bf_q, int txok, u_int8_t txbf_status,
			   u_int32_t tstamp)
#else
static inline void
ath_tx_complete_buf_aponly(struct ath_softc *sc, struct ath_buf *bf,
			   ath_bufhead * bf_q, int txok)
#endif
{
	wbuf_t wbuf = bf->bf_mpdu;
	ieee80211_tx_status_t tx_status;
#if ATH_TX_BUF_FLOW_CNTL
	struct ath_txq *txq = &sc->sc_txq[bf->bf_qnum];
#endif
	struct ath_node *an = bf->bf_node;
	struct ath_atx_tid *tid = ATH_AN_2_TID(an, bf->bf_tidno);
#if ATH_FRAG_TX_COMPLETE_DEFER
	struct ieee80211_frame *wh = NULL;
	bool istxfrag = false;
#endif

	if (bf->bf_isbar && tid->bar_paused) {
		tid->bar_paused--;
		ATH_TX_RESUME_TID(sc, tid);

		/* FIXME: keep the same behavior with the original code, needed? */
#ifdef ATH_SUPPORT_TxBF
		txbf_status = 0;
#endif
	}

	/*
	 * Set retry information.
	 * NB: Don't use the information in the descriptor, because the frame
	 * could be software retried.
	 */

	// Make sure wbuf is not NULL ! Potential Double Free.
	KASSERT((wbuf != NULL),
		("%s: NULL wbuf: lastbf %pK lastfrm %pK next %pK flags %x"
		 " status %x desc %pK framelen %d seq %d tid %d keytype %x",
		 __func__, bf->bf_lastbf, bf->bf_lastfrm,
		 bf->bf_next, bf->bf_flags, bf->bf_status,
		 bf->bf_desc, bf->bf_frmlen,
		 bf->bf_seqno, bf->bf_tidno, bf->bf_keytype));

#ifdef ATH_SUPPORT_TxBF
	/* bf status update* for TxBF */
	tx_status.txbf_status = txbf_status;
	tx_status.tstamp = tstamp;
#endif

#ifdef ATH_SWRETRY
	if (bf->bf_isswretry)
		tx_status.retries = bf->bf_totaltries;
	else
		tx_status.retries = bf->bf_retries;
#else
	tx_status.retries = bf->bf_retries;
#endif
	tx_status.flags = 0;
	tx_status.rateKbps = ath_ratecode_to_ratekbps(sc, bf->bf_tx_ratecode);

	if (bf->bf_state.bfs_ispaprd) {
		ath_tx_paprd_complete(sc, bf, bf_q);
		//printk("%s[%d]: ath_tx_paprd_complete called txok %d\n", __func__, __LINE__, txok);
		return;
	}
	if (unlikely(!txok)) {
		tx_status.flags |= ATH_TX_ERROR;
                __11nstats(sc, txunaggr_compretries);

		if (bf->bf_isxretried) {
			tx_status.flags |= ATH_TX_XRETRY;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
			ath_ald_update_excretry_stats(tid->ac);
#endif
		}
	} else {
		sc->sc_stats.ast_tx_bytes += wbuf_get_pktlen(bf->bf_mpdu);
		sc->sc_stats.ast_tx_packets++;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
		ath_ald_update_pktcnt(tid->ac);
#endif
	}
	/* Unmap this frame */
	wbuf_unmap_sg(sc->sc_osdev, wbuf,
		      OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));

#if ATH_FRAG_TX_COMPLETE_DEFER
	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	istxfrag = (wh->i_fc[1] & IEEE80211_FC1_MORE_FRAG) ||
	    (((le16toh(*((u_int16_t *) & (wh->i_seq[0]))) >>
	       IEEE80211_SEQ_FRAG_SHIFT) & IEEE80211_SEQ_FRAG_MASK) > 0);

	/*
	 * frag_chain is not empty but frame with new seqno coming.
	 * We complete the frag_chain in this case.
	 */
	if (tid->frag_chainhead && tid->frag_chaintail) {
		struct ieee80211_frame *prev_frag_wh;
		u_int16_t seqno, prev_frag_seqno;

		seqno = le16toh(*((u_int16_t *) & (wh->i_seq[0]))) >>
		    IEEE80211_SEQ_SEQ_SHIFT;

		prev_frag_wh =
		    (struct ieee80211_frame *)wbuf_header(tid->frag_chaintail);
		prev_frag_seqno =
		    le16toh(*((u_int16_t *) & (prev_frag_wh->i_seq[0]))) >>
		    IEEE80211_SEQ_SEQ_SHIFT;

		if (seqno != prev_frag_seqno) {
			/* complete the first frag, UMAC will go through the chain */
			sc->sc_ieee_ops->tx_complete(tid->frag_chainhead,
						     &tid->frag_tx_status, 1);
			tid->frag_chainhead = tid->frag_chaintail = NULL;
			OS_MEMZERO(&tid->frag_tx_status,
				   sizeof(ieee80211_tx_status_t));
		}
	}

	/*
	 * Defer the completion of fragments.
	 * Instead of tx_complete for each fragments, send single
	 * tx_complete for upper layer.
	 * Assume that all fragments coming consecutively.
	 */
	if (istxfrag) {
		/* chain this wbuf */
		if (!tid->frag_chaintail && !tid->frag_chainhead) {
			tid->frag_chainhead = wbuf;
			tid->frag_chaintail = tid->frag_chainhead;
			wbuf_set_next(tid->frag_chaintail, NULL);
#ifdef ATH_SUPPORT_TxBF
			tid->frag_tx_status.tstamp = tstamp;
#endif
		} else {
			ASSERT(tid->frag_chainhead && tid->frag_chaintail);

			wbuf_set_next(tid->frag_chaintail, wbuf);
			tid->frag_chaintail = wbuf;
			wbuf_set_next(tid->frag_chaintail, NULL);
		}

		/* update the tx_status for all frags */
#ifdef ATH_SUPPORT_TxBF
		tid->frag_tx_status.txbf_status |= txbf_status;
#endif
		tid->frag_tx_status.flags |= tx_status.flags;
		/* use the max retries and rateKbps of all frags */
		if (tx_status.retries > tid->frag_tx_status.retries)
			tid->frag_tx_status.retries = tx_status.retries;
		if (tx_status.rateKbps > tid->frag_tx_status.rateKbps)
			tid->frag_tx_status.rateKbps = tx_status.rateKbps;

		/* last fragment */
		if (istxfrag && !(wh->i_fc[1] & IEEE80211_FC1_MORE_FRAG)) {
			/* complete the first frag, UMAC will go through the chain */
			sc->sc_ieee_ops->tx_complete(tid->frag_chainhead,
						     &tid->frag_tx_status, 1);
			tid->frag_chainhead = tid->frag_chaintail = NULL;
			OS_MEMZERO(&tid->frag_tx_status,
				   sizeof(ieee80211_tx_status_t));
		}
	} else {
		/* complete non-frag wbuf */
		sc->sc_ieee_ops->tx_complete(wbuf, &tx_status, 0);
	}
#else
	sc->sc_ieee_ops->tx_complete(wbuf, &tx_status, 0);	/* complete this frame */
#endif
	bf->bf_mpdu = NULL;
	/*
	 * Return the list of ath_buf of this mpdu to free queue
	 */
	ATH_TXBUF_LOCK(sc);

	if (!TAILQ_EMPTY(bf_q)) {
		int num_buf = 0;
		ATH_NUM_BUF_IN_Q(&num_buf, bf_q);
#if ATH_TX_BUF_FLOW_CNTL
		txq->axq_num_buf_used -= num_buf;
#endif
		sc->sc_txbuf_free += num_buf;
		TAILQ_CONCAT(&sc->sc_txbuf, bf_q, bf_list);
	}
	ATH_TXBUF_UNLOCK(sc);
}

#if ATH_TX_COMPACT
#define  ATH_TX_EDMA_TASK(_sc)  ath_tx_edma_tasklet_compact(_sc);
#else
#define  ATH_TX_EDMA_TASK(_sc)  ath_tx_edma_tasklet_aponly(_sc);
#endif

#if ATH_TX_COMPACT

#ifdef ATH_SUPPORT_TxBF
void ath_edma_handle_txbf_complete(struct ath_softc *sc,
				   ath_bufhead * bf_headfree,
				   u_int8_t txbf_status, u_int32_t tstamp)
{

	struct ath_buf *bf;
	struct ath_node *an;

	bf = TAILQ_FIRST(bf_headfree);

	while (bf != NULL) {
		an = bf->bf_node;
		sc->sc_ieee_ops->tx_handle_txbf_complete((ieee80211_node_t) an->
							 an_node, txbf_status,
							 tstamp,
							 bf->bf_isxretried);
		bf = TAILQ_NEXT(bf, bf_list);
	}
}
#endif

#ifdef ATH_SWRETRY
int
_edma_tasklet_handleswretry(struct ath_softc *sc,
			    struct ath_buf *bf,
			    ath_bufhead * bf_head,
			    struct ath_txq *txq,
			    struct ath_tx_status ts, u_int32_t * txs_desc)
{

	if ((CHK_SC_DEBUG(sc, ATH_DEBUG_SWR)) &&
	    (ts.ts_status || bf->bf_isswretry)
	    && (bf->bf_status & ATH_BUFSTATUS_MARKEDSWRETRY)) {
		struct ieee80211_frame *wh;
		wh = (struct ieee80211_frame *)wbuf_header(bf->bf_mpdu);
		DPRINTF(sc, ATH_DEBUG_SWR,
			"%s: SeqCtrl0x%02X%02X --> status %08X rate %02X, swretry %d retrycnt %d totaltries %d\n",
			__func__, wh->i_seq[0], wh->i_seq[1], ts.ts_status,
			ts.ts_ratecode, (bf->bf_isswretry) ? 1 : 0,
			bf->bf_swretries,
			bf->bf_totaltries + ts.ts_longretry + ts.ts_shortretry);

		DPRINTF(sc, ATH_DEBUG_SWR, "%s, %s, type=0x%x, subtype=0x%x\n",
			(ts.ts_status & HAL_TXERR_FILT) !=
			0 ? "IS FILT" : "NOT FILT",
			(ts.ts_status & HAL_TXERR_XRETRY) !=
			0 ? "IS XRETRY" : "NOT XRETRY",
			wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK,
			wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
	}

	if (bf->bf_status & ATH_BUFSTATUS_MARKEDSWRETRY) {
#ifndef REMOVE_PKT_LOG
		/* do pktlog */
		{
			struct log_tx log_data = { 0 };
			struct ath_buf *tbf;

			TAILQ_FOREACH(tbf, bf_head, bf_list) {
				log_data.firstds = tbf->bf_desc;
				log_data.bf = tbf;
				ath_log_txctl(sc, &log_data, 0);
			}
		}
		{
			struct log_tx log_data = { 0 };
			log_data.lastds = &txs_desc;
			ath_log_txstatus(sc, &log_data, 0);
		}
#endif
		/* Put the sw retry frame back to tid queue */
		ath_tx_mpdu_requeue(sc, txq, bf_head, ts);

		/* We have completed the buf in resend in case of
		 * failure and hence not needed and will be fatal
		 * if we fall through this
		 */
		return 1;
	}

	return 0;
}

#endif

static INLINE int ath_frame_type(wbuf_t wbuf)
{
	struct ieee80211_frame *wh;
	int type;

	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;

	return type;
}

#ifndef ATH_RESTRICTED_TXQSCHED
static inline void
#else
static void
#endif
ath_tx_edma_tasklet_compact(ath_dev_t dev)
{
	struct ath_softc *sc = ATH_DEV_TO_SC(dev);
	int txok, qdepth = 0, nbad = 0;
	struct ath_txq *txq;
	struct ath_tx_status ts;
	HAL_STATUS status;
	ath_bufhead bf_head;
	ath_bufhead bf_headfree;
	struct ath_buf *bf;
	struct ath_node *an;
	u_int32_t txs_desc[9];
#ifdef ATH_RESTRICTED_TXQSCHED
	struct ath_txq *schedtxq = NULL;
	u_int8_t i;
#endif
#if ATH_SUPPORT_VOWEXT
	u_int8_t n_head_fail = 0;
	u_int8_t n_tail_fail = 0;
#endif
#ifdef ATH_SWRETRY
	struct ieee80211_frame *wh;
	bool istxfrag;
#endif
	int total_hw_retries = 0;
#ifdef ATH_RESTRICTED_TXQSCHED
	int txqsched = 0;
#endif

	for (;;) {

		/* hold lock while accessing tx status ring */
		ATH_TXSTATUS_LOCK(sc);
#ifndef REMOVE_PKT_LOG

		if (!sc->sc_nodebug)
			ath_hal_getrawtxdesc(sc->sc_ah, txs_desc);
#endif

/*
         * Process a completion event.
         */
		status = ath_hal_txprocdesc(sc->sc_ah, (void *)&ts);

		ATH_TXSTATUS_UNLOCK(sc);

		if (status == HAL_EINPROGRESS)
			break;

		if (unlikely(status == HAL_EIO)) {
			break;
		}

		/* Skip beacon completions */
		if (ts.queue_id == sc->sc_bhalq)
			continue;

		/* Make sure the event came from an active queue */
		ASSERT(ATH_TXQ_SETUP(sc, ts.queue_id));

		TAILQ_INIT(&bf_headfree);
		/* Get the txq for the completion event */
		txq = &sc->sc_txq[ts.queue_id];

		ATH_TXQ_LOCK(txq);

		txq->axq_intrcnt = 0;	/* reset periodic desc intr count */

#if ATH_HW_TXQ_STUCK_WAR
		txq->tx_done_stuck_count = 0;
#endif

		bf = TAILQ_FIRST(&txq->axq_fifo[txq->axq_tailindex]);

		if (unlikely(bf == NULL)) {
			DPRINTF(sc, ATH_DEBUG_XMIT,
				"%s: TXQ[%d] tailindex %d\n", __func__,
				ts.queue_id, txq->axq_tailindex);
			ATH_TXQ_UNLOCK(txq);
			return;
		}

		if (unlikely(txq == sc->sc_cabq || txq == sc->sc_uapsdq)) {
			ATH_EDMA_MCASTQ_MOVE_HEAD_UNTIL(txq, &bf_head,
							bf->bf_lastbf, bf_list);
		} else {
			ATH_EDMA_TXQ_MOVE_HEAD_UNTIL(txq, &bf_head,
						     bf->bf_lastbf, bf_list);
		}

		if (likely(bf->bf_isaggr)) {
			txq->axq_aggr_depth--;
		}
#ifdef ATH_SWRETRY
		txok = (ts.ts_status == 0);

		bf->bf_status &= ~ATH_BUFSTATUS_MARKEDSWRETRY;
		/*
		 * Only enable the sw retry when the following conditions are met:
		 * 1. transmission failure
		 * 2. swretry is enabled at this time
		 * 3. non-ampdu data frame (not under protection of BAW)
		 * 4. not for frames from UAPSD queue
		 * 5. not for fragments
		 * 6. not response to PS-Poll
		 * Otherwise, complete the TU
		 */
		if (!txok && !bf->bf_isampdu) {
			wh = (struct ieee80211_frame *)wbuf_header(bf->bf_mpdu);
			istxfrag = (wh->i_fc[1] & IEEE80211_FC1_MORE_FRAG) ||
			    (((le16toh(*((u_int16_t *) & (wh->i_seq[0]))) >>
			       IEEE80211_SEQ_FRAG_SHIFT) &
			      IEEE80211_SEQ_FRAG_MASK) > 0);

			if ((ath_check_swretry_req(sc, bf) == AH_TRUE)
			    && !bf->bf_isaggr && bf->bf_isdata
			    && txq != sc->sc_uapsdq && !istxfrag) {
				an = bf->bf_node;

				if (an != NULL &&
				    (!(an->an_flags & ATH_NODE_PWRSAVE)
				     || !atomic_read(&an->an_pspoll_pending))) {
					/* This frame is going through SW retry mechanism */
					bf->bf_status |=
					    ATH_BUFSTATUS_MARKEDSWRETRY;
					ath_hal_setdesclink(sc->sc_ah,
							    bf->bf_desc, 0);
				}
			}
		}
#endif

		ATH_TXQ_UNLOCK(txq);

		an = bf->bf_node;

#if ATH_SWRETRY
		if (bf->bf_ispspollresp) {
			ATH_NODE_SWRETRY_TXBUF_LOCK(an);
			if (an != NULL) {
				atomic_set(&an->an_pspoll_response, AH_FALSE);
			}
			bf->bf_ispspollresp = 0;
			ATH_NODE_SWRETRY_TXBUF_UNLOCK(an);
		}
#endif

		if (likely(an != NULL)) {
			int noratectrl;
			struct ath_vap *avp = an->an_avp;

			if (!sc->sc_nodebug) {
				ath_hal_gettxratecode(sc->sc_ah, bf->bf_desc,
						      (void *)&ts);
				ath_tx_update_stats(sc, bf, txq->axq_qnum, &ts);
			} else {
				ath_hal_gettxratecode(sc->sc_ah, bf->bf_desc,
						      (void *)&ts);
				ath_tx_update_minimal_stats(sc, bf, &ts);
			}
#if QCA_SUPPORT_SON
#define INST_STATS_INVALID_RSSI 0
#define INST_STATS_VALID_RSSI_MAX 127
			if (wbuf_get_bsteering(bf->bf_mpdu)) {
				if (bf->bf_isdata && !bf->bf_ismcast) {
					u_int8_t subtype = 0;
					if (sc->sc_ieee_ops->
					    bsteering_rate_update
					    && !bf->bf_useminrate) {
						u_int32_t rateKbps;
						wh = (struct ieee80211_frame *)
						    wbuf_header(bf->bf_mpdu);
						subtype =
						    wh->
						    i_fc[0] &
						    IEEE80211_FC0_SUBTYPE_MASK;
						if (sc->sc_nodebug)
							ath_hal_gettxratecode
							    (sc->sc_ah,
							     bf->bf_desc,
							     (void *)&ts);
						rateKbps =
						    ath_get_ratekbps_from_ratecode
						    (sc, ts.ts_ratecode,
						     IS_HT_RATE(ts.
								ts_ratecode) ?
						     bf->bf_rcs[ts.
								ts_rateindex].
						     flags : 0);
						sc->sc_ieee_ops->
						    bsteering_rate_update(sc->
									  sc_ieee,
									  ((struct ieee80211_frame *)wbuf_header(bf->bf_mpdu))->i_addr1, (ts.ts_status == 0), rateKbps);
					}
					if ((subtype ==
					     IEEE80211_FC0_SUBTYPE_NODATA)
					    || (subtype ==
						IEEE80211_FC0_SUBTYPE_QOS_NULL))
					{
						if (sc->sc_ieee_ops->
						    bsteering_rssi_update
						    && (ts.ts_rssi >
							INST_STATS_INVALID_RSSI)
						    && (ts.ts_rssi <
							INST_STATS_VALID_RSSI_MAX))
							sc->sc_ieee_ops->
							    bsteering_rssi_update
							    (sc->sc_ieee,
							     ((struct
							       ieee80211_frame
							       *)
							      wbuf_header(bf->
									  bf_mpdu))->
							     i_addr1,
							     (ts.ts_status ==
							      0), ts.ts_rssi,
							     subtype);
					}
				}
			}
#undef INST_STATS_INVALID_RSSI
#undef INST_STATS_VALID_RSSI_MAX
#endif
#ifdef ATH_SWRETRY
			ath_tx_dec_eligible_frms(sc, bf, txq->axq_qnum, &ts);
#endif
			noratectrl =
			    an->an_flags & (ATH_NODE_CLEAN | ATH_NODE_PWRSAVE);
			OS_SYNC_SINGLE(sc->sc_osdev, bf->bf_daddr,
				       sc->sc_txdesclen, BUS_DMA_FROMDEVICE,
				       NULL);

			txok = (ts.ts_status == 0);

#ifdef ATH_SWRETRY
			if ((ts.ts_status || bf->bf_isswretry)
			    && (bf->bf_status & ATH_BUFSTATUS_MARKEDSWRETRY))
				if (_edma_tasklet_handleswretry
				    (sc, bf, &bf_head, txq, ts, txs_desc))
					/*Its not good pass whole ts structure as an argument but the ampdu_reque code needs in that format */
					continue;
#endif

			/*
			 * Complete this transmit unit
			 *
			 * Node cannot be referenced past this point since it can be freed
			 * here.
			 */
			if (likely(bf->bf_isampdu)) {

#if ATH_SUPPORT_VOWEXT
				nbad = ath_tx_num_badfrms(sc, bf, &ts, txok);
				n_tail_fail = (nbad & 0xFF);
				n_head_fail = ((nbad >> 8) & 0xFF);
				nbad = ((nbad >> 16) & 0xFF);
#endif

#if QCA_AIRTIME_FAIRNESS
				if (sc->sc_atf_enable) {
					ath_atf_node_airtime_consumed(sc, bf,
								      &ts,
								      txok);
				}
#endif
				total_hw_retries =
				    ath_get_retries_num_aponly(bf,
							       ts.ts_rateindex,
							       ts.ts_longretry);
				avp->av_stats.av_retry_count +=
				    bf->bf_nframes * total_hw_retries;
				if (txok && total_hw_retries)
					avp->av_stats.av_ack_failures +=
					    total_hw_retries - 1;
				else
					avp->av_stats.av_ack_failures +=
					    total_hw_retries;

#if !ATH_SUPPORT_VOWEXT
				/* if ATH_SUPPORT_VOWEXT is not supported update the nbad here */
				nbad =
				    ath_tx_complete_aggr_compact(sc, txq, bf,
								 &bf_head, &ts,
								 txok,
								 &bf_headfree);
#else
				ath_tx_complete_aggr_compact(sc, txq, bf,
							     &bf_head, &ts,
							     txok,
							     &bf_headfree);
#endif
			} else {
				int i;
#ifndef REMOVE_PKT_LOG
				/* do pktlog */

				if (!sc->sc_nodebug) {
					struct log_tx log_data = { 0 };
					struct ath_buf *tbf;

					TAILQ_FOREACH(tbf, &bf_head, bf_list) {
						log_data.firstds = tbf->bf_desc;
						log_data.bf = tbf;
						ath_log_txctl(sc, &log_data, 0);
					}
				}
#endif
				/* Account for all HW retries for this frame */
				for (i = 0; i < ts.ts_rateindex; i++) {
					bf->bf_retries += bf->bf_rcs[i].tries;
				}
				bf->bf_retries = ts.ts_longretry;
				avp->av_stats.av_retry_count += bf->bf_retries;
				if (txok && bf->bf_retries)
					avp->av_stats.av_ack_failures +=
					    bf->bf_retries - 1;
				else
					avp->av_stats.av_ack_failures +=
					    bf->bf_retries;

				if (ts.ts_status & HAL_TXERR_XRETRY) {
					avp->av_stats.av_tx_xretries++;
					bf->bf_isxretried = 1;
					__11nstats(sc, tx_sf_hw_xretries);
				} else if (bf->bf_retries) {
					avp->av_stats.av_tx_retries++;
					if (bf->bf_retries > 1)
						avp->av_stats.av_tx_mretries++;
				}

				nbad = 0;
#if QCA_AIRTIME_FAIRNESS
				if (sc->sc_atf_enable) {
					ath_atf_node_airtime_consumed(sc, bf,
								      &ts,
								      txok);
				}
#endif
/*
                if ((ts.ts_flags & HAL_TX_FAST_TS) && ((bf->bf_retries > 0) || (bf->bf_isxretried == 1)) ) {
                    printk("%s[%d]: RT: %d, XT: %x \n", __func__, __LINE__,
                                bf->bf_retries, bf->bf_isxretried);
                }
*/
#if ATH_SUPPORT_WIFIPOS
				if (bf->bf_mpdu != NULL) {
					if (wbuf_is_keepalive(bf->bf_mpdu)) {
						u_int8_t mac_addr[ETH_ALEN],
						    status;
						wbuf_t wbuf;
						wbuf = bf->bf_mpdu;
						status = 0;
						status =
						    (ts.ts_status == 0) ? 1 : 0;
						memcpy(&mac_addr,
						       ((struct ieee80211_frame
							 *)wbuf_header(wbuf))->
						       i_addr1, ETH_ALEN);
						sc->sc_ieee_ops->
						    update_ka_done(mac_addr,
								   status);
					}
				}

				if ((ts.ts_flags & HAL_TX_FAST_TS)) {
					ieee80211_wifiposdesc_t wifiposdata;
					u_int32_t retries;
					wbuf_t wbuf;
					ieee80211_wifipos_reqdata_t *req_data;
					u_int32_t wifipos_req_id;
					u_int8_t *req_mac_addr;

					memset(&wifiposdata, 0,
					       sizeof(wifiposdata));
#ifdef ATH_SWRETRY
					if (bf->bf_isswretry)
						retries = bf->bf_totaltries;
					else
						retries = bf->bf_retries;
#else
					retries = bf->bf_retries;
#endif
					wbuf = bf->bf_mpdu;
					if (wbuf != NULL) {
						req_data =
						    (ieee80211_wifipos_reqdata_t
						     *) wbuf_get_wifipos(wbuf);
						if (req_data != NULL) {
							wifiposdata.tod =
							    ts.ts_tstamp;
							wifipos_req_id =
							    wbuf_get_wifipos_req_id
							    (wbuf);
							req_mac_addr =
							    ((struct
							      ieee80211_frame *)
							     wbuf_header
							     (wbuf))->i_addr1;
							memcpy(wifiposdata.
							       sta_mac_addr,
							       req_mac_addr,
							       ETH_ALEN);
							wifiposdata.request_id =
							    wifipos_req_id;
							if (ts.ts_status == 0) {
								wifiposdata.
								    flags |=
								    ATH_WIFIPOS_TX_STATUS;
							}
							wifiposdata.retries =
							    retries;
							wifiposdata.flags |=
							    ATH_WIFIPOS_TX_UPDATE;
							/* QUIPS -342 */
//                    if (wifiposdata.flags & ATH_WIFIPOS_TX_STATUS)
							sc->sc_ieee_ops->
							    update_wifipos_stats
							    (&wifiposdata);
						}
					}
				}
#endif

#ifdef ATH_SUPPORT_UAPSD
				if (txq == sc->sc_uapsdq) {
#ifdef ATH_SUPPORT_TxBF
					ath_tx_uapsd_complete(sc, an, bf,
							      &bf_head, txok,
							      ts.ts_txbfstatus,
							      ts.ts_tstamp);
#else
					ath_tx_uapsd_complete(sc, an, bf,
							      &bf_head, txok);
#endif
				} else
#endif
				{
					TAILQ_INSERT_TAIL(&bf_headfree, bf,
							  bf_list);
				}
			}

			if (!noratectrl
			    && likely((ts.ts_status & HAL_TXERR_FILT) == 0
				      && (bf->bf_flags & HAL_TXDESC_NOACK) ==
				      0)) {

				if (likely
				    (bf->bf_isdata && !bf->bf_useminrate
				     && !noratectrl)) {
#if ATH_SUPPORT_VOWEXT
					/* FIXME do not care Ospre related issues as on today, keep
					   this pending until we get to that
					 */
					ath_rate_tx_complete_11n(sc,
								 an,
								 &ts,
								 bf->bf_rcs,
								 TID_TO_WME_AC
								 (bf->bf_tidno),
								 bf->bf_nframes,
								 nbad,
								 n_head_fail,
								 n_tail_fail,
								 ath_tx_get_rts_retrylimit
								 (sc, txq),
								 &bf->
								 bf_pp_rcs);
#else

					ath_rate_tx_complete_11n(sc,
								 an,
								 &ts,
								 bf->bf_rcs,
								 TID_TO_WME_AC
								 (bf->bf_tidno),
								 bf->bf_nframes,
								 nbad,
								 ath_tx_get_rts_retrylimit
								 (sc, txq),
								 &bf->
								 bf_pp_rcs);
#endif

#if UNIFIED_SMARTANTENNA
					if (bf->bf_mpdu
					    &&
					    SMART_ANT_TX_FEEDBACK_ENABLED(sc)) {
						ath_smart_ant_txfeedback_aponly
						    (sc, an, bf, nbad, &ts);
					}
#endif
				}
			}
#ifndef REMOVE_PKT_LOG
			/* do pktlog */

			if (!sc->sc_nodebug) {
				struct log_tx log_data = { 0 };
				log_data.lastds = &txs_desc;
				ath_log_txstatus(sc, &log_data, 0);
			}
#endif
		} else {
			/* PAPRD has NULL an */
			if (bf->bf_state.bfs_ispaprd) {
				ath_tx_paprd_complete(sc, bf, &bf_head);
				//printk("%s[%d]: ath_tx_paprd_complete called txok %d\n", __func__, __LINE__, txok);
				return;
			}
		}

		/*
		 * schedule any pending packets if aggregation is enabled
		 */
#ifndef ATH_RESTRICTED_TXQSCHED
		/* Delay Scheduling the txqs untill all are completed */
		ATH_TXQ_LOCK(txq);
		ath_txq_schedule(sc, txq);
		ATH_TXQ_UNLOCK(txq);
#endif
#ifdef ATH_SUPPORT_TxBF
		if (!(ts.ts_txbfstatus & ~(TxBF_Valid_Status))
		    && (ts.ts_txbfstatus != 0)) {
			/*Handle Tx bf complete */
			ath_edma_handle_txbf_complete(sc, &bf_headfree,
						      ts.ts_txbfstatus,
						      ts.ts_tstamp);

		}
#endif
		ath_edma_free_complete_buf(sc, txq, &bf_headfree, txok);
		qdepth += ath_txq_depth(sc, txq->axq_qnum);
#ifdef ATH_RESTRICTED_TXQSCHED
		txqsched |= (1 << txq->axq_qnum);
#endif
#if defined(ATH_SWRETRY) && defined(ATH_SWRETRY_MODIFY_DSTMASK)
		ATH_TXQ_LOCK(txq);
		if (ath_txq_depth(sc, txq->axq_qnum) == 0)
			if (sc->sc_swRetryEnabled)
				txq->axq_destmask = AH_TRUE;
		ATH_TXQ_UNLOCK(txq);
#endif
	}
#ifdef ATH_RESTRICTED_TXQSCHED
	/* Schedule the txqs that completed */
	for (i = 0; i < HAL_NUM_TX_QUEUES; i++) {
		if (txqsched & (1 << i)) {
			struct ath_txq *schedtxq = &sc->sc_txq[i];
			if (schedtxq != NULL && schedtxq->axq_depth == 0) {
				ATH_TXQ_LOCK(schedtxq);
				ath_txq_schedule(sc, schedtxq);
				ATH_TXQ_UNLOCK(schedtxq);
			}
		}
	}
#endif

	return;
}
#else // ATH_TX_COMPACT
DECLARE_N_EXPORT_PERF_CNTR(tx_tasklet);
/*
 * Deferred processing of transmit interrupt.
 * Tx Interrupts need to be disabled before entering this.
 */
static inline void ath_tx_edma_tasklet_aponly(ath_dev_t dev)
{
	struct ath_softc *sc = ATH_DEV_TO_SC(dev);
	int txok, nacked = 0, qdepth = 0, nbad = 0;
	struct ath_txq *txq;
	struct ath_tx_status ts;
	HAL_STATUS status;
	ath_bufhead bf_head;
	struct ath_buf *bf;
	struct ath_node *an;
#ifndef REMOVE_PKT_LOG
	u_int32_t txs_desc[9];
#endif
#if ATH_SUPPORT_VOWEXT
	u_int8_t n_head_fail = 0;
	u_int8_t n_tail_fail = 0;
#endif

#ifdef ATH_SUPPORT_TxBF
	struct atheros_node *oan;
#endif
#ifdef ATH_SWRETRY
	struct ieee80211_frame *wh;
	bool istxfrag;
#endif

	START_PERF_CNTR(tx_tasklet, tx_tasklet);

	for (;;) {

		/* hold lock while accessing tx status ring */
		ATH_TXSTATUS_LOCK(sc);

#ifndef REMOVE_PKT_LOG
		ath_hal_getrawtxdesc(sc->sc_ah, txs_desc);
#endif

		/*
		 * Process a completion event.
		 */
		status = ath_hal_txprocdesc(sc->sc_ah, (void *)&ts);

		ATH_TXSTATUS_UNLOCK(sc);

		if (status == HAL_EINPROGRESS)
			break;

		if (unlikely(status == HAL_EIO)) {
			break;
		}

		/* Skip beacon completions */
		if (ts.queue_id == sc->sc_bhalq)
			continue;

		/* Make sure the event came from an active queue */
		ASSERT(ATH_TXQ_SETUP(sc, ts.queue_id));

		/* Get the txq for the completion event */
		txq = &sc->sc_txq[ts.queue_id];

		ATH_TXQ_LOCK(txq);

		txq->axq_intrcnt = 0;	/* reset periodic desc intr count */

#if ATH_HW_TXQ_STUCK_WAR
		txq->tx_done_stuck_count = 0;
#endif

		bf = TAILQ_FIRST(&txq->axq_fifo[txq->axq_tailindex]);

		if (unlikely(bf == NULL)) {
			DPRINTF(sc, ATH_DEBUG_XMIT,
				"%s: TXQ[%d] tailindex %d\n", __func__,
				ts.queue_id, txq->axq_tailindex);
			ATH_TXQ_UNLOCK(txq);
			goto done;
		}

		if (unlikely(txq == sc->sc_cabq || txq == sc->sc_uapsdq)) {
			ATH_EDMA_MCASTQ_MOVE_HEAD_UNTIL(txq, &bf_head,
							bf->bf_lastbf, bf_list);
		} else {
			ATH_EDMA_TXQ_MOVE_HEAD_UNTIL(txq, &bf_head,
						     bf->bf_lastbf, bf_list);
		}

		if (likely(bf->bf_isaggr)) {
			txq->axq_aggr_depth--;
		}
#ifdef ATH_SWRETRY
		txok = (ts.ts_status == 0);

		wh = (struct ieee80211_frame *)wbuf_header(bf->bf_mpdu);
		istxfrag = (wh->i_fc[1] & IEEE80211_FC1_MORE_FRAG) ||
		    (((le16toh(*((u_int16_t *) & (wh->i_seq[0]))) >>
		       IEEE80211_SEQ_FRAG_SHIFT) & IEEE80211_SEQ_FRAG_MASK) >
		     0);

		/*
		 * Only enable the sw retry when the following conditions are met:
		 * 1. transmission failure
		 * 2. swretry is enabled at this time
		 * 3. non-ampdu data frame (not under protection of BAW)
		 * 4. not for frames from UAPSD queue
		 * 5. not for fragments
		 * Otherwise, complete the TU
		 */
		if (txok || (ath_check_swretry_req(sc, bf) == AH_FALSE)
		    || bf->bf_isampdu || bf->bf_isaggr || !bf->bf_isdata
		    || txq == sc->sc_uapsdq || istxfrag) {
			/* Change the status of the frame and complete
			 * this frame as normal frame
			 */
			bf->bf_status &= ~ATH_BUFSTATUS_MARKEDSWRETRY;

		} else {
			/* This frame is going through SW retry mechanism
			 */
			bf->bf_status |= ATH_BUFSTATUS_MARKEDSWRETRY;
			ath_hal_setdesclink(sc->sc_ah, bf->bf_desc, 0);
		}
#endif

		ATH_TXQ_UNLOCK(txq);
		bf->bf_tx_ratecode = ts.ts_ratecode;

#if ATH_SUPPORT_WIFIPOS
		if ((ts.ts_flags & HAL_TX_FAST_TS)) {
			ieee80211_wifiposdesc_t wifiposdata;
			u_int32_t retries;
			wbuf_t wbuf;
			ieee80211_wifipos_reqdata_t *req_data;
			u_int32_t wifipos_req_id;
			u_int8_t *req_mac_addr;

			memset(&wifiposdata, 0, sizeof(wifiposdata));
#ifdef ATH_SWRETRY
			if (bf->bf_isswretry)
				retries = bf->bf_totaltries;
			else
				retries = bf->bf_retries;
#else
			retries = bf->bf_retries;
#endif
			wbuf = bf->bf_mpdu;
			if (wbuf != NULL) {
				req_data =
				    (ieee80211_wifipos_reqdata_t *)
				    wbuf_get_wifipos(wbuf);
				if (req_data != NULL) {
					wifiposdata.tod = ts.ts_tstamp;
					wifipos_req_id =
					    wbuf_get_wifipos_req_id(wbuf);
					req_mac_addr =
					    ((struct ieee80211_frame *)
					     wbuf_header(wbuf))->i_addr1;
					memcpy(wifiposdata.sta_mac_addr,
					       req_mac_addr, ETH_ALEN);
					wifiposdata.request_id = wifipos_req_id;
					if (ts.ts_status == 0) {
						wifiposdata.flags |=
						    ATH_WIFIPOS_TX_STATUS;
					}
					wifiposdata.retries = retries;
					wifiposdata.flags |=
					    ATH_WIFIPOS_TX_UPDATE;
					/* QUIPS - 342 */
					//                   if (wifiposdata.flags & ATH_WIFIPOS_TX_STATUS)
					sc->sc_ieee_ops->
					    update_wifipos_stats(&wifiposdata);
				}
			}
		}
#endif

		an = bf->bf_node;

#ifdef ATH_SWRETRY
		if (an != NULL) {
			/* If the frame is sent due to responding to PS-Poll,
			 * do not retry this frame.
			 */
			if ((an->an_flags & ATH_NODE_PWRSAVE)
			    && atomic_read(&an->an_pspoll_pending)) {
				bf->bf_status &= ~ATH_BUFSTATUS_MARKEDSWRETRY;
			}
		}
#endif

#if ATH_SWRETRY
		if (bf->bf_ispspollresp) {
			ATH_NODE_SWRETRY_TXBUF_LOCK(an);
			if (an != NULL) {
				atomic_set(&an->an_pspoll_response, AH_FALSE);
			}
			bf->bf_ispspollresp = 0;
			ATH_NODE_SWRETRY_TXBUF_UNLOCK(an);
		}
#endif

		if (likely(an != NULL)) {
			int noratectrl;

#ifdef ATH_SUPPORT_TxBF
			oan = ATH_NODE_ATHEROS(an);
			if (oan->txbf && (ts.ts_status == 0)
			    && VALID_TXBF_RATE(ts.ts_ratecode, oan->usedNss)) {

				if (ts.ts_txbfstatus & ATH_TXBF_stream_missed) {
					__11nstats(sc, bf_stream_miss);
				}
				if (ts.ts_txbfstatus & ATH_TxBF_BW_mismatched) {
					__11nstats(sc, bf_bandwidth_miss);
				}
				if (ts.
				    ts_txbfstatus & ATH_TXBF_Destination_missed)
				{
					__11nstats(sc, bf_destination_miss);
				}

			}
#endif
			noratectrl =
			    an->an_flags & (ATH_NODE_CLEAN | ATH_NODE_PWRSAVE);
			OS_SYNC_SINGLE(sc->sc_osdev, bf->bf_daddr,
				       sc->sc_txdesclen, BUS_DMA_FROMDEVICE,
				       NULL);

			ath_hal_gettxratecode(sc->sc_ah, bf->bf_desc,
					      (void *)&ts);
			/* ratecode updated */
			bf->bf_tx_ratecode = ts.ts_ratecode;

#ifdef ATH_SWRETRY
			ath_tx_dec_eligible_frms(sc, bf, txq->axq_qnum, &ts);
#endif

			ath_tx_update_stats(sc, bf, txq->axq_qnum, &ts);

			txok = (ts.ts_status == 0);

			/*
			 * Hand the descriptor to the rate control algorithm
			 * if the frame wasn't dropped for filtering or sent
			 * w/o waiting for an ack.  In those cases the rssi
			 * and retry counts will be meaningless.
			 */
			if (unlikely(!bf->bf_isampdu)) {
				/*
				 * This frame is sent out as a single frame. Use hardware retry
				 * status for this frame.
				 */
				bf->bf_retries = ts.ts_longretry;
				if (ts.ts_status & HAL_TXERR_XRETRY) {
					__11nstats(sc, tx_sf_hw_xretries);
					bf->bf_isxretried = 1;
				}
				nbad = 0;
#if ATH_SUPPORT_VOWEXT
				n_head_fail = n_tail_fail = 0;
#endif
			} else {
				nbad = ath_tx_num_badfrms(sc, bf, &ts, txok);
#if ATH_SUPPORT_VOWEXT
				n_tail_fail = (nbad & 0xFF);
				n_head_fail = ((nbad >> 8) & 0xFF);
				nbad = ((nbad >> 16) & 0xFF);
#endif
			}

			if (likely((ts.ts_status & HAL_TXERR_FILT) == 0 &&
				   (bf->bf_flags & HAL_TXDESC_NOACK) == 0)) {
				/*
				 * If frame was ack'd update the last rx time
				 * used to workaround phantom bmiss interrupts.
				 */
				if (likely(ts.ts_status == 0))
					nacked++;

				if (likely(bf->bf_isdata && !bf->bf_useminrate)) {
#ifdef ATH_SUPPORT_VOWEXT

					/* FIXME do not care Ospre related issues as on today, keep
					   this pending until we get to that
					 */
					ath_rate_tx_complete_11n(sc,
								 an,
								 &ts,
								 bf->bf_rcs,
								 TID_TO_WME_AC
								 (bf->bf_tidno),
								 bf->bf_nframes,
								 nbad,
								 n_head_fail,
								 n_tail_fail,
								 ath_tx_get_rts_retrylimit
								 (sc, txq),
#ifdef ATH_SUPPORT_UAPSD
								 (txq ==
								  sc->
								  sc_uapsdq) ?
								 NULL : &bf->
								 bf_pp_rcs);
#else
								 &bf->
								 bf_pp_rcs);
#endif
#else
					if (likely
					    (bf->bf_isdata && !noratectrl)) {
						ath_rate_tx_complete_11n(sc, an,
									 &ts,
									 bf->
									 bf_rcs,
									 TID_TO_WME_AC
									 (bf->
									  bf_tidno),
									 bf->
									 bf_nframes,
									 nbad,
									 ath_tx_get_rts_retrylimit
									 (sc,
									  txq),
#ifdef ATH_SUPPORT_UAPSD
									 (txq ==
									  sc->
									  sc_uapsdq)
									 ? NULL
									 : &bf->
									 bf_pp_rcs);
#else
									 &bf->
									 bf_pp_rcs);
#endif

#if UNIFIED_SMARTANTENNA
						if (SMART_ANT_TX_FEEDBACK_ENABLED(sc)) {
							ath_smart_ant_txfeedback_aponly
							    (sc, an, bf, nbad,
							     &ts);
						}
#endif
					}
#endif
				}
			}
#if QCA_SUPPORT_SON
			if (bf->bf_mpdu && wbuf_get_bsteering(bf->bf_mpdu) &&
			    (ts.ts_rssi > INST_STATS_INVALID_RSSI) &&
			    (ts.ts_rssi < INST_STATS_VALID_RSSI_MAX)) {
				wbuf_t wbuf;
				u_int8_t subtype = 0;
				u_int8_t mac_addr[ETH_ALEN], status;
				wbuf = bf->bf_mpdu;
				status = 0;
				status = (ts.ts_status == 0) ? 1 : 0;
				wh = (struct ieee80211_frame *)wbuf_header(bf->
									   bf_mpdu);
				subtype =
				    wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
				memcpy(&mac_addr,
				       ((struct ieee80211_frame *)
					wbuf_header(wbuf))->i_addr1, ETH_ALEN);
				if (sc->sc_ieee_ops->bsteering_rssi_update)
					sc->sc_ieee_ops->
					    bsteering_rssi_update(sc->sc_ieee,
								  mac_addr,
								  status,
								  ts.ts_rssi,
								  subtype);
			}
#endif

#ifdef ATH_SWRETRY
			if ((CHK_SC_DEBUG(sc, ATH_DEBUG_SWR)) &&
			    (ts.ts_status || bf->bf_isswretry)
			    && (bf->bf_status & ATH_BUFSTATUS_MARKEDSWRETRY)) {
				struct ieee80211_frame *wh;
				wh = (struct ieee80211_frame *)wbuf_header(bf->
									   bf_mpdu);
				DPRINTF(sc, ATH_DEBUG_SWR,
					"%s: SeqCtrl0x%02X%02X --> status %08X rate %02X, swretry %d retrycnt %d totaltries %d\n",
					__func__, wh->i_seq[0], wh->i_seq[1],
					ts.ts_status, ts.ts_ratecode,
					(bf->bf_isswretry) ? 1 : 0,
					bf->bf_swretries,
					bf->bf_totaltries + ts.ts_longretry +
					ts.ts_shortretry);

				DPRINTF(sc, ATH_DEBUG_SWR,
					"%s, %s, type=0x%x, subtype=0x%x\n",
					(ts.ts_status & HAL_TXERR_FILT) !=
					0 ? "IS FILT" : "NOT FILT",
					(ts.ts_status & HAL_TXERR_XRETRY) !=
					0 ? "IS XRETRY" : "NOT XRETRY",
					wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK,
					wh->
					i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK);
			}

			if (bf->bf_status & ATH_BUFSTATUS_MARKEDSWRETRY) {
#ifndef REMOVE_PKT_LOG
				/* do pktlog */
				{
					struct log_tx log_data = { 0 };
					struct ath_buf *tbf;

					TAILQ_FOREACH(tbf, &bf_head, bf_list) {
						log_data.firstds = tbf->bf_desc;
						log_data.bf = tbf;
						ath_log_txctl(sc, &log_data, 0);
					}
				}
				{
					struct log_tx log_data = { 0 };
					log_data.lastds = &txs_desc;
					ath_log_txstatus(sc, &log_data, 0);
				}
#endif
				/* Put the sw retry frame back to tid queue */
				ath_tx_mpdu_requeue(sc, txq, &bf_head, ts);

				/* We have completed the buf in resend in case of
				 * failure and hence not needed and will be fatal
				 * if we fall through this
				 */
				continue;
			}
#endif

			/*
			 * Complete this transmit unit
			 *
			 * Node cannot be referenced past this point since it can be freed
			 * here.
			 */
			if (likely(bf->bf_isampdu)) {
				if (unlikely(ts.ts_flags & HAL_TX_DESC_CFG_ERR))
					__11nstats(sc, txaggr_desc_cfgerr);
				if (unlikely
				    (ts.ts_flags & HAL_TX_DATA_UNDERRUN)) {
					__11nstats(sc, txaggr_data_urun);
				}
				if (unlikely
				    (ts.ts_flags & HAL_TX_DELIM_UNDERRUN)) {
					__11nstats(sc, txaggr_delim_urun);
				}
				ath_tx_complete_aggr_rifs(sc, txq, bf, &bf_head,
							  &ts, txok);
			} else {
#ifndef REMOVE_PKT_LOG
				/* do pktlog */
				{
					struct log_tx log_data = { 0 };
					struct ath_buf *tbf;

					TAILQ_FOREACH(tbf, &bf_head, bf_list) {
						log_data.firstds = tbf->bf_desc;
						log_data.bf = tbf;
						ath_log_txctl(sc, &log_data, 0);
					}
				}
#endif

#ifdef ATH_SUPPORT_UAPSD
				if (txq == sc->sc_uapsdq) {
#ifdef ATH_SUPPORT_TxBF
					ath_tx_uapsd_complete(sc, an, bf,
							      &bf_head, txok,
							      ts.ts_txbfstatus,
							      ts.ts_tstamp);
#else
					ath_tx_uapsd_complete(sc, an, bf,
							      &bf_head, txok);
#endif
				} else
#endif
				{
#ifdef  ATH_SUPPORT_TxBF
					ath_tx_complete_buf_aponly(sc, bf,
								   &bf_head,
								   txok,
								   ts.
								   ts_txbfstatus,
								   ts.
								   ts_tstamp);
#else
					ath_tx_complete_buf_aponly(sc, bf,
								   &bf_head,
								   txok);
#endif
				}
			}

#ifndef REMOVE_PKT_LOG
			/* do pktlog */
			{
				struct log_tx log_data = { 0 };
				log_data.lastds = &txs_desc;
				ath_log_txstatus(sc, &log_data, 0);
			}
#endif
		} else {
			/* PAPRD has NULL an */
			if (bf->bf_state.bfs_ispaprd) {
				ath_tx_paprd_complete(sc, bf, &bf_head);
				goto done;
			}
		}

		/*
		 * schedule any pending packets if aggregation is enabled
		 */

		ATH_TXQ_LOCK(txq);
		ath_txq_schedule(sc, txq);
		ATH_TXQ_UNLOCK(txq);

		qdepth += ath_txq_depth(sc, txq->axq_qnum);

#if defined(ATH_SWRETRY) && defined(ATH_SWRETRY_MODIFY_DSTMASK)
		ATH_TXQ_LOCK(txq);
		if (ath_txq_depth(sc, txq->axq_qnum) == 0)
			if (sc->sc_swRetryEnabled)
				txq->axq_destmask = AH_TRUE;
		ATH_TXQ_UNLOCK(txq);
#endif
	}

done:
	END_PERF_CNTR(tx_tasklet);

	return;
}

#endif // ATH_TX_COMPACT

extern void ath_tx_tasklet(ath_dev_t dev);

typedef enum {
	FILTER_STATUS_ACCEPT = 0,
	FILTER_STATUS_REJECT
} ieee80211_privasy_filter_status;

#define IS_SNAP(_llc) ((_llc)->llc_dsap == LLC_SNAP_LSAP && \
                        (_llc)->llc_ssap == LLC_SNAP_LSAP && \
                        (_llc)->llc_control == LLC_UI)
#define RFC1042_SNAP_NOT_AARP_IPX(_llc) \
            ((_llc)->llc_snap.org_code[0] == RFC1042_SNAP_ORGCODE_0 && \
            (_llc)->llc_snap.org_code[1] == RFC1042_SNAP_ORGCODE_1 && \
            (_llc)->llc_snap.org_code[2] == RFC1042_SNAP_ORGCODE_2 \
            && !((_llc)->llc_snap.ether_type == htons(ETHERTYPE_AARP) || \
                (_llc)->llc_snap.ether_type == htons(ETHERTYPE_IPX)))
#define IS_BTEP(_llc) ((_llc)->llc_snap.org_code[0] == BTEP_SNAP_ORGCODE_0 && \
            (_llc)->llc_snap.org_code[1] == BTEP_SNAP_ORGCODE_1 && \
            (_llc)->llc_snap.org_code[2] == BTEP_SNAP_ORGCODE_2)
#define IS_ORG_BTAMP(_llc) ((_llc)->llc_snap.org_code[0] == BTAMP_SNAP_ORGCODE_0 && \
                            (_llc)->llc_snap.org_code[1] == BTAMP_SNAP_ORGCODE_1 && \
                            (_llc)->llc_snap.org_code[2] == BTAMP_SNAP_ORGCODE_2)
#define IS_ORG_AIRONET(_llc) ((_llc)->llc_snap.org_code[0] == AIRONET_SNAP_CODE_0 && \
                               (_llc)->llc_snap.org_code[1] == AIRONET_SNAP_CODE_1 && \
                               (_llc)->llc_snap.org_code[2] == AIRONET_SNAP_CODE_2)

/*
 * delivers the data to the OS .
 *  will deliver standard 802.11 frames (with qos control removed)
 *  if IEEE80211_DELIVER_80211 param is set.
 *  will deliver ethernet frames (with 802.11 header decapped)
 *  if IEEE80211_DELIVER_80211 param is not set.
 *  this funcction consumes the  passed in wbuf.
 */
static ieee80211_privasy_filter_status
ieee80211_check_privacy_filters_aponly(struct wlan_objmgr_peer *peer,
				       wbuf_t wbuf, int is_mcast)
{
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
	struct llc *llc;
	u_int16_t ether_type = 0;
	u_int32_t hdrspace;
	u_int32_t i;
	struct ieee80211_frame *wh;
	privacy_filter_packet_type packet_type;
	u_int8_t is_encrypted;

	/* Safemode must avoid the PrivacyExemptionList and ExcludeUnencrypted checking */
	if (unlikely
	    (wlan_vdev_mlme_feat_ext_cap_get(vdev, WLAN_VDEV_FEXT_SAFEMODE))) {
		return FILTER_STATUS_ACCEPT;
	}
	wh = (struct ieee80211_frame *)wbuf_header(wbuf);

	hdrspace = wlan_hdrspace(pdev, wbuf_header(wbuf));

	if (unlikely(wbuf_get_pktlen(wbuf) < (hdrspace + LLC_SNAPFRAMELEN))) {
		QDF_TRACE( QDF_MODULE_ID_INPUT,
			       QDF_TRACE_LEVEL_DEBUG,
			       "%s:[%s] Discard data frame: too small packet 0x%x len %u hdrspace %u\n",
			       __func__, ether_sprintf(wh->i_addr2), ether_type,
			       wbuf_get_pktlen(wbuf), hdrspace);
		return FILTER_STATUS_REJECT;	/* filter the packet */
	}

	llc = (struct llc *)(wbuf_header(wbuf) + hdrspace);
	if (IS_SNAP(llc)
	    && (RFC1042_SNAP_NOT_AARP_IPX(llc) || IS_ORG_BTAMP(llc)
		|| IS_ORG_AIRONET(llc))) {
		ether_type = ntohs(llc->llc_snap.ether_type);
	} else {
		ether_type = htons(wbuf_get_pktlen(wbuf) - hdrspace);
	}

	is_encrypted = (wh->i_fc[1] & IEEE80211_FC1_WEP);
	wh->i_fc[1] &= ~IEEE80211_FC1_WEP;	/* XXX: we don't need WEP bit from here */

	if (is_mcast) {
		packet_type = PRIVACY_FILTER_PACKET_MULTICAST;
	} else {
		packet_type = PRIVACY_FILTER_PACKET_UNICAST;
	}

	for (i = 0; i < dp_vdev->filters_num; i++) {
		/* skip if the ether type does not match */
		if (dp_vdev->privacy_filters[i].ether_type != ether_type)
			continue;

		/* skip if the packet type does not match */
		if (dp_vdev->privacy_filters[i].packet_type != packet_type &&
		    dp_vdev->privacy_filters[i].packet_type !=
		    PRIVACY_FILTER_PACKET_BOTH)
			continue;

		if (dp_vdev->privacy_filters[i].filter_type ==
		    PRIVACY_FILTER_ALWAYS) {
			/*
			 * In this case, we accept the frame if and only if it was originally
			 * NOT encrypted.
			 */
			if (is_encrypted) {
				QDF_TRACE(
					       QDF_MODULE_ID_INPUT,
					       QDF_TRACE_LEVEL_DEBUG,
					       "%s:[%s] Discard data frame: packet encrypted ether type 0x%x len %u \n",
					       __func__,
					       ether_sprintf(wh->i_addr2),
					       ether_type,
					       wbuf_get_pktlen(wbuf));
				return FILTER_STATUS_REJECT;
			} else {
				return FILTER_STATUS_ACCEPT;
			}
		} else if (dp_vdev->privacy_filters[i].filter_type ==
			   PRIVACY_FILTER_KEY_UNAVAILABLE) {
			/*
			 * In this case, we reject the frame if it was originally NOT encrypted but
			 * we have the key mapping key for this frame.
			 */
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
			if (!is_encrypted && !is_mcast && dadp_peer_get_key_valid(peer)) {
#else
			if (!is_encrypted && !is_mcast && dp_peer->key && dp_peer->key->wk_valid) {
#endif
				QDF_TRACE(
					       QDF_MODULE_ID_INPUT,
					       QDF_TRACE_LEVEL_DEBUG,
					       "%s:[%s] Discard data frame: node has a key ether type 0x%x len %u \n",
					       __func__,
					       ether_sprintf(wh->i_addr2),
					       ether_type,
					       wbuf_get_pktlen(wbuf));
				return FILTER_STATUS_REJECT;
			} else {
				return FILTER_STATUS_ACCEPT;
			}
		} else {
			/*
			 * The privacy exemption does not apply to this frame.
			 */
			break;
		}
	}

	/*
	 * If the privacy exemption list does not apply to the frame, check ExcludeUnencrypted.
	 * if ExcludeUnencrypted is not set, or if this was oringially an encrypted frame,
	 * it will be accepted.
	 */
	if (!wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_DROPUNENC)
	    || is_encrypted) {
		/*
		 * if the node is not authorized
		 * reject the frame.
		 */
		if (!wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_AUTH)) {
			QDF_TRACE(
				       QDF_MODULE_ID_INPUT,
				       QDF_TRACE_LEVEL_DEBUG,
				       "[%s] Discard data frame: unauthorized port: ether type 0x%x len %u \n",
				       ether_sprintf(wh->i_addr2), ether_type,
				       wbuf_get_pktlen(wbuf));
			dp_vdev->vdev_stats.is_rx_unauth++;
			return FILTER_STATUS_REJECT;
		}
		return FILTER_STATUS_ACCEPT;
	}

	if (!is_encrypted
	    && wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_DROPUNENC)) {
		if (is_mcast) {
			dp_vdev->vdev_multicast_stats.ims_rx_unencrypted++;
			dp_vdev->vdev_multicast_stats.ims_rx_decryptcrc++;
		} else {
			dp_vdev->vdev_unicast_stats.ims_rx_unencrypted++;
			dp_vdev->vdev_unicast_stats.ims_rx_decryptcrc++;
		}
		IEEE80211_PEER_STAT(dp_peer, rx_unencrypted);
		IEEE80211_PEER_STAT(dp_peer, rx_decryptcrc);
	}

	QDF_TRACE( QDF_MODULE_ID_INPUT,
		       QDF_TRACE_LEVEL_DEBUG,
		       "%s:[%s] Discard data frame: ether type 0x%x len %u \n",
		       __func__, ether_sprintf(wh->i_addr2), ether_type,
		       wbuf_get_pktlen(wbuf));
	return FILTER_STATUS_REJECT;
}

static inline wbuf_t
ieee80211_decap_aponly(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf,
		       size_t hdrspace)
{
	struct ieee80211_qosframe_addr4 wh;	/* max size address frame */
	struct ether_header *eh;
	struct llc *llc;
	u_int16_t ether_type = 0;

	if (unlikely(wbuf_get_pktlen(wbuf) < (hdrspace + sizeof(*llc)))) {
		/* XXX stat, msg */
		wbuf_free(wbuf);
		wbuf = NULL;
		goto done;
	}
	OS_MEMCPY(&wh, wbuf_header(wbuf),
		  hdrspace < sizeof(wh) ? hdrspace : sizeof(wh));
	llc = (struct llc *)(wbuf_header(wbuf) + hdrspace);

	if (IS_SNAP(llc) && RFC1042_SNAP_NOT_AARP_IPX(llc)) {
		/* leave ether_tyep in  in network order */
		ether_type = llc->llc_un.type_snap.ether_type;
		wbuf_pull(wbuf,
			  (u_int16_t) (hdrspace + sizeof(struct llc) -
				       sizeof(*eh)));
		llc = NULL;
	} else if (IS_SNAP(llc) && IS_BTEP(llc)) {
		/* for bridge-tunnel encap, remove snap and 802.11 headers, keep llc ptr for type */
		wbuf_pull(wbuf,
			  (u_int16_t) (hdrspace + sizeof(struct llc) -
				       sizeof(*eh)));
	} else {
		wbuf_pull(wbuf, (u_int16_t) (hdrspace - sizeof(*eh)));
	}
	eh = (struct ether_header *)(wbuf_header(wbuf));

	switch (wh.i_fc[1] & IEEE80211_FC1_DIR_MASK) {
	case IEEE80211_FC1_DIR_NODS:
		IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr1);
		IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr2);
		break;
	case IEEE80211_FC1_DIR_TODS:
		IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr3);
		IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr2);
		break;
	case IEEE80211_FC1_DIR_FROMDS:
		IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr1);
		IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr3);
		break;
#if ATH_WDS_SUPPORT_APONLY
	case IEEE80211_FC1_DIR_DSTODS:
		IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr3);
		IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr4);
		break;
#endif
	}

	if (llc != NULL) {
		if (IS_BTEP(llc)) {
			/* leave ether_tyep in  in network order */
			eh->ether_type = llc->llc_snap.ether_type;
		} else {
			eh->ether_type =
			    htons(wbuf_get_pktlen(wbuf) - sizeof(*eh));
		}
	} else {
		eh->ether_type = ether_type;
	}
done:
	return wbuf;
}

#ifdef QCA_PARTNER_PLATFORM
extern bool osif_pltfrm_deliver_data(os_if_t osif, wbuf_t wbuf);
extern int ath_pltfrm_vlan_tag_check(struct ieee80211vap *vap, wbuf_t wbuf);
#endif
static void
ieee80211_deliver_data_aponly(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf,
			      struct wlan_objmgr_peer *peer,
			      struct ieee80211_rx_status *rs,
			      u_int32_t hdrspace, int is_mcast,
			      u_int8_t subtype)
{
	int igmp = 0;
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
#endif
	struct wlan_objmgr_psoc *psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		qdf_print("%s:psoc is NULL ", __func__);
		return;
	}

	if (!wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_DELIVER_80211)) {
		/*
		 * if the OS is interested in ethernet frame,
		 * decap the 802.11 frame and convert into
		 * ethernet frame.
		 */
		wbuf = ieee80211_decap_aponly(vdev, wbuf, hdrspace);
		if (unlikely(!wbuf)) {
			QDF_TRACE(
				       QDF_MODULE_ID_INPUT,
				       QDF_TRACE_LEVEL_DEBUG, "decap %s",
				       "failed");
			return;
		}

		/*
		 * If IQUE is not enabled, the ops table is NULL and the following
		 * steps will be skipped;
		 * If IQUE is enabled, the packet will be checked to see whether it
		 * is an IGMP packet or not, and update the mcast snoop table if necessary
		 */
		if (dp_vdev->ique_ops.wlan_me_inspect) {

			igmp =
			    dp_vdev->ique_ops.wlan_me_inspect(vdev, peer, wbuf);
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
			if (dp_vdev->me_hifi_enable
			    && igmp == IEEE80211_QUERY_FROM_STA
			    && dp_pdev->pdev_dropstaquery) {
				wbuf_complete(wbuf);
				return;
			}
#endif
		}
	}

	/* perform as a bridge within the AP */
	if (!wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_NOBRIDGE)) {
		wbuf_t wbuf_cpy = NULL;

		if (is_mcast) {
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
			/* Enable/disable flooding Report packets. */
			if ((dp_vdev->me_hifi_enable
			     && igmp) != IEEE80211_REPORT_FROM_STA
			    || !dp_pdev->pdev_blkreportflood) {
#endif
				wbuf_cpy = wbuf_clone(dp_pdev->osdev, wbuf);
#if ATH_RXBUF_RECYCLE
				if (wbuf_cpy)
					wbuf_set_cloned(wbuf_cpy);
#endif
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
			}
#endif
		} else {
			struct wlan_objmgr_peer *peer1;
			/*
			 * Check if destination is associated with the
			 * same vap and authorized to receive traffic.
			 * Beware of traffic destined for the vap itself;
			 * sending it will not work; just let it be
			 * delivered normally.
			 */
			if (wlan_vdev_mlme_feat_cap_get
			    (vdev, WLAN_VDEV_F_DELIVER_80211)) {
				struct ieee80211_frame *wh =
				    (struct ieee80211_frame *)wbuf_header(wbuf);
				peer1 =
				    ieee80211_vdev_find_node(psoc, vdev, wh->i_addr3);
			} else {
				struct ether_header *eh =
				    (struct ether_header *)wbuf_header(wbuf);
				peer1 =
				    ieee80211_vdev_find_node(psoc, vdev,
							     eh->ether_dhost);
#if ATH_WDS_SUPPORT_APONLY
				if ((peer1 == NULL)
				    &&
				    (wlan_vdev_mlme_feat_cap_get
				     (vdev, WLAN_VDEV_F_WDS))) {
					peer1 =
					    wlan_find_wds_peer(vdev,
							       eh->ether_dhost);
#if 0				/*TBD_DADP: perf impact ??? */
					ni1 =
					    ieee80211_find_wds_node_aponly
					    (&vap->iv_ic->ic_sta,
					     eh->ether_dhost);
#endif
				}
#endif
			}
			if (peer1 != NULL) {
				if (wlan_peer_get_vdev(peer1) == vdev &&
				    wlan_peer_mlme_flag_get(peer,
							    WLAN_PEER_F_AUTH)
				    && peer1 != wlan_vdev_get_bsspeer(vdev)) {
					wbuf_cpy = wbuf;
					wbuf = NULL;
				}
				wlan_objmgr_peer_release_ref(peer1,
							     WLAN_MLME_SB_ID);
			}
		}
		if (wbuf_cpy != NULL) {
			/*
			 * send the frame copy back to the interface.
			 * this frame is either multicast frame. or unicast frame
			 * to one of the stations.
			 */
#if UMAC_SUPPORT_PROXY_ARP
			if (!(wlan_do_proxy_arp(vdev, wbuf_cpy)))
#endif /* UMAC_SUPPORT_PROXY_ARP */
				dp_vdev->vdev_evtable->
				    wlan_vap_xmit_queue(wlan_vdev_get_osifp
							(vdev), wbuf_cpy);
		}
	}
	if (likely(wbuf != NULL)) {

#if UMAC_SUPPORT_VI_DBG
		wlan_vdev_vi_dbg_input(vdev, wbuf);
#endif

		/*
		 * deliver the data frame to the os. the handler cosumes the wbuf.
		 */
		__osif_deliver_data(wlan_vdev_get_osifp(vdev), wbuf);

	}
}

static void
ieee80211_input_update_data_stats_aponly(struct wlan_objmgr_peer *peer,
					 struct wlan_vdev_mac_stats *mac_stats,
					 wbuf_t wbuf,
					 struct ieee80211_rx_status *rs,
					 u_int16_t realhdrsize)
{
	u_int32_t data_bytes = 0;
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
#if UMAC_SUPPORT_STA_STATS_ENHANCEMENT
	struct ieee80211_frame *wh;
	int is_mcast;
#endif

	if (!dp_peer) {
		qdf_print("%s:dp_peer is null ", __func__);
		return;
	}

#if UMAC_SUPPORT_STA_STATS_ENHANCEMENT
	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	is_mcast = IEEE80211_IS_MULTICAST(IEEE80211_WH4(wh)->i_addr3);
#endif
	mac_stats->ims_rx_data_packets++;
	IEEE80211_PEER_STAT(dp_peer, rx_data);

	data_bytes = wbuf_get_pktlen(wbuf)
	    + rs->rs_cryptodecapcount + IEEE80211_CRC_LEN - rs->rs_padspace;

	mac_stats->ims_rx_data_bytes += data_bytes;
	IEEE80211_PEER_STAT_ADD(dp_peer, rx_bytes, data_bytes);
#if UMAC_SUPPORT_STA_STATS_ENHANCEMENT
	if (is_mcast) {
		IEEE80211_PEER_STAT(dp_peer, rx_mcast);
		IEEE80211_PEER_STAT_ADD(dp_peer, rx_mcast_bytes, data_bytes);
	} else {
		IEEE80211_PEER_STAT(dp_peer, rx_ucast);
		IEEE80211_PEER_STAT_ADD(dp_peer, rx_ucast_bytes, data_bytes);
	}
#endif

	mac_stats->ims_rx_datapyld_bytes += (data_bytes
					     - realhdrsize
					     - rs->rs_cryptodecapcount
					     - IEEE80211_CRC_LEN);
}

 /*
  * processes data frames.
  * ieee80211_input_data consumes the wbuf .
  */
static void
ieee80211_input_data_aponly(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
			    struct ieee80211_rx_status *rs, int subtype,
			    int dir)
{
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	struct wlan_objmgr_peer *bsspeer = wlan_vdev_get_bsspeer(vdev);
	struct ieee80211_frame *wh;
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint8_t key_header, key_trailer, key_miclen;
	uint8_t macaddr[IEEE80211_ADDR_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	uint8_t *mac;
	int key_set = 0;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	struct wlan_lmac_if_crypto_rx_ops *crypto_rx_ops = wlan_crypto_get_crypto_rx_ops(psoc);
	int ismcast;
	uint8_t frame_keyid = 0;
#else
	struct ieee80211_key *key;
#endif
	struct wlan_vdev_mac_stats *mac_stats;
	u_int16_t hdrspace;
	u_int16_t hdrsize;
	int is_amsdu = 0, is_mcast;
	u_int16_t associd = 0;

	rs->rs_cryptodecapcount = 0;

	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	is_mcast = IEEE80211_IS_MULTICAST(IEEE80211_WH4(wh)->i_addr3);
	mac_stats =
	    is_mcast ? &dp_vdev->vdev_multicast_stats : &dp_vdev->
	    vdev_unicast_stats;

	hdrspace = wlan_hdrspace(pdev, wbuf_header(wbuf));
	if (unlikely(wbuf_get_pktlen(wbuf) < hdrspace)) {
		goto bad;
	}

	if (dir == IEEE80211_FC1_DIR_DSTODS) {
		if (IEEE80211_ADDR_EQ
		    (IEEE80211_WH4(wh)->i_addr4,
		     wlan_vdev_mlme_get_macaddr(vdev))) {
			printk("mac %s should not in here\n",
			       ether_sprintf(IEEE80211_WH4(wh)->i_addr4));
			goto bad;
		}
	}

	hdrsize = ieee80211_hdrsize(wh);
	rs->rs_padspace = hdrspace - hdrsize;

	WLAN_VDEV_LASTDATATSTAMP(vdev, OS_GET_TIMESTAMP());
	WLAN_VDEV_TXRXBYTE(dp_vdev, wbuf_get_pktlen(wbuf));
	if (IEEE80211_CONTAIN_DATA(subtype)) {
		dp_vdev->vdev_last_traffic_indication = OS_GET_TIMESTAMP();
	}

	if (unlikely(!((dir == IEEE80211_FC1_DIR_TODS)
#if ATH_WDS_SUPPORT_APONLY
		       || (dir == IEEE80211_FC1_DIR_DSTODS
			   &&
			   (wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_WDS)))
#endif
		     ))) {
		goto bad;
	}
#if ATH_WDS_SUPPORT_APONLY
#if UMAC_SUPPORT_NAWDS
	/* if NAWDS learning feature is enabled, add the mac to NAWDS table */
	if ((peer == bsspeer) &&
	    (dir == IEEE80211_FC1_DIR_DSTODS) &&
	    (wlan_vdev_nawds_enable_learning(vdev))) {
		wlan_vdev_nawds_learn(vdev, wh->i_addr2);
		/* current node is bss node so drop it to avoid sending dis-assoc. packet */
		goto out;
	}
#endif /* UMAC_SUPPORT_NAWDS */
#endif /* ATH_WDS_SUPPORT_APONLY */

	/* check if source STA is associated */
	if (unlikely(peer == bsspeer)) {
		QDF_TRACE( QDF_MODULE_ID_INPUT,
			       QDF_TRACE_LEVEL_DEBUG,
			       "[%s] Discard data frame: unknown src",
			       ether_sprintf(wlan_wbuf_getbssid(vdev, wh)));

		/* NB: caller deals with reference */
		if (wlan_vdev_is_up(vdev) == QDF_STATUS_SUCCESS) {
			peer = wlan_create_tmp_peer(vdev, wh->i_addr2);
			if (peer != NULL) {
				QDF_TRACE(
					       QDF_MODULE_ID_AUTH,
					       QDF_TRACE_LEVEL_DEBUG,
					       "%s: sending DEAUTH to %s, unknown src reason %d\n",
					       __func__,
					       ether_sprintf(wh->i_addr2),
					       IEEE80211_REASON_NOT_AUTHED);
				wlan_send_deauth(peer,
						 IEEE80211_REASON_NOT_AUTHED);
				wlan_vdev_deliver_event_mlme_deauth_indication
				    (vdev, (wh->i_addr2), associd,
				     IEEE80211_REASON_NOT_AUTHED);

				/* claim node immediately */
				wlan_peer_delete(peer);
			}
		}
		goto bad;
	}

	if (unlikely(wlan_peer_get_associd(peer) == 0)) {
		QDF_TRACE( QDF_MODULE_ID_INPUT,
			       QDF_TRACE_LEVEL_DEBUG,
			       "[%s] Discard data frame: unassoc src",
			       ether_sprintf(wlan_wbuf_getbssid(vdev, wh)));
		wlan_send_disassoc(peer, IEEE80211_REASON_NOT_ASSOCED);
		wlan_vdev_deliver_event_mlme_disassoc_indication(vdev,
								 (wh->i_addr2),
								 0,
								 IEEE80211_REASON_NOT_ASSOCED);
		goto bad;
	}
#if ATH_WDS_SUPPORT_APONLY
	/* If we're a 4 address packet, make sure we have an entry in
	   the node table for the packet source address (addr4).  If not,
	   add one */
	if (dir == IEEE80211_FC1_DIR_DSTODS) {
		wlan_wds_update_rootwds_table(peer, pdev, wbuf);
	}
#endif

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	/*
	 *  Safemode prevents us from calling decap.
	 */
	if (!wlan_vdev_mlme_feat_ext_cap_get(vdev, WLAN_VDEV_FEXT_SAFEMODE) &&
	    (wh->i_fc[1] & IEEE80211_FC1_WEP)) {
		ismcast = IEEE80211_IS_MULTICAST(wh->i_addr1);
		if(ismcast /*|| !dadp_peer_get_key_valid(peer)*/)
		{
			mac = macaddr;
			key_header = dadp_vdev_get_key_header(vdev, dp_vdev->def_tx_keyix);
			key_trailer = dadp_vdev_get_key_trailer(vdev, dp_vdev->def_tx_keyix);
			key_miclen = dadp_vdev_get_key_miclen(vdev, dp_vdev->def_tx_keyix);
		}
		else
		{
			mac = wlan_peer_get_macaddr(peer);
			key_header = dadp_peer_get_key_header(peer);
			key_trailer = dadp_peer_get_key_trailer(peer);
			key_miclen = dadp_peer_get_key_miclen(peer);
		}
		frame_keyid = wlan_crypto_get_keyid((uint8_t *)qdf_nbuf_data(wbuf), hdrspace);
		if (crypto_rx_ops && WLAN_CRYPTO_RX_OPS_DECAP(crypto_rx_ops))
			status = WLAN_CRYPTO_RX_OPS_DECAP(crypto_rx_ops)(vdev, wbuf, mac, 0);
		if (unlikely(status != QDF_STATUS_SUCCESS)) {
			mac_stats->ims_rx_decryptcrc++;
			IEEE80211_PEER_STAT(dp_peer, rx_decryptcrc);
			QDF_TRACE(
				  QDF_MODULE_ID_INPUT,
				  QDF_TRACE_LEVEL_DEBUG,
				  " [%s] Discard data frame: key is null",
				  ether_sprintf(wlan_wbuf_getbssid
						(vdev, wh)));
			goto bad;
		} else {
			rs->rs_cryptodecapcount += (key_header +
						    key_trailer);
			key_set = 1;
		}

		wh = (struct ieee80211_frame *)wbuf_header(wbuf);
		/* NB: We clear the Protected bit later */
	}
#else
	/*
	 *  Safemode prevents us from calling decap.
	 */
	if (!wlan_vdev_mlme_feat_ext_cap_get(vdev, WLAN_VDEV_FEXT_SAFEMODE) &&
	    (wh->i_fc[1] & IEEE80211_FC1_WEP)) {
		key = wlan_crypto_peer_decap(peer, wbuf, hdrspace, rs);
		if (unlikely(key == NULL)) {
			mac_stats->ims_rx_decryptcrc++;
			IEEE80211_PEER_STAT(dp_peer, rx_decryptcrc);
			QDF_TRACE(
				  QDF_MODULE_ID_INPUT,
				  QDF_TRACE_LEVEL_DEBUG,
				  " [%s] Discard data frame: key is null",
				  ether_sprintf(wlan_wbuf_getbssid
						(vdev, wh)));
			goto bad;
		} else {
			rs->rs_cryptodecapcount += (key->wk_cipher->ic_header +
						    key->wk_cipher->ic_trailer);
		}

		wh = (struct ieee80211_frame *)wbuf_header(wbuf);
		/* NB: We clear the Protected bit later */
	} else {
		key = NULL;
	}
#endif

	/*
	 * Next up, any defragmentation. A list of wbuf will be returned.
	 * However, do not defrag when in safe mode.
	 */
	if (!wlan_vdev_mlme_feat_ext_cap_get(vdev, WLAN_VDEV_FEXT_SAFEMODE)
	    && !is_mcast) {
		wbuf = ieee80211_defrag(peer, wbuf, hdrspace);
		if (wbuf == NULL) {
			/* Fragment dropped or frame not complete yet */
			QDF_TRACE(
				  QDF_MODULE_ID_INPUT,
				  QDF_TRACE_LEVEL_DEBUG,
				  "[%s] defarg: failed",
				  ether_sprintf(wlan_wbuf_getbssid
						(vdev, wh)));
			goto out;
		}
	}
	if (subtype == IEEE80211_FC0_SUBTYPE_QOS) {
#if ATH_WDS_SUPPORT_APONLY
		if (dir == IEEE80211_FC1_DIR_DSTODS)
			is_amsdu =
			    (((struct ieee80211_qosframe_addr4 *)wh)->
			     i_qos[0] & IEEE80211_QOS_AMSDU);
		else
			is_amsdu =
			    (((struct ieee80211_qosframe *)wh)->
			     i_qos[0] & IEEE80211_QOS_AMSDU);
#else
		is_amsdu =
		    ((struct ieee80211_qosframe *)wh)->
		    i_qos[0] & IEEE80211_QOS_AMSDU;
#endif
	}
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	ASSERT(!wlan_vdev_mlme_feat_ext_cap_get(vdev, WLAN_VDEV_FEXT_SAFEMODE));
	if (key_set == 1) {
		if (unlikely
		    (crypto_rx_ops && WLAN_CRYPTO_RX_OPS_DEMIC(crypto_rx_ops) && WLAN_CRYPTO_RX_OPS_DEMIC(crypto_rx_ops)(vdev, wbuf, mac, 0, frame_keyid)))
		{
			QDF_TRACE(
				  QDF_MODULE_ID_INPUT,
				  QDF_TRACE_LEVEL_DEBUG,
				  "[%s] Discard data frame: demic error",
				  wlan_peer_get_macaddr(peer));
				  /* DA change required as part of dp stats convergence */
				  /* IEEE80211_NODE_STAT(ni, rx_demicfail); */
			QDF_TRACE(
				  QDF_MODULE_ID_INPUT,
				  QDF_TRACE_LEVEL_DEBUG,
				  "[%s] demic: failed",
				  ether_sprintf(wlan_wbuf_getbssid
						(vdev, wh)));
			goto bad;
		} else {
			rs->rs_cryptodecapcount += key_miclen;
		}
	}
#else
	/*
	 * Next strip any MSDU crypto bits.
	 */
	ASSERT(!wlan_vdev_mlme_feat_ext_cap_get(vdev, WLAN_VDEV_FEXT_SAFEMODE)
	       || (key == NULL));
	if (key != NULL) {
		if (unlikely
		    (!wlan_vdev_crypto_demic(vdev, key, wbuf, hdrspace, 0, rs)))
		{
			QDF_TRACE(
				       QDF_MODULE_ID_INPUT,
				       QDF_TRACE_LEVEL_DEBUG,
				       "[%s] Discard data frame: demic error",
				       wlan_peer_get_macaddr(peer));
				/* DA change required as part of dp stats convergence */
				/* IEEE80211_NODE_STAT(ni, rx_demicfail); */
			QDF_TRACE(
				       QDF_MODULE_ID_INPUT,
				       QDF_TRACE_LEVEL_DEBUG,
				       "[%s] demic: failed",
				       ether_sprintf(wlan_wbuf_getbssid
						     (vdev, wh)));
			goto bad;
		} else {
			rs->rs_cryptodecapcount += key->wk_cipher->ic_miclen;
		}
	}
#endif
	if (subtype == IEEE80211_FC0_SUBTYPE_NODATA) {
		ieee80211_input_update_data_stats_aponly(peer,
							 mac_stats,
							 wbuf, rs, hdrsize);

		/* no need to process the null data frames any further */
		goto bad;
	}
#if ATH_RXBUF_RECYCLE
	if (is_mcast) {
		wbuf_set_cloned(wbuf);
	} else {
		wbuf_clear_cloned(wbuf);
	}
#endif
	if (!is_amsdu) {
		if (ieee80211_check_privacy_filters_aponly(peer, wbuf, is_mcast)
		    == FILTER_STATUS_REJECT) {
			QDF_TRACE(
				       QDF_MODULE_ID_INPUT,
				       QDF_TRACE_LEVEL_DEBUG,
				       "%s:[%s] Discard data frame: privacy filter check failed \n",
				       __func__, ether_sprintf(wh->i_addr2));
			goto bad;
		}
	} else {
		ieee80211_amsdu_input(peer, wbuf, rs, is_mcast, subtype);
		goto out;
	}

	mac_stats->ims_rx_packets++;

	/* TODO: Rectify the below computation after checking for side-effects. */
	mac_stats->ims_rx_bytes += wbuf_get_pktlen(wbuf);

	ieee80211_input_update_data_stats_aponly(peer, mac_stats, wbuf, rs,
						 hdrsize);

	/* consumes the wbuf */
	ieee80211_deliver_data_aponly(vdev, wbuf, peer, rs, hdrspace, is_mcast,
				      subtype);
out:
	return;

bad:
/*  FIX ME: linux specific netdev struct iv_destats has to be replaced*/
	wbuf_free(wbuf);
}

/*
 * Process a received frame.  The node associated with the sender
 * should be supplied.  If nothing was found in the node table then
 * the caller is assumed to supply a reference to iv_bss instead.
 * The RSSI and a timestamp are also supplied.  The RSSI data is used
 * during AP scanning to select a AP to associate with; it can have
 * any units so long as values have consistent units and higher values
 * mean ``better signal''.
 */

/*
 * This function is only called for unicast QoS data frames with AMPDU enabled node
 */
static int
dadp_input_aponly(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
		  struct ieee80211_rx_status *rs)
{
#define QOS_NULL   (IEEE80211_FC0_TYPE_DATA | IEEE80211_FC0_SUBTYPE_QOS_NULL)
#define HAS_SEQ(type, subtype)   (((type & 0x4) == 0) && ((type | subtype) != QOS_NULL))
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
	struct wlan_objmgr_peer *bsspeer = wlan_vdev_get_bsspeer(vdev);
	struct ieee80211_frame *wh;
	int type = -1, subtype, dir;
	u_int16_t rxseq;
	u_int8_t *bssid;

	wbuf_set_peer(wbuf, peer);

	if (!dp_peer) {
		qdf_print("%s:dp_peer is null ", __func__);
		goto bad1;
	}

	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is null ", __func__);
		goto bad1;
	}

	if (!dp_pdev) {
		qdf_print("%s:dp_pdev is null ", __func__);
		goto bad1;
	}

	if (wbuf_get_pktlen(wbuf) < dp_pdev->pdev_minframesize) {
		goto bad1;
	}

	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	if (unlikely
	    ((wh->i_fc[0] & IEEE80211_FC0_VERSION_MASK) !=
	     IEEE80211_FC0_VERSION_0)) {
		/* XXX: no stats for it. */
		goto bad1;
	}
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
	dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;

	if (wlan_vdev_rx_gate(vdev) != 0) {
		goto bad1;
	}
#if ATH_WDS_SUPPORT_APONLY
	/* Mark node as WDS */
	if (dir == IEEE80211_FC1_DIR_DSTODS)
		wlan_peer_mlme_flag_set(peer, WLAN_PEER_F_WDS);
#endif

	/*
	 * XXX Validate received frame if we're not scanning.
	 * why do we receive only data frames when we are scanning and
	 * current (foreign channel) channel is the bss channel ?
	 * should we simplify this to if (vap->iv_bsschan == ic->ic_curchan) ?
	 */

	if (dp_vdev->vdev_evtable
	    && dp_vdev->vdev_evtable->wlan_receive_filter_80211) {
		if (dp_vdev->vdev_evtable->
		    wlan_receive_filter_80211(wlan_vdev_get_osifp(vdev), wbuf,
					      type, subtype, rs)) {
			goto bad;
		}
	}

	/*
	 * Data frame, validate the bssid.
	 */
	if ((wlan_scan_in_home_channel(pdev)) ||
	    ((wlan_vdev_get_bsschan(vdev) == wlan_pdev_get_curchan(pdev))
	     && (type == IEEE80211_FC0_TYPE_DATA))) {

		if (likely(dir != IEEE80211_FC1_DIR_NODS))
			bssid = wh->i_addr1;
		else if (type == IEEE80211_FC0_TYPE_CTL)
			bssid = wh->i_addr1;
		else {
			if (wbuf_get_pktlen(wbuf) <
			    sizeof(struct ieee80211_frame)) {
				goto bad;
			}
			bssid = wh->i_addr3;
		}
		if (likely(type == IEEE80211_FC0_TYPE_DATA)) {
			if (unlikely(!IEEE80211_ADDR_EQ(bssid, wlan_peer_get_bssid(bsspeer)) && !IEEE80211_ADDR_EQ(bssid, dp_pdev->pdev_broadcast)	/* &&
																			   subtype != IEEE80211_FC0_SUBTYPE_BEACON */
				     )) {
				/* not interested in */
				goto bad;
			}
		}

		if (rs->rs_isvalidrssi) {
			wlan_peer_set_rssi(peer, rs->rs_rssi);
			wlan_peer_set_rssi_min_max(peer);
		}
		/* Check duplicates */
		if (likely(HAS_SEQ(type, subtype))) {
			u_int8_t tid;
			if (likely(IEEE80211_QOS_HAS_SEQ(wh))) {
				tid =
				    ((struct ieee80211_qosframe *)wh)->i_qos[0]
				    & IEEE80211_QOS_TID;
			} else {
				tid = IEEE80211_NON_QOS_SEQ;
			}

			rxseq = le16toh(*(u_int16_t *) wh->i_seq);
			if (unlikely((wh->i_fc[1] & IEEE80211_FC1_RETRY) &&
				     (rxseq == dp_peer->peer_rxseqs[tid]))) {
				dp_peer->peer_last_rxseqs[tid] =
				    dp_peer->peer_rxseqs[tid];
				goto bad;
			}
			dp_peer->peer_rxseqs[tid] = rxseq;
		}
	}
	/*
	 * Check for power save state change.
	 */
	if (unlikely((peer != wlan_vdev_get_bsspeer(vdev)) &&
		     !(type == IEEE80211_FC0_TYPE_MGT
		       && subtype == IEEE80211_FC0_SUBTYPE_PROBE_REQ))) {
		if ((type == IEEE80211_FC0_TYPE_CTL)
		    && (subtype == IEEE80211_FC0_SUBTYPE_PS_POLL)) {
			if ((wlan_peer_mlme_flag_get
			     (peer, WLAN_PEER_F_PWR_MGT))) {
				if (!(wh->i_fc[1] & IEEE80211_FC1_PWR_MGT)) {
					wh->i_fc[1] |= IEEE80211_FC1_PWR_MGT;
					QDF_TRACE(
						       QDF_MODULE_ID_POWER,
						       QDF_TRACE_LEVEL_DEBUG,
						       "[%s]PS poll with PM bit clear in sleep state,Keep continue sleep SM\n",
						       __func__);
				}
			} else {
				if (!(wh->i_fc[1] & IEEE80211_FC1_PWR_MGT)) {
					QDF_TRACE(
						       QDF_MODULE_ID_POWER,
						       QDF_TRACE_LEVEL_DEBUG,
						       "[%s]Rxed PS poll with PM set in awake node state,drop packet\n",
						       __func__);
					goto bad;
				}
			}
		}
		if (unlikely((((wh->i_fc[1] & IEEE80211_FC1_PWR_MGT) >> WLAN_FC1_PWR_MGT_SHIFT) ^
			     (wlan_peer_mlme_flag_get
			      (peer, WLAN_PEER_F_PWR_MGT))))) {
			(dp_pdev->scn->sc_ops->update_node_pwrsave) (dp_pdev->
								     scn->
								     sc_dev,
								     dp_peer->
								     an,
								     wh->
								     i_fc[1] &
								     IEEE80211_FC1_PWR_MGT,
								     0);
			wlan_peer_mlme_pwrsave(peer,
					       wh->
					       i_fc[1] & IEEE80211_FC1_PWR_MGT);
		}
	}

	if (likely(type == IEEE80211_FC0_TYPE_DATA)) {
		if (unlikely(wlan_vdev_is_up(vdev) != QDF_STATUS_SUCCESS)) {
			goto bad;
		}
#if QCA_SUPPORT_SON
		if ((subtype != IEEE80211_FC0_SUBTYPE_NODATA)
		    && (subtype != IEEE80211_FC0_SUBTYPE_QOS_NULL)) {
			son_mark_node_inact(peer, false);
		}
#endif
		/* ieee80211_input_data consumes the wbuf */
		wlan_peer_reload_inact(peer);	/* peer has activity */
		if (likely
		    (wlan_pdev_get_curchan(pdev) ==
		     wlan_vdev_get_bsschan(vdev)))
			ieee80211_input_data_aponly(peer, wbuf, rs, subtype,
						    dir);
		else
			goto bad;
	} else if (unlikely(type == IEEE80211_FC0_TYPE_CTL)) {
		WLAN_VAP_STATS(dp_vdev, is_rx_ctl);
		wlan_peer_recv_ctrl(peer, wbuf, subtype, rs);
		/*
		 * deliver the frame to the os. the handler cosumes the wbuf.
		 */
		if (dp_vdev->vdev_evtable &&
		    dp_vdev->vdev_evtable->wlan_receive)
			dp_vdev->vdev_evtable->wlan_receive(wlan_vdev_get_osifp(vdev),
						    wbuf, type, subtype, rs);
	} else {
		goto bad;
	}

	wlan_vdev_rx_ungate(vdev);
	return type;

bad:
	wlan_vdev_rx_ungate(vdev);
bad1:
	wbuf_free(wbuf);
	return type;
#undef HAS_SEQ
#undef QOS_NULL
}

/*
 * This function is called only for unicast QoS data frames with AMPDU nodes
 */
static int
ath_net80211_input_aponly(void *an_peer, wbuf_t wbuf,
			  ieee80211_rx_status_t * rx_status)
{
	struct wlan_objmgr_peer *peer = (struct wlan_objmgr_peer *)an_peer;
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	struct ath_softc_net80211 *scn = dp_pdev->scn;
	struct ieee80211_rx_status rs;
	int selevm;
	struct ieee80211_frame *wh;
	uint8_t frame_type;

	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	frame_type = (wh)->i_fc[0] & IEEE80211_FC0_TYPE_MASK;

	rs.rs_flags =
	    ((rx_status->
	      flags & ATH_RX_FCS_ERROR) ? IEEE80211_RX_FCS_ERROR : 0) |
	    ((rx_status->
	      flags & ATH_RX_MIC_ERROR) ? IEEE80211_RX_MIC_ERROR : 0) |
	    ((rx_status->
	      flags & ATH_RX_DECRYPT_ERROR) ? IEEE80211_RX_DECRYPT_ERROR : 0)
	    | ((rx_status->flags & ATH_RX_KEYMISS) ? IEEE80211_RX_KEYMISS : 0);
	rs.rs_isvalidrssi = (rx_status->flags & ATH_RX_RSSI_VALID) ? 1 : 0;

	rs.rs_phymode = wlan_pdev_get_curmode(scn->sc_pdev);
	rs.rs_freq = wlan_pdev_get_curchan_freq(scn->sc_pdev);
	rs.rs_rssi = rx_status->rssi;
	rs.rs_abs_rssi = rx_status->abs_rssi;
	rs.rs_datarate = rx_status->rateKbps;
	rs.rs_isaggr = rx_status->isaggr;
	rs.rs_isapsd = rx_status->isapsd;
	rs.rs_noisefloor = rx_status->noisefloor;
	rs.rs_channel = rx_status->channel;

	/* The packet is received before changing the channel, but it
	   is indicated to UMAC after changing the channel, assign
	   rs_full_chan with received channel
	 */
	if (rs.rs_channel != wlan_pdev_get_curchan_freq(pdev)) {
		rs.rs_full_chan = wlan_pdev_find_channel(pdev, rs.rs_channel);
		if (rs.rs_full_chan == NULL) {
			rs.rs_full_chan = wlan_pdev_get_curchan(pdev);
		}
	} else {
		rs.rs_full_chan = wlan_pdev_get_curchan(pdev);
	}

	rs.rs_tstamp.tsf = rx_status->tsf;

	selevm = scn->sc_ops->ath_set_sel_evm(scn->sc_dev, 0, 1);
	if (!selevm) {
		memcpy(rs.rs_lsig, rx_status->lsig, IEEE80211_LSIG_LEN);
		memcpy(rs.rs_htsig, rx_status->htsig, IEEE80211_HTSIG_LEN);
		memcpy(rs.rs_servicebytes, rx_status->servicebytes,
		       IEEE80211_SB_LEN);
#if UMAC_PER_PACKET_DEBUG
		DPRINTF(ATH_DEV_TO_SC(scn->sc_dev), ATH_DEBUG_RECV,
			"  %d %d %d     %d %d %d %d %d %d       "
			"  %d %d  \n\n", rx_status->lsig[0], rx_status->lsig[1],
			rx_status->lsig[2], rx_status->htsig[0],
			rx_status->htsig[1], rx_status->htsig[2],
			rx_status->htsig[3], rx_status->htsig[4],
			rx_status->htsig[5], rx_status->servicebytes[0],
			rx_status->servicebytes[1]);
#endif
	} else {
		memset(rs.rs_lsig, 0, IEEE80211_LSIG_LEN);
		memset(rs.rs_htsig, 0, IEEE80211_HTSIG_LEN);
		memset(rs.rs_servicebytes, 0, IEEE80211_SB_LEN);
	}

	ath_txbf_update_rx_status(&rs, rx_status);
	rs.rs_rssi = rx_status->rssi;
#if ATH_VOW_EXT_STATS
	rs.vow_extstats_offset = rx_status->vow_extstats_offset;
#endif

	return dadp_input_aponly(peer, wbuf, &rs);
}

/*
 * Function to handle a subframe of aggregation when HT is enabled
 */
static inline int
ath_ampdu_input_aponly(struct ath_softc *sc, struct ath_node *an, wbuf_t wbuf,
		       ieee80211_rx_status_t * rx_status)
{
	struct ieee80211_frame *wh;
	struct ieee80211_qosframe *whqos;
	u_int8_t type, subtype;
	int ismcast;
	int tid;
	struct ath_arx_tid *rxtid;
	int index, cindex, rxdiff;
	u_int16_t rxseq;
	struct ath_rxbuf *rxbuf;
	wbuf_t wbuf_to_indicate;
	int dir, selevm;
#if ATH_WDS_SUPPORT_APONLY
	int is4addr;
	struct ieee80211_qosframe_addr4 *whqos_4addr;
#endif

	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
#if ATH_WDS_SUPPORT_APONLY
	is4addr =
	    (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS;
#endif

	__11nstats(sc, rx_aggr);
	/*
	 * collect stats of frames with non-zero version
	 */
	if (unlikely
	    ((wh->i_fc[0] & IEEE80211_FC0_VERSION_MASK) !=
	     IEEE80211_FC0_VERSION_0)) {
		__11nstats(sc, rx_aggrbadver);
		wbuf_free(wbuf);
		return -1;
	}

	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
	dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;
	ismcast = IEEE80211_IS_MULTICAST(wh->i_addr1);

	/*
	 * This argument also includes bcast case. For bcast, addr1 and addr3 both with LSB 1,
	 * so ismcast=1
	 */

	if (unlikely((type == IEEE80211_FC0_TYPE_CTL) &&
		     (subtype == IEEE80211_FC0_SUBTYPE_BAR))) {
		return ath_bar_rx(sc, an, wbuf);
	}

	/*
	 * special aggregate processing only for qos unicast data frames
	 */
	/*
	 * Call the fast path in ap-only thread only for aggregated QoS unicast data frames.
	 * Otherwise, call the generic rx path
	 */
	if (unlikely(type != IEEE80211_FC0_TYPE_DATA ||
		     subtype != IEEE80211_FC0_SUBTYPE_QOS || (ismcast))) {
		__11nstats(sc, rx_nonqos);
		return ath_net80211_input(an->an_node, wbuf, rx_status);
	}

	/*
	 * lookup rx tid state
	 */
#if ATH_WDS_SUPPORT_APONLY
	if (is4addr) {		/* special qos check for 4 address frames */
		whqos_4addr = (struct ieee80211_qosframe_addr4 *)wh;
		tid = whqos_4addr->i_qos[0] & IEEE80211_QOS_TID;
	} else {
		whqos = (struct ieee80211_qosframe *)wh;
		tid = whqos->i_qos[0] & IEEE80211_QOS_TID;
	}
#else
	whqos = (struct ieee80211_qosframe *)wh;
	tid = whqos->i_qos[0] & IEEE80211_QOS_TID;
#endif
	rxtid = &an->an_aggr.rx.tid[tid];

	/* if there at least one frame completed in video class, make
	 * sure that we disable the PHY restart on BB hang.
	 * Also make sure that we avoid the many register writes in
	 * rx-tx path
	 */

	ATH_RXTID_LOCK(rxtid);

	/*
	 * If the ADDBA exchange has not been completed by the source,
	 * process via legacy path (i.e. no reordering buffer is needed)
	 */
	if (unlikely(!rxtid->addba_exchangecomplete)) {
		ATH_RXTID_UNLOCK(rxtid);
		__11nstats(sc, rx_nonqos);
		return ath_net80211_input(an->an_node, wbuf, rx_status);
	}
#if ATH_SUPPORT_TIDSTUCK_WAR
	rxtid->rxtid_lastdata = OS_GET_TIMESTAMP();
#endif

	/*
	 * extract sequence number from recvd frame
	 */
	rxseq = le16toh(*(u_int16_t *) wh->i_seq) >> IEEE80211_SEQ_SEQ_SHIFT;

	if (unlikely(rxtid->seq_reset)) {
		__11nstats(sc, rx_seqreset);
		rxtid->seq_reset = 0;
		rxtid->seq_next = rxseq;
	}

	index = ATH_BA_INDEX(rxtid->seq_next, rxseq);

	/*
	 * drop frame if old sequence (index is too large)
	 */
	if (unlikely(index > (IEEE80211_SEQ_MAX - (rxtid->baw_size << 2)))) {
		/*
		 * discard frame, ieee layer may not treat frame as a dup
		 */
		ATH_RXTID_UNLOCK(rxtid);
		__11nstats(sc, rx_oldseq);
		wbuf_free(wbuf);
		return IEEE80211_FC0_TYPE_DATA;
	}

	/*
	 * sequence number is beyond block-ack window
	 */
	if (unlikely(index >= rxtid->baw_size)) {

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
				ath_net80211_input_aponly(an->an_peer,
							  wbuf_to_indicate,
							  &rxbuf->rx_status);
				__11nstats(sc, rx_recvcomp);
			}

			index--;
		}
	}

	/*
	 * add buffer to the recv ba window
	 */
	cindex = (rxtid->baw_head + index) & (ATH_TID_MAX_BUFS - 1);
	rxbuf = rxtid->rxbuf + cindex;

	if (unlikely(rxbuf->rx_wbuf != NULL)) {
		/*
		 *duplicate frame
		 */
		DPRINTF(sc, ATH_DEBUG_ANY,
			"%s[%d]:Dup frame tid %d, cindex %d, baw_head %d, baw_tail %d, seq_next %d\n",
			__func__, __LINE__, tid, cindex, rxtid->baw_head,
			rxtid->baw_tail, rxtid->seq_next);
		ATH_RXTID_UNLOCK(rxtid);
		__11nstats(sc, rx_dup);
		wbuf_free(wbuf);
		return IEEE80211_FC0_TYPE_DATA;
	}

	rxbuf->rx_wbuf = wbuf;
	rxbuf->rx_time = OS_GET_TIMESTAMP();
	rxbuf->rx_status.flags = rx_status->flags;
	rxbuf->rx_status.rssi = rx_status->rssi;
	selevm = ath_hal_setrxselevm(sc->sc_ah, 0, 1);

	if (!selevm) {
		memcpy(rxbuf->rx_status.lsig, rx_status->lsig,
		       IEEE80211_LSIG_LEN);
		memcpy(rxbuf->rx_status.htsig, rx_status->htsig,
		       IEEE80211_HTSIG_LEN);
		memcpy(rxbuf->rx_status.servicebytes, rx_status->servicebytes,
		       IEEE80211_SB_LEN);
	}

	rxdiff = (rxtid->baw_tail - rxtid->baw_head) & (ATH_TID_MAX_BUFS - 1);

	/*
	 * advance tail if sequence received is newer than any received so far
	 */
	if (unlikely(index >= rxdiff)) {
		__11nstats(sc, rx_baadvance);
		rxtid->baw_tail = cindex;
		INCR(rxtid->baw_tail, ATH_TID_MAX_BUFS);
	}

	/*
	 * indicate all in-order received frames
	 */
	while (rxtid->baw_head != rxtid->baw_tail) {
		rxbuf = rxtid->rxbuf + rxtid->baw_head;
		if (unlikely(!rxbuf->rx_wbuf))
			break;

		__11nstats(sc, rx_recvcomp);

		INCR(rxtid->baw_head, ATH_TID_MAX_BUFS);
		INCR(rxtid->seq_next, IEEE80211_SEQ_MAX);

		wbuf_to_indicate = rxbuf->rx_wbuf;
		rxbuf->rx_wbuf = NULL;
		ath_net80211_input_aponly(an->an_peer, wbuf_to_indicate,
					  &rxbuf->rx_status);
	}

	/*
	 * start a timer to flush all received frames if there are pending
	 * receive frames
	 */
	if (unlikely(rxtid->baw_head != rxtid->baw_tail)) {
		if (!ATH_RXTIMER_IS_ACTIVE(&rxtid->timer)) {
			__11nstats(sc, rx_timer_starts);
			ATH_SET_RXTIMER_PERIOD(&rxtid->timer,
					       sc->
					       sc_rxtimeout[TID_TO_WME_AC
							    (tid)]);
			ATH_START_RXTIMER(&rxtid->timer);
		}
	} else {
		if (ATH_RXTIMER_IS_ACTIVE(&rxtid->timer)) {
			__11nstats(sc, rx_timer_stops);
		}
		ATH_CANCEL_RXTIMER(&rxtid->timer, CANCEL_NO_SLEEP);
	}

	ATH_RXTID_UNLOCK(rxtid);
	return IEEE80211_FC0_TYPE_DATA;
}

#define	IS_CTL(wh)  \
    ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_CTL)
#define	IS_PSPOLL(wh)   \
    ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) == IEEE80211_FC0_SUBTYPE_PS_POLL)
#define	IS_BAR(wh) \
    ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) == IEEE80211_FC0_SUBTYPE_BAR)

static inline int
ath_net80211_rx_aponly(struct wlan_objmgr_pdev *pdev, wbuf_t wbuf,
		       ieee80211_rx_status_t * rx_status, u_int16_t keyix)
{
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	struct mgmt_rx_event_params rx_event = { 0 };
	struct ath_softc_net80211 *scn = NULL;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct dadp_peer *dp_peer = NULL;
	struct ath_node *an;
	struct ieee80211_frame *wh;
	int type, selevm;
	int32_t rssi = 0;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_peer *peer;
	uint8_t frame_type;

	if (!dp_pdev) {
		qdf_print("%s:dp_pdev is NULL ", __func__);
		return -1;
	}
	scn = dp_pdev->scn;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		qdf_print("%s:psoc is NULL ", __func__);
		return -1;
	}

	/*
	 * From this point on we assume the frame is at least
	 * as large as ieee80211_frame_min; verify that.
	 */
	if (NULL != dp_pdev->pdev_mon_vdev) {
		wbuf_t wbuf_orig = qdf_nbuf_copy((qdf_nbuf_t) wbuf);
		if (wbuf_orig) {
			wbuf_t prev_wbuf = wbuf_orig;
			wbuf_t wbuf_tmp = qdf_nbuf_next((qdf_nbuf_t) wbuf);
			wbuf_t wbuf_cpy = NULL;

			while (wbuf_tmp) {
				wbuf_cpy = qdf_nbuf_copy((qdf_nbuf_t) wbuf_tmp);
				if (!wbuf_cpy)
					break;
				qdf_nbuf_set_next_ext((qdf_nbuf_t) prev_wbuf,
						      (qdf_nbuf_t) wbuf_cpy);
				prev_wbuf = wbuf_cpy;
				wbuf_tmp = qdf_nbuf_next((qdf_nbuf_t) wbuf_tmp);
			}

			qdf_nbuf_set_next_ext((qdf_nbuf_t) prev_wbuf, NULL);
			ath_net80211_rx_monitor(pdev, wbuf_orig, rx_status);
		}
	}

	if (unlikely
	    (wbuf_get_pktlen(wbuf) <
	     (dp_pdev->pdev_minframesize + IEEE80211_CRC_LEN))) {
		DPRINTF((struct ath_softc *)scn->sc_dev, ATH_DEBUG_RECV,
			"%s: short packet %d\n", __func__,
			wbuf_get_pktlen(wbuf));
		wbuf_free(wbuf);
		return -1;
	}
#ifdef ATH_SUPPORT_TxBF
	ath_net80211_bf_rx(pdev, wbuf, rx_status);
#endif

	/*
	 * Normal receive.
	 */
	wbuf_trim(wbuf, IEEE80211_CRC_LEN);

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	/* tbd_da_crypto: Handle wep mbssid keymiss */
#else
	/*
	 * Handle packets with keycache miss if WEP MBSSID
	 * is enabled.
	 */
	{
		struct ieee80211_rx_status rs;
		rs.rs_flags =
		    ((rx_status->
		      flags & ATH_RX_DECRYPT_ERROR) ? IEEE80211_RX_DECRYPT_ERROR
		     : 0) | ((rx_status->
			      flags & ATH_RX_KEYMISS) ? IEEE80211_RX_KEYMISS :
			     0);
		if (ieee80211_crypto_handle_keymiss(pdev, wbuf, &rs))
			return -1;
	}
#endif
	/*
	 * Locate the node for sender, track state, and then
	 * pass the (referenced) node up to the 802.11 layer
	 * for its use.  If the sender is unknown spam the
	 * frame; it'll be dropped where it's not wanted.
	 */
	IEEE80211_KEYMAP_LOCK(scn);
	peer = (keyix != HAL_RXKEYIX_INVALID) ? scn->sc_keyixmap[keyix] : NULL;

	/* check if lookup is right -- using mac address in packet */
	if (qdf_likely(peer != NULL)) {
		bool correct = true;
		wh = (struct ieee80211_frame *)wbuf_header(wbuf);
		if (IS_CTL(wh) && !IS_PSPOLL(wh) && !IS_BAR(wh))
			correct =
			    (IEEE80211_ADDR_EQ
			     (wlan_peer_get_macaddr(peer), wh->i_addr1));
		else
			correct =
			    (IEEE80211_ADDR_EQ
			     (wlan_peer_get_macaddr(peer), wh->i_addr2));

		if (!correct) {
			peer = NULL;
		}
	}
	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	frame_type = (wh)->i_fc[0] & IEEE80211_FC0_TYPE_MASK;

	if (peer == NULL) {
		IEEE80211_KEYMAP_UNLOCK(scn);
		/*
		 * No key index or no entry, do a lookup and
		 * add the node to the mapping table if possible.
		 */
		peer = wlan_find_rxpeer(pdev, (struct ieee80211_frame_min *)
					wbuf_header(wbuf));
		/* If we receive a probereq with broadcast bssid which is usually
		 * sent by sender to discover new networks, all the vaps should send response */
		if (peer == NULL
		    || (IEEE80211_IS_PROBEREQ(wh)
			&& IEEE80211_IS_BROADCAST(wh->i_addr3))) {
			struct ieee80211_rx_status rs;
			rs.rs_flags =
			    ((rx_status->
			      flags & ATH_RX_FCS_ERROR) ? IEEE80211_RX_FCS_ERROR
			     : 0) | ((rx_status->
				      flags & ATH_RX_MIC_ERROR) ?
				     IEEE80211_RX_MIC_ERROR : 0) | ((rx_status->
								     flags &
								     ATH_RX_DECRYPT_ERROR)
								    ?
								    IEEE80211_RX_DECRYPT_ERROR
								    : 0)
			    | ((rx_status->flags & ATH_RX_KEYMISS) ?
			       IEEE80211_RX_KEYMISS : 0);
			rs.rs_isvalidrssi =
			    (rx_status->flags & ATH_RX_RSSI_VALID) ? 1 : 0;

			rs.rs_phymode = wlan_pdev_get_curmode(scn->sc_pdev);
			rs.rs_freq = wlan_pdev_get_curchan_freq(scn->sc_pdev);
			rs.rs_rssi = rx_status->rssi;
			rs.rs_abs_rssi = rx_status->abs_rssi;
			rs.rs_datarate = rx_status->rateKbps;
			rs.rs_isaggr = rx_status->isaggr;
			rs.rs_isapsd = rx_status->isapsd;
			rs.rs_noisefloor = rx_status->noisefloor;
			rs.rs_channel = rx_status->channel;

			/* The packet is received before changing the channel, but it
			   is indicated to UMAC after changing the channel, assign
			   rs_full_chan with received channel
			 */
			if (rs.rs_channel !=
			    wlan_pdev_get_curchan_freq(scn->sc_pdev)) {
				rs.rs_full_chan =
				    wlan_pdev_find_channel(pdev, rs.rs_channel);
				if (rs.rs_full_chan == NULL) {
					rs.rs_full_chan =
					    wlan_pdev_get_curchan(pdev);
				}
			} else {
				rs.rs_full_chan = wlan_pdev_get_curchan(pdev);
			}

			rs.rs_tstamp.tsf = rx_status->tsf;

			selevm =
			    scn->sc_ops->ath_set_sel_evm(scn->sc_dev, 0, 1);

			if (!selevm) {
				memcpy(rs.rs_lsig, rx_status->lsig,
				       IEEE80211_LSIG_LEN);
				memcpy(rs.rs_htsig, rx_status->htsig,
				       IEEE80211_HTSIG_LEN);
				memcpy(rs.rs_servicebytes,
				       rx_status->servicebytes,
				       IEEE80211_SB_LEN);
			}

			ath_txbf_update_rx_status(&rs, rx_status);
			rs.rs_rssi = rx_status->rssi;
#if ATH_VOW_EXT_STATS
			rs.vow_extstats_offset = rx_status->vow_extstats_offset;
#endif
			if (peer)
				wlan_objmgr_peer_release_ref(peer,
							     WLAN_MLME_SB_ID);
			/*
			 * hand over mgmt frames to the mgmt_txrx layer
			 * for further processing.
			 */
			if (frame_type == IEEE80211_FC0_TYPE_MGT) {

				struct ieee80211com *ic =
				    (struct ieee80211com *)
				    wlan_mlme_get_pdev_ext_hdl(pdev);
				rs.rs_ic = (void *)ic;
				if (peer)
					rs.rs_ni =
					    (void *)
					    wlan_mlme_get_peer_ext_hdl(peer);
				else
					rs.rs_ni = NULL;
				/*
				 * rs to be made available in the placeholder
				 * in rx_event structure used by mgmt_txrx
				 */
				rx_event.rx_params = (void *)&rs;
				rx_event.channel =
				    ieee80211_mhz2ieee(ic, rs.rs_freq, 0);
				rx_event.snr = rs.rs_rssi;
				return mgmt_txrx_rx_handler(psoc, wbuf,
							    (void *)&rx_event);
			}
			return dadp_input_all(pdev, wbuf, &rs);
		}
	} else {
		if (wlan_objmgr_peer_try_get_ref(peer, WLAN_MLME_SB_ID) != QDF_STATUS_SUCCESS) {
			IEEE80211_KEYMAP_UNLOCK(scn);
			wbuf_free(wbuf);
			return -EINVAL;
		}
		IEEE80211_KEYMAP_UNLOCK(scn);
	}

	vdev = wlan_peer_get_vdev(peer);
	if (!vdev) {
		qdf_print("%s:vdev is NULL ", __func__);
		wlan_objmgr_peer_release_ref(peer, WLAN_MLME_SB_ID);
		wbuf_free(wbuf);
		return -EINVAL;
	}

	dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer) {
		qdf_print("%s:dp_peer is NULL ", __func__);
		wlan_objmgr_peer_release_ref(peer, WLAN_MLME_SB_ID);
		wbuf_free(wbuf);
		return -EINVAL;
	}

	/*
	 * update node statistics
	 */
	an = dp_peer->an;
	if (an == NULL) {
		wlan_objmgr_peer_release_ref(peer, WLAN_MLME_SB_ID);
		return -1;
	}
	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	if (unlikely(IEEE80211_IS_DATA(wh) && rx_status->nomoreaggr)) {
		ATH_RATE_LPF(an->an_avgrxrate, rx_status->rateKbps);
	}

	/* For STA, update RSSI info from associated BSSID only. Don't update RSSI, if we
	   recv pkt from another BSSID(probe resp etc.)
	 */
	if (unlikely
	    ((rx_status->flags & ATH_RX_RSSI_VALID) && (rx_status->nomoreaggr)
	     && IEEE80211_IS_DATA(wh))) {
		int i;
		ATH_RSSI_LPF(an->an_avgrssi, rx_status->rssi);
		ATH_RSSI_LPF(an->an_avgdrssi, rx_status->rssi);

		if (rx_status->flags & ATH_RX_CHAIN_RSSI_VALID) {
			for (i = 0; i < ATH_HOST_MAX_ANTENNA; ++i) {
				ATH_RSSI_LPF(an->an_avgchainrssi[i],
					     rx_status->rssictl[i]);
				ATH_RSSI_LPF(an->an_avgdchainrssi[i],
					     rx_status->rssictl[i]);
			}
			if (rx_status->flags & ATH_RX_RSSI_EXTN_VALID) {
				for (i = 0; i < ATH_HOST_MAX_ANTENNA; ++i) {
					ATH_RSSI_LPF(an->an_avgchainrssiext[i],
						     rx_status->rssiextn[i]);
					ATH_RSSI_LPF(an->an_avgdchainrssiext[i],
						     rx_status->rssiextn[i]);
				}
			}
		}

	}

	rssi = an->an_avgrssi;
	if (rssi != ATH_RSSI_DUMMY_MARKER) {
		rssi = ATH_EP_RND(rssi, HAL_RSSI_EP_MULTIPLIER);
		/* check if we need to send deauth if rssi for this node is low */
		if (dp_pdev->pdev_min_rssi_enable) {
			if (wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE) {
				/* compare the user provided rssi with peer rssi received */
				if (wlan_peer_get_associd(peer)
				    && (dp_pdev->pdev_min_rssi > rssi)) {
					/* send de-auth to ni_macaddr */
					printk
					    ("Client %s(snr = %u) de-authed due to insufficient SNR\n",
					     ether_sprintf(wlan_peer_get_macaddr
							   (peer)), rssi);
					wlan_vdev_deauth_request(vdev,
								 wlan_peer_get_macaddr
								 (peer),
								 IEEE80211_REASON_UNSPECIFIED);
					wlan_objmgr_peer_release_ref(peer,
								     WLAN_MLME_SB_ID);
					return -1;
				}
			}
		}
	}
#if ATH_SUPPORT_KEYPLUMB_WAR
	if (rx_status->flags & ATH_RX_DECRYPT_ERROR) {
		an->an_decrypt_err++;
		DPRINTF((struct ath_softc *)scn->sc_dev, ATH_DEBUG_RECV,
			"%s decrypt error. STA (%s)  keyix (%d) rx_status flags (%d)\n",
			__FUNCTION__, ether_sprintf(an->an_macaddr),
			an->an_keyix, rx_status->flags);

		if (an->an_decrypt_err >= MAX_DECRYPT_ERR_KEYPLUMB) {
			DPRINTF((struct ath_softc *)scn->sc_dev,
				ATH_DEBUG_KEYCACHE,
				"%s: Check hw key for STA (%s). Key might be corrupted\n",
				__FUNCTION__, ether_sprintf(an->an_macaddr));
			if (!ath_checkandplumb_key
			    ((struct ath_softc *)scn->sc_dev, an,
			     an->an_keyix)) {
				DPRINTF((struct ath_softc *)scn->sc_dev,
					ATH_DEBUG_KEYCACHE,
					"%s: Key plumbed again for STA (%s). Key is corrupted\n",
					__FUNCTION__,
					ether_sprintf(an->an_macaddr));
			}
		}
	} else {
		an->an_decrypt_err = 0;
	}
#endif

	/*
	 * Let ath_dev do some special rx frame processing. If the frame is not
	 * consumed by ath_dev, indicate it up to the stack.
	 * if ATH_WDS_SUPPORT_APONLY is supported, then only STA WDS packets should go thorugh generic path
	 * as STA WDS path is not optimized now and only AP WDS packets should go through aponly path.
	 * if ATH_WDS_SUPPORT_APONLY is not supported then all WDS packets should go through generic path.
	 */
	if (likely
	    (IEEE80211_PEER_ISAMPDU(peer)
	     && ((struct ath_softc *)scn->sc_dev)->sc_rxaggr)) {
		u_int32_t input_aponly = TRUE;

		do {
			if (unlikely
			    (wlan_vdev_mlme_get_opmode(vdev) != QDF_SAP_MODE)) {
				input_aponly = FALSE;
				break;
			}
			if (unlikely
			    (wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_AP)))
			{
				input_aponly = FALSE;
				break;
			}
#if !defined(ATH_WDS_SUPPORT_APONLY)
			if (unlikely
			    (wlan_vdev_mlme_feat_cap_get
			     (vdev, WLAN_VDEV_F_WDS))) {
				input_aponly = FALSE;
				break;
			}
#endif
		} while (0);

		type = input_aponly ? ath_ampdu_input_aponly(scn->sc_dev,
							     ATH_NODE(dp_peer->
								      an), wbuf,
							     rx_status)
		    : ath_ampdu_input(scn->sc_dev, ATH_NODE(dp_peer->an), wbuf,
				      rx_status);
	} else {
		struct ieee80211_node *ni = wlan_mlme_get_peer_ext_hdl(peer);
		type = ath_net80211_input(ni, wbuf, rx_status);
	}

	wlan_objmgr_peer_release_ref(peer, WLAN_MLME_SB_ID);
	return type;
}

/*
 * Setup and link descriptors.
 *
 * 11N: we can no longer afford to self link the last descriptor.
 * MAC acknowledges BA status as long as it copies frames to host
 * buffer (or rx fifo). This can incorrectly acknowledge packets
 * to a sender if last desc is self-linked.
 *
 * NOTE: Caller should hold the rxbuf lock.
 */
/*
 * Add a wbuf from the free list to the rx fifo.
 * Context: Interrupt
 * NOTE: Caller should hold the rxbuf lock.
 */
static inline void
ath_rx_buf_link_aponly(struct ath_softc *sc, struct ath_buf *bf,
		       HAL_RX_QUEUE qtype)
{
	struct ath_hal *ah = sc->sc_ah;
	struct ath_rx_edma *rxedma;

	rxedma = &sc->sc_rxedma[qtype];

//    ATH_RXBUF_RESET(bf);
#ifdef ATH_RX_DESC_WAR
	bf->bf_status = 0;
#endif
	/* Reset the status part */
	OS_MEMZERO(wbuf_raw_data(bf->bf_mpdu), sc->sc_rxstatuslen);

	/*
	 ** Since the descriptor header (48 bytes, which is 64 bytes, 2-3 cache lines
	 ** depending on alignment) is cached, we need to sync to ensure harware sees
	 ** the proper information, and we don't get inconsistent cache data.  So sync
	 */

	OS_SYNC_SINGLE(sc->sc_osdev, bf->bf_buf_addr[0], sc->sc_rxstatuslen,
		       BUS_DMA_TODEVICE, OS_GET_DMA_MEM_CONTEXT(bf,
								bf_dmacontext));

	rxedma->rxfifo[rxedma->rxfifotailindex] = bf->bf_mpdu;

	/* advance the tail pointer */
	INCR(rxedma->rxfifotailindex, rxedma->rxfifohwsize);

	rxedma->rxfifodepth++;

	/* push this buffer in the MAC Rx fifo */
	ath_hal_putrxbuf(ah, bf->bf_buf_addr[0], qtype);

}

/*
 * XXX TODO the following is for non-edma case, need to add it later
 */
#if 0

static void
ath_rx_buf_link_aponly(struct ath_softc *sc, struct ath_buf *bf, bool rxenable)
{
#define DESC2PA(_sc, _va)\
    ((caddr_t)(_va) - (caddr_t)((_sc)->sc_rxdma.dd_desc) +  \
     (_sc)->sc_rxdma.dd_desc_paddr)

	struct ath_hal *ah = sc->sc_ah;
	struct ath_desc *ds;
	wbuf_t wbuf;

	ATH_RXBUF_RESET(bf);

	/* Acquire lock to have mutual exclusion with the Reset code calling ath_hal_reset */
	ATH_RESET_LOCK(sc);

	ds = bf->bf_desc;
	ds->ds_link = 0;	/* link to null */
	ds->ds_data = bf->bf_buf_addr[0];
	/* XXX For RADAR?
	 * virtual addr of the beginning of the buffer. */
	wbuf = bf->bf_mpdu;
	ASSERT(wbuf != NULL);
	bf->bf_vdata = wbuf_raw_data(wbuf);

	/* setup rx descriptors */
	ath_hal_setuprxdesc(ah, ds, wbuf_get_len(wbuf)	/* buffer size */
			    , 0);

#if ATH_RESET_SERIAL
	if (atomic_read(&sc->sc_hold_reset)) {	//hold lock
		ATH_RESET_UNLOCK(sc);
		return;
	} else {
		if (atomic_read(&sc->sc_rx_hw_en) > 0) {
			atomic_inc(&sc->sc_rx_return_processing);
			ATH_RESET_UNLOCK(sc);
		} else {
			ATH_RESET_UNLOCK(sc);
			return;
		}
	}

#else

	if (atomic_read(&sc->sc_rx_hw_en) <= 0) {
		/* ath_stoprecv() has already being called. Do not queue to hardware. */
		ATH_RESET_UNLOCK(sc);
		return;
	}
#endif

	if (sc->sc_rxlink == NULL)
		ath_hal_putrxbuf(ah, bf->bf_daddr, 0);
	else {
		*sc->sc_rxlink = bf->bf_daddr;

		OS_SYNC_SINGLE(sc->sc_osdev,
			       (dma_addr_t) (DESC2PA(sc, sc->sc_rxlink)),
			       sizeof(*sc->sc_rxlink), BUS_DMA_TODEVICE, NULL);
	}

	sc->sc_rxlink = &ds->ds_link;

	if (rxenable && !atomic_read(&sc->sc_in_reset)) {
#ifdef DBG
		if (ath_hal_getrxbuf(ah, 0) == 0) {
			/* This will cause an NMI since RXDP is 0 */
			printk
			    ("%s: FATAL ERROR: NULL RXDP while enabling RX.\n",
			     __func__);
			ASSERT(FALSE);
		} else
#endif
			ath_hal_rxena(ah);
	}
#if ATH_RESET_SERIAL
	atomic_dec(&sc->sc_rx_return_processing);
#else
	ATH_RESET_UNLOCK(sc);
#endif
#undef DESC2PA
}
#endif

static void ath_rx_removebuffer_aponly(struct ath_softc *sc, HAL_RX_QUEUE qtype)
{
	int i, size;
	struct ath_buf *bf;
	wbuf_t wbuf;
	struct ath_rx_edma *rxedma;

	rxedma = &sc->sc_rxedma[qtype];

	size = rxedma->rxfifohwsize;

	/* Remove all buffers from rx queue and insert in free queue */
	for (i = 0; i < size; i++) {
		wbuf = rxedma->rxfifo[i];
		if (wbuf) {
			bf = ATH_GET_RX_CONTEXT_BUF(wbuf);
			if (!bf) {
				printk("%s[%d] PANIC wbuf %pK Index %d\n",
				       __func__, __LINE__, wbuf, i);
			} else {
				TAILQ_INSERT_TAIL(&sc->sc_rxbuf, bf, bf_list);
			}
			rxedma->rxfifo[i] = NULL;
			rxedma->rxfifodepth--;
		}
	}

	/* reset head and tail indices */
	rxedma->rxfifoheadindex = 0;
	rxedma->rxfifotailindex = 0;
	if (rxedma->rxfifodepth)
		printk("PANIC depth non-zero %d\n", rxedma->rxfifodepth);
}

static void
ath_rx_addbuffer_aponly(struct ath_softc *sc, HAL_RX_QUEUE qtype, int size)
{
	int i;
	struct ath_buf *bf, *tbf;
	struct ath_rx_edma *rxedma;

	rxedma = &sc->sc_rxedma[qtype];

	if (TAILQ_EMPTY(&sc->sc_rxbuf)) {
		DPRINTF(sc, ATH_DEBUG_RX_PROC, "%s[%d]: Out of buffers\n",
			__func__, __LINE__);
		return;
	}

	/* Add free buffers to rx queue */
	i = 0;
	TAILQ_FOREACH_SAFE(bf, &sc->sc_rxbuf, bf_list, tbf) {
		if (i == size)
			break;

		TAILQ_REMOVE(&sc->sc_rxbuf, bf, bf_list);
		if (bf == NULL) {
			DPRINTF(sc, ATH_DEBUG_RX_PROC,
				"%s[%d]: Out of buffers\n", __func__, __LINE__);
			break;
		}
		i++;
		ath_rx_buf_link_aponly(sc, bf, qtype);
	}
}

#ifdef ATH_SUPPORT_UAPSD
static inline void
ath_rx_process_uapsd_aponly(struct ath_softc *sc, HAL_RX_QUEUE qtype,
			    wbuf_t wbuf, struct ath_rx_status *rxs,
			    bool isr_context)
{
	struct ieee80211_qosframe *qwh;

	if (!sc->sc_hwuapsdtrig) {
		/* Adjust wbuf start addr to point to data, i.e skip past the RxS */
		qwh = (struct ieee80211_qosframe *)
		    ((u_int8_t *) wbuf_raw_data(wbuf) + sc->sc_rxstatuslen);

		/* HW Uapsd trig is not supported - Process all recv frames for uapsd triggers */
		sc->sc_ieee_ops->check_uapsdtrigger(sc->sc_pdev, qwh,
						    rxs->rs_keyix, isr_context);
	} else if (qtype == HAL_RX_QUEUE_HP) {
		/* Adjust wbuf start addr to point to data, i.e skip past the RxS */
		qwh = (struct ieee80211_qosframe *)
		    ((u_int8_t *) wbuf_raw_data(wbuf) + sc->sc_rxstatuslen);

		/* HW Uapsd trig is supported - do uapsd processing only for HP queue */
		sc->sc_ieee_ops->uapsd_deliverdata(sc->sc_ieee, qwh,
						   rxs->rs_keyix,
						   rxs->rs_isapsd, isr_context);
	}
}
#endif /* ATH_SUPPORT_UAPSD */

void ath_rx_intr_aponly(ath_dev_t dev, HAL_RX_QUEUE qtype)
{
	struct ath_softc *sc = ATH_DEV_TO_SC(dev);
	struct ath_rx_edma *rxedma;
	wbuf_t wbuf;
	struct ath_buf *bf;
	struct ath_rx_status *rxs;
	HAL_STATUS retval;
	struct ath_hal *ah = sc->sc_ah;
	int frames;

#if !ATH_RESET_SERIAL
	if (atomic_read(&sc->sc_in_reset))
		return;
#endif
	rxedma = &sc->sc_rxedma[qtype];

	do {
		wbuf = rxedma->rxfifo[rxedma->rxfifoheadindex];
		if (unlikely(wbuf == NULL))
			break;
		bf = ATH_GET_RX_CONTEXT_BUF(wbuf);

		/*
		 * Invalidate the status bytes alone since we flush them (to clear status)
		 * after unmapping the buffer while queuing it to h/w.
		 */
		OS_SYNC_SINGLE(sc->sc_osdev,
			       bf->bf_buf_addr[0], sc->sc_rxstatuslen,
			       BUS_DMA_FROMDEVICE, OS_GET_DMA_MEM_CONTEXT(bf,
									  bf_dmacontext));
		bf->bf_status |= ATH_BUFSTATUS_SYNCED;

		rxs = bf->bf_desc;
		retval =
		    ath_hal_rxprocdescfast(ah, NULL, 0, NULL, rxs,
					   wbuf_raw_data(wbuf));
#ifdef ATH_RX_DESC_WAR
		if (unlikely(HAL_EINVAL == retval)) {
			struct ath_buf *next_bf;
			wbuf_t next_wbuf;
			u_int32_t next_idx = rxedma->rxfifoheadindex;

			bf->bf_status |= ATH_BUFSTATUS_WAR;

			INCR(next_idx, rxedma->rxfifohwsize);
			next_wbuf = rxedma->rxfifo[next_idx];

			if (next_wbuf == NULL)
				break;

			next_bf = ATH_GET_RX_CONTEXT_BUF(next_wbuf);
			next_bf->bf_status |= ATH_BUFSTATUS_WAR;
			DPRINTF(sc, ATH_DEBUG_RX_PROC,
				"%s: Marking first DP 0x%x for drop\n",
				__func__, (unsigned)bf->bf_buf_addr[0]);
			DPRINTF(sc, ATH_DEBUG_RX_PROC,
				"%s: Marking second DP 0x%x for drop\n",
				__func__, (unsigned)next_bf->bf_buf_addr[0]);
		}
#endif
		/* XXX Check for done bit in RxS */
		if (HAL_EINPROGRESS == retval) {
			break;
		}
#ifdef ATH_SUPPORT_UAPSD
		/* Process UAPSD triggers */
		/* Skip frames with error - except HAL_RXERR_KEYMISS since
		 * for static WEP case, all the frames will be marked with HAL_RXERR_KEYMISS,
		 * since there is no key cache entry added for associated station in that case
		 */
		if (likely((rxs->rs_status & ~HAL_RXERR_KEYMISS) == 0)) {
			/* UAPSD frames being processed from ISR context */
			ath_rx_process_uapsd_aponly(sc, qtype, wbuf, rxs, true);
		}
#endif /* ATH_SUPPORT_UAPSD */

		/* add this ath_buf for deferred processing */
		TAILQ_INSERT_TAIL(&rxedma->rxqueue, bf, bf_list);

		/* clear this element before advancing */
		rxedma->rxfifo[rxedma->rxfifoheadindex] = NULL;

		/* advance the head pointer */
		INCR(rxedma->rxfifoheadindex, rxedma->rxfifohwsize);

		if (unlikely(rxedma->rxfifodepth == 0))
			printk("ath_rx_intr: depth 0 PANIC\n");

		rxedma->rxfifodepth--;
	} while (TRUE);

	/*
	 * remove ath_bufs from free list and add it to fifo
	 */
	frames = rxedma->rxfifohwsize - rxedma->rxfifodepth;
	if (frames > 0)
		ath_rx_addbuffer_aponly(sc, qtype, frames);
}

DECLARE_N_EXPORT_PERF_CNTR(rx_intr);

/*
 * Rx interrupt handler with EDMA for ap-only
 */
void ath_rx_edma_intr_aponly(ath_dev_t dev, HAL_INT status, int *sched)
{
	struct ath_softc *sc = ATH_DEV_TO_SC(dev);
	struct ath_hal *ah = sc->sc_ah;

	START_PERF_CNTR(rx_intr, rx_intr);

	if (unlikely(status & HAL_INT_RXORN)) {
		sc->sc_stats.ast_rxorn++;
	}
	if (unlikely(status & HAL_INT_RXEOL)) {
		sc->sc_stats.ast_rxeol++;
	}
	ATH_RXBUF_LOCK_IN_ISR(sc);
	if (likely(status & (HAL_INT_RXHP | HAL_INT_RXEOL | HAL_INT_RXORN))) {
		ath_rx_intr_aponly(dev, HAL_RX_QUEUE_HP);
		*sched = ATH_ISR_SCHED;
	}
	if (likely(status & (HAL_INT_RXLP | HAL_INT_RXEOL | HAL_INT_RXORN))) {
		ath_rx_intr_aponly(dev, HAL_RX_QUEUE_LP);
		*sched = ATH_ISR_SCHED;
	}
	ATH_RXBUF_UNLOCK_IN_ISR(sc);

	/* Check if RXEOL condition was resolved */
	if (unlikely(status & HAL_INT_RXEOL)) {
		/* TODO - check rx fifo threshold here */

		/*
		 * RXEOL is always asserted after a chip reset. Ideally rxfifodepth 0
		 * should actually indicate a true RXEOL condition. Therefore checking
		 * rxfifodepth == 0 or consecutive RXEOL's to disable further interrupts.
		 * Otherwise, if the interrupt is disabled here and no packets are rx'ed
		 * for 3 secs (implicitly meaning this interrupt doesn't get re-enabled),
		 * the txq hang checker falsely indentifies it as a hang condition and
		 * does a chip reset.
		 */
		if (sc->sc_rxedma[HAL_RX_QUEUE_HP].rxfifodepth == 0 ||
		    sc->sc_rxedma[HAL_RX_QUEUE_LP].rxfifodepth == 0 ||
		    sc->sc_consecutive_rxeol_count > 5) {
			/* No buffers available - disable RXEOL/RXORN to avoid interrupt storm
			 * Disable and then enable to satisfy global isr enable reference counter
			 */
			//For further investigation

			sc->sc_consecutive_rxeol_count = 0;
			//BUG EV# 66955 Interrupt storm fix
			//Interrup bits must be cleared
			ath_hal_intrset(ah, 0);
			sc->sc_imask &= ~(HAL_INT_RXEOL | HAL_INT_RXORN);
			ath_hal_intrset(ah, sc->sc_imask);
#if ATH_HW_TXQ_STUCK_WAR
			sc->sc_last_rxeol = OS_GET_TIMESTAMP();
#endif
		} else {
			sc->sc_consecutive_rxeol_count++;
		}
	} else {
		sc->sc_consecutive_rxeol_count = 0;
	}
	END_PERF_CNTR(rx_intr);
}

static inline int ath_rx_indicate_aponly(struct ath_softc *sc, wbuf_t wbuf,
					 ieee80211_rx_status_t * status,
					 u_int16_t keyix)
{
	struct ath_buf *bf = ATH_GET_RX_CONTEXT_BUF(wbuf);
	wbuf_t nwbuf;
	int type = -1;

	/* indicate frame to the stack, which will free the old wbuf. only indicate when we can get new buffer */
	wbuf_set_next(wbuf, NULL);
#if ATH_RXBUF_RECYCLE
	/*
	 * if ATH_RXBUF_RECYCLE is enabled to recycle the skb,
	 * do the rx_indicate before we recycle the skb to avoid
	 * skb competition and headline block of the recycle queue.
	 */
	type = ath_net80211_rx_aponly(sc->sc_pdev, wbuf, status, keyix);
	nwbuf = (wbuf_t) (sc->sc_osdev->rbr_ops.osdev_wbuf_recycle((void *)sc));
	if (likely(nwbuf != NULL)) {
		bf->bf_mpdu = nwbuf;
		/*
		 * do not invalidate the cache for the new/recycled skb,
		 * because the cache will be invalidated in rx ISR/tasklet
		 */
		bf->bf_buf_addr[0] = bf->bf_dmacontext =
		    virt_to_phys(nwbuf->data);
		ATH_SET_RX_CONTEXT_BUF(nwbuf, bf);
		/* queue the new wbuf to H/W */
		ath_rx_requeue(sc, nwbuf);
	}
	return type;
#else /* !ATH_RXBUF_RECYCLE */
	/* allocate a new wbuf and queue it to for H/W processing */
	nwbuf = ath_rxbuf_alloc(sc, sc->sc_rxbufsize);
	if (likely(nwbuf != NULL)) {
		type = ath_net80211_rx_aponly(sc->sc_pdev, wbuf, status, keyix);
		bf->bf_mpdu = nwbuf;
		/*
		 * do not invalidate the cache for the new/recycled skb,
		 * because the cache will be invalidated in rx ISR/tasklet
		 */
		bf->bf_buf_addr[0] = bf->bf_dmacontext =
		    virt_to_phys(nwbuf->data);
		ATH_SET_RX_CONTEXT_BUF(nwbuf, bf);
		/* queue the new wbuf to H/W */
		ath_rx_requeue(sc, nwbuf);
	} else {
		struct sk_buff *skb;
		/* Could not allocate the buffer
		 * give the wbuf back, reset the wbuf fields
		 *
		 * do not invalidate the cache for the new/recycled skb,
		 * because the cache will be invalidated in rx ISR/tasklet
		 */

		skb = (struct sk_buff *)wbuf;
		OS_MEMZERO(skb, offsetof(struct sk_buff, tail));
#if LIMIT_RXBUF_LEN_4K
		skb->data = skb->head;
#else
		skb->data = skb->head + NET_SKB_PAD;
#endif
		skb_reset_tail_pointer(skb);	/*skb->tail = skb->data; */
		skb->dev = sc->sc_osdev->netdev;

		bf->bf_buf_addr[0] = bf->bf_dmacontext =
		    virt_to_phys(wbuf->data);

		/* Set the rx context back, as it was cleared before */
		/* wbuf_set_next(wbuf, NULL) clears it */
		bf->bf_mpdu = wbuf;
		ATH_SET_RX_CONTEXT_BUF(wbuf, bf);

		/* queue back the old wbuf to H/W */
		ath_rx_requeue(sc, wbuf);

		/* it's a drop, record it */
#ifdef QCA_SUPPORT_CP_STATS
		pdev_lmac_cp_stats_ast_rx_nobuf_inc(sc->sc_pdev, 1);
#else
		sc->sc_stats.ast_rx_nobuf++;
#endif
	}
	return type;
#endif /* !ATH_RXBUF_RECYCLE */
}

static inline void
ath_rx_process_aponly(struct ath_softc *sc, struct ath_buf *bf,
		      struct ath_rx_status *rxs, u_int8_t frame_fc0,
		      ieee80211_rx_status_t * rx_status, u_int8_t * chainreset)
{
	u_int16_t buf_len;
	wbuf_t wbuf = bf->bf_mpdu;
	int type, selevm;
#if UNIFIED_SMARTANTENNA
	static uint32_t total_pkts = 0;
#endif
#if ATH_VOW_EXT_STATS
	struct ath_phy_stats *phy_stats = &sc->sc_phy_stats[sc->sc_curmode];
#endif
#ifdef QCA_SUPPORT_CP_STATS
	struct pdev_ic_cp_stats pdev_cps;
#endif
#if defined(ATH_ADDITIONAL_STATS) || ATH_SLOW_ANT_DIV || defined(ATH_SUPPORT_TxBF)
	struct ieee80211_frame *wh;
	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
#endif

#ifdef ATH_SUPPORT_TxBF
	ath_rx_bf_process(sc, bf, rxs, rx_status);
	if (ath_txbf_chk_rpt_frm(wh)) {
		/* get time stamp for txbf report frame only */
		rx_status->rpttstamp = rxs->rs_tstamp;
	} else {
		rx_status->rpttstamp = 0;
	}
#endif

	/*
	 * Sync and unmap the frame.  At this point we're
	 * committed to passing the sk_buff somewhere so
	 * clear buf_skb; this means a new sk_buff must be
	 * allocated when the rx descriptor is setup again
	 * to receive another frame.
	 */
	buf_len = wbuf_get_pktlen(wbuf);

	rx_status->tsf = 0;
	rx_status->rateieee = sc->sc_hwmap[rxs->rs_rate].ieeerate;
	rx_status->rateKbps = sc->sc_hwmap[rxs->rs_rate].rateKbps;
	rx_status->ratecode = rxs->rs_rate;
	rx_status->nomoreaggr = rxs->rs_moreaggr ? 0 : 1;

	rx_status->isapsd = rxs->rs_isapsd;
	rx_status->noisefloor = (sc->sc_noise_floor == 0) ?
	    ATH_DEFAULT_NOISE_FLOOR : sc->sc_noise_floor;
	/*
	 * During channel switch, frms already in requeue should be marked as the
	 * previous channel rather than sc_curchan.channel
	 */
	rx_status->channel = rxs->rs_channel;

	selevm = ath_hal_setrxselevm(sc->sc_ah, 0, 1);
	if (!selevm) {
		rx_status->lsig[0] = rxs->evm0 >> 24 & 0xFF;
		rx_status->lsig[1] = rxs->evm0 >> 16 & 0xFF;
		rx_status->lsig[2] = rxs->evm0 >> 8 & 0xFF;
		rx_status->servicebytes[0] = rxs->evm0 & 0xFF;
		rx_status->servicebytes[1] = rxs->evm1 >> 24 & 0xFF;
		rx_status->htsig[0] = rxs->evm1 >> 16 & 0xFF;
		rx_status->htsig[1] = rxs->evm1 >> 8 & 0xFF;
		rx_status->htsig[2] = rxs->evm1 & 0xFF;
		rx_status->htsig[3] = rxs->evm2 >> 24 & 0xFF;
		rx_status->htsig[4] = rxs->evm2 >> 16 & 0xFF;
		rx_status->htsig[5] = rxs->evm2 >> 8 & 0xFF;
	}

	/* HT rate */
	if (rx_status->ratecode & 0x80) {
		/* TODO - add table to avoid division */
		/* For each case, do division only one time */
		if (rxs->rs_flags & HAL_RX_2040) {
			rx_status->flags |= ATH_RX_40MHZ;
			if (rxs->rs_flags & HAL_RX_GI) {
				rx_status->rateKbps =
				    ATH_MULT_30_DIV_13(rx_status->rateKbps);
			} else {
				rx_status->rateKbps =
				    ATH_MULT_27_DIV_13(rx_status->rateKbps);
				rx_status->flags |= ATH_RX_SHORT_GI;
			}
		} else {
			if (rxs->rs_flags & HAL_RX_GI) {
				rx_status->rateKbps =
				    ATH_MULT_10_DIV_9(rx_status->rateKbps);
			} else {
				rx_status->flags |= ATH_RX_SHORT_GI;
			}
		}
	}

	/* duration = Length of packet / rate */
	if (rx_status->rateKbps)
		sc->sc_rx_packet_dur +=
		    (((wbuf_get_pktlen(wbuf) * 8) /
		      (rx_status->rateKbps / 100))) * 10;

	/* sc->sc_noise_floor is only available when the station attaches to an AP,
	 * so we use a default value if we are not yet attached.
	 */
	/* XXX we should use either sc->sc_noise_floor or
	 * ath_hal_getChanNoise(ah, &sc->sc_curchan)
	 * to calculate the noise floor.
	 * However, the value returned by ath_hal_getChanNoise seems to be incorrect
	 * (-31dBm on the last test), so we will use a hard-coded value until we
	 * figure out what is going on.
	 */
	if (rxs->rs_rssi != ATH_RSSI_BAD) {
		rx_status->abs_rssi = rxs->rs_rssi + ATH_DEFAULT_NOISE_FLOOR;
	}

	if (unlikely(!(bf->bf_status & ATH_BUFSTATUS_SYNCED))) {
		OS_SYNC_SINGLE(sc->sc_osdev,
			       bf->bf_buf_addr[0], wbuf_get_pktlen(wbuf),
			       BUS_DMA_FROMDEVICE, OS_GET_DMA_MEM_CONTEXT(bf,
									  bf_dmacontext));
		bf->bf_status |= ATH_BUFSTATUS_SYNCED;
	} else {
		OS_SYNC_SINGLE(sc->sc_osdev,
			       bf->bf_buf_addr[0] + sc->sc_rxstatuslen,
			       wbuf_get_pktlen(wbuf), BUS_DMA_FROMDEVICE,
			       OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
	}
	/*
	 * ast_ant_rx can only accommodate 8 antennas
	 */
	sc->sc_stats.ast_ant_rx[rxs->rs_antenna & 0x7]++;
#if WLAN_SUPPORT_GREEN_AP
	/* This is the debug feature to print out the RSSI. This is the only
	 * way to check if the Rx chains are disabled and enabled correctly.
	 */
	{
		static u_int32_t print_now = 0;
		if (ath_green_ap_get_enable_print(sc)) {
			if (!(print_now & 0xff)) {
				qdf_print("Rx rssi0 %d rssi1 %d rssi2 %d",
				       (int8_t) rxs->rs_rssi_ctl0,
				       (int8_t) rxs->rs_rssi_ctl1,
				       (int8_t) rxs->rs_rssi_ctl2);
			}
		}
		print_now++;
	}
#endif /* WLAN_SUPPORT_GREEN_AP */
	if (likely(sc->sc_hashtsupport)) {
#if UNIFIED_SMARTANTENNA
		if ((frame_fc0 & IEEE80211_FC0_TYPE_MASK) ==
		    IEEE80211_FC0_TYPE_DATA) {
			total_pkts++;
		}
#endif
		rx_status->isaggr = rxs->rs_isaggr;
		if (rxs->rs_moreaggr == 0) {
			int rssi_chain_valid_count = 0;
			int numchains = sc->sc_rx_numchains;

			if (rxs->rs_rssi != ATH_RSSI_BAD) {
				rx_status->rssi = rxs->rs_rssi;
				rx_status->flags |= ATH_RX_RSSI_VALID;
				sc->sc_stats.ast_rx_rssi = rxs->rs_rssi;
			}
			if (rxs->rs_rssi_ctl0 != ATH_RSSI_BAD) {
				rx_status->rssictl[0] = rxs->rs_rssi_ctl0;
				rssi_chain_valid_count++;
				sc->sc_stats.ast_rx_rssi_ctl0 =
				    rxs->rs_rssi_ctl0;
			}

			if (rxs->rs_rssi_ctl1 != ATH_RSSI_BAD) {
				rx_status->rssictl[1] = rxs->rs_rssi_ctl1;
				rssi_chain_valid_count++;
				sc->sc_stats.ast_rx_rssi_ctl1 =
				    rxs->rs_rssi_ctl1;
			}

			if (rxs->rs_rssi_ctl2 != ATH_RSSI_BAD) {
				rx_status->rssictl[2] = rxs->rs_rssi_ctl2;
				rssi_chain_valid_count++;
				sc->sc_stats.ast_rx_rssi_ctl2 =
				    rxs->rs_rssi_ctl2;
			}

			if (rxs->rs_flags & HAL_RX_2040) {
				int rssi_extn_valid_count = 0;
				if (rxs->rs_rssi_ext0 != ATH_RSSI_BAD) {
					rx_status->rssiextn[0] =
					    rxs->rs_rssi_ext0;
					rssi_extn_valid_count++;
					sc->sc_stats.ast_rx_rssi_ext0 =
					    rxs->rs_rssi_ext0;
				}
				if (rxs->rs_rssi_ext1 != ATH_RSSI_BAD) {
					rx_status->rssiextn[1] =
					    rxs->rs_rssi_ext1;
					rssi_extn_valid_count++;
					sc->sc_stats.ast_rx_rssi_ext1 =
					    rxs->rs_rssi_ext1;
				}
				if (rxs->rs_rssi_ext2 != ATH_RSSI_BAD) {
					rx_status->rssiextn[2] =
					    rxs->rs_rssi_ext2;
					rssi_extn_valid_count++;
					sc->sc_stats.ast_rx_rssi_ext2 =
					    rxs->rs_rssi_ext2;
				}
				if (rssi_extn_valid_count == numchains) {
					rx_status->flags |=
					    ATH_RX_RSSI_EXTN_VALID;
				}
			}
			if (rssi_chain_valid_count == numchains) {
				rx_status->flags |= ATH_RX_CHAIN_RSSI_VALID;
			}
#if UNIFIED_SMARTANTENNA
			if (SMART_ANT_RX_FEEDBACK_ENABLED(sc)
			    && ((frame_fc0 & IEEE80211_FC0_TYPE_MASK) ==
				IEEE80211_FC0_TYPE_DATA)) {
				ath_smart_ant_rxfeedback_aponly(sc, wbuf, rxs,
								total_pkts);
				total_pkts = 0;
			}
#endif
		}
	} else {
		/*
		 * Need to insert the "combined" rssi into the status structure
		 * for upper layer processing
		 */

		rx_status->rssi = rxs->rs_rssi;
		rx_status->flags |= ATH_RX_RSSI_VALID;
		rx_status->isaggr = 0;
#if UNIFIED_SMARTANTENNA
		if (SMART_ANT_RX_FEEDBACK_ENABLED(sc)
		    && ((frame_fc0 & IEEE80211_FC0_TYPE_MASK) ==
			IEEE80211_FC0_TYPE_DATA)) {
			ath_smart_ant_rxfeedback_aponly(sc, wbuf, rxs, 1);
		}
#endif
	}

#ifdef ATH_ADDITIONAL_STATS
	do {
		u_int8_t frm_type;
		u_int8_t frm_subtype;
		frm_type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
		frm_subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
		if (frm_type == IEEE80211_FC0_TYPE_DATA) {
			if (frm_subtype == IEEE80211_FC0_SUBTYPE_QOS) {
				struct ieee80211_qosframe *whqos;
				int tid;
				whqos = (struct ieee80211_qosframe *)wh;
				tid = whqos->i_qos[0] & IEEE80211_QOS_TID;
				sc->sc_stats.ast_rx_num_qos_data[tid]++;
			} else {
				sc->sc_stats.ast_rx_num_nonqos_data++;
			}
		}
	} while (0);
#endif

	if (unlikely(sc->sc_diversity)) {
		/*
		 * When using hardware fast diversity, change the default rx
		 * antenna if rx diversity chooses the other antenna 3
		 * times in a row.
		 */

		/*
		 * TODO: vicks discusses with team regarding this
		 * beacuse of rx diversity def antenna is changing ..
		 */

		if (sc->sc_defant != rxs->rs_antenna) {
			if (++sc->sc_rxotherant >= 3) {
#if UNIFIED_SMARTANTENNA
				if (!SMART_ANT_ENABLED(sc)) {
					ath_setdefantenna(sc, rxs->rs_antenna);
				}
#else
				ath_setdefantenna(sc, rxs->rs_antenna);
#endif

			}
		} else {
			sc->sc_rxotherant = 0;
		}
	}

	/* increment count of received bytes */
	/*
	 * Increment rx_pkts count.
	 */
	__11nstats(sc, rx_pkts);
	sc->sc_stats.ast_rx_packets++;
	sc->sc_stats.ast_rx_bytes += wbuf_get_pktlen(wbuf);

#if ATH_SLOW_ANT_DIV
	if (sc->sc_slowAntDiv && (rx_status->flags & ATH_RX_RSSI_VALID)
	    && IEEE80211_IS_BEACON(wh)) {
		ath_slow_ant_div(&sc->sc_antdiv, wh, rxs);
	}
#endif

#if ATH_ANT_DIV_COMB
	if (sc->sc_antDivComb) {
		ath_ant_div_comb_scan(&sc->sc_antcomb, rxs);
	}
#endif

#if ATH_VOW_EXT_STATS
	/* make sure we do not corrupt non-decrypted frame */
	if (sc->sc_vowext_stats && !(rx_status->flags & ATH_RX_KEYMISS))
		ath_add_ext_stats(rxs, wbuf, sc, phy_stats, rx_status);
#endif //

	/*
	 * Pass frames up to the stack.
	 * Note: After calling ath_rx_indicate(), we should not assumed that the
	 * contents of wbuf and wh are valid.
	 */
	type = ath_rx_indicate_aponly(sc, wbuf, rx_status, rxs->rs_keyix);
#ifdef QCA_SUPPORT_CP_STATS
	wlan_cfg80211_get_pdev_cp_stats(sc->sc_pdev, &pdev_cps);
#endif

	if (type == IEEE80211_FC0_TYPE_DATA) {
		sc->sc_stats.ast_rx_num_data++;
	} else if (type == IEEE80211_FC0_TYPE_MGT) {
#ifdef QCA_SUPPORT_CP_STATS
		pdev_cps.stats.cs_rx_num_mgmt++;
#else
		sc->sc_stats.ast_rx_num_mgmt++;
#endif
	} else if (type == IEEE80211_FC0_TYPE_CTL) {
#ifdef QCA_SUPPORT_CP_STATS
		pdev_cps.stats.cs_rx_num_ctl++;
#else
		sc->sc_stats.ast_rx_num_ctl++;
#endif
	} else {
#ifdef QCA_SUPPORT_CP_STATS
		pdev_cps.lmac_stats.cs_ast_rx_num_unknown++;
#else
		sc->sc_stats.ast_rx_num_unknown++;
#endif
	}

	/* report data flow to LED module */
#if ATH_SUPPORT_LED || defined(ATH_BT_COEX)
	if (type == IEEE80211_FC0_TYPE_DATA) {
		int subtype = frame_fc0 & IEEE80211_FC0_SUBTYPE_MASK;
		if (subtype != IEEE80211_FC0_SUBTYPE_NODATA &&
		    subtype != IEEE80211_FC0_SUBTYPE_QOS_NULL) {
#if ATH_SUPPORT_LED
			ath_led_report_data_flow(&sc->sc_led_control, buf_len);
#endif
#ifdef ATH_BT_COEX
			sc->sc_btinfo.wlanRxPktNum++;
#endif
		}
	}
#endif
}

#if ATH_VOW_EXT_STATS
/*
 * Insert some stats info into the test packet's header.
 *
 * Test packets are Data type frames in the Clear or encrypted
 *   with WPA2-PSK CCMP, identified by a specific length && UDP
 *   && RTP && RTP eXtension && magic number.
 */
void
ath_add_ext_stats(struct ath_rx_status *rxs, wbuf_t wbuf,
		  struct ath_softc *sc, struct ath_phy_stats *phy_stats,
		  ieee80211_rx_status_t * rx_status)
{
	/*
	 * TODO:
	 *  - packet size is hardcoded, should be configurable
	 *  - assumes no security, or WPA2-PSK CCMP security
	 *  - EXT_HDR_SIZE is hardcoded, should calc from hdr size field
	 *  - EXT_HDR fields are hardcoded, should be defined
	 *  - EXT_HDR src fields are hardcoded, should be read from hdr
	 */

#define IPV4_PROTO_OFF  38
#define UDP_PROTO_OFF   47
#define UDP_CKSUM_OFF   64
#define RTP_HDR_OFF     66
#define EXT_HDR_OFF     78
#define EXT_HDR_SIZE    ((8+1) * 4)	// should determine from ext hdr
#define AR_RCCNT        0x80f4	// Profile count receive clear
#define AR_CCCNT        0x80f8	// Profile count cycle counter
#define ATH_EXT_STAT_DFL_LEN 1434
#define IP_VER4_N_NO_EXTRA_HEADERS 0x45
#define IP_PDU_PROTOCOL_UDP 0x11
#define UDP_PDU_RTP_EXT    ((2 << 6) | (1 << 4))	/* RTP Version 2 + X bit */
	unsigned char *bp;
	u_int32_t reg_rccnt;
	u_int32_t reg_cccnt;
	struct ieee80211_frame *wh;
	u_int16_t seqctrl;
	u_int16_t buf_len;
	int test_len = ATH_EXT_STAT_DFL_LEN;
	int frm_type, frm_subtype;
	int offset;

	wh = (struct ieee80211_frame *)wbuf_raw_data(wbuf);
	frm_type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	frm_subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
	seqctrl = *(u_int16_t *) (wh->i_seq);
	bp = (unsigned char *)wbuf_raw_data(wbuf);
	buf_len = wbuf_get_pktlen(wbuf);

	/* Ignore non Data Types */
	if (!(frm_type & IEEE80211_FC0_TYPE_DATA)) {
		return;
	}

	/* Adjust for WDS */
	if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS) {
		bp += QDF_MAC_ADDR_SIZE;
		test_len += QDF_MAC_ADDR_SIZE;
		offset = 4;
	} else {
		offset = 2;
	}

	/* Adjust for QoS Header */
	if (!(frm_subtype & IEEE80211_FC0_SUBTYPE_QOS)) {
		offset += 4;
	}

	bp -= offset;
	test_len -= offset;

	/* Adjust for AES security */
	/* Assumes WPA2-PSK CCMP only if security enabled, else open */
	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		bp += IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN
		    + IEEE80211_WEP_CRCLEN;
		test_len += 16;
	}

	/* only mark very specifc packets */
	if ((buf_len == test_len) &&
	    (*(bp + RTP_HDR_OFF) == UDP_PDU_RTP_EXT) &&
	    (*(bp + UDP_PROTO_OFF) == IP_PDU_PROTOCOL_UDP) &&
	    (*(bp + IPV4_PROTO_OFF) == IP_VER4_N_NO_EXTRA_HEADERS)) {
		/* Check for magic number and header length */
		if ((*(bp + EXT_HDR_OFF + 0) == 0x12) &&
		    (*(bp + EXT_HDR_OFF + 1) == 0x34) &&
		    (*(bp + EXT_HDR_OFF + 2) == 0x00) &&
		    (*(bp + EXT_HDR_OFF + 3) == 0x08)) {
			if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
				/* don't clear the udp checksum here. In case of security, we may need
				 * to do SW MIC on this packet. clear the checksum in ieee layer after
				 * passing thru crpto layer. Store the udp checksum offset value in rx_stats */
				rx_status->vow_extstats_offset =
				    (bp - (uint8_t *) wh) + UDP_CKSUM_OFF;
			} else {
				/* clear udp checksum so we do not have to recalculate it after
				 * filling in status fields */
				*(bp + UDP_CKSUM_OFF) = 0x00;
				*(bp + UDP_CKSUM_OFF + 1) = 0x00;
			}

			reg_rccnt = ath_hal_reg_read(sc->sc_ah, AR_RCCNT);
			reg_cccnt = ath_hal_reg_read(sc->sc_ah, AR_CCCNT);

			bp += EXT_HDR_OFF + 12;	// skip hdr and src fields

			/* Store the ext stats offset in rx_status which will be used at the time of SW MIC */
			rx_status->vow_extstats_offset =
			    (rx_status->
			     vow_extstats_offset) |
			    (((uint32_t) (bp - (uint8_t *) wh)) << 16);

			*bp++ = rxs->rs_rssi_ctl0;
			*bp++ = rxs->rs_rssi_ctl1;
			*bp++ = rxs->rs_rssi_ctl2;
			*bp++ = rxs->rs_rssi_ext0;
			*bp++ = rxs->rs_rssi_ext1;
			*bp++ = rxs->rs_rssi_ext2;
			*bp++ = rxs->rs_rssi;

			*bp++ = (unsigned char)(rxs->rs_flags & 0xff);

			*bp++ = (unsigned char)((rxs->rs_tstamp >> 8) & 0x7f);
			*bp++ = (unsigned char)(rxs->rs_tstamp & 0xff);

			*bp++ =
			    (unsigned char)((phy_stats->ast_rx_phyerr >> 8) &
					    0xff);
			*bp++ =
			    (unsigned char)(phy_stats->ast_rx_phyerr & 0xff);

			*bp++ = (unsigned char)((reg_rccnt >> 24) & 0xff);
			*bp++ = (unsigned char)((reg_rccnt >> 16) & 0xff);
			*bp++ = (unsigned char)((reg_rccnt >> 8) & 0xff);
			*bp++ = (unsigned char)(reg_rccnt & 0xff);

			*bp++ = (unsigned char)((reg_cccnt >> 24) & 0xff);
			*bp++ = (unsigned char)((reg_cccnt >> 16) & 0xff);
			*bp++ = (unsigned char)((reg_cccnt >> 8) & 0xff);
			*bp++ = (unsigned char)(reg_cccnt & 0xff);

			*bp++ = rxs->rs_rate;
			*bp++ = rxs->rs_moreaggr;

			*bp++ = (unsigned char)((seqctrl >> 8) & 0xff);
			*bp++ = (unsigned char)(seqctrl & 0xff);
		}
	}
#undef AR_RCCNT
#undef AR_CCCNT
}
#endif // EXT_STATS

/*
 * Helper routine for ath_rx_edma_requeue
 * Context: ISR
\ */
struct ath_rx_edma_requeue_request {
	struct ath_softc *sc;
	struct ath_buf *bf;
};

/*
 * This routine adds a new buffer to the free list
 * Context: Tasklet
 */
static inline void ath_rx_edma_requeue_aponly(ath_dev_t dev, wbuf_t wbuf)
{
	struct ath_softc *sc = ATH_DEV_TO_SC(dev);
	struct ath_buf *bf = ATH_GET_RX_CONTEXT_BUF(wbuf);
	struct ath_hal *ah = sc->sc_ah;
	unsigned long flags;

	ASSERT(bf != NULL);

	ATH_RXBUF_LOCK(sc);
	ATH_LOCK_IRQ(sc->sc_osdev);
	local_irq_save(flags);
	TAILQ_INSERT_TAIL(&sc->sc_rxbuf, bf, bf_list);

	/* If RXEOL interrupts were disabled (due to no buffers available), re-enable RXEOL interrupts. */
	if (!(sc->sc_imask & HAL_INT_RXEOL)) {
		if (sc->sc_edmarxdpc) {
			/* In rxdpc - so do not enable interrupt, just set the sc_imask
			 * interrupt gets enabled at the end of DPC
			 */
			sc->sc_imask |= HAL_INT_RXEOL | HAL_INT_RXORN;
		} else {
			/* Disable and then enable to satisfy the global isr enable reference counter */
			ath_hal_intrset(ah, 0);
			sc->sc_imask |= HAL_INT_RXEOL | HAL_INT_RXORN;
			ath_hal_intrset(ah, sc->sc_imask);
		}
	}
	local_irq_restore(flags);
	ATH_UNLOCK_IRQ(sc->sc_osdev);
	ATH_RXBUF_UNLOCK(sc);
}

#if ATH_SUPPORT_WIFIPOS
static inline void ath_rx_wifipos_isresp_aponly(struct ath_rx_status *rxs,
						struct ath_softc *sc,
						wbuf_t wbuf,
						unsigned char pkt_type)
{
	ieee80211_wifiposdesc_t wifiposdata;
	void *ds, *bf_vdata;
	struct ieee80211_frame *wh;

	memset(&wifiposdata, 0, sizeof(wifiposdata));
	ds = (void *)wbuf_raw_data(wbuf);
	bf_vdata = (void *)((u_int8_t *) ds + sc->sc_rxstatuslen +
			    (ATH_DESC_CDUMP_SIZE(sc->sc_rx_numchains)));
	wh = (struct ieee80211_frame *)bf_vdata;
	wifiposdata.sa = (wh->i_addr2[IEEE80211_ADDR_LEN - 2] << 8) |
	    (wh->i_addr2[IEEE80211_ADDR_LEN - 1]);

	/*
	 * if AP is connected to other stations then location bit is set
	 * for some packets from those STAs.
	 * Source address is zero for ACK packets
	 * TODO - Need to find out any situation when location bit is
	 * set for any incoming packet and need to filter out here
	 */
	wifiposdata.toa = rxs->rs_tstamp;
	wifiposdata.hdump = rxs->hdump;
	/* For NBP 1.1 changing to chain masks
	 * 3:0 - RX chain mask
	 * 4-7 - TX chain mask
	 */
	wifiposdata.txrxchain_mask =
	    (sc->sc_tx_chainmask << 4) | sc->sc_rx_chainmask;
	wifiposdata.rate = rxs->rs_rate;
	wifiposdata.rssi0 = rxs->rs_rssi_ctl0;
	wifiposdata.rssi1 = rxs->rs_rssi_ctl1;
	wifiposdata.rssi2 = rxs->rs_rssi_ctl2;
	wifiposdata.rx_pkt_type = pkt_type;
	sc->sc_ieee_ops->update_wifipos_stats(&wifiposdata);
}
#endif

DECLARE_N_EXPORT_PERF_CNTR(rx_tasklet);

#if AP_MULTIPLE_BUFFER_RCV
wbuf_t ath_rx_edma_buf_merge_aponly(struct ath_softc *sc, wbuf_t sbuf,
				    wbuf_t wbuf, struct ath_rx_status *rxs)
{
	wbuf_t databuf, pointbuf, tempbuf;

	u_int16_t datalen = 0;
	int i = 0;

	pointbuf = tempbuf = sbuf;
	if (wbuf != NULL) {
		databuf =
		    ath_rxbuf_alloc(sc,
				    sc->sc_prebuflen + rxs->rs_datalen +
				    sc->sc_rxstatuslen);
	} else {
		databuf =
		    ath_rxbuf_alloc(sc, rxs->rs_datalen + sc->sc_rxstatuslen);

	}

	if (databuf == NULL) {
		DPRINTF(sc, ATH_DEBUG_ANY,
			"%s: no buffer to used!\n", __func__);
		return NULL;
	}

	while (pointbuf != NULL)	// here will release all wbuf, it needs to set sc->sc_bfpending->bf_mpdu = NULL; before re-link
	{
		wbuf_copydata(pointbuf, 0, wbuf_get_pktlen(pointbuf),
			      wbuf_header(databuf) + datalen);
		datalen += wbuf_get_pktlen(pointbuf);
		i++;
		pointbuf = wbuf_next(tempbuf);
		wbuf_set_next(tempbuf, NULL);
		wbuf_free(tempbuf);
		tempbuf = pointbuf;
	}
	if (wbuf != NULL) {
		wbuf_init(wbuf, rxs->rs_datalen + sc->sc_rxstatuslen);
		wbuf_pull(wbuf, sc->sc_rxstatuslen);
		wbuf_copydata(wbuf, 0, wbuf_get_pktlen(wbuf),
			      wbuf_header(databuf) + datalen);
		datalen += rxs->rs_datalen;
#ifdef ATH_SUPPORT_TxBF
		if (rxs->rx_hw_upload_data)
			rxs->rs_datalen = datalen;
		else
#endif
			rxs->rs_datalen = (datalen - sc->sc_rxstatuslen);
	}
	return databuf;
}

HAL_BOOL ath_rx_edma_buf_relink_aponly(ath_dev_t dev, struct ath_buf * bf)
{
	wbuf_t tempbuf;

	struct ath_softc *sc = ATH_DEV_TO_SC(dev);

	tempbuf = bf->bf_mpdu;

	if (tempbuf != NULL)
		wbuf_free(tempbuf);

	tempbuf = ath_rxbuf_alloc(sc, sc->sc_rxbufsize);

	if (tempbuf == NULL) {
		DPRINTF(sc, ATH_DEBUG_ANY,
			"%s: no buffer to used!\n", __func__);
		return AH_FALSE;
	}

	bf->bf_mpdu = tempbuf;
	/*
	 * do not invalidate the cache for the new/recycled skb,
	 * because the cache will be invalidated in rx ISR/tasklet
	 */
	bf->bf_buf_addr[0] = bf->bf_dmacontext = virt_to_phys(tempbuf->data);
	ATH_SET_RX_CONTEXT_BUF(tempbuf, bf);
	/* queue the new wbuf to H/W */
	ath_rx_edma_requeue_aponly(dev, tempbuf);

	return AH_TRUE;

}
#endif
/*
 * Process receive queue, as well as LED, etc.
 * Arg "flush":
 * 0: Process rx frames in rx interrupt.
 * 1: Drop rx frames in flush routine.
 * 2: Flush and indicate rx frames, must be synchronized with other flush threads.
 */
static int ath_rx_handler_aponly(ath_dev_t dev, int flush, HAL_RX_QUEUE qtype)
{
	struct ath_softc *sc = ATH_DEV_TO_SC(dev);
	struct ath_rx_edma *rxedma;
	struct ath_buf *bf;
#if defined(ATH_SUPPORT_DFS) || defined(WLAN_SPECTRAL_ENABLE)
	struct ath_hal *ah = sc->sc_ah;
#endif
	struct ath_rx_status *rxs;
	void *ds;
	u_int phyerr;
	struct ieee80211_frame *wh;
	wbuf_t wbuf = NULL;
	ieee80211_rx_status_t rx_status;
	struct ath_phy_stats *phy_stats = &sc->sc_phy_stats[sc->sc_curmode];
	u_int8_t chainreset = 0;
	int rx_processed = 0;
	unsigned long flags;
#if ATH_SUPPORT_WIFIPOS
	static unsigned char lastpkt = 0x00;
	int subtype;
#endif
#if ATH_SUPPORT_RX_PROC_QUOTA
	u_int process_frame_cnt = sc->sc_process_rx_num;
#endif
	int type = -1;

	START_PERF_CNTR(rx_tasklet, rx_tasklet);

	rxedma = &sc->sc_rxedma[qtype];
	do {
		/* If handling rx interrupt and flush is in progress => exit */
		if (unlikely(sc->sc_rxflush)) {
			break;
		}

		/* Get completed ath_buf from rxqueue. Must synchronize with the ISR */
		bf = NULL;
		ATH_RXQ_LOCK(rxedma);
		ATH_LOCK_IRQ(sc->sc_osdev);
		local_irq_save(flags);
		bf = TAILQ_FIRST(&rxedma->rxqueue);
		if (likely(bf)) {
			TAILQ_REMOVE(&rxedma->rxqueue, bf, bf_list);
		}
		local_irq_restore(flags);
		ATH_UNLOCK_IRQ(sc->sc_osdev);
		ATH_RXQ_UNLOCK(rxedma);
		if (bf == NULL) {
			break;
		}

		wbuf = bf->bf_mpdu;
		if (unlikely(wbuf == NULL)) {	/* XXX ??? can this happen */
			continue;
		}
		++rx_processed;

		rxs = (struct ath_rx_status *)(bf->bf_desc);
		wh = (struct ieee80211_frame *)((unsigned char *)
						wbuf_raw_data((wbuf_t) bf->
							      bf_mpdu) +
						sc->sc_rxstatuslen);
#if ATH_SUPPORT_WIFIPOS
		if (rxs->hdump) {
			ath_rx_wifipos_isresp_aponly(rxs, sc, wbuf, lastpkt);
			goto rx_next;
		}
		lastpkt = wh->i_fc[0];
		type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
		subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
		if (subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP) {
			if (!memcmp(wh->i_addr2, wh->i_addr3, IEEE80211_ADDR_LEN)) {	//TODO: Put proper comparision
				sc->sc_tsf_tstamp = rxs->rs_tstamp;
			}
		}
#endif

		/*
		 * Save RxS location for packetlog.
		 */
		ds = (void *)wbuf_raw_data(wbuf);

#ifdef ATH_RX_DESC_WAR
		if (unlikely(bf->bf_status & ATH_BUFSTATUS_WAR)) {
			DPRINTF(sc, ATH_DEBUG_RX_PROC, "%s: Dropping DP 0x%x\n",
				__func__, (unsigned)bf->bf_buf_addr[0]);
			goto rx_next;
		}
#endif

		OS_MEMZERO(&rx_status, sizeof(ieee80211_rx_status_t));

		/* point to the beginning of actual frame */
		bf->bf_vdata = (void *)((u_int8_t *) ds + sc->sc_rxstatuslen);

#ifndef REMOVE_PKT_LOG
		/* do pktlog */
		{
			struct log_rx log_data = { 0 };
			log_data.ds = ds;
			log_data.status = rxs;
			log_data.bf = bf;
			ath_log_rx(sc, &log_data, 0);
		}
#endif

#ifdef ATH_SUPPORT_TxBF
		{		//Check if Have H, V/CV upload from HW
			int next_do = ath_rx_bf_handler(dev, wbuf, rxs, bf);

			if (next_do == TX_BF_DO_RX_NEXT) {
				goto rx_next;
			} else if (next_do == TX_BF_DO_CONTINUE) {
				continue;
			}
		}
#endif
		if (unlikely((rxs->rs_status == 0) && (rxs->rs_more))) {
			/*
			 * Frame spans multiple descriptors; this
			 * cannot happen yet as we don't support
			 * jumbograms.    If not in monitor mode,
			 * discard the frame.
			 */
#if (AP_MULTIPLE_BUFFER_RCV && !ATH_SUPPORT_TxBF)
			if (rxs->rs_more) {
				if (sc->sc_rxpending) {
					wbuf_t tempbuf = sc->sc_rxpending;

					while (wbuf_next(tempbuf) != NULL) {
						tempbuf = wbuf_next(tempbuf);
					}

					wbuf_init(wbuf,
						  rxs->rs_datalen +
						  sc->sc_rxstatuslen);
					wbuf_pull(wbuf, sc->sc_rxstatuslen);
					wbuf_set_next(wbuf, NULL);
					bf->bf_mpdu = sc->sc_rxpending;
					sc->sc_bfpending->bf_mpdu = NULL;
					wbuf_set_next(tempbuf, wbuf);
					ath_rx_edma_buf_relink_aponly(dev,
								      sc->
								      sc_bfpending);
					sc->sc_bfpending = bf;
					sc->sc_prebuflen += rxs->rs_datalen;
				} else {
					sc->sc_rxpending = wbuf;
					sc->sc_bfpending = bf;
					sc->sc_prebuflen = rxs->rs_datalen;
					wbuf_init(wbuf,
						  rxs->rs_datalen +
						  sc->sc_rxstatuslen);
					wbuf_set_next(wbuf, NULL);
				}
				continue;
			}
#endif
			goto rx_next;
		} else {	// if (rxs->rs_status != 0)
			if (unlikely(rxs->rs_status & HAL_RXERR_CRC)) {
				rx_status.flags |= ATH_RX_FCS_ERROR;
				phy_stats->ast_rx_crcerr++;
			}
			if (unlikely(rxs->rs_status & HAL_RXERR_FIFO))
				phy_stats->ast_rx_fifoerr++;
			if (unlikely(rxs->rs_status & HAL_RXERR_PHY)) {
				phy_stats->ast_rx_phyerr++;
				phyerr = rxs->rs_phyerr & 0x1f;
				phy_stats->ast_rx_phy[phyerr]++;
#ifdef ATH_SUPPORT_DFS
				{
					u_int64_t tsf = ath_hal_gettsf64(ah);
					/* Process phyerrs */
					ath_process_phyerr(sc, bf, rxs, tsf);
				}
#endif

#if WLAN_SPECTRAL_ENABLE
				{
					struct ath_spectral *spectral =
					    (struct ath_spectral *)sc->
					    sc_spectral;
					u_int64_t tsf = ath_hal_gettsf64(ah);
					if (is_spectral_phyerr
					    (spectral, bf, rxs)) {
						SPECTRAL_LOCK(spectral);
						ath_process_spectraldata
						    (spectral, bf, rxs, tsf);
						SPECTRAL_UNLOCK(spectral);
					}

				}
#endif /* WLAN_SPECTRAL_ENABLE */

				goto rx_next;
			}

			if (unlikely(rxs->rs_status & HAL_RXERR_DECRYPT)) {
				/*
				 * Decrypt error. We only mark packet status here
				 * and always push up the frame up to let NET80211 layer
				 * handle the actual error case, be it no decryption key
				 * or real decryption error.
				 * This let us keep statistics there.
				 */
				phy_stats->ast_rx_decrypterr++;
				rx_status.flags |= ATH_RX_DECRYPT_ERROR;
			} else if (unlikely(rxs->rs_status & HAL_RXERR_MIC)) {
				rx_status.flags |= ATH_RX_MIC_ERROR;
			}

			/*
			 * Reject error frames with the exception of decryption, MIC,
			 * and key-miss failures.
			 * For monitor mode, we also ignore the CRC error.
			 */
			if (unlikely(rxs->rs_status &
				     ~(HAL_RXERR_DECRYPT | HAL_RXERR_MIC |
				       HAL_RXERR_KEYMISS))) {
				goto rx_next;
			} else {
				if (unlikely
				    (rxs->rs_status & HAL_RXERR_KEYMISS)) {
					rx_status.flags |= ATH_RX_KEYMISS;
				}
			}
		}

#if AP_MULTIPLE_BUFFER_RCV
		if (sc->sc_rxpending != NULL) {

			wbuf_t databuf;
			databuf =
			    ath_rx_edma_buf_merge_aponly(sc, sc->sc_rxpending,
							 wbuf, rxs);
			if (databuf == NULL) {
				DPRINTF(sc, ATH_DEBUG_ANY,
					"%s: no buffer to used!\n", __func__);
				goto rx_next;
			}
			wbuf_init(databuf,
				  rxs->rs_datalen + sc->sc_rxstatuslen);
			ATH_SET_RX_CONTEXT_BUF(databuf, bf);
			wbuf_free(wbuf);
			bf->bf_mpdu = wbuf = databuf;
			sc->sc_bfpending->bf_mpdu = NULL;
			ath_rx_edma_buf_relink_aponly(dev, sc->sc_bfpending);
			sc->sc_rxpending = NULL;
			sc->sc_bfpending = NULL;
			sc->sc_prebuflen = 0;
			goto skip_wbuf_init;
		}
#endif

		/*
		 * Initialize wbuf; the length includes packet length
		 * and status length. The status length later deducted
		 * from the total len by the wbuf_pull
		 */
		wbuf_init(wbuf, (rxs->rs_datalen + sc->sc_rxstatuslen));

#if USE_MULTIPLE_BUFFER_RCV || AP_MULTIPLE_BUFFER_RCV
skip_wbuf_init:
#endif

#if ATH_SUPPORT_VOW_DCS
		if (sc->sc_scanning) {
			const HAL_RATE_TABLE *rt = sc->sc_currates;
			u_int8_t rix = 0;

			rix = rt->rateCodeToIndex[rxs->rs_rate];
			if (rt->info[rix].phy == IEEE80211_T_CCK) {
				type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
				if (type == IEEE80211_FC0_TYPE_DATA) {
					sc->sc_11b_data_dur +=
					    ath_rx_duration(sc, rix,
							    rxs->rs_datalen,
							    rxs->rs_moreaggr, 0,
							    (rxs->
							     rs_flags &
							     HAL_RX_2040) != 0,
							    (rxs->
							     rs_flags &
							     HAL_RX_GI) != 0,
							    1);
				}
			}
		}
#endif /* ATH_SUPPORT_VOW_DCS */
		/*
		 * Adjust wbuf start addr to point to data, i.e skip past the RxS.
		 */
		wbuf_pull(wbuf, sc->sc_rxstatuslen);

		wh = (struct ieee80211_frame *)wbuf_header(wbuf);
		ath_rx_process_aponly(sc, bf, rxs, wh->i_fc[0], &rx_status,
				      &chainreset);

		/*
		 * For frames successfully indicated, the buffer will be
		 * returned to us by upper layers by calling ath_rx_mpdu_requeue,
		 * either synchronusly or asynchronously.
		 * So we don't want to do it here in this loop.
		 */
		continue;

rx_next:

#if  AP_MULTIPLE_BUFFER_RCV
		if (sc->sc_rxpending) {
			// We come to rx_next whenever we have packet errors of any kind.
			// It's now safe to requeue a pending packet...
			wbuf_t pointbuf, tempbuf;

			pointbuf = tempbuf = sc->sc_rxpending;
			while (pointbuf != NULL) {
				pointbuf = wbuf_next(tempbuf);
				wbuf_set_next(tempbuf, NULL);
				wbuf_free(tempbuf);
				tempbuf = pointbuf;
			}
			sc->sc_bfpending->bf_mpdu = NULL;
			ath_rx_edma_buf_relink_aponly(dev, sc->sc_bfpending);
			sc->sc_rxpending = NULL;
			sc->sc_bfpending = NULL;
		}
#endif /* USE_MULTIPLE_BUFFER_RCV || AP_MULTIPLE_BUFFER_RCV */

		ath_rx_edma_requeue_aponly(dev, wbuf);
#if  ATH_SUPPORT_RX_PROC_QUOTA
	} while (process_frame_cnt > rx_processed);
#else
	} while (TRUE);
#endif

#ifdef ATH_ADDITIONAL_STATS
	if (rx_processed < ATH_RXBUF) {
		sc->sc_stats.ast_pkts_per_intr[rx_processed]++;
	} else {
		sc->sc_stats.ast_pkts_per_intr[ATH_RXBUF]++;
	}
#endif

	if (unlikely(chainreset)) {
		printk("Reset rx chain mask. Do internal reset. (%s)\n",
		       __func__);
		ath_internal_reset(sc);
	}

	END_PERF_CNTR(rx_tasklet);
	/*setting Rx interupt uncondionally as when we schedule tasklet
	   from below condition we disable RX interrupt and in further
	   iterations if we genuinely exit tasklet then Rx interrupt will
	   remain disabled for ever */
#if ATH_SUPPORT_RX_PROC_QUOTA
	sc->sc_imask |= (HAL_INT_RXHP | HAL_INT_RXLP);
	if (HAL_RX_QUEUE_LP == qtype) {
		sc->sc_rx_work_lp = 0;
	} else if (HAL_RX_QUEUE_HP == qtype) {
		sc->sc_rx_work_hp = 0;
	}
	if (!TAILQ_EMPTY(&rxedma->rxqueue)) {
		if (HAL_RX_QUEUE_LP == qtype) {
			sc->sc_rx_work_lp = 1;
			sc->sc_imask &= ~(HAL_INT_RXLP);
		} else if (HAL_RX_QUEUE_HP == qtype) {
			sc->sc_rx_work_hp = 1;
			sc->sc_imask &= ~(HAL_INT_RXHP);
		}
#if ATH_RX_LOOPLIMIT_TIMER
		sc->sc_imask |= HAL_INT_GENTIMER;
		/* mark this when the timer expires */
		sc->sc_rx_work_lp = 0;
		sc->sc_rx_work_hp = 0;
		/* mark to start the timer at then end of the tasklet */
		sc->sc_rx_looplimit = 1;
#else
		ATH_SCHEDULE_TQUEUE(&sc->sc_osdev->intr_tq, &needmark);
#endif
	}
#endif

	return 0;
}

DECLARE_N_EXPORT_PERF_CNTR(tasklet);

/*
 * Deferred interrupt processing
 */
void ath_handle_intr_aponly(ath_dev_t dev)
{
	struct ath_softc *sc = ATH_DEV_TO_SC(dev);
	u_int32_t status = sc->sc_intrstatus;
	u_int32_t rxmask;
	struct hal_bb_panic_info hal_bb_panic;
	struct ath_bb_panic_info bb_panic;
	int i;

	START_PERF_CNTR(tasklet, tasklet);

	sc->sc_intrstatus &= (~status);
	ATH_PS_WAKEUP(sc);

	do {
		if (unlikely(sc->sc_invalid)) {
			/*
			 * The hardware is not ready/present, don't touch anything.
			 * Note this can happen early on if the IRQ is shared.
			 */
			DPRINTF(sc, ATH_DEBUG_INTR, "%s called when invalid.\n",
				__func__);
			break;
		}

		if (unlikely(status & HAL_INT_FATAL)) {
			/* need a chip reset */
			DPRINTF(sc, ATH_DEBUG_INTR, "%s: Got fatal intr\n",
				__func__);
			sc->sc_reset_type = ATH_RESET_NOLOSS;
			ath_internal_reset(sc);
			sc->sc_reset_type = ATH_RESET_DEFAULT;
			break;
		} else {
			if (unlikely(status & HAL_INT_BBPANIC)) {
				if (!ath_hal_get_bbpanic_info
				    (sc->sc_ah, &hal_bb_panic)) {
					bb_panic.status = hal_bb_panic.status;
					bb_panic.tsf = hal_bb_panic.tsf;
					bb_panic.wd = hal_bb_panic.wd;
					bb_panic.det = hal_bb_panic.det;
					bb_panic.rdar = hal_bb_panic.rdar;
					bb_panic.r_odfm = hal_bb_panic.r_odfm;
					bb_panic.r_cck = hal_bb_panic.r_cck;
					bb_panic.t_odfm = hal_bb_panic.t_odfm;
					bb_panic.t_cck = hal_bb_panic.t_cck;
					bb_panic.agc = hal_bb_panic.agc;
					bb_panic.src = hal_bb_panic.src;
					bb_panic.phy_panic_wd_ctl1 =
					    hal_bb_panic.phy_panic_wd_ctl1;
					bb_panic.phy_panic_wd_ctl2 =
					    hal_bb_panic.phy_panic_wd_ctl2;
					bb_panic.phy_gen_ctrl =
					    hal_bb_panic.phy_gen_ctrl;
					bb_panic.cycles = hal_bb_panic.cycles;
					bb_panic.rxc_pcnt =
					    hal_bb_panic.rxc_pcnt;
					bb_panic.rxf_pcnt =
					    hal_bb_panic.rxf_pcnt;
					bb_panic.txf_pcnt =
					    hal_bb_panic.txf_pcnt;
					bb_panic.valid = 1;

					for (i = 0; i < MAX_BB_PANICS - 1; i++)
						sc->sc_stats.ast_bb_panic[i] =
						    sc->sc_stats.
						    ast_bb_panic[i + 1];
					sc->sc_stats.
					    ast_bb_panic[MAX_BB_PANICS - 1] =
					    bb_panic;
				}

				if (!(ath_hal_handle_radar_bbpanic(sc->sc_ah))) {
					/* reset to recover from the BB hang */
					sc->sc_reset_type = ATH_RESET_NOLOSS;
					ATH_RESET_LOCK(sc);
					ath_hal_set_halreset_reason(sc->sc_ah,
								    HAL_RESET_BBPANIC);
					ATH_RESET_UNLOCK(sc);
					ath_internal_reset(sc);
					ATH_RESET_LOCK(sc);
					ath_hal_clear_halreset_reason(sc->
								      sc_ah);
					ATH_RESET_UNLOCK(sc);
					sc->sc_reset_type = ATH_RESET_DEFAULT;
#ifdef QCA_SUPPORT_CP_STATS
					pdev_lmac_cp_stats_ast_reset_on_error_inc(sc->sc_pdev, 1);
#else
					sc->sc_stats.ast_resetOnError++;
#endif
					/* EV92527 -- we are doing internal reset. break out */
					break;
				}
				/* EV 92527 -- We are not doing any internal reset, continue normally */
			}
#ifdef ATH_BEACON_DEFERRED_PROC
			/* Handle SWBA first */
			if (unlikely(status & HAL_INT_SWBA)) {
				int needmark = 0;
				ath_beacon_tasklet(sc, &needmark);
			}
#endif

			if (unlikely
			    (((AH_TRUE == sc->sc_hang_check)
			      && ath_hw_hang_check(sc)) || (!sc->sc_noreset
							    && (sc->sc_bmisscount > (BSTUCK_THRESH_PERVAP * sc->sc_nvaps))))) {
				ath_bstuck_tasklet(sc);
				ATH_CLEAR_HANGS(sc);
				break;
			}
			/*
			 * Howl needs DDR FIFO flush before any desc/dma data can be read.
			 */
			ATH_FLUSH_FIFO();
			if (likely(sc->sc_enhanceddmasupport)) {
				rxmask =
				    (HAL_INT_RXHP | HAL_INT_RXLP | HAL_INT_RXEOL
				     | HAL_INT_RXORN);
			} else {
				rxmask =
				    (HAL_INT_RX | HAL_INT_RXEOL |
				     HAL_INT_RXORN);
			}

			if (likely(status & rxmask)
#if ATH_SUPPORT_RX_PROC_QUOTA
			    || (sc->sc_rx_work_hp) || (sc->sc_rx_work_lp)
#endif
			    ) {
				if (sc->sc_enhanceddmasupport) {
					sc->sc_edmarxdpc = 1;
					if ((status &
					     (HAL_INT_RXHP | HAL_INT_RXEOL |
					      HAL_INT_RXORN))
#if ATH_SUPPORT_RX_PROC_QUOTA
					    || (sc->sc_rx_work_hp)
#endif
					    ) {
						ath_rx_handler_aponly(dev, 0,
								      HAL_RX_QUEUE_HP);
					}
					if ((status & HAL_INT_RXLP)
#if ATH_SUPPORT_RX_PROC_QUOTA
					    || (sc->sc_rx_work_lp)
#endif
					    ) {
						ath_rx_handler_aponly(dev, 0,
								      HAL_RX_QUEUE_LP);
					}
					sc->sc_edmarxdpc = 0;
				} else {
					ath_handle_rx_intr(sc);
				}
			} else if (sc->sc_rxfreebuf != NULL) {
				DPRINTF(sc, ATH_DEBUG_INTR,
					"%s[%d] ---- Athbuf FreeQ Not Empty - Calling AllocRxbufs for FreeList \n",
					__func__, __LINE__);
				// There are athbufs with no associated mbufs. Let's try to allocate some mbufs for these.
				if (sc->sc_enhanceddmasupport) {
					ath_edmaAllocRxbufsForFreeList(sc);
				} else {
					ath_allocRxbufsForFreeList(sc);
				}
			}
#if ATH_TX_POLL
			if (sc->sc_enhanceddmasupport) {
				ATH_TX_EDMA_TASK(sc);
			} else {
				ath_tx_tasklet(sc);
			}
#else
			if (likely(status & HAL_INT_TX)) {
#ifdef ATH_TX_INACT_TIMER
				sc->sc_tx_inact = 0;
#endif
				if (sc->sc_enhanceddmasupport) {
					ATH_TX_EDMA_TASK(sc);
				} else {
					ath_tx_tasklet(sc);
				}
			}
#endif
			if (unlikely(status & HAL_INT_BMISS)) {
				ath_bmiss_tasklet(sc);
			}
			if (unlikely(status & HAL_INT_CST)) {
				ath_txto_tasklet(sc);
			}
			if (unlikely(status & (HAL_INT_TIM | HAL_INT_DTIMSYNC))) {
				if (status & HAL_INT_TIM) {
					if (sc->sc_ieee_ops->proc_tim)
						sc->sc_ieee_ops->proc_tim(sc->
									  sc_ieee);
				}
				if (status & HAL_INT_DTIMSYNC) {
					DPRINTF(sc, ATH_DEBUG_INTR,
						"%s: Got DTIMSYNC intr\n",
						__func__);
				}
			}
			if (unlikely(status & HAL_INT_GPIO)) {
#ifdef ATH_RFKILL
				ath_rfkill_gpio_intr(sc);
#endif
#ifdef ATH_BT_COEX
				if (unlikely(sc->sc_btinfo.bt_gpioIntEnabled)) {
					ath_bt_coex_gpio_intr(sc);
				}
#endif
			}
#ifdef ATH_GEN_TIMER

			if (unlikely(status & HAL_INT_TSFOOR)) {
				/* There is a jump in the TSF time with this OUT OF RANGE interupt. */
				DPRINTF(sc, ATH_DEBUG_ANY,
					"%s: Got HAL_INT_TSFOOR intr\n",
					__func__);

				/* If the current mode is Station, then we need to reprogram the beacon timers. */
				if (sc->sc_opmode == HAL_M_STA) {
					ath_beacon_config(sc,
							  ATH_BEACON_CONFIG_REASON_RESET,
							  ATH_IF_ID_ANY);
				}

				ath_gen_timer_tsfoor_isr(sc);
			}

			if (unlikely(status & HAL_INT_GENTIMER)) {
#ifdef TARGET_SUPPORT_TSF_TIMER
				ath_gen_timer_isr(sc, 0, 0, 0);
#else
				ath_gen_timer_isr(sc);
#endif
			}
#endif
		}

		/* re-enable hardware interrupt */
		if (likely(sc->sc_enhanceddmasupport)) {
			/* For enhanced DMA, certain interrupts are already enabled (e.g. RXEOL),
			 * but now re-enable _all_ interrupts.
			 * Note: disable and then enable to satisfy the global isr enable reference counter.
			 */
			ath_hal_intrset(sc->sc_ah, 0);
			ath_hal_intrset(sc->sc_ah, sc->sc_imask);
		} else {
			ath_hal_intrset(sc->sc_ah, sc->sc_imask);
		}
#if ATH_RX_LOOPLIMIT_TIMER
		if (sc->sc_rx_looplimit) {
			if (sc->sc_rx_looplimit_timer->cached_state.active !=
			    true) {
				ath_gen_timer_stop(sc,
						   sc->sc_rx_looplimit_timer);
				ath_gen_timer_start(sc,
						    sc->sc_rx_looplimit_timer,
						    ath_gen_timer_gettsf32(sc,
									   sc->
									   sc_rx_looplimit_timer)
						    + sc->sc_rx_looplimit_timeout, 0);	/* one shot */
#ifdef QCA_SUPPORT_CP_STATS
				pdev_cp_stats_rx_looplimit_start_inc(sc->sc_pdev, 1);
#else
				sc->sc_stats.ast_rx_looplimit_start++;
#endif
			}
		}
#endif
	} while (FALSE);

	ATH_PS_SLEEP(sc);

	END_PERF_CNTR(tasklet);
}

DECLARE_N_EXPORT_PERF_CNTR(isr);

irqreturn_t
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
ath_isr_aponly(int irq, void *dev_id)
#else
ath_isr_aponly(int irq, void *dev_id, struct pt_regs *regs)
#endif
{
	struct net_device *dev = dev_id;
	struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
	int sched, needmark = 0;
	irqreturn_t ret = IRQ_HANDLED;

#if PCI_INTERRUPT_WAR_ENABLE
	scn->int_scheduled_cnt++;
#endif

	START_PERF_CNTR(isr, isr);

	/* always acknowledge the interrupt */

	sched = ath_intr_aponly(scn->sc_dev);

	if (unlikely(sched == ATH_ISR_NOSCHED)) {
		goto done;
	}
	if (unlikely(sched == ATH_ISR_NOTMINE)) {
		ret = IRQ_NONE;
		goto done;
	}

	if (unlikely
	    ((dev->flags & (IFF_RUNNING | IFF_UP)) != (IFF_RUNNING | IFF_UP))) {
		DPRINTF_INTSAFE((struct ath_softc *)scn->sc_dev, ATH_DEBUG_INTR,
				"%s: flags 0x%x\n", __func__, dev->flags);

		scn->sc_ops->disable_interrupt(scn->sc_dev);	/* disable further intr's */
		goto done;
	}

	/*
	 ** See if the transmit queue processing needs to be scheduled
	 */
	ATH_SCHEDULE_TQUEUE(&scn->sc_osdev->intr_tq, &needmark);
	if (needmark)
		mark_bh(IMMEDIATE_BH);

done:
	END_PERF_CNTR(isr);
	return ret;
}

#ifndef ATH_UPDATE_COMMON_INTR_STATS
#define ATH_UPDATE_COMMON_INTR_STATS(sc, status)
#endif
#ifndef ATH_UPDATE_INTR_STATS
#define ATH_UPDATE_INTR_STATS(sc, intr)
#endif

static inline int ath_common_intr_aponly(ath_dev_t dev, HAL_INT status)
{
	struct ath_softc *sc = ATH_DEV_TO_SC(dev);
	struct ath_hal *ah = sc->sc_ah;
	int sched = ATH_ISR_NOSCHED;

	ATH_UPDATE_COMMON_INTR_STATS(sc, status);

	do {
#ifdef ATH_MIB_INTR_FILTER
		/* Notify the MIB interrupt filter that we received some other interrupt. */
		if (likely(!(status & HAL_INT_MIB))) {
			ath_filter_mib_intr(sc, AH_FALSE);
		}
#endif

		if (unlikely(status & HAL_INT_FATAL)) {
			/* need a chip reset */
#ifdef QCA_SUPPORT_CP_STATS
			pdev_lmac_cp_stats_ast_hardware_inc(sc->sc_pdev, 1);
#else
			sc->sc_stats.ast_hardware++;
#endif
			sched = ATH_ISR_SCHED;
		} else
		    if (unlikely
			((status & HAL_INT_RXORN)
			 && !sc->sc_enhanceddmasupport)) {
			/* need a chip reset? */
#if ATH_SUPPORT_DESCFAST
			ath_rx_proc_descfast(dev);
#endif
			sc->sc_stats.ast_rxorn++;
			sched = ATH_ISR_SCHED;
		} else {
			if (unlikely(status & HAL_INT_SWBA)) {
#ifdef ATH_BEACON_DEFERRED_PROC
				/* Handle beacon transmission in deferred interrupt processing */
				sched = ATH_ISR_SCHED;
#else
				int needmark = 0;

				/*
				 * Software beacon alert--time to send a beacon.
				 * Handle beacon transmission directly; deferring
				 * this is too slow to meet timing constraints
				 * under load.
				 */
				ath_beacon_tasklet(sc, &needmark);

				if (needmark) {
					/* We have a beacon stuck. Beacon stuck processing
					 * should be done in DPC instead of here. */
					sched = ATH_ISR_SCHED;
				}
#endif /* ATH_BEACON_DEFERRED_PROC */
				ATH_UPDATE_INTR_STATS(sc, swba);
			}
			if (unlikely(status & HAL_INT_TXURN)) {
				sc->sc_stats.ast_txurn++;
				/* bump tx trigger level */
				ath_hal_updatetxtriglevel(ah, AH_TRUE);
			}
			if (likely(sc->sc_enhanceddmasupport)) {
				ath_rx_edma_intr_aponly(sc, status, &sched);
			} else {
				if (unlikely(status & HAL_INT_RXEOL)) {
					/*
					 * NB: the hardware should re-read the link when
					 *     RXE bit is written, but it doesn't work at
					 *     least on older hardware revs.
					 */
#if ATH_SUPPORT_DESCFAST
					ath_rx_proc_descfast(dev);
#endif
					sc->sc_imask &=
					    ~(HAL_INT_RXEOL | HAL_INT_RXORN);
					ath_hal_intrset(ah, sc->sc_imask);
#if ATH_HW_TXQ_STUCK_WAR
					sc->sc_last_rxeol = OS_GET_TIMESTAMP();
#endif
					sc->sc_stats.ast_rxeol++;
					sched = ATH_ISR_SCHED;
				}
				if (likely(status & HAL_INT_RX)) {
					ATH_UPDATE_INTR_STATS(sc, rx);
#if ATH_SUPPORT_DESCFAST
					ath_rx_proc_descfast(dev);
#endif
					sched = ATH_ISR_SCHED;
				}
			}

			if (likely(status & HAL_INT_TX)) {
				ATH_UPDATE_INTR_STATS(sc, tx);
				sched = ATH_ISR_SCHED;
			}
#if ATH_GEN_RANDOMNESS
			if (unlikely(!(status & HAL_INT_RXHP))) {
				ath_gen_randomness(sc);
			}
#endif
			if (unlikely(status & HAL_INT_BMISS)) {
#ifdef QCA_SUPPORT_CP_STATS
				pdev_lmac_cp_stats_ast_bmiss_inc(sc->sc_pdev, 1);
#else
				sc->sc_stats.ast_bmiss++;
#endif
				sched = ATH_ISR_SCHED;
			}
			if (unlikely(status & HAL_INT_GTT)) {	/* tx timeout interrupt */
				sc->sc_stats.ast_txto++;
			}
			if (unlikely(status & HAL_INT_CST)) {	/* carrier sense timeout */
				sc->sc_stats.ast_cst++;
				sched = ATH_ISR_SCHED;
			}

			if (unlikely(status & HAL_INT_MIB)) {
#ifdef QCA_SUPPORT_CP_STATS
				pdev_cp_stats_mib_int_count_inc(sc->sc_pdev, 1);
#else
				sc->sc_stats.ast_mib++;
#endif
				/*
				 * Disable interrupts until we service the MIB
				 * interrupt; otherwise it will continue to fire.
				 */
				ath_hal_intrset(ah, 0);

#ifdef ATH_MIB_INTR_FILTER
				/* Check for bursts of MIB interrupts */
				ath_filter_mib_intr(sc, AH_TRUE);
#endif

				/*
				 * Let the hal handle the event.  We assume it will
				 * clear whatever condition caused the interrupt.
				 */
				ath_hal_mibevent(ah, &sc->sc_halstats);
				ath_hal_intrset(ah, sc->sc_imask);
			}
			if (unlikely(status & HAL_INT_GPIO)) {
				ATH_UPDATE_INTR_STATS(sc, gpio);
				/* Check if this GPIO interrupt is caused by RfKill */
#ifdef ATH_RFKILL
				if (ath_rfkill_gpio_isr(sc))
					sched = ATH_ISR_SCHED;
#endif
				if (sc->sc_wpsgpiointr) {
					/* Check for WPS push button press (GPIO polarity low) */
					if (ath_hal_gpioget
					    (sc->sc_ah,
					     sc->sc_reg_parm.wpsButtonGpio) ==
					    0) {
						sc->sc_wpsbuttonpushed = 1;

						/* Disable associated GPIO interrupt to prevent flooding */
						ath_hal_gpioSetIntr(ah,
								    sc->
								    sc_reg_parm.
								    wpsButtonGpio,
								    HAL_GPIO_INTR_DISABLE);
						sc->sc_wpsgpiointr = 0;
					}
				}
#ifdef ATH_BT_COEX
				if (sc->sc_btinfo.bt_gpioIntEnabled) {
					sched = ATH_ISR_SCHED;
				}
#endif
			}
			if (unlikely(status & HAL_INT_TIM_TIMER)) {
				ATH_UPDATE_INTR_STATS(sc, tim_timer);
				if (!sc->sc_hasautosleep) {
					/* Clear RxAbort bit so that we can receive frames */
					ath_hal_setrxabort(ah, 0);
					/* Set flag indicating we're waiting for a beacon */
					sc->sc_waitbeacon = 1;

					sched = ATH_ISR_SCHED;
				}
			}
#ifdef ATH_GEN_TIMER
			if (unlikely(status & HAL_INT_GENTIMER)) {
				ATH_UPDATE_INTR_STATS(sc, gentimer);
				/* generic TSF timer interrupt */
				sched = ATH_ISR_SCHED;
			}
#endif

			if (unlikely(status & HAL_INT_TSFOOR)) {
				ATH_UPDATE_INTR_STATS(sc, tsfoor);
				DPRINTF(sc, ATH_DEBUG_PWR_SAVE,
					"%s: HAL_INT_TSFOOR - syncing beacon\n",
					__func__);
				/* Set flag indicating we're waiting for a beacon */
				sc->sc_waitbeacon = 1;

				sched = ATH_ISR_SCHED;
			}

			if (unlikely(status & HAL_INT_BBPANIC)) {
				ATH_UPDATE_INTR_STATS(sc, bbevent);
				/* schedule the DPC to get bb panic info */
				sched = ATH_ISR_SCHED;
			}

		}
	} while (0);

	if (likely(sched == ATH_ISR_SCHED)) {
		DPRINTF_INTSAFE(sc, ATH_DEBUG_INTR, "%s: Scheduling BH/DPC\n",
				__func__);
		if (likely(sc->sc_enhanceddmasupport)) {
			/* For enhanced DMA turn off all interrupts except RXEOL, RXORN, SWBA.
			 * Disable and then enable to satisfy the global isr enable reference counter.
			 */
			ath_hal_intrset(ah, 0);
#if ATH_RX_LOOPLIMIT_TIMER
			if (sc->sc_rx_looplimit_timer->cached_state.active)
				ath_hal_intrset(ah,
						sc->
						sc_imask & (HAL_INT_GLOBAL |
							    HAL_INT_RXEOL |
							    HAL_INT_RXORN |
							    HAL_INT_SWBA |
							    HAL_INT_GENTIMER));
			else
#endif
				ath_hal_intrset(ah,
						sc->
						sc_imask & (HAL_INT_GLOBAL |
							    HAL_INT_RXEOL |
							    HAL_INT_RXORN |
							    HAL_INT_SWBA));
		} else {
#ifdef ATH_BEACON_DEFERRED_PROC
			/* turn off all interrupts */
			ath_hal_intrset(ah, 0);
#else
			/* turn off every interrupt except SWBA */
			ath_hal_intrset(ah, (sc->sc_imask & HAL_INT_SWBA));
#endif
		}
	}

	return sched;
}

int ath_intr_aponly(ath_dev_t dev)
{
	struct ath_softc *sc = ATH_DEV_TO_SC(dev);
	struct ath_hal *ah = sc->sc_ah;
	HAL_INT status;
	int isr_status = ATH_ISR_NOTMINE;

	atomic_inc(&sc->sc_inuse_cnt);

	do {
		if (unlikely(!ath_hal_intrpend(ah))) {	/* shared irq, not for us */
			isr_status = ATH_ISR_NOTMINE;
			break;
		}

		if (unlikely(sc->sc_invalid)) {
			/*
			 * The hardware is either not ready or is entering full sleep,
			 * don't touch any RTC domain register.
			 */
			ath_hal_intrset_nortc(ah, 0);
			ath_hal_getisr_nortc(ah, &status, 0, 0);
			isr_status = ATH_ISR_NOSCHED;
			DPRINTF_INTSAFE(sc, ATH_DEBUG_INTR,
					"%s: recv interrupts when invalid.\n",
					__func__);
			break;
		}

		/*
		 * Figure out the reason(s) for the interrupt.  Note
		 * that the hal returns a pseudo-ISR that may include
		 * bits we haven't explicitly enabled so we mask the
		 * value to insure we only process bits we requested.
		 */
		ath_hal_getisr(ah, &status, HAL_INT_LINE, 0);	/* NB: clears ISR too */
		DPRINTF_INTSAFE(sc, ATH_DEBUG_INTR,
				"%s: status 0x%x  Mask: 0x%x\n", __func__,
				status, sc->sc_imask);

		status &= sc->sc_imask;	/* discard unasked-for bits */

		/*
		 ** If there are no status bits set, then this interrupt was not
		 ** for me (should have been caught above).
		 */

		if (unlikely(!status)) {
			DPRINTF_INTSAFE(sc, ATH_DEBUG_INTR,
					"%s: Not My Interrupt\n", __func__);
			isr_status = ATH_ISR_NOSCHED;
			break;
		}

		sc->sc_intrstatus |= status;

		isr_status = ath_common_intr_aponly(dev, status);
	} while (FALSE);

	atomic_dec(&sc->sc_inuse_cnt);

	return isr_status;
}

static inline void ieee80211_set_tim_aponly(struct ieee80211_node *ni, int set)
{
	struct ieee80211vap *vap = ni->ni_vap;
	u_int16_t aid;

	KASSERT(vap->iv_opmode == IEEE80211_M_HOSTAP
		|| vap->iv_opmode == IEEE80211_M_IBSS,
		("operating mode %u", vap->iv_opmode));

	aid = IEEE80211_AID(ni->ni_associd);
	KASSERT(aid < vap->iv_max_aid,
		("bogus aid %u, max %u", aid, vap->iv_max_aid));

	if (set != (isset(vap->iv_tim_bitmap, aid) != 0)) {
		if (set) {
			setbit(vap->iv_tim_bitmap, aid);
			vap->iv_ps_pending++;
		} else {
			clrbit(vap->iv_tim_bitmap, aid);
			vap->iv_ps_pending--;
		}
		IEEE80211_VAP_TIMUPDATE_ENABLE(vap);
	}
}

static inline void
ath_uapsd_pwrsave_check_aponly(wbuf_t wbuf, struct wlan_objmgr_peer *peer)
{
	struct dadp_peer *dp_peer = NULL;
	struct dadp_vdev *dp_vdev = NULL;
	struct wlan_objmgr_vdev *vdev = NULL;

	if (!peer) {
		qdf_print("%s:peer is NULL ", __func__);
		return;
	}

	vdev = wlan_peer_get_vdev(peer);
	if (!vdev) {
		qdf_print("%s:vdev is NULL ", __func__);
		return;
	}

	dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer) {
		qdf_print("%s:dp_peer is NULL ", __func__);
		return;
	}

	dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		return;
	}

	if (UAPSD_AC_ISDELIVERYENABLED(wbuf_get_priority(wbuf), dp_peer)) {
		/* U-APSD power save queue for delivery enabled AC */
		wbuf_set_uapsd(wbuf);
		wbuf_set_moredata(wbuf);
		IEEE80211_PEER_STAT(dp_peer, tx_uapsd);
#if 0				/* TBD_DADP: perf impact ??? */
		if ((vap->iv_set_tim != NULL)
		    && IEEE80211_NODE_UAPSD_USETIM(ni)) {
			ieee80211_set_tim_aponly(ni, 1);
		}
#endif
		if (IEEE80211_PEER_UAPSD_USETIM(dp_peer))
			VDEV_SET_TIM(peer, 1, false);
	}
}

#if IEEE80211_DEBUG_REFCNT

#define _ieee80211_find_node_aponly(nt, mac) _ieee80211_find_node_debug_aponly(nt, mac, __func__, __LINE__, __FILE__);

static inline struct ieee80211_node *_ieee80211_find_node_debug_aponly(struct
								       ieee80211_node_table
								       *nt,
								       const
								       u_int8_t
								       *
								       macaddr,
								       const
								       char
								       *func,
								       int line,
								       const
								       char
								       *file)
#else
static inline struct ieee80211_node *_ieee80211_find_node_aponly(struct
								 ieee80211_node_table
								 *nt,
								 const u_int8_t
								 * macaddr)
#endif
{
	struct ieee80211_node *ni;
	int hash;

	hash = IEEE80211_NODE_HASH(macaddr);
	LIST_FOREACH(ni, &nt->nt_hash[hash], ni_hash) {
		if (IEEE80211_ADDR_EQ(ni->ni_macaddr, macaddr)) {
#if IEEE80211_DEBUG_REFCNT
			if (!ieee80211_try_ref_node_debug(ni, func, line, file))	/* mark referenced */
#else
			if (!ieee80211_try_ref_node(ni))	/* mark referenced */
#endif
				return NULL;

			return ni;
		}
	}
	return NULL;
}

#if ATH_WDS_SUPPORT_APONLY

static inline struct ieee80211_node *_ieee80211_find_wds_node_aponly(struct
								     ieee80211_node_table
								     *nt,
								     const
								     u_int8_t *
								     macaddr,
								     struct
								     ieee80211_wds_addr
								     **wds_stag,
								     u_int8_t *
								     stage)
{
	struct ieee80211_node *ni;
	struct ieee80211_wds_addr *wds;
	int hash;

	hash = IEEE80211_NODE_HASH(macaddr);
	LIST_FOREACH(wds, &nt->nt_wds_hash[hash], wds_hash) {
		if (IEEE80211_ADDR_EQ(wds->wds_macaddr, macaddr)) {

			/* if the node is flagged as STAGE, it means, node has gone
			 * probably and quickly came back. We should be checking that
			 * and add clear the flag, if necessary.
			 *
			 * If the wireless station behind the AP is moved/roamed onto
			 * other AP, l2UF would have cleared the path. But we do not
			 * expect the wired stations to go quickly from one wds station
			 * to other. In any case, if there any frame coming from one
			 * station should clear the path.
			 */
			if (wds->flags & IEEE80211_NODE_F_WDS_STAGE) {
				*wds_stag = wds;
				*stage = 1;
				return NULL;
			} else {
				ni = wds->wds_ni;
				wds->wds_agingcount = WDS_AGING_COUNT;	/* reset the aging count */
				wds->wds_staging_age = 2 * WDS_AGING_COUNT;

				if (ni) {
					ni = ieee80211_try_ref_node(ni);
				}
				return ni;
			}
		}
	}
	return NULL;
}

static inline struct ieee80211_node *ieee80211_find_wds_node_aponly(struct
								    ieee80211_node_table
								    *nt,
								    const
								    u_int8_t *
								    macaddr)
{
	struct ieee80211_node *ni;
	struct ieee80211_wds_addr *wds = NULL;
	u_int8_t stag = 0;

	rwlock_state_t lock_state;
	OS_RWLOCK_WRITE_LOCK(&nt->nt_wds_nodelock, &lock_state);
	ni = _ieee80211_find_wds_node_aponly(nt, macaddr, &wds, &stag);
	/* find wds node should return the pointer the ni, for some reasons,
	 * wds entry would have been found on staging. At that instance
	 * probably we do not have the node pointer. It, means, there is no
	 * real association between wds and wds_ni. Now because it is found
	 * on staging, try to establish the relationship between these two in
	 * a special way.
	 *
	 * the 'wds' argument should contain the pointer to the wds that is in
	 * staging. If node is found, that would be NULL, and NI pointer would
	 * be returned properly.
	 */
	if (ni == NULL && stag == 1) {
		if (wds) {
			ni = _ieee80211_find_node_aponly(nt,
							 wds->wds_ni_macaddr);
			if (ni) {
				wds->wds_ni = ni;
				wds->wds_agingcount = WDS_AGING_COUNT;
				wds->wds_staging_age = 2 * WDS_AGING_COUNT;
				wds->flags &= ~IEEE80211_NODE_F_WDS_STAGE;
				IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_WDS,
						  "%s attaching the node macaddr %s wds mac %s\n",
						  __func__, ni->ni_macaddr,
						  macaddr);

				ni = ieee80211_try_ref_node(ni);	/* Reference node */
			}
		}
	}
	OS_RWLOCK_WRITE_UNLOCK(&nt->nt_wds_nodelock, &lock_state);
	return ni;
}
#endif

/*
 * Check if an ADDBA is required.
 */
static inline int
ath_aggr_check_aponly(ath_dev_t dev, ath_node_t node, u_int8_t tidno)
{
	struct ath_softc *sc = ATH_DEV_TO_SC(dev);
	struct ath_node *an = ATH_NODE(node);
	struct ath_atx_tid *tid;

#ifdef ATH_RIFS
	if (!sc->sc_txaggr && !sc->sc_txrifs)
#else
	if (!sc->sc_txaggr)
#endif
		return 0;

	/* ADDBA exchange must be completed before sending aggregates */
	tid = ATH_AN_2_TID(an, tidno);

	if (tid->cleanup_inprogress || (an->an_flags & ATH_NODE_CLEAN)) {
		return 0;
	}

	if (!tid->addba_exchangecomplete) {
		if (!tid->addba_exchangeinprogress &&
		    (tid->addba_exchangeattempts < ADDBA_EXCHANGE_ATTEMPTS)) {
			tid->addba_exchangeattempts++;
			return 1;
		}
	}
	return 0;
}

/*
 *  Get transmit rate index from rateKbps
 */
static inline
    u_int8_t ath_rate_findrix_aponly(const HAL_RATE_TABLE * rt,
				     u_int32_t rateKbps)
{
	u_int8_t i, rix = 0;

	for (i = 0; i < rt->rateCount; i++) {
		if (rt->info[i].rateKbps == rateKbps) {
			rix = i;
			break;
		}
	}
	return rix;
}

/*
 * Get transmit rate index using rate in Kbps
 */
static inline int ath_tx_findindex_aponly(const HAL_RATE_TABLE * rt, int rate)
{
	int i;
	int ndx = 0;

	for (i = 0; i < rt->rateCount; i++) {
		if (rt->info[i].rateKbps == rate) {
			ndx = i;
			break;
		}
	}

	return ndx;
}

/*
 * Insert a chain of ath_buf (descriptors) on a multicast txq
 * but do NOT start tx DMA on this queue.
 * NB: must be called with txq lock held
 */
static inline void
ath_tx_mcastqaddbuf_internal_aponly(struct ath_softc *sc, struct ath_txq *txq,
				    ath_bufhead * head)
{
#define DESC2PA(_sc, _va)	\
		((caddr_t)(_va) - (caddr_t)((_sc)->sc_txdma.dd_desc) + \
				(_sc)->sc_txdma.dd_desc_paddr)
	struct ath_hal *ah = sc->sc_ah;
	struct ath_buf *bf, *tbf;

	/*
	 * Insert the frame on the outbound list and
	 * pass it on to the hardware.
	 */
	bf = TAILQ_FIRST(head);
	if (bf == NULL)
		return;

	/*
	 * The CAB queue is started from the SWBA handler since
	 * frames only go out on DTIM and to avoid possible races.
	 */
	ath_hal_intrset(ah, 0);

	/*
	 ** If there is anything in the mcastq, we want to set the "more data" bit
	 ** in the last item in the queue to indicate that there is "more data".  This
	 ** is an alternate implementation of changelist 289513 put within the code
	 ** to add to the mcast queue.  It makes sense to add it here since you are
	 ** *always* going to have more data when adding to this queue, no matter where
	 ** you call from.
	 */

	if (txq->axq_depth) {
		struct ath_buf *lbf;
		struct ieee80211_frame *wh;

		/*
		 ** Add the "more data flag" to the last frame
		 */

		lbf = TAILQ_LAST(&txq->axq_q, ath_bufhead_s);
		wh = (struct ieee80211_frame *)wbuf_header(lbf->bf_mpdu);
		wh->i_fc[1] |= IEEE80211_FC1_MORE_DATA;
		OS_SYNC_SINGLE(sc->sc_osdev, lbf->bf_buf_addr[0],
			       lbf->bf_frmlen, BUS_DMA_TODEVICE,
			       OS_GET_DMA_MEM_CONTEXT(lbf, bf_dmacontext));

		/*
		 * And add the "more data flag" to all frames in the new head
		 * except the last one.
		 */
		TAILQ_FOREACH(tbf, head, bf_list) {
			if (tbf != TAILQ_LAST(head, ath_bufhead_s)) {
				wh = (struct ieee80211_frame *)wbuf_header(tbf->
									   bf_mpdu);
				wh->i_fc[1] |= IEEE80211_FC1_MORE_DATA;
				OS_SYNC_SINGLE(sc->sc_osdev,
					       tbf->bf_buf_addr[0],
					       tbf->bf_frmlen, BUS_DMA_TODEVICE,
					       OS_GET_DMA_MEM_CONTEXT(tbf,
								      bf_dmacontext));
			}
		}
	}

	TAILQ_FOREACH(tbf, head, bf_list) {
		OS_SYNC_SINGLE(sc->sc_osdev, tbf->bf_daddr,
			       sc->sc_txdesclen, BUS_DMA_TODEVICE, NULL);
	}

	/*
	 ** Now, concat the frame onto the queue
	 */
	ATH_TXQ_CONCAT(txq, head);
	DPRINTF(sc, ATH_DEBUG_TX_PROC, "%s: txq depth = %d\n", __func__,
		txq->axq_depth);
	if (!sc->sc_enhanceddmasupport) {
		if (txq->axq_link != NULL) {
#ifdef AH_NEED_DESC_SWAP
			*txq->axq_link = cpu_to_le32(bf->bf_daddr);
#else
			*txq->axq_link = bf->bf_daddr;
#endif
			OS_SYNC_SINGLE(sc->sc_osdev,
				       (dma_addr_t) (DESC2PA
						     (sc, txq->axq_link)),
				       sizeof(u_int32_t *), BUS_DMA_TODEVICE,
				       NULL);
			DPRINTF(sc, ATH_DEBUG_XMIT,
				"%s: link[%u](%pK)=%llx (%pK)\n", __func__,
				txq->axq_qnum, txq->axq_link,
				ito64(bf->bf_daddr), bf->bf_desc);
		}
		ath_hal_getdesclinkptr(ah, bf->bf_lastbf->bf_desc,
				       &(txq->axq_link));
	} else {
		if (txq->axq_link != NULL) {
			ath_hal_setdesclink(ah, txq->axq_link, bf->bf_daddr);
			OS_SYNC_SINGLE(sc->sc_osdev,
				       (dma_addr_t) (DESC2PA
						     (sc, txq->axq_link)),
				       sc->sc_txdesclen, BUS_DMA_TODEVICE,
				       NULL);
			DPRINTF(sc, ATH_DEBUG_XMIT,
				"%s: link[%u](%pK)=%llx (%pK)\n", __func__,
				txq->axq_qnum, txq->axq_link,
				ito64(bf->bf_daddr), bf->bf_desc);
		}
		txq->axq_link = bf->bf_lastbf->bf_desc;
	}
	ath_hal_intrset(ah, sc->sc_imask);
#undef DESC2PA
}

static ath_get_buf_status_t
ath_tx_get_buf_aponly(struct ath_softc *sc, sg_t * sg, struct ath_buf **pbf,
		      ath_bufhead * bf_head, u_int32_t * buf_used)
{
	struct ath_buf *bf = *pbf;

	if (likely(!bf || !bf->bf_avail_buf)) {
		ATH_TXBUF_LOCK(sc);
		bf = TAILQ_FIRST(&sc->sc_txbuf);
		if (bf == NULL) {
			ATH_TXBUF_UNLOCK(sc);
			return ATH_BUF_NONE;
		}
		*pbf = bf;
		TAILQ_REMOVE(&sc->sc_txbuf, bf, bf_list);
		sc->sc_txbuf_free--;
		(*buf_used)++;
		ATH_TXBUF_UNLOCK(sc);
		TAILQ_INSERT_TAIL(bf_head, bf, bf_list);

		/* set up this buffer */
#if ATH_TX_COMPACT
		bf->bf_status = 0;
		bf->bf_lastbf = NULL;
		bf->bf_lastfrm = NULL;
		bf->bf_next = NULL;
		bf->bf_avail_buf = sc->sc_num_txmaps;
		OS_MEMZERO(&(bf->bf_state), sizeof(struct ath_buf_state));
#else
		ATH_TXBUF_RESET(bf, sc->sc_num_txmaps);
#endif
	}

	bf->bf_buf_addr[sc->sc_num_txmaps - bf->bf_avail_buf] =
	    sg_dma_address(sg);
	bf->bf_buf_len[sc->sc_num_txmaps - bf->bf_avail_buf] = sg_dma_len(sg);

	bf->bf_avail_buf--;

	if (likely(bf->bf_avail_buf))
		return ATH_BUF_CONT;
	else
		return ATH_BUF_LAST;
}

/*
 * Minimum buffers reserved per AC. This is to
 * provide some breathing space for low priority
 * traffic when high priority traffic is flooding
 */
#if ATH_DEBUG
#define MIN_BUF_RESV (min_buf_resv)
#else
#define MIN_BUF_RESV 16
#endif

/*
 * The function that actually starts the DMA.
 * It will either be called by the wbuf_map() function,
 * or called in a different thread if asynchronus DMA
 * mapping is used (NDIS 6.0).
 */
static inline int
ath_tx_start_dma_aponly(wbuf_t wbuf, sg_t * sg, u_int32_t n_sg, void *arg)
{
	ieee80211_tx_control_t *txctl = (ieee80211_tx_control_t *) arg;
	struct ath_softc *sc = (struct ath_softc *)txctl->dev;
	struct ath_node *an = txctl->an;
	struct ath_buf *bf = NULL, *firstbf = NULL;
	ath_bufhead bf_head;
	void *ds, *firstds = NULL, *lastds = NULL;
	struct ath_hal *ah = sc->sc_ah;
	struct ath_txq *txq = &sc->sc_txq[txctl->qnum];
	size_t i;
	struct ath_rc_series *rcs;
	int send_to_cabq = 0;
	struct ath_vap *avp = sc->sc_vaps[txctl->if_id];
#if QCA_AIRTIME_FAIRNESS
	struct wlan_objmgr_pdev *pdev = sc->sc_pdev;
	struct wlan_objmgr_psoc *psoc = NULL;
	int atf_based_alloc = 0;
	int atf_buf_alloc = 0;
	int8_t ac = 0;
#endif

	bool no_wait_for_vap_pause = false;

	u_int32_t *buf_used;
#if !ATH_TX_BUF_FLOW_CNTL
	u_int32_t buf_usedt = 0;
	buf_used = &buf_usedt;
#else
	buf_used = &txq->axq_num_buf_used;
#endif

	atomic_inc(&an->an_active_tx_cnt);
	if (unlikely(an->an_flags & ATH_NODE_CLEAN)) {
		atomic_dec(&an->an_active_tx_cnt);
		return -EIO;
	}
	if (wbuf_is_eapol(wbuf)) {
		txctl->iseap = 1;
	}

	if (unlikely(txctl->ismcast)) {
		/*
		 * When servicing one or more stations in power-save mode (or)
		 * if there is some mcast data waiting on mcast queue
		 * (to prevent out of order delivery of mcast,bcast packets)
		 * multicast frames must be buffered until after the beacon.
		 * We use the private mcast queue for that.
		 */
		if ((!txctl->nocabq) && (txctl->ps || avp->av_mcastq.axq_depth)) {
			send_to_cabq = 1;
#if ATH_TX_BUF_FLOW_CNTL
			buf_used = &sc->sc_cabq->axq_num_buf_used;
#endif
		}
	}
#if ATH_TX_BUF_FLOW_CNTL
	/*
	 * This the using of tx_buf flow control for different priority
	 * queue. It is critical for WMM. Without this flow control,
	 * at lease for Linux and Maverick STA, WMM will fail even HW WMM queue
	 * works properly. Also the sc_txbuf_free counter must be count
	 * precisely, otherwise, tx_buf leak may happen or this flow control
	 * may not work.
	 */
	if (unlikely((*buf_used > MIN_BUF_RESV) &&
		     (sc->sc_txbuf_free < txq->axq_minfree))) {
		{
#if ATH_SUPPORT_FLOWMAC_MODULE
			/* check if OS can be told to stop sending frames */
			if (!sc->sc_osnetif_flowcntrl) {
#endif
				sc->sc_stats.ast_tx_nobuf++;
				sc->sc_stats.ast_txq_nobuf[txctl->qnum]++;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
				ath_ald_update_nobuf_stats((&an->
							    an_tx_tid[txctl->
								      tidno])->
							   ac);
#endif
				atomic_dec(&an->an_active_tx_cnt);
				return -ENOMEM;
#if ATH_SUPPORT_FLOWMAC_MODULE
			} else {
				/* inform kernel to stop sending the frames down to ath
				 * layer and try to send this frame alone.
				 */
				if (sc->sc_osnetif_flowcntrl) {
					ath_netif_stop_queue(sc);
				}
			}
#endif
		}
	}
#endif
#if QCA_AIRTIME_FAIRNESS
	psoc = wlan_pdev_get_psoc(pdev);
#endif
	/* For each sglist entry, allocate an ath_buf for DMA */
	TAILQ_INIT(&bf_head);
	for (i = 0; i < n_sg; i++, sg++) {
		int more_maps;
		ath_get_buf_status_t retval = ATH_BUF_NONE;

		more_maps = (n_sg - i) > 1;	//more than one descriptor
#if QCA_AIRTIME_FAIRNESS
		atf_based_alloc = 0;
		if (an->an_node && an->an_atf_capable) {
			if (sc->sc_ieee_ops->atf_buf_distribute) {
				atf_buf_alloc =
					sc->sc_ieee_ops->atf_buf_distribute(
					     pdev, (an)->an_node, txctl->tidno);
			}
			if (atf_buf_alloc == ATH_ATF_BUF_ALLOC) {
				atf_based_alloc = 1;
				/* Allocate buffer & account in ATF buf algo */
				retval = ath_tx_get_buf_aponly(sc, sg,
						&bf, &bf_head, buf_used);
			} else if (atf_buf_alloc == ATH_ATF_BUF_ALLOC_SKIP) {
				/* Allocate buffer, but do not account in
				 * ATF Buffer accouting algorithm
				 */
				retval = ath_tx_get_buf_aponly(sc, sg, &bf,
							&bf_head, buf_used);
			} else if (atf_buf_alloc == ATH_ATF_BUF_NOALLOC) {
				atf_based_alloc = 1;
				/* Do not allocate buffer */
				an->an_pkt_drop_nobuf++;
				retval = ATH_BUF_NONE;
			}
		} else {
			retval = ath_tx_get_buf_aponly(sc, sg, &bf, &bf_head, buf_used);
		}
#else
		retval = ath_tx_get_buf_aponly(sc, sg, &bf, &bf_head, buf_used);
#endif
#if QCA_AIRTIME_FAIRNESS
		if ((likely(retval != ATH_BUF_NONE)) && (atf_based_alloc == 1) && bf) {
			if (bf->bf_atf_accounting) {
			}
			bf->bf_atf_accounting = 1;
			bf->bf_atf_accounting_size = wbuf_get_pktlen(wbuf);
			ac = TID_TO_WME_AC(txctl->tidno);
			bf->bf_atf_sg = (void *)target_if_atf_update_buf_held(psoc, an->an_peer, ac);
		}
#endif
		if (unlikely(more_maps && (ATH_BUF_CONT == retval))) {
			continue;
		} else if (unlikely(ATH_BUF_NONE == retval)) {
			DPRINTF(sc, ATH_DEBUG_ANY,
				"%s no more ath bufs. num phys frags %d \n",
				__func__, n_sg);
			goto bad;
		}
		if (!bf) {
			DPRINTF(sc, ATH_DEBUG_ANY, "%s bf is NULL \n", __func__);
			goto bad;
		}
		bf->bf_frmlen = txctl->frmlen;
		bf->bf_isdata = txctl->isdata;
		bf->bf_ismcast = txctl->ismcast;
		bf->bf_useminrate = txctl->use_minrate;
		bf->bf_isbar = txctl->isbar;
		bf->bf_ispspoll = txctl->ispspoll;
#if ATH_SWRETRY
		bf->bf_ispspollresp = 0;
#endif
		bf->bf_calcairtime = txctl->calcairtime;
		bf->bf_flags = txctl->flags;
		bf->bf_shpreamble = txctl->shortPreamble;
		bf->bf_keytype = txctl->keytype;
		bf->bf_tidno = txctl->tidno;
		bf->bf_qnum =
		    (!send_to_cabq) ? txctl->qnum : sc->sc_cabq->axq_qnum;
		bf->bf_iseapol = txctl->iseap;
		bf->bf_isnulldata = txctl->isnulldata;
		bf->bf_nextfraglen = txctl->nextfraglen;

		rcs = (struct ath_rc_series *)&txctl->priv[0];
		bf->bf_rcs[0] = rcs[0];
		bf->bf_rcs[1] = rcs[1];
		bf->bf_rcs[2] = rcs[2];
		bf->bf_rcs[3] = rcs[3];
		bf->bf_node = an;
		bf->bf_mpdu = wbuf;
		bf->bf_reftxpower = txctl->txpower;

		/* setup descriptor */
		ds = bf->bf_desc;
		ath_hal_setdesclink(ah, ds, 0);
#ifndef REMOVE_PKT_LOG
		bf->bf_vdata = wbuf_header(wbuf);
#endif
		ASSERT(sc->sc_num_txmaps);

		bf->bf_pp_rcs.rate = 0;
		bf->bf_pp_rcs.tries = 0;
		/* if passed through wbuf fill that rate in */
#if UMAC_PER_PACKET_DEBUG
		if (unlikely(wbuf_get_rate(wbuf))) {
			bf->bf_pp_rcs.rate = wbuf_get_rate(wbuf);
			bf->bf_pp_rcs.tries = wbuf_get_retries(wbuf);
		}
#endif

		if (likely(0 == (i / sc->sc_num_txmaps))) {

			/*
			 * Save the DMA context in the first ath_buf
			 */
			OS_COPY_DMA_MEM_CONTEXT(OS_GET_DMA_MEM_CONTEXT
						(bf, bf_dmacontext),
						OS_GET_DMA_MEM_CONTEXT(txctl,
								       dmacontext));

			/*
			 * Formulate first tx descriptor with tx controls.
			 */
			ath_hal_set11n_txdesc(ah, ds, bf->bf_frmlen	/* frame length */
					      , txctl->atype	/* Atheros packet type */
					      , MIN(txctl->txpower, 60)	/* txpower */
					      , txctl->keyix	/* key cache index */
					      , txctl->keytype	/* key type */
					      , txctl->flags	/* flags */
			    );

			firstds = ds;
			firstbf = bf;

			ath_hal_filltxdesc(ah, ds, (bf->bf_buf_addr)	/* buffer address */
					   , bf->bf_buf_len	/* buffer length */
					   , 0	/* descriptor id */
					   , bf->bf_qnum	/* QCU number */
					   , txctl->keytype	/* key type */
					   , AH_TRUE	/* first segment */
					   , (n_sg <= sc->sc_num_txmaps) ? AH_TRUE : AH_FALSE	/* last segment */
					   , ds	/* first descriptor */
			    );
		} else {
			/* chain descriptor together */
			ath_hal_setdesclink(ah, lastds, bf->bf_daddr);

			ath_hal_filltxdesc(ah, ds, bf->bf_buf_addr	/* buffer address */
					   , (u_int32_t *) bf->bf_buf_len	/* buffer length */
					   , 0	/* descriptor id */
					   , bf->bf_qnum	/* QCU number */
					   , txctl->keytype	/* key type */
					   , AH_FALSE	/* first segment */
					   , (i == n_sg - 1) ? AH_TRUE : AH_FALSE	/* last segment */
					   , firstds	/* first descriptor */
			    );
		}

		lastds = ds;
	}

	if (firstbf) {
		struct ath_atx_tid *tid = ATH_AN_2_TID(an, txctl->tidno);

		firstbf->bf_lastfrm = bf;
		firstbf->bf_ht = txctl->ht;
#ifdef ATH_SUPPORT_UAPSD
		if (txctl->isuapsd) {
			ath_tx_queue_uapsd(sc, txq, &bf_head, txctl);
			atomic_dec(&an->an_active_tx_cnt);
			return 0;
		}
#endif
		/*
		 * No need to wait for sc_vap_pause_in_progress to be
		 * cleared in vap pause && tid paused, since eventually
		 * frame goes into TID rather than HW Q.
		 */
		if (ath_vap_pause_in_progress(sc) &&
		    qdf_atomic_read(&tid->paused))
			no_wait_for_vap_pause = true;
		if (!no_wait_for_vap_pause) {
			ath_vap_pause_txq_use_inc(sc);
#if ATH_VAP_PAUSE_SUPPORT
			if (sc->sc_vap_pause_timeout) {
				ath_vap_pause_txq_use_dec(sc);
				printk("%s:txq_pause in progress flag is set\n",
				       __func__);
				goto bad;
			}
#endif
		}
		ATH_TXQ_LOCK(txq);

		if (likely(txctl->ht && sc->sc_txaggr) && !an->an_pspoll) {
			if (likely(ath_aggr_query(tid))) {
				/*
				 * Try aggregation if it's a unicast data frame
				 * and the destination is HT capable.
				 */
				ath_tx_send_ampdu(sc, txq, tid, &bf_head,
						  txctl);
			} else {
				/*
				 * Send this frame as regular when ADDBA exchange
				 * is neither complete nor pending.
				 */
				ath_tx_send_normal(sc, txq, tid, &bf_head,
						   txctl);
			}
#if defined(ATH_ADDITIONAL_STATS) || ATH_SUPPORT_IQUE
			sc->sc_stats.ast_txq_packets[txq->axq_qnum]++;
#endif
		} else {
			firstbf->bf_lastbf = bf;
			firstbf->bf_nframes = 1;

			if (txctl->isbar) {
				/* This is required for resuming tid during BAR completion */
				firstbf->bf_tidno = wbuf_get_tid(wbuf);
			}

			if (!send_to_cabq) {
				ath_tx_send_normal(sc, txq, tid, &bf_head,
								txctl);
			} else {
#if ATH_TX_BUF_FLOW_CNTL
				/* reserving minimum buffer for unicast packets */
				if (sc->sc_txbuf_free < MCAST_MIN_FREEBUF) {
					ATH_TXQ_UNLOCK(txq);
					goto bad;
				}
#endif
				atomic_inc(&avp->av_beacon_cabq_use_cnt);
				if (atomic_read(&avp->av_stop_beacon) ||
				    avp->av_bcbuf == NULL) {
					ATH_TXQ_UNLOCK(txq);
					ath_vap_pause_txq_use_dec(sc);
					atomic_dec(&avp->
						   av_beacon_cabq_use_cnt);
					goto bad;
				}
#ifdef ATH_SWRETRY
				/*
				 * clear the dest mask if this is the first frame scheduled after all swr eligible
				 * frames have been popped out from txq
				 */
				if (sc->sc_enhanceddmasupport) {
					if ((an->
					     an_swretry_info[sc->sc_cabq->
							     axq_qnum]).
					    swr_state_filtering
					    && !(an->
						 an_swretry_info[sc->sc_cabq->
								 axq_qnum]).
					    swr_num_eligible_frms) {
						DPRINTF(sc, ATH_DEBUG_SWR,
							"%s: clear dest mask\n",
							__func__);
						ATH_NODE_SWRETRY_TXBUF_LOCK(an);
#if ATH_SWRETRY_MODIFY_DSTMASK
						(an->
						 an_swretry_info[sc->sc_cabq->
								 axq_qnum]).
						swr_need_cleardest = AH_TRUE;
						ath_tx_modify_cleardestmask(sc,
									    sc->
									    sc_cabq,
									    &bf_head);
#endif
						(an->
						 an_swretry_info[sc->sc_cabq->
								 axq_qnum]).
						swr_state_filtering = AH_FALSE;
						ATH_NODE_SWRETRY_TXBUF_UNLOCK
						    (an);
					}
				}
#endif

				ath_buf_set_rate(sc, firstbf);
				ATH_TXQ_LOCK(&avp->av_mcastq);
				ath_tx_mcastqaddbuf_internal_aponly(sc,
								    &avp->
								    av_mcastq,
								    &bf_head);
				ATH_TXQ_UNLOCK(&avp->av_mcastq);
				atomic_dec(&avp->av_beacon_cabq_use_cnt);

#ifdef ATH_SWRETRY
				if (!firstbf->bf_isampdu && firstbf->bf_isdata) {
					struct ieee80211_frame *wh;
					wh = (struct ieee80211_frame *)
					    wbuf_header(firstbf->bf_mpdu);
					ATH_NODE_SWRETRY_TXBUF_LOCK(an);
					(an->
					 an_swretry_info[sc->sc_cabq->
							 axq_qnum]).
					swr_num_eligible_frms++;
					ATH_NODE_SWRETRY_TXBUF_UNLOCK(an);
					DPRINTF(sc, ATH_DEBUG_SWR,
						"%s: dst=%s SeqCtrl=0x%02X%02X qnum=%d swr_num_eligible_frms=%d\n",
						__func__,
						ether_sprintf(wh->i_addr1),
						wh->i_seq[0], wh->i_seq[1],
						sc->sc_cabq->axq_qnum,
						(an->
						 an_swretry_info[sc->sc_cabq->
								 axq_qnum]).
						swr_num_eligible_frms);
				}
#endif
			}
		}

		if (!no_wait_for_vap_pause)
			ath_vap_pause_txq_use_dec(sc);
		atomic_dec(&an->an_active_tx_cnt);
		ATH_TXQ_UNLOCK(txq);
		return 0;
	}
bad:
	/*
	 * XXX: In other OS's, we can probably drop the frame. But in de-serialized
	 * windows driver (NDIS6.0), we're not allowd to tail drop frame when out
	 * of resources. So we just return NOMEM here and let OS shim to do whatever
	 * OS wants.
	 */
	ATH_TXBUF_LOCK(sc);
	if (!TAILQ_EMPTY(&bf_head)) {
		int num_buf = 0;
		ATH_NUM_BUF_IN_Q(&num_buf, &bf_head);
#if ATH_TX_BUF_FLOW_CNTL
		(*buf_used) -= num_buf;
#endif
		sc->sc_txbuf_free += num_buf;
		TAILQ_CONCAT(&sc->sc_txbuf, &bf_head, bf_list);
	}

	ATH_TXBUF_UNLOCK(sc);

	sc->sc_stats.ast_tx_nobuf++;
	sc->sc_stats.ast_txq_nobuf[txctl->qnum]++;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
	ath_ald_update_nobuf_stats((&an->an_tx_tid[txctl->tidno])->ac);
#endif

	atomic_dec(&an->an_active_tx_cnt);
	return -ENOMEM;
}

EXPORT_SYMBOL(ath_tx_start_dma_aponly);

/*
 * Sets the min-rate for non-data packets or for data packets where
 * use min-rate is set (e.g. EAPOL packets)
 */
static inline void
ath_rate_set_minrate_aponly(struct ath_softc *sc,
			    ieee80211_tx_control_t * txctl,
			    const HAL_RATE_TABLE * rt,
			    struct ath_rc_series *rcs)
{
	if (txctl->min_rate != 0)
		rcs[0].rix = ath_rate_findrix_aponly(rt, txctl->min_rate);
	else
		rcs[0].rix = sc->sc_minrateix;
	rcs[0].tries = ATH_MGT_TXMAXTRY;

	rcs[1].tries = 0;
	rcs[2].tries = 0;
	rcs[3].tries = 0;

}

static inline int
__ath_tx_prepare_aponly(struct ath_softc *sc, wbuf_t wbuf,
			ieee80211_tx_control_t * txctl)
{
	struct ath_node *an;
	u_int8_t rix;
#ifdef ATH_SUPERG_COMP
	int comp = ATH_COMP_PROC_NO_COMP_NO_CCS;
#endif
	struct ath_txq *txq = NULL;
	struct ieee80211_frame *wh;
	const HAL_RATE_TABLE *rt;
#ifdef USE_LEGACY_HAL
	u_int8_t antenna;
#endif
	struct ath_rc_series *rcs;
	//int subtype;

	txctl->dev = sc;

	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	rt = sc->sc_currates;
	KASSERT(rt != NULL, ("no rate table, mode %u", sc->sc_curmode));

	an = txctl->an;
	txq = &sc->sc_txq[txctl->qnum];

	/*
	 * Setup for rate calculations.
	 */
	rcs = (struct ath_rc_series *)&txctl->priv[0];
	/*
	 * Optimize : memzero is not required and only tries
	 * member can be initialized.
	 */
#if !ATH_TX_COMPACT
	OS_MEMZERO(rcs, sizeof(struct ath_rc_series) * 4);
#endif

	if (likely(txctl->isdata)) {
		if (unlikely(txctl->ismcast)) {
			rcs[0].rix =
			    (u_int8_t) ath_tx_findindex_aponly(rt,
							       txctl->
							       mcast_rate);

			/*
			 * mcast packets are not re-tried.
			 */
			rcs[0].tries = 1;
#if ATH_TX_COMPACT
			rcs[1].tries = 0;
			rcs[2].tries = 0;
			rcs[3].tries = 0;
#endif

		} else {
			/*
			 * For aggregation enabled nodes there is no need to do rate find
			 * on each of these frames.
			 */
			txctl->tidno = wbuf_get_tid(wbuf);
			if (unlikely(
#ifdef ATH_RIFS
					    !txctl->ht || (!sc->sc_txaggr
							   && !sc->sc_txrifs) ||
#else
					    !txctl->ht || !sc->sc_txaggr ||
#endif
					    !ath_aggr_query(ATH_AN_2_TID
							    (an,
							     txctl->tidno)))) {

				if (likely(!txctl->use_minrate)) {
				} else {
					ath_rate_set_minrate_aponly(sc, txctl,
								    rt, rcs);
				}

				if (
#ifdef ATH_RIFS
					   txctl->ht && (sc->sc_txaggr
							 || sc->sc_txrifs)
#else
					   txctl->ht && sc->sc_txaggr
#endif
				    ) {
					if (likely
					    (!(txctl->
					       flags & HAL_TXDESC_FRAG_IS_ON)))
					{
						struct ath_atx_tid *tid;

						tid =
						    ATH_AN_2_TID(an,
								 txctl->tidno);
						ATH_TXQ_LOCK(txq);
						*(u_int16_t *) wh->i_seq =
						    htole16(tid->
							    seq_next <<
							    IEEE80211_SEQ_SEQ_SHIFT);
						txctl->seqno = tid->seq_next;
						INCR(tid->seq_next,
						     IEEE80211_SEQ_MAX);
						ATH_TXQ_UNLOCK(txq);
					}
				}
			} else {
				//case for aggregates
				/*
				 * For HT capable stations, we save tidno for later use.
				 * We also override seqno set by upper layer with the one
				 * in tx aggregation state.
				 *
				 * First, the fragmentation stat is determined.  If fragmentation
				 * is on, the sequence number is not overridden, since it has been
				 * incremented by the fragmentation routine.
				 */
#if ATH_SUPPORT_IQUE
				/* If this frame is a HBR (headline block removal) probing QoSNull frame,
				 *                  * it should be sent at the min rate which is cached in ath_node->an_minRate[ac]
				 *                                   */
				if (wbuf_is_probing(wbuf)) {
					int isProbe;
					int ac = TID_TO_WME_AC(txctl->tidno);
					ath_rate_findrate(sc, an, AH_FALSE,
							  txctl->frmlen, 1, 0,
							  ac, rcs, &isProbe,
							  AH_FALSE,
							  txctl->flags, NULL);
					rcs[0].tries = 1;
					rcs[1].tries = 0;
					rcs[2].tries = 0;
					rcs[3].tries = 0;
				} else
#endif
				if (unlikely(txctl->use_minrate)) {
					ath_rate_set_minrate_aponly(sc, txctl,
								    rt, rcs);
				}
				if (likely
				    (!(txctl->flags & HAL_TXDESC_FRAG_IS_ON))) {
					struct ath_atx_tid *tid;

					tid = ATH_AN_2_TID(an, txctl->tidno);
					ATH_TXQ_LOCK(txq);
					*(u_int16_t *) wh->i_seq =
					    htole16(tid->
						    seq_next <<
						    IEEE80211_SEQ_SEQ_SHIFT);
					txctl->seqno = tid->seq_next;
					INCR(tid->seq_next, IEEE80211_SEQ_MAX);
					ATH_TXQ_UNLOCK(txq);
				}
			}
		}
	} else {
		txctl->tidno = WME_MGMT_TID;

		ath_rate_set_minrate_aponly(sc, txctl, rt, rcs);

#ifdef ATH_SUPPORT_TxBF
		{
			/*
			 * Force the Rate of Delay Report to be within 6 ~54 Mbps
			 * Use the Rate less than one the sounding request used, so the report will be delivered reliably
			 */
			u_int8_t *v_cv_data =
			    (u_int8_t *) (wbuf_header(wbuf) +
					  sizeof(struct ieee80211_frame));

			if ((wh->i_fc[0] == IEEE80211_FC0_SUBTYPE_ACTION)
			    && (*v_cv_data == IEEE80211_ACTION_CAT_HT)) {
				if ((*(v_cv_data + 1) ==
				     IEEE80211_ACTION_HT_COMP_BF)
				    || (*(v_cv_data + 1) ==
					IEEE80211_ACTION_HT_NONCOMP_BF)) {
					static u_int8_t map_rate[] = { 12, 18, 24, 36, 48, 72, 96, 108 };	/*dot11 Rate */
					u_int8_t rate_index =
					    ARRAY_LENGTH(map_rate);
					/*
					 * transfer Kbps to dot11 Rate for compare, dot11 rate units is 500 Kbps
					 * (c.f. IEEE802.11-2007 7.3.2.2) so divide Kbps by 500 to get dot11 rate units
					 */
					u_int16_t used_rate =
					    sc->sounding_rx_kbps / 500;

					do {
						rate_index--;
						if (rate_index == 0) {
							break;
						}
					} while (used_rate <=
						 map_rate[rate_index]);

					rcs[0].rix =
					    ath_rate_findrix_aponly(rt,
								    map_rate
								    [rate_index]);
					rcs[0].tries = 1;	/*Retry not need */
				}
			}
		}
#endif
		/* check and adjust the tsf for  probe response */
		if (txctl->atype == HAL_PKT_TYPE_PROBE_RESP) {
			struct ath_vap *avp = sc->sc_vaps[txctl->if_id];
			OS_MEMCPY(&wh[1], &avp->av_tsfadjust,
				  sizeof(avp->av_tsfadjust));
		}
	}
	rix = rcs[0].rix;

	/*
	 * Calculate duration.  This logically belongs in the 802.11
	 * layer but it lacks sufficient information to calculate it.
	 */
	if (likely((txctl->flags & HAL_TXDESC_NOACK) == 0 &&
		   (wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) !=
		   IEEE80211_FC0_TYPE_CTL)) {
		u_int16_t dur;
		/*
		 * XXX not right with fragmentation.
		 */
		//11g and 11n - short preamble, more likely
		if (likely(txctl->shortPreamble))
			dur = rt->info[rix].spAckDuration;
		else
			dur = rt->info[rix].lpAckDuration;

		if (unlikely(wh->i_fc[1] & IEEE80211_FC1_MORE_FRAG)) {
			dur += dur;	/* Add additional 'SIFS + ACK' */

			/*
			 ** Compute size of next fragment in order to compute
			 ** durations needed to update NAV.
			 ** The last fragment uses the ACK duration only.
			 ** Add time for next fragment.
			 */
			dur +=
			    ath_hal_computetxtime(sc->sc_ah, rt,
						  txctl->nextfraglen, rix,
						  txctl->shortPreamble);
		}

		if (unlikely(txctl->istxfrag)) {
			/*
			 **  Force hardware to use computed duration for next
			 **  fragment by disabling multi-rate retry, which
			 **  updates duration based on the multi-rate
			 **  duration table.
			 */
			rcs[1].tries = rcs[2].tries = rcs[3].tries = 0;
			rcs[1].rix = rcs[2].rix = rcs[3].rix = 0;
			rcs[0].tries = ATH_TXMAXTRY;	/* reset tries but keep rate index */
		}

		*(u_int16_t *) wh->i_dur = cpu_to_le16(dur);
	}

	/*
	 * Determine if a tx interrupt should be generated for
	 * this descriptor.  We take a tx interrupt to reap
	 * descriptors when the h/w hits an EOL condition or
	 * when the descriptor is specifically marked to generate
	 * an interrupt.  We periodically mark descriptors in this
	 * way to insure timely replenishing of the supply needed
	 * for sending frames.  Defering interrupts reduces system
	 * load and potentially allows more concurrent work to be
	 * done but if done to aggressively can cause senders to
	 * backup.
	 *
	 * NB: use >= to deal with sc_txintrperiod changing
	 *     dynamically through sysctl.
	 */
	ATH_TXQ_LOCK(txq);
	if (
#ifdef ATH_SUPPORT_UAPSD
		   (!txctl->isuapsd) &&
#endif
		   (++txq->axq_intrcnt >= sc->sc_txintrperiod)) {
		txctl->flags |= HAL_TXDESC_INTREQ;
		txq->axq_intrcnt = 0;
	}
	ATH_TXQ_UNLOCK(txq);

	if (unlikely(txctl->ismcast))
		sc->sc_mcastantenna = (sc->sc_mcastantenna + 1) & 0x1;

	/* Allow modifying destination mask only if ATH_SWRETRY_MODIFY_DSTMASK
	 * is enabled. This will enable a HW optimization to filter out pkts
	 * to particular destination after HW retry-failure.
	 */
#if defined(ATH_SWRETRY) && defined(ATH_SWRETRY_MODIFY_DSTMASK)
	/* Management frames will not go for SW Retry
	 * process unless they are failed with filtered
	 * error.
	 */
	if (!(an) || (an && !(an->an_swrenabled))) {
		txctl->flags |= HAL_TXDESC_CLRDMASK;
	} else {
		ATH_TXQ_LOCK(txq);
		if (txq->axq_destmask) {
			txctl->flags |= HAL_TXDESC_CLRDMASK;
			if (txctl->isdata) {
				txq->axq_destmask = AH_FALSE;	/*Turn-off destmask only for subsequent data frames */
			}
		}
		ATH_TXQ_UNLOCK(txq);
	}
#endif

	/*
	 * XXX: Update some stats ???
	 */
	if (likely(txctl->shortPreamble))
#ifdef QCA_SUPPORT_CP_STATS
		pdev_lmac_cp_stats_ast_tx_shortpre_inc(sc->sc_pdev, 1);
#else
		sc->sc_stats.ast_tx_shortpre++;
#endif
	if (unlikely(txctl->flags & HAL_TXDESC_NOACK))
#ifdef QCA_SUPPORT_CP_STATS
		pdev_lmac_cp_stats_ast_tx_noack_inc(sc->sc_pdev, 1);
#else
		sc->sc_stats.ast_tx_noack++;
#endif

	return 0;
}

#ifndef DECR
#define DECR(_l,  _sz)  (_l)--; (_l) &= ((_sz) - 1)
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22))
#define  UNI_SKB_END_POINTER(skb)   (skb)->end
#else
#define  UNI_SKB_END_POINTER(skb)    skb_end_pointer(skb)
#endif

static inline int
__wbuf_map_sg_aponly(osdev_t osdev, qdf_nbuf_t nbf, dma_addr_t * pa, void *arg)
{
	struct scatterlist sg;

	//begin of shim_2
	*pa =
	    bus_map_single(osdev, nbf->data,
			   UNI_SKB_END_POINTER(nbf) - nbf->data,
			   BUS_DMA_TODEVICE);

	/* setup S/G list */
	memset(&sg, 0, sizeof(struct scatterlist));
	sg_dma_address(&sg) = *pa;
	sg_dma_len(&sg) = nbf->len;

	if (unlikely(ath_tx_start_dma_aponly(nbf, &sg, 1, arg) != 0)) {
		ieee80211_tx_control_t *txctl = (ieee80211_tx_control_t *) arg;	/* XXX */
		struct ath_softc *sc = (struct ath_softc *)txctl->dev;
		ieee80211_tx_status_t tx_status;
		struct ath_atx_tid *tid;
		struct ath_txq *txq = &sc->sc_txq[txctl->qnum];

		/*
		 * NB: common code doesn't tail drop frame
		 * because it's not allowed in NDIS 6.0.
		 * For Linux, we have to do it here.
		 */
		bus_unmap_single(osdev, *pa,
				 UNI_SKB_END_POINTER(nbf) - nbf->data,
				 BUS_DMA_TODEVICE);

		tx_status.retries = 0;
		tx_status.flags = ATH_TX_ERROR;
#ifdef ATH_SUPPORT_TxBF
		tx_status.txbf_status = 0;
#endif

#ifdef ATH_RIFS
		if (txctl->ht && (sc->sc_txaggr || sc->sc_txrifs)) {
#else
		if (txctl->ht && sc->sc_txaggr) {
#endif
			// Reclaim the seqno.
			ATH_TXQ_LOCK(txq);
			tid =
			    ATH_AN_2_TID((struct ath_node *)txctl->an,
					 txctl->tidno);
			DECR(tid->seq_next, IEEE80211_SEQ_MAX);
			ATH_TXQ_UNLOCK(txq);
		}

		sc->sc_ieee_ops->tx_complete(nbf, &tx_status, 0);
	}
	return 0;
}

#ifdef QCA_PARTNER_PLATFORM
int
ath_tx_start_aponly(ath_dev_t dev, wbuf_t wbuf, ieee80211_tx_control_t * txctl)
#else
static inline int
ath_tx_start_aponly(ath_dev_t dev, wbuf_t wbuf, ieee80211_tx_control_t * txctl)
#endif
{
	struct ath_softc *sc = ATH_DEV_TO_SC(dev);
	int error = 0;

#ifdef PROFILE_LMAC_1
	int i = 0;

	if (wbuf_is_encap_done(wbuf)) {
		if (!lmac_root) {
			lmac_root =
			    qdf_perf_init(0, "lmac1_tx_path_ap_only",
					  EVENT_GROUP);

			perf_cntr[0] =
			    qdf_perf_init(umac_root, "cpu_cycles",
					  EVENT_CPU_CYCLES);
			perf_cntr[1] =
			    qdf_perf_init(umac_root, "dcache_miss",
					  EVENT_ICACHE_MISS);
			perf_cntr[2] =
			    qdf_perf_init(umac_root, "icache_miss",
					  EVENT_DCACHE_MISS);
		}

		for (i = 0; i < 3; i++)
			qdf_perf_start(perf_cntr[i]);
	}
#endif

#if LMAC_SUPPORT_POWERSAVE_QUEUE
	if (wbuf_is_legacy_ps(wbuf)) {
		struct ath_node_pwrsaveq *dataq, *mgmtq, *psq;
		struct ath_node *an = (struct ath_node *)txctl->an;
		ath_wbuf_t athwbuf = (ath_wbuf_t) OS_MALLOC(sc->sc_osdev,
							    sizeof(struct
								   ath_wbuf),
							    GFP_KERNEL);

		if (!athwbuf) {
			return -EIO;
		}

		ASSERT(an);

		dataq = ATH_NODE_PWRSAVEQ_DATAQ(an);
		mgmtq = ATH_NODE_PWRSAVEQ_MGMTQ(an);
		psq = txctl->isdata ? dataq : mgmtq;

		athwbuf->wbuf = wbuf;
		athwbuf->next = NULL;
		OS_MEMCPY(&athwbuf->txctl, txctl,
			  sizeof(ieee80211_tx_control_t));

		ath_node_pwrsaveq_queue(an, athwbuf,
					txctl->
					isdata ? IEEE80211_FC0_TYPE_DATA :
					IEEE80211_FC0_TYPE_MGT);
		return EOK;
	}
#endif

	error = __ath_tx_prepare_aponly(sc, wbuf, txctl);

	//almost end of lmac
#ifdef PROFILE_LMAC_1
	if (wbuf_is_encap_done(wbuf)) {
		for (i = 0; i < 3; i++)
			qdf_perf_start(perf_cntr[i]);
	}
#endif

	if (likely(error == 0)) {
		/*
		 * Start DMA mapping.
		 * ath_tx_start_dma() will be called either synchronously
		 * or asynchrounsly once DMA is complete.
		 */
		error = __wbuf_map_sg_aponly(sc->sc_osdev, wbuf,
					     OS_GET_DMA_MEM_CONTEXT(txctl,
								    dmacontext),
					     txctl);
	}
	/* failed packets will be dropped by the caller */
	return error;
}

struct ieee80211_txctl_cap {
	u_int8_t ismgmt;
	u_int8_t ispspoll;
	u_int8_t isbar;
	u_int8_t isdata;
	u_int8_t isqosdata;
	u_int8_t use_minrate;
	u_int8_t atype;
	u_int8_t ac;
	u_int8_t use_ni_minbasicrate;
	u_int8_t use_mgt_rate;
};
enum {
	IEEE80211_MGMT_DEFAULT = 0,
	IEEE80211_MGMT_BEACON = 1,
	IEEE80211_MGMT_PROB_RESP = 2,
	IEEE80211_MGMT_PROB_REQ = 3,
	IEEE80211_MGMT_ATIM = 4,
	IEEE80211_CTL_DEFAULT = 5,
	IEEE80211_CTL_PSPOLL = 6,
	IEEE80211_CTL_BAR = 7,
	IEEE80211_DATA_DEFAULT = 8,
	IEEE80211_DATA_NODATA = 9,
	IEEE80211_DATA_QOS = 10,
	IEEE80211_TYPE4TXCTL_MAX = 11,
};

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
static const HAL_KEY_TYPE keytype_table[WLAN_CRYPTO_CIPHER_MAX+1] = {
	HAL_KEY_TYPE_WEP,  /* WLAN_CRYPTO_CIPHER_WEP */
	HAL_KEY_TYPE_TKIP, /* WLAN_CRYPTO_CIPHER_TKIP */
	HAL_KEY_TYPE_AES,  /* WLAN_CRYPTO_CIPHER_AES_OCB */
	HAL_KEY_TYPE_AES,  /* WLAN_CRYPTO_CIPHER_AES_CCM */
#if ATH_SUPPORT_WAPI
	HAL_KEY_TYPE_WAPI, /* WLAN_CRYPTO_CIPHER_WAPI_SMS4 */
#else
	HAL_KEY_TYPE_CLEAR,
#endif
	HAL_KEY_TYPE_WEP,  /* WLAN_CRYPTO_CIPHER_CKIP */
	HAL_KEY_TYPE_CLEAR,/* WLAN_CRYPTO_CIPHER_AES_CMAC */
	HAL_KEY_TYPE_CLEAR,/* WLAN_CRYPTO_CIPHER_AES_CCM_256 */
	HAL_KEY_TYPE_CLEAR,/* WLAN_CRYPTO_CIPHER_AES_CMAC_256 */
	HAL_KEY_TYPE_CLEAR,/* WLAN_CRYPTO_CIPHER_AES_GCM */
	HAL_KEY_TYPE_CLEAR,/* WLAN_CRYPTO_CIPHER_AES_GCM_256 */
	HAL_KEY_TYPE_CLEAR,/* WLAN_CRYPTO_CIPHER_AES_GMAC */
	HAL_KEY_TYPE_CLEAR,/* WLAN_CRYPTO_CIPHER_AES_GMAC_256 */
	HAL_KEY_TYPE_CLEAR,/* WLAN_CRYPTO_CIPHER_WAPI_GCM4 */
	HAL_KEY_TYPE_CLEAR,/* WLAN_CRYPTO_CIPHER_FILS_AEAD */
	HAL_KEY_TYPE_CLEAR,/* WLAN_CRYPTO_CIPHER_NONE */
};
#else
static const HAL_KEY_TYPE keytype_table[IEEE80211_CIPHER_MAX] = {
	HAL_KEY_TYPE_WEP,	/*IEEE80211_CIPHER_WEP */
	HAL_KEY_TYPE_TKIP,	/*IEEE80211_CIPHER_TKIP */
	HAL_KEY_TYPE_AES,	/*IEEE80211_CIPHER_AES_OCB */
	HAL_KEY_TYPE_AES,	/*IEEE80211_CIPHER_AES_CCM */
#if ATH_SUPPORT_WAPI
	HAL_KEY_TYPE_WAPI,	/*IEEE80211_CIPHER_WAPI */
#else
	HAL_KEY_TYPE_CLEAR,
#endif
	HAL_KEY_TYPE_WEP,	/*IEEE80211_CIPHER_CKIP */
	HAL_KEY_TYPE_CLEAR,	/*IEEE80211_CIPHER_NONE */
};
#endif

static struct ieee80211_txctl_cap txctl_cap[IEEE80211_TYPE4TXCTL_MAX] = {
	{1, 0, 0, 0, 0, 1, HAL_PKT_TYPE_NORMAL, WME_AC_VO, 1, 1},	/*default for mgmt */
	{1, 0, 0, 0, 0, 1, HAL_PKT_TYPE_BEACON, WME_AC_VO, 1, 1},	/*beacon */
	{1, 0, 0, 0, 0, 1, HAL_PKT_TYPE_PROBE_RESP, WME_AC_VO, 1, 1},	/*prob resp */
	{1, 0, 0, 0, 0, 1, HAL_PKT_TYPE_NORMAL, WME_AC_VO, 0, 1},	/*prob req */
	{1, 0, 0, 0, 0, 1, HAL_PKT_TYPE_ATIM, WME_AC_VO, 1, 1},	/*atim */
	{0, 0, 0, 0, 0, 1, HAL_PKT_TYPE_NORMAL, WME_AC_VO, 0, 0},	/*default for ctl */
	{0, 1, 0, 0, 0, 1, HAL_PKT_TYPE_PSPOLL, WME_AC_VO, 0, 0},	/*pspoll */
	{0, 0, 1, 0, 0, 1, HAL_PKT_TYPE_NORMAL, WME_AC_VO, 0, 0},	/*bar */
	{0, 0, 0, 1, 0, 0, HAL_PKT_TYPE_NORMAL, WME_AC_BE, 0, 1},	/*default for data */
	{1, 0, 0, 0, 0, 1, HAL_PKT_TYPE_NORMAL, WME_AC_VO, 1, 1},	/*nodata */
	{0, 0, 0, 1, 1, 0, HAL_PKT_TYPE_NORMAL, WME_AC_BE, 0, 1},	/*qos data, the AC to be modified based on pkt's ac */
};

static inline int
ath_tx_prepare_aponly(struct ath_softc_net80211 *scn, wbuf_t wbuf,
		      int nextfraglen, ieee80211_tx_control_t * txctl)
{
	struct wlan_objmgr_peer *peer = wbuf_get_peer(wbuf);
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
	struct ieee80211_frame *wh;
	int keyix = 0, hdrlen, pktlen;
	int type, subtype;
	int txctl_tab_index;
	u_int32_t txctl_flag_mask = 0;
	u_int8_t acnum, use_ni_minbasicrate, use_mgt_rate;
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	uint8_t macaddr[IEEE80211_ADDR_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	uint8_t *mac;
#endif

	HAL_KEY_TYPE keytype = HAL_KEY_TYPE_CLEAR;

	OS_MEMZERO(txctl, sizeof(ieee80211_tx_control_t));

	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		return -EIO;
	}
	wh = (struct ieee80211_frame *)wbuf_header(wbuf);

	txctl->ismcast = IEEE80211_IS_MULTICAST(wh->i_addr1);
	txctl->istxfrag = (wh->i_fc[1] & IEEE80211_FC1_MORE_FRAG) ||
	    (((le16toh(*((u_int16_t *) & (wh->i_seq[0]))) >>
	       IEEE80211_SEQ_FRAG_SHIFT) & IEEE80211_SEQ_FRAG_MASK) > 0);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

	/*
	 * Packet length must not include any
	 * pad bytes; deduct them here.
	 */
	hdrlen = ieee80211_anyhdrsize(wh);
	pktlen = wbuf_get_pktlen(wbuf);
	pktlen -= (hdrlen & 3);

	if (wlan_vdev_mlme_feat_ext_cap_get(vdev, WLAN_VDEV_FEXT_SAFEMODE)) {
		/* For Safe Mode, the encryption and its encap is already done
		   by the upper layer software. Driver do not modify the packet. */
		keyix = HAL_TXKEYIX_INVALID;
	}
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	else if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		struct wlan_objmgr_psoc *psoc;
		struct wlan_lmac_if_crypto_rx_ops *crypto_rx_ops;

		psoc = wlan_pdev_get_psoc(pdev);
		if (!psoc || !dp_peer) {
			qdf_print("%s[%d]psoc or dp_peer is NULL", __func__, __LINE__);
			return -EIO;
		}

		if(txctl->ismcast || dp_peer->key_type == WLAN_CRYPTO_CIPHER_NONE) {
			mac = macaddr;
			if ((false /* tbd_da_crypto: Needs API in crypto component to check if MFP + SW encryption is enabled */
				 && IEEE80211_IS_MFP_FRAME(wh))) {
				keyix = dadp_vdev_get_clearkeyix(vdev, dp_vdev->def_tx_keyix);
				keytype = HAL_KEY_TYPE_CLEAR;
			} else
				keyix = dp_vdev->vkey[dp_vdev->def_tx_keyix].keyix;
		}
		else {
			mac = wlan_peer_get_macaddr(peer);
			if ((false /* tbd_da_crypto: Needs API in crypto component to check if MFP + SW encryption is enabled */
				 && IEEE80211_IS_MFP_FRAME(wh))) {
				keyix = dadp_peer_get_clearkeyix(peer);
				keytype = HAL_KEY_TYPE_CLEAR;
			} else
				keyix = dp_peer->keyix;
		}

		crypto_rx_ops = wlan_crypto_get_crypto_rx_ops(psoc);
		if (crypto_rx_ops && WLAN_CRYPTO_RX_OPS_ENCAP(crypto_rx_ops)) {
#ifdef QCA_PARTNER_PLATFORM
			WLAN_CRYPTO_RX_OPS_ENCAP(crypto_rx_ops) (vdev,
								 wbuf,
								 mac,
								 wbuf_is_encap_done(wbuf));
#else
			WLAN_CRYPTO_RX_OPS_ENCAP(crypto_rx_ops) (vdev,
								 wbuf,
								 mac,
								 0);
#endif
		}

		pktlen = wbuf_get_pktlen(wbuf);
		pktlen -= (hdrlen & 3);

		if (dp_peer->key_type <= WLAN_CRYPTO_CIPHER_MAX)
			keytype = keytype_table[dp_peer->key_type];

	} else if (dp_peer->key_type ==
		   WLAN_CRYPTO_CIPHER_NONE) {
		/*
		 * Use station key cache slot, if assigned.
		 */
		keyix = dp_peer->keyix;
		if (keyix == IEEE80211_KEYIX_NONE)
			keyix = HAL_TXKEYIX_INVALID;
	} else
		keyix = HAL_TXKEYIX_INVALID;
#else
	else if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		const struct ieee80211_cipher *cip;
		struct ieee80211_key *k;

		/*
		 * Construct the 802.11 header+trailer for an encrypted
		 * frame. The only reason this can fail is because of an
		 * unknown or unsupported cipher/key type.
		 */

		/* FFXXX: change to handle linked wbufs */
		k = wlan_crypto_peer_encap(peer, wbuf);
		if (k == NULL) {
			/*
			 * This can happen when the key is yanked after the
			 * frame was queued.  Just discard the frame; the
			 * 802.11 layer counts failures and provides
			 * debugging/diagnostics.
			 */
			return -EIO;
		}
		/* update the value of wh since encap can reposition the header */
		wh = (struct ieee80211_frame *)wbuf_header(wbuf);

		/*
		 * Adjust the packet + header lengths for the crypto
		 * additions and calculate the h/w key index. When
		 * a s/w mic is done the frame will have had any mic
		 * added to it prior to entry so wbuf pktlen above will
		 * account for it. Otherwise we need to add it to the
		 * packet length.
		 */
		cip = k->wk_cipher;
		hdrlen += cip->ic_header;
#ifndef QCA_PARTNER_PLATFORM
		pktlen += cip->ic_header + cip->ic_trailer;
#else
		if (wbuf_is_encap_done(wbuf))
			pktlen += cip->ic_trailer;
		else
			pktlen += cip->ic_header + cip->ic_trailer;
#endif

		if (likely((k->wk_flags & IEEE80211_KEY_SWMIC) == 0)) {
			if (!txctl->istxfrag)
				pktlen += cip->ic_miclen;
			else {
				if (cip->ic_cipher != IEEE80211_CIPHER_TKIP)
					pktlen += cip->ic_miclen;
			}
		} else {
			pktlen += cip->ic_miclen;
		}
		if (cip->ic_cipher < IEEE80211_CIPHER_MAX) {
			keytype = keytype_table[cip->ic_cipher];
		}
		if (unlikely
		    (((k->wk_flags & IEEE80211_KEY_MFP)
		      && IEEE80211_IS_MFP_FRAME(wh)))) {
			if (cip->ic_cipher == IEEE80211_CIPHER_TKIP) {
				DPRINTF((struct ath_softc *)scn->sc_dev,
					ATH_DEBUG_KEYCACHE,
					"%s: extend MHDR IE\n", __func__);
				/* mfp packet len could be extended by MHDR IE */
				pktlen += sizeof(struct ieee80211_ccx_mhdr_ie);
			}

			keyix = k->wk_clearkeyix;
			keytype = HAL_KEY_TYPE_CLEAR;
		} else
			keyix = k->wk_keyix;

	} else if (dp_peer->key
		   && dp_peer->key->wk_cipher == &ieee80211_cipher_none) {
		/*
		 * Use station key cache slot, if assigned.
		 */
		keyix = dp_peer->key->wk_keyix;
		if (keyix == IEEE80211_KEYIX_NONE)
			keyix = HAL_TXKEYIX_INVALID;
	} else
		keyix = HAL_TXKEYIX_INVALID;
#endif /* WLAN_CONV_CRYPTO */

	pktlen += IEEE80211_CRC_LEN;

	txctl->frmlen = pktlen;
	txctl->keyix = keyix;
	txctl->keytype = keytype;
	txctl->txpower = wlan_peer_get_txpower(peer);
	txctl->nextfraglen = nextfraglen;
#ifdef USE_LEGACY_HAL
	txctl->hdrlen = hdrlen;
#endif
#if ATH_SUPPORT_IQUE
	txctl->tidno = wbuf_get_tid(wbuf);
#endif
	/*
	 * NB: the 802.11 layer marks whether or not we should
	 * use short preamble based on the current mode and
	 * negotiated parameters.
	 */
	if (wlan_pdev_nif_feat_cap_get(pdev, WLAN_PDEV_F_SHPREAMBLE) &&
	    !wlan_pdev_nif_feat_cap_get(pdev, WLAN_PDEV_F_USEBARKER) &&
	    wlan_peer_check_cap(peer, IEEE80211_CAPINFO_SHORT_PREAMBLE)) {
		txctl->shortPreamble = 1;
	}
#if !defined(ATH_SWRETRY) || !defined(ATH_SWRETRY_MODIFY_DSTMASK)
	txctl->flags = HAL_TXDESC_CLRDMASK;	/* XXX needed for crypto errs */
#endif

	/*
	 * Calculate Atheros packet type from IEEE80211
	 * packet header and select h/w transmit queue.
	 */
	if (type == IEEE80211_FC0_TYPE_MGT) {
		if (subtype == IEEE80211_FC0_SUBTYPE_BEACON) {
			txctl_tab_index = IEEE80211_MGMT_BEACON;
		} else if (subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP) {
			txctl_tab_index = IEEE80211_MGMT_PROB_RESP;
		} else if (subtype == IEEE80211_FC0_SUBTYPE_PROBE_REQ) {
			txctl_tab_index = IEEE80211_MGMT_PROB_REQ;
		} else if (subtype == IEEE80211_FC0_SUBTYPE_ATIM) {
			txctl_tab_index = IEEE80211_MGMT_ATIM;
		} else {
			txctl_tab_index = IEEE80211_MGMT_DEFAULT;
		}
	} else if (type == IEEE80211_FC0_TYPE_CTL) {
		if (subtype == IEEE80211_FC0_SUBTYPE_PS_POLL) {
			txctl_tab_index = IEEE80211_CTL_PSPOLL;
		} else if (subtype == IEEE80211_FC0_SUBTYPE_BAR) {
			txctl_tab_index = IEEE80211_CTL_BAR;
		} else {
			txctl_tab_index = IEEE80211_CTL_DEFAULT;
		}
	} else if (type == IEEE80211_FC0_TYPE_DATA) {
		if (subtype == IEEE80211_FC0_SUBTYPE_NODATA) {
			txctl_tab_index = IEEE80211_DATA_NODATA;
		} else if (subtype & IEEE80211_FC0_SUBTYPE_QOS) {
			txctl_tab_index = IEEE80211_DATA_QOS;
		} else {
			txctl_tab_index = IEEE80211_DATA_DEFAULT;
		}
	} else {
		printk("bogus frame type 0x%x (%s)\n",
		       wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK, __func__);
		/* XXX statistic */
		return -EIO;
	}
	txctl->ismgmt = txctl_cap[txctl_tab_index].ismgmt;
	txctl->ispspoll = txctl_cap[txctl_tab_index].ispspoll;
	txctl->isbar = txctl_cap[txctl_tab_index].isbar;
	txctl->isdata = txctl_cap[txctl_tab_index].isdata;
	txctl->isqosdata = txctl_cap[txctl_tab_index].isqosdata;
	txctl->use_minrate = txctl_cap[txctl_tab_index].use_minrate;
	txctl->atype = txctl_cap[txctl_tab_index].atype;
	acnum = txctl_cap[txctl_tab_index].ac;
	use_ni_minbasicrate = txctl_cap[txctl_tab_index].use_ni_minbasicrate;
	use_mgt_rate = txctl_cap[txctl_tab_index].use_mgt_rate;

	/*
	 * Update some txctl fields
	 */
	if (likely
	    (type == IEEE80211_FC0_TYPE_DATA
	     && subtype != IEEE80211_FC0_SUBTYPE_NODATA)) {
		if (unlikely(wbuf_is_eapol(wbuf))) {
			txctl->use_minrate = 1;
			txctl->iseap = 1;
		} else {
			txctl->iseap = 0;
		}
		if (unlikely(txctl->ismcast)) {
			txctl->mcast_rate = dp_vdev->vdev_mcast_rate;
		}
		if (likely(subtype & IEEE80211_FC0_SUBTYPE_QOS)) {
			/* XXX validate frame priority, remove mask */
			acnum = wbuf_get_priority(wbuf) & 0x03;

			if (wlan_pdev_get_wmep_noackPolicy(pdev, acnum))
				txctl_flag_mask |= HAL_TXDESC_NOACK;

#ifdef ATH_SUPPORT_TxBF
			/* Qos frame with Order bit set indicates an HTC frame */
			if (wh->i_fc[1] & IEEE80211_FC1_ORDER) {
				int is4addr;
				u_int8_t *htc;
				u_int8_t *tmpdata;

				is4addr =
				    ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) ==
				     IEEE80211_FC1_DIR_DSTODS) ? 1 : 0;
				if (!is4addr) {
					htc =
					    ((struct ieee80211_qosframe_htc *)
					     wh)->i_htc;
				} else {
					htc =
					    ((struct
					      ieee80211_qosframe_htc_addr4 *)
					     wh)->i_htc;
				}

				tmpdata = (u_int8_t *) wh;
				/* This is a sounding frame */
				if ((htc[2] == IEEE80211_HTC2_CSI_COMP_BF) ||
				    (htc[2] == IEEE80211_HTC2_CSI_NONCOMP_BF) ||
				    ((htc[2] & IEEE80211_HTC2_CalPos) == 3)) {
					//printk("==>%s,txctl flag before attach sounding%x,\n",__func__,txctl->flags);
					if (wlan_pdev_get_tx_staggered_sounding
					    (pdev)
					    &&
					    wlan_peer_get_rx_staggered_sounding
					    (peer)) {
						//txctl->flags |= HAL_TXDESC_STAG_SOUND;
						txctl_flag_mask |=
						    (HAL_TXDESC_STAG_SOUND <<
						     HAL_TXDESC_TXBF_SOUND_S);
					} else {
						txctl_flag_mask |=
						    (HAL_TXDESC_SOUND <<
						     HAL_TXDESC_TXBF_SOUND_S);
					}
					txctl_flag_mask |=
					    ((wlan_peer_get_channel_estimation_cap(peer)) << HAL_TXDESC_CEC_S);
					//printk("==>%s,txctl flag %x,tx staggered sounding %x, rx staggered sounding %x\n"
					//  ,__func__,txctl->flags,ic->ic_txbf.tx_staggered_sounding,ni->ni_txbf.rx_staggered_sounding);
				}

				if ((htc[2] & IEEE80211_HTC2_CalPos) != 0)	// this is a calibration frame
				{
					txctl_flag_mask |= HAL_TXDESC_CAL;
				}
			}
#endif

		} else {
			/*
			 * Default all non-QoS traffic to the best-effort queue.
			 */
			wbuf_set_priority(wbuf, WME_AC_BE);
		}

		ath_uapsd_txctl_update(scn, wbuf, txctl);

		txctl_flag_mask |=
		    ((wlan_pdev_get_ldpcap(pdev) & IEEE80211_HTCAP_C_LDPC_TX) &&
		     (wlan_peer_get_htcap(peer) & IEEE80211_HTCAP_C_ADVCODING))
		    ? HAL_TXDESC_LDPC : 0;

		/*
		 * For HT capable stations, we save tidno for later use.
		 * We also override seqno set by upper layer with the one
		 * in tx aggregation state.
		 */
		if (!txctl->ismcast
		    && wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_HT))
			txctl->ht = 1;
	}
	/*
	 * Set min rate and qnum in txctl based on acnum
	 */
	if (txctl->use_minrate) {
		/*
		 * if management rate is set, then use it.
		 */
		if (use_mgt_rate) {
			if (wlan_vdev_get_mgt_rate(vdev)) {
				txctl->min_rate = wlan_vdev_get_mgt_rate(vdev);
			}
		}
	}
	txctl->qnum = scn->sc_ac2q[acnum];
	/* Update the uapsd ctl for all frames */
	ath_uapsd_txctl_update(scn, wbuf, txctl);

	/*
	 * If we are servicing one or more stations in power-save mode.
	 */
	txctl->if_id = wlan_vdev_get_id(vdev);
	if (wlan_vdev_has_pssta(vdev))
		txctl->ps = 1;

	if (wlan_vdev_mlme_feat_ext_cap_get(vdev, IEEE80211_FEXT2_NOCABQ)) {
		txctl->nocabq = 1;
	}

	/*
	 * Calculate miscellaneous flags.
	 */
	if (txctl->ismcast) {
		txctl_flag_mask |= HAL_TXDESC_NOACK;	/* no ack on broad/multicast */
	} else if (pktlen > wlan_vdev_get_rtsthreshold(vdev)) {
		txctl_flag_mask |= HAL_TXDESC_RTSENA;	/* RTS based on frame length */
	}

	/* Frame to enable SM power save */
	if (wbuf_is_smpsframe(wbuf)) {
		txctl_flag_mask |= HAL_TXDESC_LOWRXCHAIN;
	}

	/*
	 * Update txctl->flags based on the flag mask
	 */
	txctl->flags |= txctl_flag_mask;
	IEEE80211_HTC_SET_NODE_INDEX(txctl, wbuf);

	return 0;
}

static inline wbuf_t
ieee80211_encap_80211_aponly(struct wlan_objmgr_peer *peer, wbuf_t wbuf)
{
	int key_mapping_key = 0;
	struct ieee80211_frame *wh;
	int type, subtype;
	int useqos = 0, use4addr = 0, usecrypto = 0;
	int hdrsize, datalen, pad, addlen;	/* additional header length we want to append */
	int ac = wbuf_get_priority(wbuf);
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	int key_set = 0;
#else
	struct ieee80211_key *key = NULL;
#endif

	if (!dp_peer) {
		qdf_print("%s:dp_peer is NULL ", __func__);
		goto bad;
	}

	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
#if ATH_WDS_SUPPORT_APONLY
	use4addr = ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) ==
		    IEEE80211_FC1_DIR_DSTODS) ? 1 : 0;

	if (use4addr)
		hdrsize = sizeof(struct ieee80211_frame_addr4);
	else
		hdrsize = sizeof(struct ieee80211_frame);
#else
	use4addr = 0;
	hdrsize = sizeof(struct ieee80211_frame);
#endif
	datalen = wbuf_get_pktlen(wbuf) - (hdrsize + sizeof(struct llc));	/* NB: w/o 802.11 header */

	if (wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_PRIVACY) &&	/* crypto is on */
	    (type == IEEE80211_FC0_TYPE_DATA)) {	/* only for data frame */
		/*
		 * Find the key that would be used to encrypt the frame if the
		 * frame were to be encrypted. For unicast frame, search the
		 * matching key in the key mapping table first. If not found,
		 * used default key. For multicast frame, only use the default key.
		 */
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
		if (!dp_vdev || !dp_peer) {
			qdf_print("%s[%d]dp_vdev or dp_peer is NULL", __func__, __LINE__);
			goto bad;
		}

		if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
			/* use unicast key */
			key_set = ((dp_peer->keyix == WLAN_CRYPTO_KEYIX_NONE) ? 0 : 1);
		}
		if (key_set && dadp_peer_get_key_valid(peer))
			key_mapping_key = 1;
		else {
			if(dadp_vdev_get_default_keyix(vdev) != WLAN_CRYPTO_KEYIX_NONE) {
				if(dp_vdev->vkey[dp_vdev->def_tx_keyix].keyix != WLAN_CRYPTO_KEYIX_NONE
				   && dadp_vdev_get_key_valid(vdev, dp_vdev->def_tx_keyix))
					key_set = 1;
			}
		}
#else
		if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
			/* use unicast key */
			key = dp_peer->key;
		}
		if (dp_peer->key && dp_peer->key->wk_valid) {
			key_mapping_key = 1;
		} else {
			if (wlan_vdev_get_def_txkey(vdev) !=
			    IEEE80211_KEYIX_NONE) {
				key = wlan_vdev_get_nw_keys(vdev);
				if (!key->wk_valid) {
					key = NULL;
				}
			} else {
				key = NULL;
			}
		}
#endif
		/*
		 * Assert our Exemption policy.  We assert it blindly at first, then
		 * take the presence/absence of a key into acct.
		 *
		 * Lookup the ExemptionActionType in the send context info of this frame
		 * to determine if we need to encrypt the frame.
		 */
		switch (wbuf_get_exemption_type(wbuf)) {
		case WBUF_EXEMPT_NO_EXEMPTION:
			/*
			 * We want to encrypt this frame.
			 */
			usecrypto = 1;
			break;

		case WBUF_EXEMPT_ALWAYS:
			/*
			 * We don't want to encrypt this frame.
			 */
			break;

		case WBUF_EXEMPT_ON_KEY_MAPPING_KEY_UNAVAILABLE:
			/*
			 * We encrypt this frame if and only if a key mapping key is set.
			 */
			if (key_mapping_key) {
				usecrypto = 1;
			}
			break;

		default:
			ASSERT(0);
			usecrypto = 1;
			break;
		}

		/*
		 * If the frame is to be encrypted, but no key is not set, either reject the frame
		 * or clear the WEP bit.
		 */
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
		if (usecrypto && !key_set) {
#else
		if (usecrypto && !key) {
#endif
			/*
			 * If this is a unicast frame or if the BSSPrivacy is on, reject the frame.
			 * Otherwise, clear the WEP bit so we will not encrypt the frame. In other words,
			 * we'll send multicast frame in clear if multicast key hasn't been setup.
			 */
			if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
				goto bad;
			} else
				usecrypto = 0;	/* XXX: is this right??? */
		}

		if (usecrypto)
			wh->i_fc[1] |= IEEE80211_FC1_WEP;
		else
			wh->i_fc[1] &= ~IEEE80211_FC1_WEP;
	}
	/*
	 * XXX: If it's an EAPOL frame:
	 * Some 11n APs drop non-QoS frames after ADDBA sequence. For example,
	 * bug 31812: Connection failure with Buffalo AMPG144NH. To fix it,
	 * seq. number in the same tid space, as requested in ADDBA, need to be
	 * used for the EAPOL frames. Therefore, wb_eapol cannot be set.
	 *
	 * if (((struct llc *)&wh[1])->llc_snap.ether_type == htobe16(ETHERTYPE_PAE))
	 *    wbuf_set_eapol(wbuf);
	 */

	/*
	 * Figure out additional header length we want to append after the wireless header.
	 * - Add Qos Control field if necessary
	 *   XXX: EAPOL frames will be encapsulated as QoS frames as well.
	 * - Additional QoS control field for OWL WDS workaround
	 * - IV will be added in ieee80211_crypto_encap().
	 */
	addlen = 0;
	pad = 0;
	if (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_QOS) {
		useqos = 1;
		hdrsize += sizeof(struct ieee80211_qoscntl);

		/* For TxBF CV cache update add +HTC field */
#ifdef ATH_SUPPORT_TxBF
		if (!(wbuf_is_eapol(wbuf))
		    && (wlan_peer_get_bf_update_cv(peer))) {
			hdrsize += sizeof(struct ieee80211_htc);
		}
#endif

		/*
		 * XXX: we assume a QoS frame must come from ieee80211_encap_8023() function,
		 * meaning it's already padded. If OS sends a QoS frame (thus without padding),
		 * then it'll break.
		 */
		pad = roundup(hdrsize, sizeof(u_int32_t)) - hdrsize;
		/*if (ic->ic_flags & IEEE80211_F_DATAPAD) {
		   pad = roundup(hdrsize, sizeof(u_int32_t)) - hdrsize;
		   } */
	} else if (likely(type == IEEE80211_FC0_TYPE_DATA &&
			  ((wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_QOS)) ||
			   IEEE80211_PEER_USEAMPDU(peer)))) {
		useqos = 1;
		addlen += sizeof(struct ieee80211_qoscntl);
		/* For TxBF CV cache update add +HTC field */
#ifdef ATH_SUPPORT_TxBF
		if (!(wbuf_is_eapol(wbuf))
		    && (wlan_peer_get_bf_update_cv(peer))) {
			addlen += sizeof(struct ieee80211_htc);
		}
#endif
	}
#if ATH_WDS_SUPPORT_APONLY
	else if (wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_SEND_80211) &&
		 type == IEEE80211_FC0_TYPE_DATA && use4addr) {

		/*
		 * If a non-QoS 4-addr frame comes from ieee80211_encap_8023() function,
		 * then it should be padded. Only need padding non-QoS 4-addr frames
		 * if OS sends it with 802.11 header already but without padding.
		 */
		addlen = roundup((hdrsize), sizeof(u_int32_t)) - hdrsize;
	}
#endif

	if (likely(addlen)) {

		/*
		 * XXX: if we already have enough padding, then
		 * don't need to push in more bytes, otherwise,
		 * put in bytes after the original padding.
		 */
		if (addlen > pad)
			addlen =
			    roundup((hdrsize + addlen),
				    sizeof(u_int32_t)) - hdrsize - pad;
		else
			addlen = 0;

		if (likely(addlen)) {
			struct ieee80211_frame *wh0;

			wh0 = wh;
			wh = (struct ieee80211_frame *)wbuf_push(wbuf, addlen);
			if (unlikely(wh == NULL)) {
				goto bad;
			}
			memmove(wh, wh0, hdrsize);
		}
	}

	if (likely(useqos)) {
		u_int8_t *qos;
		int tid;

		ac = wbuf_get_priority(wbuf);
		tid = wbuf_get_tid(wbuf);
#if ATH_WDS_SUPPORT_APONLY
		if (!use4addr)
			qos = ((struct ieee80211_qosframe *)wh)->i_qos;
		else
			qos = ((struct ieee80211_qosframe_addr4 *)wh)->i_qos;
#else
		qos = ((struct ieee80211_qosframe *)wh)->i_qos;
#endif

		qos[0] = tid & IEEE80211_QOS_TID;
		if (wlan_pdev_get_wmep_noackPolicy(pdev, ac))
			qos[0] |= (1 << IEEE80211_QOS_ACKPOLICY_S);
#ifdef ATH_AMSDU
		if (wbuf_is_amsdu(wbuf)) {
			qos[0] |=
			    (1 << IEEE80211_QOS_AMSDU_S) & IEEE80211_QOS_AMSDU;
		}
#endif
		qos[1] = 0;
		wh->i_fc[0] |= IEEE80211_FC0_SUBTYPE_QOS;

		/* Fill in the sequence number from the TID sequence space. */
		*(u_int16_t *) & wh->i_seq[0] =
		    htole16((wlan_peer_get_txseqs(peer, tid)) <<
			    IEEE80211_SEQ_SEQ_SHIFT);
		wlan_peer_incr_txseq(peer, tid);

#ifdef ATH_SUPPORT_TxBF
		//IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,"==>%s:CV update\n",__func__);
		if (!(wbuf_is_eapol(wbuf))
		    && (wlan_peer_get_bf_update_cv(peer))) {
			wlan_request_cv_update(pdev, peer, wbuf, use4addr);
			/* clear flag */
			// ni->ni_bf_update_cv = 0;
		}
#endif

	} else {
		*(u_int16_t *) wh->i_seq =
		    htole16((wlan_peer_get_txseqs(peer, IEEE80211_NON_QOS_SEQ))
			    << IEEE80211_SEQ_SEQ_SHIFT);
		wlan_peer_incr_txseq(peer, IEEE80211_NON_QOS_SEQ);
	}

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
	if (!ieee80211_check_and_fragment
	    (vdev, wbuf, wh, usecrypto, key, hdrsize)) {
		goto bad;
	}
#endif

	IEEE80211_PEER_STAT(dp_peer, tx_data);
	IEEE80211_PEER_STAT_ADD(dp_peer, tx_bytes, datalen);

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
	wlan_ald_record_tx(vdev, wbuf, datalen);
#endif

	return wbuf;

bad:
	while (wbuf != NULL) {
		wbuf_t wbuf1 = wbuf_next(wbuf);
		IEEE80211_TX_COMPLETE_WITH_ERROR(wbuf);
		wbuf = wbuf1;
	}
	return NULL;
}

static inline wbuf_t
ieee80211_encap_8023_aponly(struct wlan_objmgr_peer *peer, wbuf_t wbuf)
{
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_peer *bsspeer = wlan_vdev_get_bsspeer(vdev);
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
	struct ieee80211_rsnparms *rsn = NULL;
#endif
	struct ether_header eh;
	struct ieee80211_frame *wh;
	struct llc *llc;
	int hdrsize, hdrspace, addqos, use4addr, isMulticast;
	int is_amsdu = wbuf_is_amsdu(wbuf);
#ifdef ATH_SUPPORT_TxBF
	int addhtc;
#endif

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
	rsn = wlan_vdev_get_rsn(vdev);
	if (!rsn) {
		qdf_print("%s:rsn is NULL ", __func__);
		goto bad;
	}
#endif

	/*
	 * Copy existing Ethernet header to a safe place.  The
	 * rest of the code assumes it's ok to strip it when
	 * reorganizing state for the final encapsulation.
	 */
	KASSERT(wbuf_get_pktlen(wbuf) >= sizeof(eh), ("no ethernet header!"));
	OS_MEMCPY(&eh, wbuf_header(wbuf), sizeof(struct ether_header));
	addqos = (IEEE80211_PEER_USEAMPDU(peer)
		  || (wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_QOS)));

#ifdef ATH_SUPPORT_TxBF
	addhtc = (wlan_peer_get_bf_update_cv(peer) == 1);

	if (addhtc && !wbuf_is_eapol(wbuf)) {
		hdrsize = sizeof(struct ieee80211_qosframe_htc);
	} else if (likely(addqos))
#else
	if (likely(addqos))
#endif
		hdrsize = sizeof(struct ieee80211_qosframe);
	else
		hdrsize = sizeof(struct ieee80211_frame);

	isMulticast = (IEEE80211_IS_MULTICAST(eh.ether_dhost)) ? 1 : 0;
	use4addr = 0;
#if ATH_WDS_SUPPORT_APONLY
	if (wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_WDS)) {
		if (isMulticast == 0) {
			use4addr = wlan_wds_is4addr(vdev, eh, peer);
		}
		if (dp_peer->nawds)
			use4addr = 1;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
		if (isMulticast &&
		    (peer != wlan_vdev_get_bsspeer(vdev)) &&
		    (wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_WDS)))
			use4addr = 1;
#endif
		hdrsize = hdrsize + (use4addr ? IEEE80211_ADDR_LEN : 0);
	}
#endif
	hdrspace = roundup(hdrsize, sizeof(u_int32_t));

	if (likely(!is_amsdu && htons(eh.ether_type) >= IEEE8023_MAX_LEN)) {

		/*
		 * push the data by
		 * required total bytes for 802.11 header (802.11 header + llc - ether header).
		 */
		if (wbuf_push(wbuf, (u_int16_t) (hdrspace
						 + sizeof(struct llc) -
						 sizeof(struct ether_header)))
		    == NULL) {
			QDF_TRACE(
				       QDF_MODULE_ID_OUTPUT,
				       QDF_TRACE_LEVEL_DEBUG,
				       "%s:  %s::wbuf_push failed \n", __func__,
				       ether_sprintf(eh.ether_dhost));
			goto bad;
		}

		wh = (struct ieee80211_frame *)wbuf_header(wbuf);
		llc = (struct llc *)((u_int8_t *) wh + hdrspace);
		llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
		llc->llc_control = LLC_UI;
		llc->llc_snap.org_code[0] = RFC1042_SNAP_ORGCODE_0;	/* 0x0 */
		llc->llc_snap.org_code[1] = RFC1042_SNAP_ORGCODE_1;	/* 0x0 */
		llc->llc_snap.org_code[2] = RFC1042_SNAP_ORGCODE_2;	/* 0x0 */
		llc->llc_snap.ether_type = eh.ether_type;
	} else {
		/*
		 * push the data by
		 * required total bytes for 802.11 header (802.11 header - ether header).
		 */
		if (wbuf_push
		    (wbuf,
		     (u_int16_t) (hdrspace - sizeof(struct ether_header))) ==
		    NULL) {
			QDF_TRACE(
				       QDF_MODULE_ID_OUTPUT,
				       QDF_TRACE_LEVEL_DEBUG,
				       "%s:  %s::wbuf_push failed \n", __func__,
				       ether_sprintf(eh.ether_dhost));
			goto bad;
		}
		wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	}

	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA;
	*(u_int16_t *) wh->i_dur = 0;
    /** WDS FIXME */
#if ATH_WDS_SUPPORT_APONLY
	if (wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_WDS)) {
		struct wlan_objmgr_peer *peer_wds = NULL;
#define IEEE80211_WDS_NODE_AGE_THRESHOLD  2000
		peer_wds = wlan_find_wds_peer(vdev, eh.ether_shost);
		/* Last call increments ref count if !NULL */
		if (peer_wds != NULL) {
			/* if 4 address source pkt reachable through same node as dest
			 * then remove the source addr from wds table
			 */
			if (use4addr && (peer_wds == peer)) {
				wlan_remove_wds_addr(vdev, eh.ether_shost,
						     IEEE80211_NODE_F_WDS_REMOTE);
			} else if (isMulticast) {
				u_int32_t wds_age;
				wds_age =
				    wlan_find_wds_peer_age(vdev,
							   eh.ether_shost);
				if (wds_age > IEEE80211_WDS_NODE_AGE_THRESHOLD) {
					wlan_remove_wds_addr(vdev,
							     eh.ether_shost,
							     IEEE80211_NODE_F_WDS_REMOTE);
				}
			}
			wlan_objmgr_peer_release_ref(peer_wds, WLAN_MLME_SB_ID);	/* Decr ref count */
		}
	}
	if (use4addr) {
		wh->i_fc[1] = IEEE80211_FC1_DIR_DSTODS;
		IEEE80211_ADDR_COPY(wh->i_addr1, wlan_peer_get_macaddr(peer));
		IEEE80211_ADDR_COPY(wh->i_addr2,
				    wlan_vdev_mlme_get_macaddr(vdev));
		IEEE80211_ADDR_COPY(wh->i_addr3, eh.ether_dhost);
		IEEE80211_ADDR_COPY(IEEE80211_WH4(wh)->i_addr4, eh.ether_shost);
	} else
#endif
	{
		wh->i_fc[1] = IEEE80211_FC1_DIR_FROMDS;
		IEEE80211_ADDR_COPY(wh->i_addr1, eh.ether_dhost);
		IEEE80211_ADDR_COPY(wh->i_addr2, wlan_peer_get_bssid(bsspeer));
		IEEE80211_ADDR_COPY(wh->i_addr3, eh.ether_shost);
		if (wbuf_is_moredata(wbuf)) {
			wh->i_fc[1] |= IEEE80211_FC1_MORE_DATA;
		}
	}
	if (likely(addqos)) {
		/*
		 * Just mark the frame as QoS, and QoS control filed will be filled
		 * in ieee80211_encap_80211().
		 */
		wh->i_fc[0] |= IEEE80211_FC0_SUBTYPE_QOS;
	}

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	/*
	 * Set per-packet exemption type
	 */
	if (eh.ether_type == htons(ETHERTYPE_PAE)) {
		/*
		 * IEEE 802.1X: send EAPOL frames always in the clear.
		 * WPA/WPA2: encrypt EAPOL keys when pairwise keys are set.
		 */
		if (wlan_crypto_vdev_has_auth_mode(vdev,
						   (1 << WLAN_CRYPTO_AUTH_RSNA))
		    || wlan_crypto_vdev_has_auth_mode(vdev,
						      (1 << WLAN_CRYPTO_AUTH_WPA))) {
			wbuf_set_exemption_type(wbuf,
						WBUF_EXEMPT_ON_KEY_MAPPING_KEY_UNAVAILABLE);
		} else {
			wbuf_set_exemption_type(wbuf, WBUF_EXEMPT_ALWAYS);
		}
	}
#else
	/*
	 * Set per-packet exemption type
	 */
	if (unlikely(eh.ether_type == htons(ETHERTYPE_PAE))) {
		/*
		 * IEEE 802.1X: send EAPOL frames always in the clear.
		 * WPA/WPA2: encrypt EAPOL keys when pairwise keys are set.
		 */
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
            if (wlan_crypto_vdev_has_auth_mode(vdev, ((1 << WLAN_CRYPTO_AUTH_WPA) | (1 << WLAN_CRYPTO_AUTH_RSNA)))) {
#else
		if (RSN_AUTH_IS_WPA(rsn) || RSN_AUTH_IS_WPA2(rsn)) {
#endif
			wbuf_set_exemption_type(wbuf,
						WBUF_EXEMPT_ON_KEY_MAPPING_KEY_UNAVAILABLE);
		} else {
			wbuf_set_exemption_type(wbuf, WBUF_EXEMPT_ALWAYS);
		}
	}
#endif
#if ATH_SUPPORT_WAPI
	else if (eh.ether_type == htons(ETHERTYPE_WAI)) {
		wbuf_set_exemption_type(wbuf, WBUF_EXEMPT_ALWAYS);
	}
#endif

	else {
		wbuf_set_exemption_type(wbuf, WBUF_EXEMPT_NO_EXEMPTION);
	}

	return ieee80211_encap_80211_aponly(peer, wbuf);

bad:
	/* complete the failed wbuf here */
	IEEE80211_TX_COMPLETE_WITH_ERROR(wbuf);
	return NULL;
}

static inline u_int32_t ath_txq_depth_aponly(ath_dev_t dev, int qnum)
{
	struct ath_softc *sc = ATH_DEV_TO_SC(dev);

	return sc->sc_txq[qnum].axq_depth;
#ifdef ATH_SWRETRY
	/* XXX TODO the num of frames present in SW Retry queue
	 * are not reported. No problems are forseen at this
	 * moment due to this. Need to revisit this if problem
	 * occurs
	 */
#endif
}

static inline u_int32_t ath_txq_aggr_depth_aponly(ath_dev_t dev, int qnum)
{
	struct ath_softc *sc = ATH_DEV_TO_SC(dev);

	return sc->sc_txq[qnum].axq_aggr_depth;
}

static inline void
ath_net80211_addba_status_aponly(struct wlan_objmgr_peer *peer, u_int8_t tidno,
				 u_int16_t * status)
{
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
	struct ath_softc_net80211 *scn = NULL;

	if (!dp_pdev) {
		qdf_print("%s:dp_pdev is null ", __func__);
		return;
	}

	if (!dp_peer) {
		qdf_print("%s:dp_peer is null ", __func__);
		return;
	}
	scn = dp_pdev->scn;

	*status = scn->sc_ops->addba_status(scn->sc_dev, dp_peer->an, tidno);
}

/*
 * The function to send a frame (i.e., hardstart). The wbuf should already be
 * associated with the actual frame, and have a valid node instance.
 */

static inline int ath_tx_send_aponly(wbuf_t wbuf)
{
	struct wlan_objmgr_peer *peer = NULL;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct dadp_pdev *dp_pdev = NULL;
	struct ath_softc_net80211 *scn = NULL;
	struct dadp_peer *dp_peer = NULL;
	wbuf_t next_wbuf;

	peer = wbuf_get_peer(wbuf);
	if (!peer) {
		qdf_print("%s:peer is NULL ", __func__);
		goto bad;
	}

	vdev = wlan_peer_get_vdev(peer);
	if (!vdev) {
		qdf_print("%s:vdev is NULL ", __func__);
		goto bad;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		qdf_print("%s:pdev is NULL ", __func__);
		goto bad;
	}

	dp_pdev = wlan_get_dp_pdev(pdev);
	if (!dp_pdev) {
		qdf_print("%s:dp_pdev is NULL ", __func__);
		goto bad;
	}

	scn = dp_pdev->scn;
	if (!scn) {
		qdf_print("%s:scn is NULL ", __func__);
		goto bad;
	}

	dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer) {
		qdf_print("%s:dp_peer is NULL ", __func__);
		goto bad;
	}
	//begin of umac-2
	//
#ifdef PROFILE_UMAC_2
	int i = 0;

	if (wbuf_is_encap_done(wbuf)) {
		if (!umac_root) {
			umac_root =
			    qdf_perf_init(0, "umac2_tx_path_ap_only",
					  EVENT_GROUP);

			perf_cntr[0] =
			    qdf_perf_init(umac_root, "cpu_cycles",
					  EVENT_CPU_CYCLES);
			perf_cntr[1] =
			    qdf_perf_init(umac_root, "dcache_miss",
					  EVENT_ICACHE_MISS);
			perf_cntr[2] =
			    qdf_perf_init(umac_root, "icache_miss",
					  EVENT_DCACHE_MISS);
		}

		for (i = 0; i < 3; i++)
			qdf_perf_start(perf_cntr[i]);
	}
#endif

#if defined(ATH_SUPPORT_DFS)

	/*
	 * EV 10538/79856
	 * If we detect radar on the current channel, stop sending data
	 * packets. There is a DFS requirment that the AP should stop
	 * sending data packet within 250 ms of radar detection
	 */

	if (wlan_vdev_is_radar_channel(vdev)) {
		goto bad;
	}
#endif

	/*
	 * XXX TODO: Fast frame here
	 */

	ath_uapsd_pwrsave_check_aponly(wbuf, peer);

#ifdef ATH_AMSDU

	if ((IEEE80211_PEER_USEAMPDU(peer) &&
	     (ath_get_amsdusupported_aponly(scn->sc_dev,
					    dp_peer->an, wbuf_get_tid(wbuf))))
#ifdef QCA_PARTNER_PLATFORM
	    && (!wbuf_is_encap_done(wbuf))
#endif
	    ) {
		wbuf = ath_amsdu_send_aponly(wbuf);
		if (wbuf == NULL)
			return 0;
	}
#endif

	/*
	 * Encapsulate the packet for transmission
	 */
#ifdef QCA_PARTNER_PLATFORM
	if (wbuf_is_encap_done(wbuf)) {
		struct ieee80211_frame *wh;
		wh = (struct ieee80211_frame *)wbuf_header(wbuf);
		if (wbuf_is_moredata(wbuf)) {
			wh->i_fc[1] |= IEEE80211_FC1_MORE_DATA;
		}
		wbuf = ieee80211_encap_80211_aponly(peer, wbuf);
	} else {
		wbuf = ieee80211_encap_8023_aponly(peer, wbuf);
	}
#else
	wbuf = ieee80211_encap_8023_aponly(peer, wbuf);
#endif
	if (unlikely(wbuf == NULL)) {
		goto bad;
	}

	/*
	 * If node is HT capable, then send out ADDBA if
	 * we haven't done so.
	 *
	 * XXX: send ADDBA here to avoid re-entrance of other
	 * tx functions.
	 */
	wlan_send_addba(peer, wbuf);

	/* send down each fragment */
	while (wbuf != NULL) {
		int nextfraglen = 0;
		int error = 0;
		ATH_DEFINE_TXCTL(txctl, wbuf);
		HTC_WBUF_TX_DELCARE next_wbuf = wbuf_next(wbuf);
		if (next_wbuf != NULL)
			nextfraglen = wbuf_get_pktlen(next_wbuf);

#ifdef ENCAP_OFFLOAD
		if (ath_tx_data_prepare(scn, wbuf, nextfraglen, txctl) != 0)
			goto bad;
#else
		if (ath_tx_prepare_aponly(scn, wbuf, nextfraglen, txctl) != 0)
			goto bad;
#endif
		/* send this frame to hardware */
		txctl->an = dp_peer->an;

#if ATH_DEBUG
		/* For testing purpose, set the RTS/CTS flag according to global setting */
		if (!txctl->ismcast) {
			if (ath_rtscts_enable == 2)
				txctl->flags |= HAL_TXDESC_RTSENA;
			else if (ath_rtscts_enable == 1)
				txctl->flags |= HAL_TXDESC_CTSENA;
		}
#endif

		HTC_WBUF_TX_DATA_PREPARE(ic, scn);

		if (likely(error == 0)) {
#ifdef PROFILE_UMAC_2
			if (wbuf_is_encap_done(wbuf)) {
				for (i = 0; i < 3; i++)
					qdf_perf_end(perf_cntr[i]);
			}
#endif

#if UMAC_PER_PACKET_DEBUG
			wbuf_set_rate(wbuf, dp_vdev->vdev_userrate);
			wbuf_set_retries(wbuf, dp_vdev->vdev_userretries);
			wbuf_set_txpower(wbuf, dp_vdev->vdev_usertxpower);
			wbuf_set_txchainmask(wbuf,
					     dp_vdev->vdev_usertxchainmask);
#endif

			if ((ath_tx_start_aponly(scn->sc_dev, wbuf, txctl)) !=
			    0)
				goto bad;
			else {
				HTC_WBUF_TX_DATA_COMPLETE_STATUS(ic);
			}
		}

		wbuf = next_wbuf;
	}

	return 0;

bad:
	/* drop rest of the un-sent fragments */
	while (wbuf != NULL) {
		next_wbuf = wbuf_next(wbuf);
		IEEE80211_TX_COMPLETE_WITH_ERROR(wbuf);

		wbuf = next_wbuf;
	}

	return -EIO;
}

DECLARE_N_EXPORT_PERF_CNTR(netdev_xmit);

int ath_netdev_hardstart_aponly(struct sk_buff *skb, struct net_device *dev)
{
	struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
	struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
	struct ieee80211_cb *cb;
	struct wlan_objmgr_peer *peer = NULL;
	struct dadp_peer *dp_peer = NULL;
	int error = 0;
	struct ether_header *eh = (struct ether_header *)skb->data;
	int ismulti = IEEE80211_IS_MULTICAST(eh->ether_dhost) ? 1 : 0;
	u_int16_t addba_status;
	u_int32_t txq_depth, txq_aggr_depth;
	u_int32_t txbuf_freecount;
	struct ath_txq *txq;
	int qnum;
	int buffer_limit;
	int early_drop = 1;	/* allow tx buffer calculation to drop the packet by default */
	/* make early_drop = 0 for important control plane packets like EAPOL and DHCP */

	START_PERF_CNTR(netdev_xmit, netdev_xmit);

	cb = (struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb);
	peer = wbuf_get_peer(skb);

	if (wbuf_is_highpriority(skb))
		early_drop = 0;

	if ((wbuf_is_dhcp(skb)) || (wbuf_is_eapol(skb))
	    || (wbuf_is_qosnull(skb)) || (wbuf_is_arp(skb))) {
		early_drop = 0;
	}
	/*
	 * NB: check for valid node in case kernel directly sends packets
	 * on wifiX interface (such as broadcast packets generated by ipv6)
	 */
	if (unlikely(peer == NULL)) {
		wbuf_free(skb);
		return 0;
	}

	dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer) {
		qdf_print("%s:dp_peer is NULL ", __func__);
		wbuf_free(skb);
		return 0;
	}
#ifdef ATH_SUPPORT_UAPSD
	/* Limit UAPSD node queue depth to 2*WME_UAPSD_NODE_MAXQDEPTH */
	if ((wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_UAPSD)) &&
	    (scn->sc_ops->uapsd_depth(dp_peer->an) >=
	     ATH_NODE_NET80211_UAPSD_MAXQDEPTH) && early_drop) {
		goto bad;
	}
#endif

	qnum = scn->sc_ac2q[skb->priority];
	txq = &sc->sc_txq[qnum];

	txq_depth =
	    ath_txq_depth_aponly(scn->sc_dev, scn->sc_ac2q[skb->priority]);
	txq_aggr_depth =
	    ath_txq_aggr_depth_aponly(scn->sc_dev, scn->sc_ac2q[skb->priority]);
	ath_net80211_addba_status_aponly(peer, cb->u_tid, &addba_status);

	/*
	 * This logic throttles legacy and unaggregated HT frames if they share the hardware
	 * queue with aggregates. This improves the transmit throughput performance to
	 * aggregation enabled nodes when they coexist with legacy nodes.
	 */
	/* Do not throttle EAPOL packets - this causes the REKEY packets
	 * to be dropped and station disconnects.
	 */
	DPRINTF((struct ath_softc *)scn->sc_dev, ATH_DEBUG_RESET,
		"skb->priority=%d cb->u_tid=%d addba_status=%d txq_aggr_depth=%d txq_depth=%d\n",
		skb->priority, cb->u_tid, addba_status, txq_aggr_depth,
		txq_depth);

	if ((addba_status != IEEE80211_STATUS_SUCCESS)
	    && (txq_aggr_depth > 0)
	    && early_drop) {

		if (txq_depth >= 25) {
			goto bad;
		}
	}

	/*
	 * Try to avoid running out of descriptors
	 */
	txbuf_freecount = scn->sc_ops->get_txbuf_free(scn->sc_dev);
	if (ismulti && early_drop) {
		buffer_limit = MULTICAST_DROP_THRESHOLD;
		if (txbuf_freecount <= buffer_limit) {	/* check for 10% txbuf availability */
			goto bad;
		}
	}
	/* Reserve 16 Tx buffers for EAPOL, DHCP, ARP and QOS NULL frames */
	if (early_drop && (txbuf_freecount <= txq->axq_minfree + 16)) {
		goto bad;
	}

#if !ATH_DATABUS_ERROR_RESET_LOCK_3PP
	error = ath_tx_send_aponly(skb);
#else
    ATH_INTERNAL_RESET_LOCK(sc);
    error = ath_tx_send_aponly(skb);
    ATH_INTERNAL_RESET_UNLOCK(sc);
#endif

	if (unlikely(error)) {
		DPRINTF((struct ath_softc *)scn->sc_dev, ATH_DEBUG_XMIT,
			"%s: Tx failed with error %d\n", __func__, error);
	}
	goto done;

bad:
	__11nstats(((struct ath_softc *)(scn->sc_dev)), tx_drops);
	sc->sc_stats.ast_tx_nobuf++;
	sc->sc_stats.ast_txq_packets[scn->sc_ac2q[skb->priority]]++;
	sc->sc_stats.ast_txq_nobuf[scn->sc_ac2q[skb->priority]]++;

	IEEE80211_TX_COMPLETE_WITH_ERROR(skb);
	DPRINTF((struct ath_softc *)scn->sc_dev, ATH_DEBUG_XMIT,
		"%s: Tx failed with error %d\n", __func__, error);
done:
	END_PERF_CNTR(netdev_xmit);
	return 0;
}

#ifdef QCA_PARTNER_PLATFORM
int
ieee80211_send_wbuf_internal_aponly(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf)
#else
static inline int
ieee80211_send_wbuf_internal_aponly(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf)
#endif
{
	int retval;
	struct dadp_vdev *dp_vdev = NULL;
#if ATH_TX_COMPACT && !ATH_SUPPORT_FLOWMAC_MODULE
	os_if_t osif;
#endif

	if (!vdev) {
		qdf_print("%s:vdev is NULL ", __func__);
		return -EIO;
	}

	dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		return -EIO;
	}
#if ATH_WDS_SUPPORT_APONLY
	if (wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_WDS)) {
		if (wlan_nawds_send_wbuf(vdev, wbuf) == -1) {
			/* NAWDS node packet but mode is off, drop packet */
			return 0;
		}
	}
#endif

	wlan_vdev_set_lastdata_timestamp(vdev);
	dp_vdev->vdev_txrxbytes += wbuf_get_pktlen(wbuf);
	/*
	 * call back to shim layer to queue it to hardware device.
	 */
#if ATH_TX_COMPACT && !ATH_SUPPORT_FLOWMAC_MODULE
	osif = wlan_vdev_get_osifp(vdev);
	retval =
	    ath_netdev_hardstart_aponly(wbuf, ((osif_dev *) osif)->os_comdev);
#else
	retval =
	    dp_vdev->vdev_evtable->
	    wlan_dev_xmit_queue(wlan_vdev_get_osifp(vdev), wbuf);
#endif // ATH_TX_COMPACT

	return retval;
}

static INLINE int
ieee80211_dscp_override(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf)
{
	int tid = -1;
#if ATH_SUPPORT_DSCP_OVERRIDE
	u_int32_t is_igmp;
	u_int8_t tos;
	struct ip_header *iph = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct dadp_vdev *dp_vdev = NULL;
	struct dadp_pdev *dp_pdev = NULL;

	if (!vdev) {
		qdf_print("%s:vdev is NULL ", __func__);
		return -1;
	}

	dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		return -1;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		qdf_print("%s:pdev is NULL ", __func__);
		return -1;
	}

	dp_pdev = wlan_get_dp_pdev(pdev);
	if (!dp_pdev) {
		qdf_print("%s:dp_pdev is NULL ", __func__);
		return -1;
	}

	if (!dp_vdev->vdev_dscp_map_id &&
	    !dp_pdev->pdev_override_dscp &&
	    !dp_pdev->pdev_override_igmp_dscp &&
	    !dp_pdev->pdev_override_hmmc_dscp)
		return -1;
	tos = wbuf_get_iptos(wbuf, &is_igmp, (void **)&iph);
	if (dp_vdev->vdev_dscp_map_id) {
		tid =
		    dp_pdev->pdev_dscp_tid_map[dp_vdev->
					       vdev_dscp_map_id][tos >>
								 IP_DSCP_SHIFT]
		    & 0x7;
	} else if (is_igmp && dp_pdev->pdev_override_igmp_dscp) {
		tid = dp_pdev->pdev_dscp_igmp_tid;
	} else if (iph && dp_pdev->pdev_override_hmmc_dscp &&
		   dp_vdev->ique_ops.wlan_me_hmmc_find &&
		   dp_vdev->ique_ops.wlan_me_hmmc_find(vdev,
						       be32toh(iph->daddr))) {
		tid = dp_pdev->pdev_dscp_hmmc_tid;
	} else if (dp_pdev->pdev_override_dscp) {
		if (wbuf_mark_eapol(wbuf))
			tid = OSDEP_EAPOL_TID;
		else
			tid =
			    dp_pdev->pdev_dscp_tid_map[dp_vdev->
						       vdev_dscp_map_id][tos >>
									 IP_DSCP_SHIFT]
			    & 0x7;
	}
#endif
	return tid;
}

static inline int
ieee80211_classify_aponly(struct wlan_objmgr_peer *peer, wbuf_t wbuf)
{
	int ac = WME_AC_BE;
	int tid;
#if ATH_SUPPORT_VLAN
	int v_wme_ac = 0;
	int v_pri = 0;
#endif
	struct wlan_objmgr_pdev *pdev = NULL;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct dadp_pdev *dp_pdev = NULL;
	struct dadp_vdev *dp_vdev = NULL;
#ifdef __ubicom32__
	unsigned int priority = wbuf_get_priority(wbuf);
	/*
	 * Check Ubicom WISH classifier first (marker = 0xffef subject to change)
	 */
	if ((priority >> 16) == 0xffef) {
		static const int WISH_TO_WME_AC[] = {
			WME_AC_BE, WME_AC_BK, WME_AC_BK, WME_AC_BK,
			WME_AC_BE, WME_AC_BK, WME_AC_VO, WME_AC_VI,
			WME_AC_BE, WME_AC_BE, WME_AC_BE, WME_AC_BE,
			WME_AC_BE, WME_AC_BE, WME_AC_BE, WME_AC_BE,
		};
		priority = WISH_TO_WME_AC[priority & 0xf];
		wbuf_set_priority(wbuf, priority);
		wbuf_set_tid(wbuf, WME_AC_TO_TID(priority));
		return 0;
	}
#endif

	if (!peer) {
		qdf_print("%s:peer is NULL ", __func__);
		return 1;
	}

	vdev = wlan_peer_get_vdev(peer);
	if (!vdev) {
		qdf_print("%s:vdev is NULL ", __func__);
		return 1;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		qdf_print("%s:pdev is NULL ", __func__);
		return 1;
	}

	dp_pdev = wlan_get_dp_pdev(pdev);
	if (!dp_pdev) {
		qdf_print("%s:dp_pdev is NULL ", __func__);
		return 1;
	}

	dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		return 1;
	}

	/*
	 * Call wbuf_classify(wbuf) function before the
	 * "(ni->ni_flags & IEEE80211_NODE_QOS)" check. The reason is that
	 * wbuf_classify() is overloaded with setting EAPOL flag in addition to
	 * returning TOS for Maverick and Linux platform, where as for Windows it
	 * just returns TOS.
	 */
#if ATH_SUPPORT_HS20
	if (unlikely(dp_vdev->iv_qos_map.valid)) {
		struct ieee80211_qos_map *qos_map = &dp_vdev->iv_qos_map;
		struct ether_header *eh =
		    (struct ether_header *)wbuf_header(wbuf);
		int i;
		u_int8_t dscp = 0;
		if (eh->ether_type == __constant_htons(ETHERTYPE_IP)) {
			const struct iphdr *ip = (struct iphdr *)
			    ((u_int8_t *) eh + sizeof(struct ether_header));
			dscp = (ip->tos & (~INET_ECN_MASK)) >> 2;
		} else if (eh->ether_type == __constant_htons(ETHERTYPE_IPV6)) {
			unsigned long ver_pri_flowlabel =
			    *(unsigned long *)(eh + 1);
			unsigned long pri;

			pri =
			    (ntohl(ver_pri_flowlabel) & IPV6_PRIORITY_MASK) >>
			    IPV6_PRIORITY_SHIFT;
			dscp = (pri & (~INET_ECN_MASK)) >> 2;
		} else if (eh->ether_type == __constant_htons(ETHERTYPE_PAE)) {
			/* for EAPOL frames, override the tid value */
			N_EAPOL_SET(wbuf);
			tid = 6;	/* send it on VO queue */
			goto found;
		}

		/* search the DSCP exceptions */
		for (i = 0; i < qos_map->num_dscp_except; i++) {
			if (qos_map->dscp_exception[i].dscp == dscp) {
				tid = qos_map->dscp_exception[i].up;
				goto found;
			}
		}

		/* search the UP range */
		for (i = 0; i < IEEE80211_MAX_QOS_UP_RANGE; i++) {
			if (qos_map->up[i].low <= dscp &&
			    qos_map->up[i].high >= dscp) {
				tid = i;
				goto found;
			}
		}
		/* the DSCP was not found on the QoS Map. Fallback mechanism. */
		tid =
		    wbuf_classify(wbuf,
				  dp_pdev->
				  pdev_tid_override_queue_mapping) & 0x7;
	} else {
		if (dp_vdev->vdev_dscp_map_id)
			tid = ieee80211_dscp_override(vdev, wbuf);
		else
			tid =
			    wbuf_classify(wbuf,
					  dp_pdev->
					  pdev_tid_override_queue_mapping) &
			    0x7;
	}
found:
	ac = TID_TO_WME_AC(tid);
#else

	if ((tid = ieee80211_dscp_override(vdev, wbuf)) < 0)
		tid =
		    wbuf_classify(wbuf,
				  dp_pdev->
				  pdev_tid_override_queue_mapping) & 0x7;
	ac = TID_TO_WME_AC(tid);

#endif /* ATH_SUPPORT_HS20 */

	/* default priority */
	if (!(wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_QOS))) {
		wbuf_set_priority(wbuf, WME_AC_BE);
		wbuf_set_tid(wbuf, 0);
		return 0;
	}
#if ATH_SUPPORT_VLAN
	/*
	 ** If this is a QoS node (set after the above comparison, and there is a
	 ** VLAN tag associated with the packet, we need to ensure we set the
	 ** priority correctly for the VLAN
	 */

	if (unlikely(qdf_net_vlan_tag_present(wbuf))) {
		unsigned short tag;
		unsigned short vlanID;
		osdev_t osifp = (osdev_t) wlan_vdev_get_osifp(vdev);

		vlanID = qdf_net_get_vlan(osifp);
#ifdef QCA_PARTNER_PLATFORM
		if (ath_pltfrm_vlan_tag_check(vap, wbuf))
			return 1;
#endif
		if (!qdf_net_is_vlan_defined(osifp))
			return 1;

		if (((tag =
		      qdf_net_get_vlan_tag(wbuf)) & VLAN_VID_MASK) !=
		    (vlanID & VLAN_VID_MASK))
			return 1;

		v_pri = (tag >> VLAN_PRI_SHIFT) & VLAN_PRI_MASK;
	} else {
		/*
		 * If not a member of a VLAN, check if VLAN type and TCI are present in packet.
		 * If so, obtain VLAN priority from TCI.
		 * Use for determining 802.1p priority.
		 */
		v_pri = wbuf_8021p(wbuf);

	}

	/*
	 ** Determine the VLAN AC
	 */

	v_wme_ac = TID_TO_WME_AC(v_pri);

	/* Choose higher priority of implicit VLAN tag or IP DSCP */
	/* TODO: check this behaviour */
	if (v_wme_ac > ac) {
		tid = v_pri;
		ac = v_wme_ac;
	}
#endif

	wlan_admctl_classify(vdev, peer, &tid, &ac);
	wbuf_set_priority(wbuf, ac);
	wbuf_set_tid(wbuf, tid);

	return 0;
}

/*
 * Return a reference to the appropriate node for sending
 * a data frame.  This handles node discovery in adhoc networks.
 */
struct wlan_objmgr_peer *ieee80211_find_txnode_aponly(struct wlan_objmgr_vdev
						      *vdev,
						      const u_int8_t * macaddr)
{
	struct wlan_objmgr_peer *bsspeer = NULL, *peer = NULL;
	struct wlan_objmgr_psoc *psoc = wlan_vdev_get_psoc(vdev);
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	uint8_t pdev_id;

	if (!psoc) {
		qdf_print("%s:psoc is NULL ", __func__);
		return NULL;
	}

	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		return NULL;
	}

	bsspeer = wlan_vdev_get_bsspeer(vdev);
	pdev_id = wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(vdev));

	if (unlikely(IEEE80211_IS_MULTICAST(macaddr))) {
		if (likely(wlan_vdev_get_sta_assoc(vdev) > 0)) {
			wlan_objmgr_peer_try_get_ref(bsspeer, WLAN_MLME_SB_ID);
			peer = bsspeer;
		} else {
			/* No station associated to AP */
			dp_vdev->vdev_stats.is_tx_nonode++;
			peer = NULL;
		}
	} else {
		peer =
		    wlan_objmgr_get_peer(psoc, pdev_id, (uint8_t *)macaddr,
					 WLAN_MLME_SB_ID);
#if ATH_WDS_SUPPORT_APONLY
		if ((peer == NULL)
		    && (wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_WDS)))
			peer = wlan_find_wds_peer(vdev, macaddr);
#endif
	}
	return peer;
}

/*
 * the main xmit data entry point from OS
 */
static inline int
wlan_vap_send_aponly(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf)
{
	struct wlan_objmgr_peer *peer = NULL;
	u_int8_t *daddr;
	int is_data, retval;
	struct ether_header *eh;
	struct dadp_vdev *dp_vdev = NULL;
	struct dadp_peer *dp_peer = NULL;

	if (!vdev) {
		qdf_print("%s:vdev is NULL ", __func__);
		goto bad1;
	}

	dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		goto bad1;
	}

	/*
	 * Initialize Completion function to NULL
	 */
	wbuf_set_complete_handler(wbuf, NULL, NULL);

	/*
	 * Find the node for the destination so we can do
	 * things like power save and fast frames aggregation.
	 */
	eh = (struct ether_header *)wbuf_header(wbuf);
	daddr = eh->ether_dhost;
	is_data = 1;		/* ethernet frame */

#if ATH_SUPPORT_IQUE
	/*
	 * If IQUE is NOT enabled, the ops table is empty and
	 * the follow step will be skipped;
	 * If IQUE is enabled, and if the packet is a mcast one
	 * (and NOT a bcast one), the packet will be converted
	 * into ucast packets if the destination in found in the
	 * snoop table, in either Translate way or Tunneling way
	 * depending on the mode of mcast enhancement
	 */
	/*
	 * Allow snoop convert only on IPv4 multicast addresses. Because
	 * IPv6's ARP is multicast and carries top two byte as
	 * 33:33:xx:xx:xx:xx, snoop should let these frames pass-though than
	 * filtering through convert function.
	 */
	if ((IEEE80211_IS_IPV4_MULTICAST(eh->ether_dhost) ||
	     IEEE80211_IS_IPV6_MULTICAST(eh->ether_dhost)) &&
	    wlan_vdev_get_sta_assoc(vdev) > 0 &&
	    !IEEE80211_IS_BROADCAST(eh->ether_dhost)) {
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
		if (dp_vdev->me_hifi_enable
		    && dp_vdev->ique_ops.wlan_me_hifi_convert) {
			if (!dp_vdev->ique_ops.wlan_me_hifi_convert(vdev, wbuf))
				return 0;
		} else
#endif
		{
			/*
			 * if the convert function returns some value larger
			 * than 0, it means that one or more frames have been
			 * transmitted and we are safe to return from here.
			 */
#if ATH_SUPPORT_ME_SNOOP_TABLE
			if (dp_vdev->ique_ops.wlan_me_convert &&
                            dp_vdev->ique_ops.wlan_me_convert(vdev, wbuf) > 0) {
				return 0;
			}
#endif
		}
	}
#endif

	peer = ieee80211_find_txnode_aponly(vdev, daddr);
	if (unlikely(peer == NULL)) {
		QDF_TRACE( QDF_MODULE_ID_OUTPUT,
			       QDF_TRACE_LEVEL_DEBUG,
			       "%s: could not send packet, NI equal to NULL for %s\n",
			       __func__, ether_sprintf(daddr));
		/* NB: ieee80211_find_txnode does stat+msg */
		goto bad;
	}

	dp_peer = wlan_get_dp_peer(peer);

	if (!dp_peer)
		goto bad;

	/* calculate priority so driver can find the tx queue */
	if (unlikely(ieee80211_classify_aponly(peer, wbuf))) {
		QDF_TRACE( QDF_MODULE_ID_OUTPUT,
			       QDF_TRACE_LEVEL_DEBUG,
			       "%s: discard, classification failure (%s)\n",
			       __func__, ether_sprintf(daddr));
		goto bad;
	}

	if (peer != wlan_vdev_get_bsspeer(vdev)) {
		if (unlikely
		    (wlan_get_aid(peer) == 0
		     || (!(wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_AUTH))
			 && !wbuf_is_eapol(wbuf)
#ifdef ATH_SUPPORT_WAPI
			 && !wbuf_is_wai(wbuf)
#endif
		     ))) {
			/*
			 * Destination is not authenticated
			 */
			QDF_TRACE(
				       QDF_MODULE_ID_OUTPUT,
				       QDF_TRACE_LEVEL_DEBUG,
				       "%s: could not send packet, DA (%s) is not yet authorized\n",
				       __func__, ether_sprintf(daddr));
			goto bad;
		}
	}
#if ATH_SUPPORT_IQUE
	/*
	 *  Headline block removal: if the state machine is in
	 *  BLOCKING or PROBING state, transmision of UDP data frames
	 *  are blocked untill swtiches back to ACTIVE state.
	 */
	if (dp_vdev->ique_ops.wlan_hbr_dropblocked) {
		if (dp_vdev->ique_ops.wlan_hbr_dropblocked(vdev, peer, wbuf)) {
			QDF_TRACE( QDF_MODULE_ID_IQUE,
				       QDF_TRACE_LEVEL_DEBUG,
				       "%s: packet dropped coz it blocks the headline\n",
				       __func__);
			goto bad;
		}
	}
#endif

	wbuf_set_peer(wbuf, peer);	/* associate node with wbuf */

#if QCA_SUPPORT_SON
	if (wlan_son_is_vdev_enabled(vdev) &&
	    !IEEE80211_IS_MULTICAST(daddr) && !IEEE80211_IS_BROADCAST(daddr)) {
		wbuf_set_bsteering(wbuf);
	}
#endif

	/* power-save checks */
	if (unlikely
	    ((!UAPSD_AC_ISDELIVERYENABLED(wbuf_get_priority(wbuf), dp_peer))
	     && (wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_PAUSED))
	     && !wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_TEMP))) {
		/*
		 * Station in power save mode; pass the frame
		 * to the 802.11 layer and continue.  We'll get
		 * the frame back when the time is right.
		 * XXX lose WDS vap linkage?
		 */
		wlan_peer_pause(peer);	/* pause it to make sure that no one else unpaused it after the node_is_paused check above, pause operation is ref counted */
		QDF_TRACE( QDF_MODULE_ID_OUTPUT,
			       QDF_TRACE_LEVEL_DEBUG,
			       "%s: could not send packet, STA (%s) powersave %d paused %d\n",
			       __func__, ether_sprintf(daddr),
			       (wlan_peer_mlme_flag_get
				(peer, WLAN_PEER_F_PWR_MGT)) ? 1 : 0,
			       wlan_peer_mlme_flag_get(peer,
						       WLAN_PEER_F_PAUSED));
		wlan_peer_saveq_queue(peer, wbuf,
				      (is_data ? IEEE80211_FC0_TYPE_DATA :
				       IEEE80211_FC0_TYPE_MGT));
		wlan_peer_unpause(peer);	/* unpause it if we are the last one, the frame will be flushed out */
#if !LMAC_SUPPORT_POWERSAVE_QUEUE
		wlan_objmgr_peer_release_ref(peer, WLAN_MLME_SB_ID);
		return 0;
#endif
	}

	ieee80211_vap_pause_update_xmit_stats(dp_vdev, wbuf);	/* update the stats for vap pause module */
	retval = ieee80211_send_wbuf_internal_aponly(vdev, wbuf);

	return retval;
bad:
	if (IEEE80211_IS_MULTICAST(daddr)) {
		dp_vdev->vdev_multicast_stats.ims_tx_discard++;
	} else {
		dp_vdev->vdev_unicast_stats.ims_tx_discard++;

		if (dp_peer != NULL) {
			IEEE80211_PEER_STAT(dp_peer, tx_discard);
		}
	}

	if (peer != NULL)
		wlan_objmgr_peer_release_ref(peer, WLAN_MLME_SB_ID);

bad1:
	/* NB: callee's responsibilty to complete the packet */
	wbuf_set_status(wbuf, WB_STATUS_TX_ERROR);
	wbuf_complete(wbuf);

	return -EIO;
}

DECLARE_N_EXPORT_PERF_CNTR(vap_hardstart);

int osif_vap_hardstart_aponly(struct sk_buff *skb, struct net_device *dev)
{
	osif_dev *osdev = ath_netdev_priv(dev);
	struct wlan_objmgr_vdev *vdev = osdev->ctrl_vdev;
	struct net_device *comdev = osdev->os_comdev;
	struct dadp_vdev *dp_vdev = NULL;

	nbuf_debug_add_record(skb);
	if (!vdev) {
		qdf_print("%s:vdev is NULL ", __func__);
		goto bad;
	}

	dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		goto bad;
	}

	START_PERF_CNTR(vap_hardstart, vap_hardstart);

	spin_lock(&osdev->tx_lock);
	if (!osdev->is_up) {
		goto bad;
	}

	/* NB: parent must be up and running */
	if ((comdev->flags & (IFF_RUNNING | IFF_UP)) != (IFF_RUNNING | IFF_UP)
	    || (wlan_vdev_is_up(vdev) != QDF_STATUS_SUCCESS)
	    || (wlan_vdev_is_radar_channel(vdev))) {
		goto bad;
	}

	if (skb_headroom(skb) < dev->hard_header_len + dev->needed_headroom) {
		int delta = (dev->hard_header_len + dev->needed_headroom) - skb_headroom(skb);
		skb = qdf_nbuf_realloc_headroom(skb, SKB_DATA_ALIGN(delta));

		if (skb == NULL) {
			QDF_TRACE(
				       QDF_MODULE_ID_OUTPUT,
				       QDF_TRACE_LEVEL_DEBUG,
				       "%s: cannot expand skb\n", __func__);
			goto bad;
		}
	}
	N_FLAG_KEEP_ONLY(skb, N_PWR_SAV);
	wbuf_set_cboffset((wbuf_t) skb, 0);

#if UMAC_SUPPORT_WNM
	if (wlan_vdev_wnm_tfs_filter(vdev, (wbuf_t) skb)) {
		goto bad;
	}
#endif

#if UMAC_SUPPORT_PROXY_ARP
	if (wlan_do_proxy_arp(vdev, skb))
		goto bad;
#endif /* UMAC_SUPPORT_PROXY_ARP */

	wlan_vap_send_aponly(vdev, (wbuf_t) skb);

	spin_unlock(&osdev->tx_lock);

	goto done;

bad:
	spin_unlock(&osdev->tx_lock);
	if (skb != NULL)
		wbuf_free(skb);

done:
	END_PERF_CNTR(vap_hardstart);
	return 0;
}

#if UNIFIED_SMARTANTENNA
static uint32_t ath_smart_ant_txfeedback_aponly(struct ath_softc *sc,
						struct ath_node *an,
						struct ath_buf *bf, int nbad,
						struct ath_tx_status *ts)
{
	void *ds = bf->bf_desc;
	struct sa_tx_feedback tx_feedback;
	int i = 0;

	OS_MEMZERO(&tx_feedback, sizeof(struct sa_tx_feedback));
	if (sc->sc_ieee_ops->smart_ant_update_txfeedback) {
		tx_feedback.nPackets = bf->bf_nframes;
		tx_feedback.nBad = nbad;

		/* single bandwidh */
		for (i = 0; i < ts->ts_rateindex; i++) {
			tx_feedback.nlong_retries[i] = bf->bf_rcs[i].tries;
		}
		tx_feedback.nshort_retries[i] = ts->ts_shortretry;
		tx_feedback.nlong_retries[i] = ts->ts_longretry;

		ath_hal_get_smart_ant_tx_info(sc->sc_ah, ds,
					      &tx_feedback.rate_mcs[0],
					      &tx_feedback.tx_antenna[0]);

		tx_feedback.rssi[0] =
		    ((ts->ts_rssi_ctl0 | ts->
		      ts_rssi_ext0 << 8) | INVALID_RSSI_WORD);
		tx_feedback.rssi[1] =
		    ((ts->ts_rssi_ctl1 | ts->
		      ts_rssi_ext1 << 8) | INVALID_RSSI_WORD);
		tx_feedback.rssi[2] =
		    ((ts->ts_rssi_ctl2 | ts->
		      ts_rssi_ext2 << 8) | INVALID_RSSI_WORD);
		tx_feedback.rssi[3] = INVALID_RSSI_DWORD;

		tx_feedback.rate_index = ts->ts_rateindex;

		tx_feedback.is_trainpkt =
		    wbuf_is_smart_ant_train_packet(bf->bf_mpdu);
		sc->sc_ieee_ops->
		    smart_ant_update_txfeedback((struct ieee80211_node *)an->
						an_node, (void *)&tx_feedback);
	}

	if (tx_feedback.is_trainpkt) {
		wbuf_smart_ant_unset_train_packet(bf->bf_mpdu);
	}

	return 0;
}

static inline uint32_t ath_smart_ant_rxfeedback_aponly(struct ath_softc *sc,
						       wbuf_t wbuf,
						       struct ath_rx_status
						       *rxs, uint32_t pkts)
{
	struct sa_rx_feedback rx_feedback;
	if (sc->sc_ieee_ops->smart_ant_update_rxfeedback) {

		rx_feedback.rx_rate_mcs = rxs->rs_rate;
		rx_feedback.rx_antenna = rxs->rs_antenna;
		rx_feedback.rx_rssi[0] =
		    ((rxs->rs_rssi_ctl0 | rxs->
		      rs_rssi_ext0 << 8) | INVALID_RSSI_WORD);
		rx_feedback.rx_rssi[1] =
		    ((rxs->rs_rssi_ctl1 | rxs->
		      rs_rssi_ext1 << 8) | INVALID_RSSI_WORD);
		rx_feedback.rx_rssi[2] =
		    ((rxs->rs_rssi_ctl2 | rxs->
		      rs_rssi_ext2 << 8) | INVALID_RSSI_WORD);
		rx_feedback.rx_rssi[3] = INVALID_RSSI_DWORD;

		rx_feedback.rx_evm[0] = rxs->evm0;
		rx_feedback.rx_evm[1] = rxs->evm1;
		rx_feedback.rx_evm[2] = rxs->evm2;
		rx_feedback.rx_evm[3] = rxs->evm3;
		rx_feedback.rx_evm[4] = rxs->evm4;
		rx_feedback.npackets = pkts;
		sc->sc_ieee_ops->smart_ant_update_rxfeedback(sc->sc_ieee, wbuf,
							     (void *)
							     &rx_feedback);
	}

	return 0;
}
#endif /* UNIFIED_SMARTANTENNA */

#endif

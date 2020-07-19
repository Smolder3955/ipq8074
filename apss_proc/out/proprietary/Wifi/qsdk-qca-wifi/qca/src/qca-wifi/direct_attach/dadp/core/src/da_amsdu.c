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
/*
 * A-MSDU
 */
#include "if_athvar.h"
#include <da_amsdu.h>
#include "if_llc.h"
#include "if_upperproto.h"
#include "ath_timer.h"
#include "if_ath_htc.h"
#include <if_athproto.h>
#include <da_dp.h>
#ifdef ATH_AMSDU
/*
 * This routine picks an AMSDU buffer, calls the platform specific 802.11 layer for
 * WLAN encapsulation and then dispatches it to hardware for transmit.
 */
void
ath_amsdu_stageq_flush(struct wlan_objmgr_pdev *pdev,
		       struct ath_amsdu_tx *amsdutx)
{
	struct dadp_pdev *dp_pdev = NULL;
	struct dadp_peer *dp_peer = NULL;
	struct wlan_objmgr_peer *peer = NULL;
	struct ath_softc_net80211 *scn = NULL;
	wbuf_t wbuf;

	dp_pdev = wlan_get_dp_pdev(pdev);
	if (!dp_pdev) {
		qdf_print("%s:dp_pdev is NULL ", __func__);
		return;
	}
	scn = dp_pdev->scn;
	ATH_AMSDU_TXQ_LOCK(dp_pdev);
	wbuf = amsdutx->amsdu_tx_buf;
	if (!wbuf) {
		ATH_AMSDU_TXQ_UNLOCK(dp_pdev);
		return;
	}
	amsdutx->amsdu_tx_buf = NULL;
	ATH_AMSDU_TXQ_UNLOCK(dp_pdev);
	peer = wbuf_get_peer(wbuf);

	dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer) {
		qdf_print("%s:dp_peer is NULL ", __func__);
		return;
	}
	/*
	 * Encapsulate the packet for transmission
	 */
	wbuf = ieee80211_encap(peer, wbuf);
	if (wbuf == NULL) {
		printk("%s[%d] : ERROR: ieee80211_encap ret NULL\n", __func__,
		       __LINE__);
		return;
	}
	/* There is only one wbuf to send */
	if (wbuf != NULL) {
		int error = 0;

		ATH_DEFINE_TXCTL(txctl, wbuf);
		HTC_WBUF_TX_DELCARE txctl->iseap = 0;
		/* prepare this frame */
		if (ath_tx_prepare(scn->sc_pdev, wbuf, 0, txctl) != 0) {
			goto bad;
		}

		HTC_WBUF_TX_DATA_PREPARE(pdev, scn);

		if (error == 0) {
			/* send this frame to hardware */
			txctl->an = dp_peer->an;
			if (scn->sc_ops->tx(scn->sc_dev, wbuf, txctl) != 0) {
				goto bad;
			} else {
				HTC_WBUF_TX_DATA_COMPLETE_STATUS(ic);
			}
		}
	}
	return;
bad:
	/* drop rest of the un-sent fragments */
	if (wbuf != NULL) {
		IEEE80211_TX_COMPLETE_WITH_ERROR(wbuf);
	}
}

void ath_amsdu_tx_drain(struct wlan_objmgr_pdev *pdev)
{
	struct ath_amsdu_tx *amsdutx;
	struct dadp_pdev *dp_pdev = NULL;
	wbuf_t wbuf;

	dp_pdev = wlan_get_dp_pdev(pdev);
	if (!dp_pdev) {
		qdf_print("%s:dp_pdev is NULL ", __func__);
		return;
	}
	do {
		ATH_AMSDU_TXQ_LOCK(dp_pdev);
		amsdutx = TAILQ_FIRST(&dp_pdev->sc_amsdu_txq);
		if (amsdutx == NULL) {
			/* nothing to process */
			ATH_AMSDU_TXQ_UNLOCK(dp_pdev);
			break;
		}
		TAILQ_REMOVE(&dp_pdev->sc_amsdu_txq, amsdutx, amsdu_qelem);
		amsdutx->sched = 0;
		wbuf = amsdutx->amsdu_tx_buf;
		amsdutx->amsdu_tx_buf = NULL;
		ATH_AMSDU_TXQ_UNLOCK(dp_pdev);
		if (wbuf != NULL) {
			IEEE80211_TX_COMPLETE_WITH_ERROR(wbuf);
		}
	} while (!TAILQ_EMPTY(&dp_pdev->sc_amsdu_txq));
}

/*
 * This global timer routine checks a queue for posted AMSDU events.
 * For every AMSDU that we started, we add an event to this queue.
 * When the timer expires, it sends out all outstanding AMSDU bufffers.
 * We want to make sure that no buffer is sitting around for too long.
 */
int ath_amsdu_flush_timer(void *arg)
{
	struct wlan_objmgr_pdev *pdev = (struct wlan_objmgr_pdev *)arg;
	struct ath_amsdu_tx *amsdutx;
	struct dadp_pdev *dp_pdev = NULL;

	dp_pdev = wlan_get_dp_pdev(pdev);
	if (!dp_pdev) {
		qdf_print("%s:dp_pdev is NULL ", __func__);
		return -1;
	}

	do {
		ATH_AMSDU_TXQ_LOCK(dp_pdev);
		amsdutx = TAILQ_FIRST(&dp_pdev->sc_amsdu_txq);
		if (amsdutx == NULL) {
			/* nothing to process */
			ATH_AMSDU_TXQ_UNLOCK(dp_pdev);
			break;
		}
		TAILQ_REMOVE(&dp_pdev->sc_amsdu_txq, amsdutx, amsdu_qelem);
		amsdutx->sched = 0;
		ATH_AMSDU_TXQ_UNLOCK(dp_pdev);
		ath_amsdu_stageq_flush(pdev, amsdutx);
	} while (!TAILQ_EMPTY(&dp_pdev->sc_amsdu_txq));
	return 1;		/* dont re-schedule */
}

/*
 * If the receive phy rate is lower than the threshold or our transmit queue has at least one
 * frame to work on, we keep building the AMSDU.
 */
int
ath_amsdu_sched_check(struct ath_softc_net80211 *scn, void *an, int priority)
{
	if ((scn->sc_ops->get_noderate(an, IEEE80211_RATE_RX) <= 162000) ||
	    (scn->sc_ops->txq_depth(scn->sc_dev, scn->sc_ac2q[priority]) >= 1))
	{
		return 1;
	}
	return 0;
}

/*
 * This routine either starts a new AMSDU or it adds data to an existing one.
 * Each buffer is AMSDU encapsulated by our platform specific 802.11 protocol layer.
 * Once we have completed preparing the AMSDU, we call the flush routine to send it
 * out. We can stop filling an AMSDU for the following reasons -
 *    - the 2K AMSDU buffer is full
 *    - the hw transmit queue has only one frame to work on
 *    - we want to transmit a frame that cannot be in an AMSDU. i.e. frame larger than
 *      max subframe limit, EAPOL or multicast frame
 *    - the no activity timer expires and flushes any outstanding AMSDUs.
 */
wbuf_t ath_amsdu_send(wbuf_t wbuf)
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
	if (dp_peer->an_amsdu == NULL) {
		printk("ERROR: ath_amsdu_attach not called\n");
		return wbuf;
	}
	if (dp_vdev->vdev_fragthreshold < 2346 ||
	    !(wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE ||
	      wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE)) {
		return wbuf;
	}
	framelen = roundup(wbuf_get_pktlen(wbuf), 4);
	amsdu_deny = ieee80211_amsdu_check(vdev, wbuf);
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
		if (ath_amsdu_sched_check
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
			wlan_complete_wbuf(wbuf, &ts);
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

int ath_amsdu_attach(struct wlan_objmgr_pdev *pdev)
{
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);

	if (!dp_pdev) {
		qdf_print("%s:dp_pdev is NULL ", __func__);
		return -1;
	}

	TAILQ_INIT(&dp_pdev->sc_amsdu_txq);
	ATH_AMSDU_TXQ_LOCK_INIT(dp_pdev);
	/* Setup the default  amsdu size. If a different value is desired, setup at init time via IEEE80211_DEVICE_MAX_AMSDU_SIZE  */
	if (dp_pdev->pdev_amsdu_max_size == 0) {
		// If not already inited, set the default.
		dp_pdev->pdev_amsdu_max_size = AMSDU_MAX_LEN;
	}

	/* Initialize the no-activity timer */
	ath_initialize_timer(dp_pdev->osdev, &dp_pdev->sc_amsdu_flush_timer,
			     AMSDU_TIMEOUT, ath_amsdu_flush_timer, pdev);
	return 0;
}

void ath_amsdu_detach(struct wlan_objmgr_pdev *pdev)
{
	struct dadp_pdev *dp_pdev = NULL;

	dp_pdev = wlan_get_dp_pdev(pdev);
	if (!dp_pdev) {
		qdf_print("%s:dp_pdev is NULL ", __func__);
		return;
	}
	ATH_AMSDU_TXQ_LOCK_DESTROY(dp_pdev);
	ath_cancel_timer(&dp_pdev->sc_amsdu_flush_timer, CANCEL_NO_SLEEP);
	ath_free_timer(&dp_pdev->sc_amsdu_flush_timer);
}

int
ath_amsdu_node_attach(struct wlan_objmgr_pdev *pdev,
		      struct wlan_objmgr_peer *peer)
{
	int tidno;
	struct ath_amsdu_tx *amsdutx;
	struct dadp_peer *dp_peer = NULL;
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);

	if (!dp_pdev) {
		qdf_print("%s:dp_pdev is NULL ", __func__);
		return -ENOMEM;
	}

	dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer) {
		qdf_print("%s:dp_peer is NULL ", __func__);
		return ENOMEM;
	}

	dp_peer->an_amsdu = (struct ath_amsdu *)OS_MALLOC(dp_pdev->osdev,
							  sizeof(struct
								 ath_amsdu),
							  GFP_ATOMIC);
	if (dp_peer->an_amsdu == NULL) {
		return ENOMEM;
	}
	OS_MEMZERO(dp_peer->an_amsdu, sizeof(struct ath_amsdu));
	for (tidno = 0; tidno < WME_NUM_TID; tidno++) {
		amsdutx = &(dp_peer->an_amsdu->amsdutx[tidno]);
	}
	return 0;
}

void
ath_amsdu_node_detach(struct wlan_objmgr_pdev *pdev,
		      struct wlan_objmgr_peer *peer)
{
	wbuf_t wbuf;
	int tidno;
	struct dadp_peer *dp_peer = NULL;
	struct dadp_pdev *dp_pdev = NULL;

	dp_pdev = wlan_get_dp_pdev(pdev);
	if (!dp_pdev) {
		qdf_print("%s:dp_pdev is NULL ", __func__);
		return;
	}

	dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer) {
		qdf_print("%s:dp_peer is NULL ", __func__);
		return;
	}

	/* Cleanup resources */
	if (dp_peer->an_amsdu) {
		struct ath_amsdu_tx *amsdutx = NULL;

		for (tidno = 0; tidno < WME_NUM_TID; tidno++) {
			amsdutx = &(dp_peer->an_amsdu->amsdutx[tidno]);
			/* if the tx queue is scheduled, then remove it from scheduled queue and clean up the buffers */
			if (amsdutx->sched) {
				ATH_AMSDU_TXQ_LOCK(dp_pdev);

				TAILQ_REMOVE(&dp_pdev->sc_amsdu_txq, amsdutx,
					     amsdu_qelem);
				amsdutx->sched = 0;

				wbuf = amsdutx->amsdu_tx_buf;
				amsdutx->amsdu_tx_buf = NULL;
				ATH_AMSDU_TXQ_UNLOCK(dp_pdev);

				if (wbuf != NULL) {
					IEEE80211_TX_COMPLETE_WITH_ERROR(wbuf);
				}

			}
		}
		OS_FREE(dp_peer->an_amsdu);
		dp_peer->an_amsdu = NULL;
	}
}
#endif /* ATH_AMSDU */

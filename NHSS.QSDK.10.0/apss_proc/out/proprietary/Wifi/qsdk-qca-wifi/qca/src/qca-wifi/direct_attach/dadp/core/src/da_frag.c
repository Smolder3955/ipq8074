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
#include <da_dp.h>
#include <da_frag.h>

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
int wlan_crypto_enmic(struct wlan_objmgr_vdev *vdev,
		      struct ieee80211_key *key, wbuf_t wbuf, int force,
		      bool encap);
#endif
struct ieee80211_node_table *wlan_peer_get_nodetable(struct wlan_objmgr_peer
						     *peer);
#if UMAC_SUPPORT_TX_FRAG

/*
 * Fragment the frame according to the specified mtu.
 * The size of the 802.11 header (w/o padding) is provided
 * so we don't need to recalculate it.  We create a new
 * mbuf for each fragment and chain it through m_nextpkt;
 * we might be able to optimize this by reusing the original
 * packet's mbufs but that significantly more complicated.
 */
int
ieee80211_fragment(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf0,
		   u_int hdrsize, u_int ciphdrsize, u_int mtu)
{
	struct ieee80211_frame *wh, *whf;
	wbuf_t wbuf, prev;
	u_int totalhdrsize, fragno, fragsize, off, remainder = 0, payload;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct dadp_pdev *dp_pdev = NULL;

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

	if (wlan_pdev_nif_feat_cap_get(pdev, WLAN_PDEV_F_DATAPAD))
		hdrsize = roundup(hdrsize, sizeof(u_int32_t));

	wh = (struct ieee80211_frame *)wbuf_header(wbuf0);
	/* NB: mark the first frag; it will be propagated below */
	wh->i_fc[1] |= IEEE80211_FC1_MORE_FRAG;
	totalhdrsize = hdrsize + ciphdrsize;
	fragno = 1;
	off = mtu - ciphdrsize;
	remainder = wbuf_get_pktlen(wbuf0) - off;
	prev = wbuf0;

	QDF_TRACE( QDF_MODULE_ID_OUTPUT,
		       QDF_TRACE_LEVEL_DEBUG,
		       "%s: wbuf0 %pK, len %u nextpkt %pK\n", __func__, wbuf0,
		       wbuf_get_len(wbuf0), wbuf_next(wbuf0));

	QDF_TRACE( QDF_MODULE_ID_OUTPUT,
		       QDF_TRACE_LEVEL_DEBUG,
		       "%s: hdrsize %u, ciphdrsize %u mtu %u\n", __func__,
		       hdrsize, ciphdrsize, mtu);
	KASSERT(wbuf_next(wbuf0) == NULL, ("mbuf already chained?"));

	do {
		fragsize = totalhdrsize + remainder;
		QDF_TRACE( QDF_MODULE_ID_OUTPUT,
			       QDF_TRACE_LEVEL_DEBUG,
			       "%s: fragno %u off %u fragsize %u remainder %u\n",
			       __func__, fragno, off, fragsize, remainder);
		if (fragsize > mtu)
			fragsize = mtu;

		wbuf = wbuf_alloc(dp_pdev->osdev, WBUF_TX_DATA, fragsize);
		if (wbuf == NULL)
			goto bad;

#if LMAC_SUPPORT_POWERSAVE_QUEUE
		if (wbuf_is_legacy_ps(wbuf0))
			wbuf_set_legacy_ps(wbuf);
#endif

		wbuf_set_priority(wbuf, wbuf_get_priority(wbuf0));
		wbuf_set_tid(wbuf, wbuf_get_tid(wbuf0));
		if (wlan_objmgr_peer_try_get_ref(wbuf_get_peer(wbuf0), WLAN_MLME_SB_ID) != QDF_STATUS_SUCCESS) {
			wbuf_release(dp_pdev->osdev, wbuf);
			goto bad;
		}

		wbuf_set_peer(wbuf, wbuf_get_peer(wbuf0));
		/*
		 * reserve cipher size bytes to accommodate for the cipher size.
		 * wbuf_append followed by wbuf_pull is equivalent to skb_reserve.
		 */
		wbuf_append(wbuf, ciphdrsize);

		if (!wbuf_pull(wbuf, ciphdrsize))
			goto bad;

		/*
		 * Form the header in the fragment.  Note that since
		 * we mark the first fragment with the MORE_FRAG bit
		 * it automatically is propagated to each fragment; we
		 * need only clear it on the last fragment (done below).
		 */
		whf = (struct ieee80211_frame *)wbuf_header(wbuf);
		OS_MEMCPY(whf, wh, hdrsize);
		*(u_int16_t *) & whf->i_seq[0] |= htole16((fragno &
							   IEEE80211_SEQ_FRAG_MASK)
							  <<
							  IEEE80211_SEQ_FRAG_SHIFT);
		fragno++;

		payload = fragsize - totalhdrsize;
		wbuf_copydata(wbuf0, off, payload, (u_int8_t *) whf + hdrsize);
		wbuf_set_pktlen(wbuf, hdrsize + payload);

		/* chain up the fragment */
		wbuf_set_next(prev, wbuf);
		prev = wbuf;

		/* deduct fragment just formed */
		remainder -= payload;
		off += payload;
	} while (remainder != 0);

	whf->i_fc[1] &= ~IEEE80211_FC1_MORE_FRAG;
	/* strip first mbuf now that everything has been copied */
	wbuf_trim(wbuf0, wbuf_get_pktlen(wbuf0) - (mtu - ciphdrsize));
	return 1;
bad:
	printk("CHH(%s): Bad Frag Return, remainder = %d\n", __func__,
	       remainder);
	return 0;
}

/*
 * checkand if required fragment the frame.
 */
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
bool ieee80211_check_and_fragment(struct wlan_objmgr_vdev * vdev, wbuf_t wbuf,
				  struct ieee80211_frame * wh, int usecrypto,
				  struct wlan_crypto_key * key, u_int hdrsize)
#else
bool ieee80211_check_and_fragment(struct wlan_objmgr_vdev * vdev, wbuf_t wbuf,
				  struct ieee80211_frame * wh, int usecrypto,
				  struct ieee80211_key * key, u_int hdrsize)
#endif
{
	int txfrag;
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	/*TBD_CRYPTO */
#else
	bool encap = false;
#endif
	struct dadp_vdev *dp_vdev = NULL;

	dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		return false;
	}

	/*
	 * check if xmit fragmentation is required
	 */
	txfrag = ((wbuf_get_pktlen(wbuf) > dp_vdev->vdev_fragthreshold) && !IEEE80211_IS_MULTICAST(wh->i_addr1) && !wbuf_is_fastframe(wbuf));	/* NB: don't fragment ff's */

	if (txfrag) {

		/*
		 * When packet arrives here, it is the 802.11 type packet without tkip header.
		 * Hence, it should set encap=0 when perform enmic.
		 * In some customer platform, the packet is already 802.11 type + tkip header.
		 * In such a case, we set encap=1.
		 */
#ifdef __CARRIER_PLATFORM__
		if (wbuf_is_encap_done(wbuf)) {
			encap = true;
		}
#endif
		if (usecrypto) {
			/*
			 * For tx fragmentation, we have to do software enmic.
			 * NB: In fact, it's for TKIP only, but the function will always return
			 * success for non-TKIP cipher types.
			 */
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
#if 0				/*TBD_CRYPTO */
			if (WLAN_CRYPTO_RX_OPS_ENMIC(crypto_rx_ops)
			    (vdev, wbuf, wh->i_addr1,
			     encap) != QDF_STATUS_SUCCESS) {
			}
#endif
#else
			if (!wlan_crypto_enmic(vdev, key, wbuf, txfrag, encap)) {
				QDF_TRACE(
					       QDF_MODULE_ID_OUTPUT,
					       QDF_TRACE_LEVEL_DEBUG,
					       wh->i_addr1, "%s",
					       "enmic failed, discard frame");
				dp_vdev->vdev_stats.is_crypto_enmicfail++;
				return false;
			}
#endif
		}
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
		if (!ieee80211_fragment(vdev, wbuf, hdrsize,
					key !=
					NULL
					? (wlan_crypto_peer_get_header(key)) :
					0, dp_vdev->vdev_fragthreshold)) {
#else
		if (!ieee80211_fragment(vdev, wbuf, hdrsize,
					key !=
					NULL ? key->wk_cipher->ic_header : 0,
					dp_vdev->vdev_fragthreshold)) {
#endif
			QDF_TRACE(
				       QDF_MODULE_ID_OUTPUT,
				       QDF_TRACE_LEVEL_DEBUG, wh->i_addr1, "%s",
				       "fragmentation failed, discard frame");
			return false;
		}
	}

	return true;
}

#endif

wbuf_t ieee80211_defrag(struct wlan_objmgr_peer * peer, wbuf_t wbuf, int hdrlen)
{
	struct ieee80211_frame *wh, *lwh = NULL;
	u_int16_t rxseq, last_rxseq;
	u_int8_t fragno, last_fragno;
	u_int8_t more_frag;
	int tick_counter = 0;
	struct dadp_peer *dp_peer = NULL;

	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	rxseq =
	    le16_to_cpu(*(u_int16_t *) wh->i_seq) >> IEEE80211_SEQ_SEQ_SHIFT;
	fragno =
	    le16_to_cpu(*(u_int16_t *) wh->i_seq) & IEEE80211_SEQ_FRAG_MASK;
	more_frag = wh->i_fc[1] & IEEE80211_FC1_MORE_FRAG;

	dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer) {
		qdf_print("%s:dp_peer is NULL ", __func__);
		wbuf_free(wbuf);
		return NULL;
	}

	/* Quick way out, if there's nothing to defragment */
	if (!more_frag && fragno == 0 && dp_peer->peer_rxfrag[0] == NULL)
		return wbuf;

	/*
	 * Remove frag to insure it doesn't get reaped by timer.
	 */
	if (wlan_peer_get_nodetable(peer) == NULL) {
		/*
		 * Should never happen.  If the node is orphaned (not in
		 * the table) then input packets should not reach here.
		 * Otherwise, a concurrent request that yanks the table
		 * should be blocked by other interlocking and/or by first
		 * shutting the driver down.  Regardless, be defensive
		 * here and just bail
		 */
		/* XXX need msg+stat */
		wbuf_free(wbuf);
		return NULL;
	}

	/*
	 * Use this lock to make sure dp_peer->peer_rxfrag[0] is
	 * not freed by the timer process while we use it.
	 */

	while (OS_ATOMIC_CMPXCHG(&(dp_peer->peer_rxfrag_lock), 0, 1) == 1) {
		/* busy wait; can be executed at IRQL <= DISPATCH_LEVEL */
		if (tick_counter++ > 100) {	// no more than 1ms
			break;
		}
		OS_DELAY(10);
	}
	dp_peer->peer_rxfragstamp = OS_GET_TIMESTAMP();

	/*
	 * Validate that fragment is in order and
	 * related to the previous ones.
	 */
	if (dp_peer->peer_rxfrag[0]) {
		lwh =
		    (struct ieee80211_frame *)wbuf_header(dp_peer->
							  peer_rxfrag[0]);
		last_rxseq =
		    le16_to_cpu(*(u_int16_t *) lwh->
				i_seq) >> IEEE80211_SEQ_SEQ_SHIFT;
		last_fragno =
		    le16_to_cpu(*(u_int16_t *) lwh->
				i_seq) & IEEE80211_SEQ_FRAG_MASK;
		if (rxseq != last_rxseq || fragno != last_fragno + 1
		    || (!IEEE80211_ADDR_EQ(wh->i_addr1, lwh->i_addr1))
		    || (!IEEE80211_ADDR_EQ(wh->i_addr2, lwh->i_addr2))) {
			/*
			 * Unrelated fragment or no space for it,
			 * clear current fragments
			 */
			wbuf_free(dp_peer->peer_rxfrag[0]);
			dp_peer->peer_rxfrag[0] = NULL;
		}
	}

	/* If this is the first fragment */
	if (dp_peer->peer_rxfrag[0] == NULL) {
		if (fragno != 0) {	/* !first fragment, discard */
			/* XXX: msg + stats */
			wbuf_free(wbuf);
			(void)OS_ATOMIC_CMPXCHG(&(dp_peer->peer_rxfrag_lock), 1,
						0);
			return NULL;
		}
		dp_peer->peer_rxfrag[0] = wbuf;
	} else {
		ASSERT(fragno != 0);

		wbuf_pull(wbuf, hdrlen);	/* strip the header of fragments other than the first one */
		wbuf_concat(dp_peer->peer_rxfrag[0], wbuf);

		/* track last seqnum and fragno */
		*(u_int16_t *) lwh->i_seq = *(u_int16_t *) wh->i_seq;
	}

	if (more_frag) {
		/* More to come */
		wbuf = NULL;
	} else {
		/* Last fragment received, we're done! */
		wbuf = dp_peer->peer_rxfrag[0];
		dp_peer->peer_rxfrag[0] = NULL;

		/* clear fragno in the header because the frame we indicate
		 * could include wireless header (for Native-WiFi). */
		*((u_int16_t *) lwh->i_seq) &= ~IEEE80211_SEQ_FRAG_MASK;
	}

	(void)OS_ATOMIC_CMPXCHG(&(dp_peer->peer_rxfrag_lock), 1, 0);
	return wbuf;
}


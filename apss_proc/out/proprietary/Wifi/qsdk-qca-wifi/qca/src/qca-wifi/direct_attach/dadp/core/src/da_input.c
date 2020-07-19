/*
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
#include <da_input.h>
#include <da_frag.h>
#include <da_mat.h>
#include <wlan_utility.h>

#define IEEE80211_PEER_WDSWAR_ISSENDDELBA(_peer)    \
    ( 0 )

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

static int
ieee80211_qos_decap(struct wlan_objmgr_vdev *vdev,
		    wbuf_t wbuf, int hdrlen, struct ieee80211_rx_status *rs);

int
dadp_input(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
	   struct ieee80211_rx_status *rs);
static INLINE int dadp_input_ap(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
				struct ieee80211_frame *wh,
				struct wlan_vdev_mac_stats *mac_stats, int type,
				int subtype, int dir);
int dadp_input_sta(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
		   struct ieee80211_rx_status *rs);
int dadp_input_ibss(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
		    struct ieee80211_rx_status *rs);
int dadp_input_wds(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
		   struct ieee80211_rx_status *rs);

static INLINE int
dadp_input_data_ap(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
		   struct ieee80211_frame *wh, int dir);
int dadp_input_data_sta(struct wlan_objmgr_peer *peer, wbuf_t wbuf, int dir,
			int subtype);
int dadp_input_data_ibss(struct wlan_objmgr_peer *peer, wbuf_t wbuf, int dir);
int dadp_input_data_wds(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf, int dir);

static void
_ieee80211_input_update_data_stats(struct wlan_objmgr_peer *peer,
				   struct wlan_vdev_mac_stats *mac_stats,
				   wbuf_t wbuf,
				   struct ieee80211_rx_status *rs,
				   u_int16_t realhdrsize);
static void
dadp_input_data(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
		struct ieee80211_rx_status *rs, int subtype, int dir);
static ieee80211_privasy_filter_status ieee80211_check_privacy_filters(struct
								       wlan_objmgr_peer
								       *peer,
								       wbuf_t
								       wbuf,
								       int
								       is_mcast);
static void ieee80211_deliver_data(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf,
				   struct wlan_objmgr_peer *peer,
				   struct ieee80211_rx_status *rs,
				   u_int32_t hdrspace, int is_mcast,
				   u_int8_t subtype);
static wbuf_t ieee80211_decap(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf,
			      size_t hdrspace, struct ieee80211_rx_status *rs);

/*
 * check the frame against the registered  privacy flteres.
 * returns 1 if the frame needs to be filtered out.
 * returns 0 if the frame needs to be indicated up.
 */

static ieee80211_privasy_filter_status
ieee80211_check_privacy_filters(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
				int is_mcast)
{
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
	struct llc *llc;
	u_int16_t ether_type = 0;
	u_int32_t hdrspace;
	u_int32_t i;
	struct ieee80211_frame *wh;
	privacy_filter_packet_type packet_type;
	u_int8_t is_encrypted;

	if (!dp_peer) {
		qdf_print("%s:dp_peer is NULL ", __func__);
		return FILTER_STATUS_REJECT;
	}
	/* Safemode must avoid the PrivacyExemptionList and ExcludeUnencrypted checking */
	if (wlan_vdev_mlme_feat_ext_cap_get(vdev, WLAN_VDEV_FEXT_SAFEMODE)) {
		return FILTER_STATUS_ACCEPT;
	}

	wh = (struct ieee80211_frame *)wbuf_header(wbuf);

	if (wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_DELIVER_80211)) {
		/* QoS-Ctrl & Padding bytes have already been stripped off */
		hdrspace = ieee80211_hdrsize(wbuf_header(wbuf));

	} else {
		hdrspace = wlan_hdrspace(pdev, wbuf_header(wbuf));
	}
	if (wbuf_get_pktlen(wbuf) < (hdrspace + LLC_SNAPFRAMELEN)) {
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

	if (ether_type == ETHERTYPE_PAE) {
		QDF_TRACE( QDF_MODULE_ID_CRYPTO,
			       QDF_TRACE_LEVEL_DEBUG, "%s: RX EAPOL Frame \n",
			       __func__);
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
			if (!is_encrypted && !is_mcast && peer
			    && dadp_peer_get_key_valid(peer)) {
#else
			if (!is_encrypted && !is_mcast && dp_peer
			    && dp_peer->key && dp_peer->key->wk_valid) {
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
		if (dp_pdev->pdev_softap_enable)
			return FILTER_STATUS_ACCEPT;

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

int
ieee80211_amsdu_input(struct wlan_objmgr_peer *peer,
		      wbuf_t wbuf, struct ieee80211_rx_status *rs,
		      int is_mcast, u_int8_t subtype)
{
#define AMSDU_LLC_SIZE  (sizeof(struct ether_header) + sizeof(struct llc))
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
	struct wlan_pdev_phy_stats *phy_stats;
	struct wlan_vdev_mac_stats *mac_stats;
	struct ether_header *subfrm_hdr;
	u_int32_t subfrm_len, subfrm_datalen, frm_len = 0;
	u_int32_t hdrsize;
	wbuf_t wbuf_new, wbuf_subfrm, wbuf_save = wbuf;
	struct ieee80211_qosframe_addr4 *wh;
	u_int16_t orig_hdrsize = 0;
	u_int reserve = 64;	/* MIN_HEAD_ROOM as defined in ath_wbuf.c */

	if (!dp_peer) {
		qdf_print("%s:dp_peer is null ", __func__);
		goto err_amsdu;
	}
#if !ATH_SUPPORT_STATS_APONLY
	phy_stats = &dp_pdev->pdev_phy_stats[wlan_vdev_get_curmode(vdev)];
#endif
	mac_stats =
	    IEEE80211_IS_MULTICAST(wbuf_header(wbuf)) ? &dp_vdev->
	    vdev_multicast_stats : &dp_vdev->vdev_unicast_stats;

	UNREFERENCED_PARAMETER(phy_stats);

	hdrsize = wlan_hdrspace(pdev, wbuf_header(wbuf));
	/* point to ieee80211 header */
	wh = (struct ieee80211_qosframe_addr4 *)wbuf_header(wbuf);
	wbuf_pull(wbuf, hdrsize);

	frm_len = wbuf_get_pktlen(wbuf);
	while (wbuf_next(wbuf) != NULL) {
		wbuf = wbuf_next(wbuf);
		frm_len += wbuf_get_pktlen(wbuf);
	}

	wbuf = wbuf_save;

	/*
	 * each iteration of this loop creates one complete wlan frame from each
	 * of the AMSDU subframes.
	 */
	while (frm_len >= AMSDU_LLC_SIZE) {
#if USE_MULTIPLE_BUFFER_RCV
		u_int32_t from_offset, to_offset, copy_size;
#endif
		u_int32_t pkt_len;
		u_int8_t eth_llc[AMSDU_LLC_SIZE], eth_straddle = 0;
		if ((pkt_len = wbuf_get_pktlen(wbuf)) < AMSDU_LLC_SIZE) {	/* wbuf left too less */
			if ((wbuf_new = wbuf_next(wbuf)) == NULL) {
				QDF_TRACE(
					       QDF_MODULE_ID_ANY,
					       QDF_TRACE_LEVEL_DEBUG,
					       "[%s] Discard amsdu frame: A-MSDU: pullup failed, len %u",
					       ether_sprintf
					       (wlan_peer_get_macaddr(peer)),
					       frm_len);
				goto err_amsdu;
			}
			/* check new wbuf for data length */
			else if (wbuf_get_pktlen(wbuf_new) < AMSDU_LLC_SIZE) {
				goto err_amsdu;
			}
			if (pkt_len) {
				eth_straddle = 1;
				OS_MEMCPY(eth_llc, wbuf_header(wbuf), pkt_len);
				OS_MEMCPY(eth_llc + pkt_len,
					  wbuf_header(wbuf_new),
					  AMSDU_LLC_SIZE - pkt_len);

			}
			wbuf = wbuf_new;	/* update wbuf */
		}

		if (eth_straddle) {
			subfrm_hdr = (struct ether_header *)eth_llc;
			wbuf_pull(wbuf, AMSDU_LLC_SIZE - pkt_len);
		} else {
			subfrm_hdr = (struct ether_header *)wbuf_header(wbuf);
		}
		subfrm_datalen = ntohs(subfrm_hdr->ether_type);
		subfrm_len = subfrm_datalen + sizeof(struct ether_header);

		if (subfrm_len < sizeof(LLC_SNAPFRAMELEN)) {
			QDF_TRACE(
				       QDF_MODULE_ID_INPUT,
				       QDF_TRACE_LEVEL_DEBUG,
				       "[%s] Discard amsdu frame: A-MSDU sub-frame too short: len %u",
				       ether_sprintf(subfrm_hdr->ether_shost),
				       subfrm_len);
			goto err_amsdu;
		}

		if (frm_len != subfrm_len && frm_len < roundup(subfrm_len, 4)) {
			QDF_TRACE(
				       QDF_MODULE_ID_INPUT,
				       QDF_TRACE_LEVEL_DEBUG,
				       "[%s] Discard amsdu frame: Short A-MSDU: len %u",
				       ether_sprintf(subfrm_hdr->ether_shost),
				       frm_len);
			goto err_amsdu;
		}

		wbuf_subfrm = wbuf_alloc(dp_pdev->osdev,
					 WBUF_RX_INTERNAL,
					 subfrm_datalen + hdrsize + reserve);

		if (wbuf_subfrm == NULL) {
			QDF_TRACE(
				       QDF_MODULE_ID_INPUT,
				       QDF_TRACE_LEVEL_DEBUG,
				       "[%s] Discard amsdu frame: amsdu A-MSDU no memory",
				       ether_sprintf(subfrm_hdr->ether_shost));
			goto err_amsdu;
		}
		qdf_nbuf_reserve(wbuf_subfrm, reserve);

		wbuf_init(wbuf_subfrm, subfrm_datalen + hdrsize);

		switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
		case IEEE80211_FC1_DIR_NODS:
			IEEE80211_ADDR_COPY(wh->i_addr1,
					    subfrm_hdr->ether_dhost);
			IEEE80211_ADDR_COPY(wh->i_addr2,
					    subfrm_hdr->ether_shost);
			break;

		case IEEE80211_FC1_DIR_TODS:
			IEEE80211_ADDR_COPY(wh->i_addr2,
					    subfrm_hdr->ether_shost);
			IEEE80211_ADDR_COPY(wh->i_addr3,
					    subfrm_hdr->ether_dhost);
			break;

		case IEEE80211_FC1_DIR_FROMDS:
			IEEE80211_ADDR_COPY(wh->i_addr1,
					    subfrm_hdr->ether_dhost);
			IEEE80211_ADDR_COPY(wh->i_addr3,
					    subfrm_hdr->ether_shost);
			break;

		case IEEE80211_FC1_DIR_DSTODS:
			IEEE80211_ADDR_COPY(wh->i_addr3,
					    subfrm_hdr->ether_dhost);
			IEEE80211_ADDR_COPY(wh->i_addr4,
					    subfrm_hdr->ether_shost);
			break;

		default:
			wbuf_free(wbuf_subfrm);
			goto err_amsdu;
		}

		wbuf_set_priority(wbuf_subfrm, wbuf_get_priority(wbuf));
		if (wbuf_is_qosframe(wbuf)) {
			wbuf_set_qosframe(wbuf_subfrm);
		}

		/* copy ieee80211 header */
		OS_MEMCPY(wbuf_header(wbuf_subfrm), wh, hdrsize);
#if USE_MULTIPLE_BUFFER_RCV
		/*
		 * take care of the case where a received frame may straddle two or more buffers.
		 * copy from SNAP/LLC onwards.
		 */
		from_offset = sizeof(struct ether_header);
		to_offset = hdrsize;
		copy_size =
		    MIN(wbuf_get_pktlen(wbuf) - sizeof(struct ether_header),
			subfrm_datalen);
		if (eth_straddle) {
			OS_MEMCPY(wbuf_header(wbuf_subfrm) + to_offset,
				  eth_llc + from_offset, sizeof(struct llc));
			copy_size -= sizeof(struct llc);
			to_offset += sizeof(struct llc);
			from_offset = 0;
		}
		while (subfrm_datalen) {
			OS_MEMCPY(wbuf_header(wbuf_subfrm) + to_offset,
				  wbuf_header(wbuf) + from_offset, copy_size);
			subfrm_datalen -= copy_size;
			to_offset += copy_size;
			if (subfrm_datalen) {
				wbuf_pull(wbuf, copy_size);
				wbuf = wbuf_next(wbuf);
				from_offset = 0;
				frm_len -=
				    copy_size + sizeof(struct ether_header);
				subfrm_len -=
				    copy_size + sizeof(struct ether_header);
				if (!wbuf) {
					wbuf_free(wbuf_subfrm);
					goto err_amsdu;
				}
				copy_size =
				    MIN(wbuf_get_pktlen(wbuf), subfrm_datalen);
			}
		}
#else
		OS_MEMCPY(wbuf_header(wbuf_subfrm) + hdrsize,
			  wbuf_header(wbuf) + sizeof(struct ether_header),
			  subfrm_datalen);
#endif

		/* update stats for received frames (all fragments) */
		WLAN_PHY_STATS(phy_stats, ips_rx_packets);
		if (is_mcast) {
			WLAN_PHY_STATS(phy_stats, ips_rx_multicast);
		}
		mac_stats->ims_rx_packets++;
		mac_stats->ims_rx_bytes += subfrm_len;

		mac_stats->ims_rx_data_packets++;
		IEEE80211_PEER_STAT(dp_peer, rx_data);
		mac_stats->ims_rx_data_bytes += subfrm_len;
		IEEE80211_PEER_STAT_ADD(dp_peer, rx_bytes, subfrm_len);
#if UMAC_SUPPORT_STA_STATS_ENHANCEMENT
		if (is_mcast) {
			IEEE80211_PEER_STAT(dp_peer, rx_mcast);
			IEEE80211_PEER_STAT_ADD(dp_peer, rx_mcast_bytes,
						subfrm_len);
		} else {
			IEEE80211_PEER_STAT(dp_peer, rx_ucast);
			IEEE80211_PEER_STAT_ADD(dp_peer, rx_ucast_bytes,
						subfrm_len);
		}
#endif
		mac_stats->ims_rx_datapyld_bytes += subfrm_len;

		if (ieee80211_check_privacy_filters(peer, wbuf_subfrm, is_mcast)
		    == FILTER_STATUS_REJECT) {
			wbuf_free(wbuf_subfrm);
		} else {
			ieee80211_deliver_data(vdev, wbuf_subfrm, peer, rs,
					       hdrsize, is_mcast, subtype);
		}

		if (frm_len > subfrm_len) {
			wbuf_pull(wbuf, roundup(subfrm_len, 4));	/* point to next e_header */
			frm_len -= roundup(subfrm_len, 4);
		} else {
			frm_len = 0;
		}
	}
	/* we count 802.11 header into our rx_bytes */
	/* Note: Rectify the below computation after checking for side effects */
	mac_stats->ims_rx_bytes += hdrsize;

	orig_hdrsize = hdrsize +
	    rs->rs_qosdecapcount +
	    rs->rs_cryptodecapcount + IEEE80211_CRC_LEN - rs->rs_padspace;
	mac_stats->ims_rx_data_bytes += orig_hdrsize;
	IEEE80211_PEER_STAT_ADD(dp_peer, rx_bytes, orig_hdrsize);
#if UMAC_SUPPORT_STA_STATS_ENHANCEMENT
	if (is_mcast) {
		IEEE80211_PEER_STAT_ADD(dp_peer, rx_mcast_bytes, orig_hdrsize);
	} else {
		IEEE80211_PEER_STAT_ADD(dp_peer, rx_ucast_bytes, orig_hdrsize);
	}
#endif

err_amsdu:
	while (wbuf_save) {
		wbuf = wbuf_next(wbuf_save);
		wbuf_free(wbuf_save);
		wbuf_save = wbuf;
	}

	return IEEE80211_FC0_TYPE_DATA;
}

static int
ieee80211_qos_decap(struct wlan_objmgr_vdev *vdev,
		    wbuf_t wbuf, int hdrlen, struct ieee80211_rx_status *rs)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	struct ieee80211_frame *wh;
	u_int32_t hdrsize;

	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	if (IEEE80211_QOS_HAS_SEQ(wh)) {
		u_int16_t qoslen = sizeof(struct ieee80211_qoscntl);
		u_int8_t *qos;
#ifdef ATH_SUPPORT_TxBF
		/* Qos frame with Order bit set indicates an HTC frame */
		if (wh->i_fc[1] & IEEE80211_FC1_ORDER) {
			qoslen += sizeof(struct ieee80211_htc);
			QDF_TRACE( QDF_MODULE_ID_ANY,
				       QDF_TRACE_LEVEL_DEBUG,
				       "==>%s: receive +HTC frame\n", __func__);
		}
#endif
		rs->rs_qosdecapcount = qoslen;

		if (wlan_pdev_nif_feat_cap_get(pdev, WLAN_PDEV_F_DATAPAD)) {
			hdrsize = ieee80211_hdrsize(wh);

			/* If the frame has padding, include padding length as well */
			if ((hdrsize % sizeof(u_int32_t) != 0)) {
				rs->rs_padspace = 0;
				qoslen = roundup(qoslen, sizeof(u_int32_t));
			}
		}

		if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) ==
		    IEEE80211_FC1_DIR_DSTODS) {
			qos =
			    &((struct ieee80211_qosframe_addr4 *)wh)->i_qos[0];
		} else {
			qos = &((struct ieee80211_qosframe *)wh)->i_qos[0];
		}

		/* save priority */
		wbuf_set_qosframe(wbuf);
		wbuf_set_priority(wbuf, (qos[0] & IEEE80211_QOS_TID));

		if (dp_vdev->vdev_ccx_evtable
		    && dp_vdev->vdev_ccx_evtable->wlan_ccx_process_qos) {
			dp_vdev->vdev_ccx_evtable->
			    wlan_ccx_process_qos(dp_vdev->vdev_ccx_arg,
						 IEEE80211_RX_QOS_FRAME,
						 (qos[0] & IEEE80211_QOS_TID));
		}

		/* remove QoS filed from header */
		hdrlen -= qoslen;
		memmove((u_int8_t *) wh + qoslen, wh, hdrlen);
		wh = (struct ieee80211_frame *)wbuf_pull(wbuf, qoslen);
		if (wh == NULL) {
			return 1;
		}
		/* clear QoS bit */
		wh->i_fc[0] &= ~IEEE80211_FC0_SUBTYPE_QOS;

	} else {
		u_int16_t padlen = 0;

		/* Non-Qos Frames, remove padding if any */
		if (wlan_pdev_nif_feat_cap_get(pdev, WLAN_PDEV_F_DATAPAD)) {
			hdrsize = ieee80211_hdrsize(wh);
			padlen =
			    roundup((hdrsize), sizeof(u_int32_t)) - hdrsize;
		}

		if (padlen) {
			/* remove padding from header */
			hdrlen -= padlen;
			memmove((u_int8_t *) wh + padlen, wh, hdrlen);
			wh = (struct ieee80211_frame *)wbuf_pull(wbuf, padlen);
			rs->rs_padspace = 0;
		}
	}

	return 0;
}

struct wlan_check_mcastecho_args {
	u_int8_t *sender_mac;
	int mcastecho;
};

static INLINE void
ieee80211_iter_check_mcastecho(struct wlan_objmgr_pdev *pdev, void *obj,
			       void *arg)
{
	struct wlan_check_mcastecho_args *params =
	    (struct wlan_check_mcastecho_args *)arg;
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)obj;

	if (IEEE80211_ADDR_EQ
	    (params->sender_mac, wlan_vdev_mlme_get_macaddr(vdev))) {
		QDF_TRACE( QDF_MODULE_ID_WDS,
			       QDF_TRACE_LEVEL_DEBUG,
			       "*** multicast echo *** sender address equals own address (%s) \n",
			       ether_sprintf(params->sender_mac));
		params->mcastecho = 1;
		return;
	}
}

static INLINE int
ieee80211_is_mcastecho(struct wlan_objmgr_vdev *vdev,
		       struct ieee80211_frame *wh)
{
	u_int8_t *sender;
	u_int8_t dir, subtype;
	int mcastecho = 0;
#if ATH_SUPPORT_WRAP
	struct wlan_check_mcastecho_args params;
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct wlan_objmgr_peer *bsspeer = wlan_vdev_get_bsspeer(vdev);
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	struct wlan_objmgr_peer *peer;
	struct wlan_objmgr_psoc *psoc = wlan_vdev_get_psoc(vdev);
	int isolation = 0;
	isolation = wlan_pdev_get_qwrap_isolation(pdev);
#endif

	KASSERT(wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE, ("!sta mode"));

	dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;
	subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

	if (dir == IEEE80211_FC1_DIR_DSTODS) {
		if (subtype == IEEE80211_FC0_SUBTYPE_QOS) {
			struct ieee80211_qosframe_addr4 *wh4q =
			    (struct ieee80211_qosframe_addr4 *)wh;

#ifdef IEEE80211_DEBUG_CHATTY
			QDF_TRACE( QDF_MODULE_ID_WDS,
				       QDF_TRACE_LEVEL_DEBUG,
				       "*** MCAST QoS 4 addr *** "
				       "ADDR1 %s, ADDR2 %s, ADDR3 %s, ADDR4 %s\n",
				       ether_sprintf(wh4q->i_addr1),
				       ether_sprintf(wh4q->i_addr2),
				       ether_sprintf(wh4q->i_addr3),
				       ether_sprintf(wh4q->i_addr4));
#endif
			sender = wh4q->i_addr4;
		} else {
			struct ieee80211_frame_addr4 *wh4 =
			    (struct ieee80211_frame_addr4 *)wh;

#ifdef IEEE80211_DEBUG_CHATTY
			QDF_TRACE( QDF_MODULE_ID_WDS,
				       QDF_TRACE_LEVEL_DEBUG,
				       "*** MCAST 4 addr *** "
				       "ADDR1 %s, ADDR2 %s, ADDR3 %s, ADDR4 %s\n",
				       ether_sprintf(wh4->i_addr1),
				       ether_sprintf(wh4->i_addr2),
				       ether_sprintf(wh4->i_addr3),
				       ether_sprintf(wh4->i_addr4));
#endif
			sender = wh4->i_addr4;
		}
	} else {
#ifdef IEEE80211_DEBUG_CHATTY
		QDF_TRACE( QDF_MODULE_ID_WDS,
			       QDF_TRACE_LEVEL_DEBUG,
			       "*** MCAST 3 addr  *** "
			       "ADDR1 %s, ADDR2 %s, ADDR3 %s\n",
			       ether_sprintf(wh->i_addr1),
			       ether_sprintf(wh->i_addr2),
			       ether_sprintf(wh->i_addr3));
#endif
		sender = wh->i_addr3;
	}
#if ATH_SUPPORT_WRAP
	if (dp_vdev->av_is_psta) {
		if (!(dp_vdev->av_is_mpsta)) {
			/*Drop multicast packets received on other proxy sta vap except main sta vap,
			 * in order to prevent packet looping*/
			mcastecho = 1;
			goto done;
		}
		if (IEEE80211_ADDR_EQ(sender, wlan_peer_get_bssid(bsspeer))) {
			mcastecho = 0;
			goto done;
		}
		/*When client roam from WRAP to root and if client sends mulicast pkt
		 * after connecting to root, then drop that pkt on WRAP till we have client
		 * in WRAP node list*/
		if (!psoc) {
			qdf_print("%s:psoc is NULL ", __func__);
			return -1;
		}
		peer = ieee80211_vdev_find_node(psoc, vdev, sender);
		if (peer) {
			if (IEEE80211_ADDR_EQ
			    (sender, wlan_peer_get_macaddr(peer))) {
				mcastecho = 1;
				QDF_TRACE(
					       QDF_MODULE_ID_WDS,
					       QDF_TRACE_LEVEL_DEBUG,
					       "*** multicast echo *** sender address equals own address (%s) \n",
					       ether_sprintf(sender));
				wlan_objmgr_peer_release_ref(peer,
							     WLAN_MLME_SB_ID);
				goto done;
			}
			wlan_objmgr_peer_release_ref(peer, WLAN_MLME_SB_ID);
		}
		if (isolation) {
			mcastecho = 0;
			goto done;
		}
		params.sender_mac = sender;
		params.mcastecho = 0;

		wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_VDEV_OP,
						  ieee80211_iter_check_mcastecho,
						  (void *)&params, 1,
						  WLAN_MLME_SB_ID);
		if (params.mcastecho)
			goto done;
	} else
#endif
	if (IEEE80211_ADDR_EQ(sender, wlan_vdev_mlme_get_macaddr(vdev))) {
		QDF_TRACE( QDF_MODULE_ID_WDS,
			       QDF_TRACE_LEVEL_DEBUG,
			       "*** multicast echo *** sender address equals own address (%s) \n",
			       ether_sprintf(sender));
		mcastecho = 1;
		goto done;
	}

	/*
	 * if it is brodcasted by me on behalf of
	 * a station behind me, drop it.
	 *
	 */
	if (wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_WDS))
		mcastecho = wlan_vdev_wds_sta_chkmcecho(vdev, sender);
done:
	return mcastecho;
}

static void
_ieee80211_input_update_data_stats(struct wlan_objmgr_peer *peer,
				   struct wlan_vdev_mac_stats *mac_stats,
				   wbuf_t wbuf,
				   struct ieee80211_rx_status *rs,
				   u_int16_t realhdrsize)
{
	u_int32_t data_bytes = 0;
#if UMAC_SUPPORT_STA_STATS_ENHANCEMENT
	struct ieee80211_frame *wh;
	int is_mcast;
#endif
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);

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
	    + rs->rs_qosdecapcount
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
	/* No need to subtract rs->rs_qosdecapcount because this count
	   would be included in realhdrsize. */
	mac_stats->ims_rx_datapyld_bytes += (data_bytes
					     - realhdrsize
					     - rs->rs_cryptodecapcount
					     - IEEE80211_CRC_LEN);
}

/*
 * Process data frames if the opmode is AP,
 * and return to dadp_input_data()
 */
static INLINE int
dadp_input_data_ap(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
		   struct ieee80211_frame *wh, int dir)
{
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct wlan_objmgr_peer *bsspeer = wlan_vdev_get_bsspeer(vdev);
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	/*
	 * allow all frames with ToDS bit set .
	 * allow Frames with DStoDS if the vap is WDS capable.
	 */
	if (!((dir == IEEE80211_FC1_DIR_TODS) ||
	      (dir == IEEE80211_FC1_DIR_DSTODS
	       && (wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_WDS))))) {
		if (dir == IEEE80211_FC1_DIR_DSTODS) {
			QDF_TRACE(
				       QDF_MODULE_ID_INPUT | QDF_MODULE_ID_WDS,
				       QDF_TRACE_LEVEL_DEBUG,
				       "[%s] Discard 4-address data frame: WDS not enabled",
				       ether_sprintf(wlan_wbuf_getbssid
						     (vdev, wh)));
			WLAN_VAP_STATS(dp_vdev, is_rx_nowds);
		} else {
			QDF_TRACE(
				       QDF_MODULE_ID_INPUT,
				       QDF_TRACE_LEVEL_DEBUG,
				       "[%s] Discard data frame: invalid dir 0x%x",
				       ether_sprintf(wlan_wbuf_getbssid
						     (vdev, wh)), dir);

			WLAN_VAP_STATS(dp_vdev, is_rx_wrongdir);
		}
		return 0;
	}
#if UMAC_SUPPORT_NAWDS
	/* if NAWDS learning feature is enabled, add the mac to NAWDS table */
	if ((peer == bsspeer) &&
	    (dir == IEEE80211_FC1_DIR_DSTODS) &&
	    (wlan_vdev_nawds_enable_learning(vdev))) {
		wlan_vdev_nawds_learn(vdev, wh->i_addr2);
		/* current node is bss node so drop it to avoid sending dis-assoc. packet */
		return 0;
	}
#endif /* UMAC_SUPPORT_NAWDS */
	/* check if source STA is associated */
	if (peer == bsspeer) {
		QDF_TRACE( QDF_MODULE_ID_INPUT,
			       QDF_TRACE_LEVEL_DEBUG,
			       "[%s] Discard data frame: unknown src",
			       ether_sprintf(wlan_wbuf_getbssid(vdev, wh)));

		/* NB: caller deals with reference */

		/*  the following WIFIPOS logic is WAR, the WAR is due to the data frames
		   are received by the off-channel AP, which is causing the AP
		   to send the deauth packets, whcih is eating the BW.
		   So, to avoid this we have a WAR which checks if we are in
		   off-channel and if we are then we don't send the deauth
		   packets.
		 */
#if ATH_SUPPORT_WIFIPOS
		if ((wlan_vdev_is_up(vdev) == QDF_STATUS_SUCCESS)
		    /* && (ic->ic_isoffchan == 0) */ )
#else
		if (wlan_vdev_is_up(vdev) == QDF_STATUS_SUCCESS)
#endif
		{
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
				    (vdev, (wh->i_addr2), 0,
				     IEEE80211_REASON_NOT_AUTHED);

				/* claim node immediately */
				wlan_peer_delete(peer);
			}
		}
		WLAN_VAP_STATS(dp_vdev, is_rx_notassoc);
		return 0;
	}

	if (wlan_peer_get_associd(peer) == 0) {
		QDF_TRACE( QDF_MODULE_ID_INPUT,
			       QDF_TRACE_LEVEL_DEBUG,
			       "[%s] Discard data frame: unassoc src",
			       ether_sprintf(wlan_wbuf_getbssid(vdev, wh)));
		wlan_send_disassoc(peer, IEEE80211_REASON_NOT_ASSOCED);
		wlan_vdev_deliver_event_mlme_disassoc_indication(vdev,
								 (wh->i_addr2),
								 0,
								 IEEE80211_REASON_NOT_ASSOCED);
		WLAN_VAP_STATS(dp_vdev, is_rx_notassoc);
		return 0;
	}

	/* If we're a 4 address packet, make sure we have an entry in
	   the node table for the packet source address (addr4).  If not,
	   add one */
	if (dir == IEEE80211_FC1_DIR_DSTODS) {
		wlan_wds_update_rootwds_table(peer, pdev, wbuf);
	}

#ifdef IEEE80211_DWDS
	/*
	 * For 4-address packets handle WDS discovery
	 * notifications.  Once a WDS link is setup frames
	 * are just delivered to the WDS vap (see below).
	 */
	if (dir == IEEE80211_FC1_DIR_DSTODS && ni->ni_wdsvap == NULL) {
		if (!ieee80211_node_is_authorized(ni)) {
			QDF_TRACE(
				       QDF_MODULE_ID_INPUT | QDF_MODULE_ID_WDS,
				       QDF_TRACE_LEVEL_DEBUG,
				       "[%s] Discard 4-address data frame: unauthorized port",
				       ether_sprintf(wlan_wbuf_getbssid
						     (vdev, wh)));

			WLAN_VAP_STATS(vap, is_rx_unauth);
			/* DA change required as part of dp stats convergence */
			/* IEEE80211_NODE_STAT(ni, rx_unauth); */
			return -1;
		}
		ieee80211_wds_discover(ni, m);
		return 0;
	}
#endif
	return 1;
}

 /*
  * processes data frames.
  * dadp_input_data consumes the wbuf .
  */
static void
dadp_input_data(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
		struct ieee80211_rx_status *rs, int subtype, int dir)
{
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
	struct ieee80211_frame *wh;
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	struct wlan_lmac_if_crypto_rx_ops *crypto_rx_ops =
	    wlan_crypto_get_crypto_rx_ops(psoc);
	uint8_t key_header, key_trailer, key_miclen;
	uint8_t macaddr[IEEE80211_ADDR_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	uint8_t *mac;
	int key_set = 0;
	uint8_t frame_keyid = 0;
#else
	struct ieee80211_key *key = NULL;
#endif
	u_int16_t hdrspace;
	int realhdrsize;
	int padsize;
	int is_mcast, is_amsdu = 0, is_bcast;
#if not_yet
	struct ieee80211vap *tmp_vap = NULL;
#endif
	struct wlan_pdev_phy_stats *phy_stats;
	struct wlan_vdev_mac_stats *mac_stats;

	if (!dp_peer) {
		qdf_print("%s:dp_peer is null ", __func__);
		goto bad;
	}

	rs->rs_cryptodecapcount = 0;
	rs->rs_qosdecapcount = 0;

	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	is_mcast = (dir == IEEE80211_FC1_DIR_DSTODS ||
		    dir == IEEE80211_FC1_DIR_TODS) ?
	    IEEE80211_IS_MULTICAST(IEEE80211_WH4(wh)->i_addr3) :
	    IEEE80211_IS_MULTICAST(wh->i_addr1);
	is_bcast =
	    (dir ==
	     IEEE80211_FC1_DIR_FROMDS) ? IEEE80211_IS_BROADCAST(wh->
								i_addr1) :
	    FALSE;

#if not_yet
	if (dir == IEEE80211_FC1_DIR_DSTODS) {
		TAILQ_FOREACH(tmp_vap, &ic->ic_vaps, iv_next) {
			if (IEEE80211_ADDR_EQ
			    (IEEE80211_WH4(wh)->i_addr4, tmp_vap->iv_myaddr)) {
				printk(" %s mac %s should not in here\n",
				       __func__,
				       ether_sprintf(IEEE80211_WH4(wh)->
						     i_addr4));
				goto bad;
			}
		}
	}
#endif

#if !ATH_SUPPORT_STATS_APONLY
	phy_stats = &dp_pdev->pdev_phy_stats[wlan_vdev_get_curmode(vdev)];
#endif
	UNREFERENCED_PARAMETER(phy_stats);

#if UMAC_PER_PACKET_DEBUG
	QDF_TRACE( QDF_MODULE_ID_INPUT,
		       QDF_TRACE_LEVEL_DEBUG,
		       "  %d %d %d     %d %d %d %d %d %d       "
		       "  %d %d  \n\n", rs->rs_lsig[0], rs->rs_lsig[1],
		       rs->rs_lsig[2], rs->rs_htsig[0], rs->rs_htsig[1],
		       rs->rs_htsig[2], rs->rs_htsig[3], rs->rs_htsig[4],
		       rs->rs_htsig[5], rs->rs_servicebytes[0],
		       rs->rs_servicebytes[1]);
#endif
	mac_stats =
	    is_mcast ? &dp_vdev->vdev_multicast_stats : &dp_vdev->
	    vdev_unicast_stats;

	hdrspace = wlan_hdrspace(pdev, wbuf_header(wbuf));
	if (wbuf_get_pktlen(wbuf) < hdrspace) {
		WLAN_MAC_STATS(mac_stats, ims_rx_discard);
		QDF_TRACE( QDF_MODULE_ID_INPUT,
			       QDF_TRACE_LEVEL_DEBUG,
			       "[%s] Discard data frame: too small len %d ",
			       ether_sprintf(wlan_wbuf_getbssid(vdev, wh)),
			       wbuf_get_pktlen(wbuf));
		goto bad;
	}

	realhdrsize = ieee80211_hdrsize(wh);	/* no _F_DATAPAD */
	padsize = hdrspace - realhdrsize;

	rs->rs_padspace = padsize;

#if ATH_WDS_WAR_UNENCRYPTED
	/* Owl 2.2 WDS War for Non-Encrypted 4 Addr QoS Frames - Extra QoS Ctl field */
	if ((dir == IEEE80211_FC1_DIR_DSTODS) && IEEE80211_PEER_USEWDSWAR(ni) &&
	    ((wh->i_fc[1] & IEEE80211_FC1_WEP) == 0) &&
	    (subtype == IEEE80211_FC0_SUBTYPE_QOS)) {
		u_int8_t *header = (u_int8_t *) wh;

		if (padsize == 0) {
			/* IEEE80211_F_DATAPAD had no local effect,
			 * but we need to account for remote */
			padsize +=
			    roundup(sizeof(struct ieee80211_qoscntl),
				    sizeof(u_int32_t));
		}

		if (padsize > 0) {
			memmove(header + padsize, header, realhdrsize);
			wbuf_pull(wbuf, padsize);
			rs->rs_padspace = 0;
			/* data ptr moved */
			wh = (struct ieee80211_frame *)wbuf_header(wbuf);
		}
	}
#endif

	WLAN_VDEV_LASTDATATSTAMP(vdev, OS_GET_TIMESTAMP());
	WLAN_PHY_STATS(phy_stats, ips_rx_fragments);
	WLAN_VDEV_TXRXBYTE(dp_vdev, wbuf_get_pktlen(wbuf));

	/*
	 * Store timestamp for actual (non-NULL) data frames.
	 * This provides other modules such as SCAN and LED with correct
	 * information about the actual data traffic in the system.
	 * We don't take broadcast traffic into consideration.
	 */
	WLAN_VDEV_TRAFIC_INDICATION(dp_vdev, is_bcast, subtype);

	switch (wlan_vdev_mlme_get_opmode(vdev)) {
	case QDF_STA_MODE:
		if (dadp_input_data_sta(peer, wbuf, dir, subtype) == 0) {
			goto bad;
		}
		break;

	case QDF_IBSS_MODE:
		if (dadp_input_data_ibss(peer, wbuf, dir) == 0) {
			goto bad;
		}
		break;
	case QDF_SAP_MODE:
		if (likely
		    ((wlan_pdev_get_curchan(pdev)) ==
		     (wlan_vdev_get_bsschan(vdev)))) {
			if (dadp_input_data_ap(peer, wbuf, wh, dir) == 0) {
				goto bad;
			}
		} else
			goto bad;
		break;

	case QDF_WDS_MODE:
		if (dadp_input_data_wds(vdev, wbuf, dir) == 0) {
			goto bad;
		}
		break;
	default:
		break;
	}

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	if (!wlan_vdev_mlme_feat_ext_cap_get(vdev, WLAN_VDEV_FEXT_SAFEMODE) &&
	    (wh->i_fc[1] & IEEE80211_FC1_WEP)) {
		if(IEEE80211_IS_MULTICAST(wh->i_addr1) /*|| !dadp_peer_get_key_valid(peer)*/)
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
		if ((crypto_rx_ops && WLAN_CRYPTO_RX_OPS_DECAP(crypto_rx_ops) &&
		     (WLAN_CRYPTO_RX_OPS_DECAP(crypto_rx_ops) (vdev,
							       wbuf,
							       mac,
							       0) != QDF_STATUS_SUCCESS))) {
			IEEE80211_PEER_STAT(dp_peer, rx_decryptcrc);
			QDF_TRACE(
				       QDF_MODULE_ID_INPUT,
				       QDF_TRACE_LEVEL_DEBUG,
				       " [%s] Discard data frame: key is null",
				       ether_sprintf(wlan_wbuf_getbssid
						     (vdev, wh)));
			goto bad;
		} else {
			WLAN_MAC_STATS(mac_stats, ims_rx_decryptok);
			rs->rs_cryptodecapcount += (key_header + key_trailer);
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
		if (key == NULL) {
			WLAN_MAC_STATS(mac_stats, ims_rx_decryptcrc);
			IEEE80211_PEER_STAT(dp_peer, rx_decryptcrc);
			QDF_TRACE(
				       QDF_MODULE_ID_INPUT,
				       QDF_TRACE_LEVEL_DEBUG,
				       " [%s] Discard data frame: key is null",
				       ether_sprintf(wlan_wbuf_getbssid
						     (vdev, wh)));
			goto bad;
		} else {
			WLAN_MAC_STATS(mac_stats, ims_rx_decryptok);
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
	 * deliver 802.11 frame if the OS is interested in it.
	 * if os returns a non zero value, drop the frame .
	 */
	if (dp_vdev->vdev_evtable
	    && dp_vdev->vdev_evtable->wlan_receive_filter_80211) {
		if (dp_vdev->vdev_evtable->
		    wlan_receive_filter_80211(wlan_vdev_get_osifp(vdev), wbuf,
					      IEEE80211_FC0_TYPE_DATA, subtype,
					      rs)) {
			goto bad;
		}
	}

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
		is_amsdu = (dir != IEEE80211_FC1_DIR_DSTODS) ?
		    (((struct ieee80211_qosframe *)wh)->
		     i_qos[0] & IEEE80211_QOS_AMSDU)
		    : (((struct ieee80211_qosframe_addr4 *)wh)->
		       i_qos[0] & IEEE80211_QOS_AMSDU);
	}
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	/*
	 * Next strip any MSDU crypto bits.
	 */
	ASSERT(!wlan_vdev_mlme_feat_ext_cap_get(vdev, WLAN_VDEV_FEXT_SAFEMODE));
	if (key_set == 1) {
		if ((crypto_rx_ops && WLAN_CRYPTO_RX_OPS_DEMIC(crypto_rx_ops) &&
		     (WLAN_CRYPTO_RX_OPS_DEMIC(crypto_rx_ops) (vdev,
							       wbuf,
							       mac,
							       0,
							       frame_keyid) != QDF_STATUS_SUCCESS))) {
			QDF_TRACE(
				       QDF_MODULE_ID_INPUT,
				       QDF_TRACE_LEVEL_DEBUG,
				       "[%s] Discard data frame: demic error",
				       wlan_peer_get_macaddr(peer));

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
		if (!wlan_vdev_crypto_demic(vdev, key, wbuf, hdrspace, 0, rs)) {
			QDF_TRACE(
				       QDF_MODULE_ID_INPUT,
				       QDF_TRACE_LEVEL_DEBUG,
				       "[%s] Discard data frame: demic error",
				       wlan_peer_get_macaddr(peer));

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
	/*
	 * decapsulate the QoS header if the OS asks us to deliver standard 802.11
	 * headers. if OS does not want us to deliver 802.11 header then it wants us
	 * to deliver ethernet header in which case the qos header will be decapped
	 * along with 802.11 header ieee80211_decap function.
	 */
	if (wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_DELIVER_80211) &&
	    ieee80211_qos_decap(vdev, wbuf, hdrspace, rs)) {
		QDF_TRACE( QDF_MODULE_ID_INPUT,
			       QDF_TRACE_LEVEL_DEBUG,
			       "[%s] Discard data frame: decap error",
			       wlan_peer_get_macaddr(peer));
		WLAN_VAP_STATS(dp_vdev, is_rx_decap);
		goto bad;
	}

	if (subtype == IEEE80211_FC0_SUBTYPE_NODATA
	    || subtype == IEEE80211_FC0_SUBTYPE_QOS_NULL) {
		IEEE80211_INPUT_UPDATE_DATA_STATS(peer, mac_stats, wbuf, rs,
						  realhdrsize);
		/* no need to process the null data frames any further */
		goto bad;
	}
#if ATH_RXBUF_RECYCLE
	if (is_mcast || is_bcast) {
		wbuf_set_cloned(wbuf);
	} else {
		wbuf_clear_cloned(wbuf);
	}
#endif
	if (!is_amsdu) {
		if (ieee80211_check_privacy_filters(peer, wbuf, is_mcast) ==
		    FILTER_STATUS_REJECT) {
			QDF_TRACE(
				       QDF_MODULE_ID_INPUT,
				       QDF_TRACE_LEVEL_DEBUG, wh->i_addr2,
				       "data", "privacy filter check", "%s \n",
				       "failed");
			goto bad;
		}
	} else {
		ieee80211_amsdu_input(peer, wbuf, rs, is_mcast, subtype);
		goto out;
	}
	/* update stats for received frames (all fragments) */
	WLAN_PHY_STATS(phy_stats, ips_rx_packets);
	if (is_mcast) {
		WLAN_PHY_STATS(phy_stats, ips_rx_multicast);
	}

	WLAN_MAC_STATS(mac_stats, ims_rx_packets);
	/* TODO: Rectify the below computation after checking for side effects. */
	WLAN_MAC_STATSINCVAL(mac_stats, ims_rx_bytes, wbuf_get_pktlen(wbuf));

	IEEE80211_INPUT_UPDATE_DATA_STATS(peer, mac_stats, wbuf, rs,
					  realhdrsize);
	/* consumes the wbuf */
	ieee80211_deliver_data(vdev, wbuf, peer, rs, hdrspace, is_mcast,
			       subtype);

out:
	return;

bad:
/*  FIX ME: linux specific netdev struct iv_destats has to be replaced*/
//    vap->iv_devstats.rx_errors++;
	wbuf_free(wbuf);
}

/*
 * Process a received frame if the opmode is AP
 * and returns back to ieee80211_input()
 */
static INLINE int
dadp_input_ap(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
	      struct ieee80211_frame *wh, struct wlan_vdev_mac_stats *mac_stats,
	      int type, int subtype, int dir)
{
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct wlan_objmgr_peer *bsspeer = wlan_vdev_get_bsspeer(vdev);
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
	u_int8_t *bssid;

	if (!dp_peer) {
		qdf_print("%s:dp_peer is null ", __func__);
		return 0;
	}

	if (dir != IEEE80211_FC1_DIR_NODS)
		bssid = wh->i_addr1;
	else if (type == IEEE80211_FC0_TYPE_CTL)
		bssid = wh->i_addr1;
	else {
		if (wbuf_get_pktlen(wbuf) < sizeof(struct ieee80211_frame)) {
			WLAN_MAC_STATS(mac_stats, ims_rx_discard);
			return 0;
		}
		bssid = wh->i_addr3;
	}
	if (type != IEEE80211_FC0_TYPE_DATA)
		return 1;
	/*
	 * Data frame, validate the bssid.
	 */
	if (!IEEE80211_ADDR_EQ(bssid, wlan_peer_get_bssid(bsspeer)) &&
	    !IEEE80211_ADDR_EQ(bssid, dp_pdev->pdev_broadcast)) {
		/* NAWDS repeaters send ADDBA with the BSSID
		 * set to their BSSID and not ours
		 * We should not drop those frames
		 */
		if (!((dp_peer->nawds) &&
		      (type == IEEE80211_FC0_TYPE_MGT) &&
		      (subtype == IEEE80211_FC0_SUBTYPE_ACTION))) {
			/* not interested in */
			WLAN_MAC_STATS(mac_stats, ims_rx_discard);

			return 0;
		}
	}
	return 1;
}

#ifndef ATH_HTC_MII_DISCARD_NETDUPCHECK
#define IEEE80211_CHECK_DUPPKT(_dp_pdev, _dp_peer,  _type,_subtype,_dir, _wh, _phystats, _rs, _errorlable, _rxseqno)        \
        /* Check duplicates */                                                                          \
        if (HAS_SEQ(_type, _subtype)) {                                                                 \
            u_int8_t tid;                                                                               \
            if (IEEE80211_QOS_HAS_SEQ(wh)) {                                                            \
                if (dir == IEEE80211_FC1_DIR_DSTODS) {                                                  \
                    tid = ((struct ieee80211_qosframe_addr4 *)wh)->                                     \
                        i_qos[0] & IEEE80211_QOS_TID;                                                   \
                } else {                                                                                \
                    tid = ((struct ieee80211_qosframe *)wh)->                                           \
                        i_qos[0] & IEEE80211_QOS_TID;                                                   \
                }                                                                                       \
                if (TID_TO_WME_AC(tid) >= WME_AC_VI)                                                    \
                    dp_pdev->wme_hipri_traffic++;                                                     \
            } else {                                                                                    \
                    tid = IEEE80211_NON_QOS_SEQ;                            \
            }                                                               \
                                                                            \
            _rxseqno = le16toh(*(u_int16_t *)wh->i_seq);                       \
            if ((_wh->i_fc[1] & IEEE80211_FC1_RETRY) &&                     \
                (_rxseqno == _dp_peer->peer_rxseqs[tid])) {                           \
                WLAN_PHY_STATS(_phystats, ips_rx_dup);                       \
                                                                            \
                if (_dp_peer->peer_last_rxseqs[tid] == _dp_peer->peer_rxseqs[tid]) {      \
                    WLAN_PHY_STATS(_phystats, ips_rx_mdup);                 \
                }                                                           \
                _dp_peer->peer_last_rxseqs[tid] = _dp_peer->peer_rxseqs[tid];             \
                goto _errorlable;                                           \
            }                                                               \
            _dp_peer->peer_rxseqs[tid] = _rxseqno;                                    \
                                                                            \
        }
#else
#define IEEE80211_CHECK_DUPPKT(_dp_pdev, _dp_peer,  _type,_subtype,_dir, _wh, _phystats, _rs, _errorlable)
#endif

/*
 * Process a received frame.  The node associated with the sender
 * should be supplied.  If nothing was found in the node table then
 * the caller is assumed to supply a reference to iv_bss instead.
 * The RSSI and a timestamp are also supplied.  The RSSI data is used
 * during AP scanning to select a AP to associate with; it can have
 ;* any units so long as values have consistent units and higher values
 * mean ``better signal''.
 */
void ieee80211_sta2ibss_header(struct ieee80211_frame *wh)
{
	unsigned char DA[IEEE80211_ADDR_LEN];
	unsigned char SA[IEEE80211_ADDR_LEN];
	unsigned char BSSID[IEEE80211_ADDR_LEN];
	int i;

	wh->i_fc[1] &= ~IEEE80211_FC1_DIR_MASK;
	wh->i_fc[1] |= IEEE80211_FC1_DIR_NODS;
	for (i = 0; i < IEEE80211_ADDR_LEN; i++) {
		BSSID[i] = wh->i_addr1[i];
		SA[i] = wh->i_addr2[i];
		DA[i] = wh->i_addr3[i];
	}
	for (i = 0; i < IEEE80211_ADDR_LEN; i++) {
		wh->i_addr1[i] = DA[i];
		wh->i_addr2[i] = SA[i];
		wh->i_addr3[i] = BSSID[i];
	}
}

int wlan_handle_pwrsave(struct wlan_objmgr_vdev *vdev,
			struct wlan_objmgr_peer *peer,
			struct ieee80211_frame *wh)
{
	int type = -1, subtype;
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);

	if (!dp_peer) {
		qdf_print("%s:dp_peer is null ", __func__);
		return -EINVAL;
	}

	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

	/*
	 * Check for power save state change.
	 */
	if ((wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE) &&
	    (peer != wlan_vdev_get_bsspeer(vdev)) &&
	    !(type == IEEE80211_FC0_TYPE_MGT
	      && subtype == IEEE80211_FC0_SUBTYPE_PROBE_REQ)) {

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
					return -EINVAL;
				}
			}
		}

		if (((wh->i_fc[1] & IEEE80211_FC1_PWR_MGT) >> WLAN_FC1_PWR_MGT_SHIFT) ^
		    (wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_PWR_MGT))) {
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
	return EOK;
}

int
dadp_input(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
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
//      struct ath_softc_net80211 *scn = dp_pdev->scn;
	struct ieee80211_frame *wh;
	struct wlan_pdev_phy_stats *phy_stats;
	struct wlan_vdev_mac_stats *mac_stats;
	struct wlan_stats *vdev_stats;
	int type = -1, subtype, dir;
#if UMAC_SUPPORT_WNM
	u_int8_t secured;
#endif
	u_int16_t rxseq = 0;
	u_int8_t *bssid;
	bool rssi_update = true;

	if (!dp_pdev) {
		qdf_print("%s:dp_pdev is null ", __func__);
		goto bad1;
	}

	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is null ", __func__);
		goto bad1;
	}

	if (!dp_peer) {
		qdf_print("%s:dp_peer is null ", __func__);
		goto bad1;
	}

	KASSERT((wbuf_get_pktlen(wbuf) >= dp_pdev->pdev_minframesize),
		("frame length too short: %u", wbuf_get_pktlen(wbuf)));

	wbuf_set_peer(wbuf, peer);

	if (wbuf_get_pktlen(wbuf) < dp_pdev->pdev_minframesize) {
		goto bad1;
	}

	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	if ((wh->i_fc[0] & IEEE80211_FC0_VERSION_MASK) !=
	    IEEE80211_FC0_VERSION_0) {
		/* XXX: no stats for it. */
		goto bad1;
	}

	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
	dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;
	if ((wlan_vdev_mlme_get_opmode(vdev) == QDF_IBSS_MODE)
	    && type == IEEE80211_FC0_TYPE_DATA && dp_pdev->pdev_softap_enable) {
		if (dir == IEEE80211_FC1_DIR_TODS) {
			ieee80211_sta2ibss_header(wh);
			dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;
		}
	}

	if (wlan_vdev_rx_gate(vdev) != 0) {
		goto bad1;
	}

	/* Mark node as WDS */
	if (dir == IEEE80211_FC1_DIR_DSTODS)
		wlan_peer_mlme_flag_set(peer, WLAN_PEER_F_WDS);

#if !ATH_SUPPORT_STATS_APONLY
	phy_stats = &dp_pdev->pdev_phy_stats[wlan_vdev_get_curmode(vdev)];
#endif
	mac_stats =
	    IEEE80211_IS_MULTICAST(wh->i_addr1) ? &dp_vdev->
	    vdev_multicast_stats : &dp_vdev->vdev_unicast_stats;
	vdev_stats = &dp_vdev->vdev_stats;
	UNREFERENCED_PARAMETER(phy_stats);

	/*
	 * XXX Validate received frame if we're not scanning.
	 * why do we receive only data frames when we are scanning and
	 * current (foreign channel) channel is the bss channel ?
	 * should we simplify this to if (vap->iv_bsschan == ic->ic_curchan) ?
	 */
	if (true /* TBD_DADP:Replace with (scn->sc_ops->ath_scan_in_home_channel(scn->sc_dev)) */ ||
		   (type == IEEE80211_FC0_TYPE_DATA)) {
		switch (wlan_vdev_mlme_get_opmode(vdev)) {
		case QDF_STA_MODE:
			if (dadp_input_sta(peer, wbuf, rs) == 0) {
				goto bad;
			}
			break;
		case QDF_IBSS_MODE:
		case QDF_AHDEMO_MODE:
			if (dadp_input_ibss(peer, wbuf, rs) == 0) {
				goto bad;
			}
			if (dir != IEEE80211_FC1_DIR_NODS)
				bssid = wh->i_addr1;
			else if (type == IEEE80211_FC0_TYPE_CTL)
				bssid = wh->i_addr1;
			else {
				if (wbuf_get_pktlen(wbuf) <
				    sizeof(struct ieee80211_frame)) {
					mac_stats->ims_rx_discard++;
					goto bad;
				}
				bssid = wh->i_addr3;
			}
			if ((wlan_vdev_is_up(vdev) == QDF_STATUS_SUCCESS) &&
			    !IEEE80211_ADDR_EQ(bssid,
					       wlan_peer_get_bssid(bsspeer))) {
				rssi_update = false;
			}
			break;
		case QDF_SAP_MODE:
			{
				if (dadp_input_ap
				    (peer, wbuf, wh, mac_stats, type, subtype,
				     dir) == 0) {
					goto bad;
				}
			}
			break;

		case IEEE80211_M_WDS:
			if (dadp_input_wds(peer, wbuf, rs) == 0) {
				goto bad;
			}
			break;
		case IEEE80211_M_BTAMP:
			break;

		default:
			goto bad;
		}

		if (rssi_update && rs->rs_isvalidrssi)
			wlan_peer_set_rssi(peer, rs->rs_rssi);

		IEEE80211_CHECK_DUPPKT(dp_pdev, dp_peer, type, subtype, dir, wh,
				       phy_stats, rs, bad, rxseq);

	}
#if UMAC_SUPPORT_WNM
	if (wlan_vdev_wnm_is_set(vdev)) {
		secured = wh->i_fc[1] & IEEE80211_FC1_WEP;
		wlan_wnm_bssmax_updaterx(peer, secured);
	}
#endif

	if (wlan_handle_pwrsave(vdev, peer, wh))
		goto bad;

	if (type == IEEE80211_FC0_TYPE_DATA) {
		if (wlan_vdev_is_up(vdev) != QDF_STATUS_SUCCESS) {
			goto bad;
		}
#if QCA_SUPPORT_SON
		if ((subtype != IEEE80211_FC0_SUBTYPE_NODATA)
		    && (subtype != IEEE80211_FC0_SUBTYPE_QOS_NULL))
			son_mark_node_inact(peer, false);
#endif
		/* dadp_input_data consumes the wbuf */
		wlan_peer_reload_inact(peer);	/* peer has activity */
#if ATH_SW_WOW
		if (wlan_vdev_get_wow(vdev)) {
			wlan_peer_wow_magic_parser(peer, wbuf);
		}
#endif
		dadp_input_data(peer, wbuf, rs, subtype, dir);
	} else if (type == IEEE80211_FC0_TYPE_CTL) {

		WLAN_VAP_STATS(dp_vdev, is_rx_ctl);
		wlan_peer_recv_ctrl(peer, wbuf, subtype, rs);
		/*
		 * deliver the frame to the os. the handler cosumes the wbuf.
		 */
		if (dp_vdev->vdev_evtable) {
			dp_vdev->vdev_evtable->
			    wlan_receive(wlan_vdev_get_osifp(vdev), wbuf, type,
					 subtype, rs);
		}
	} else {
		goto bad;
	}

	wlan_vdev_rx_ungate(vdev);
	return type;

bad:
	wlan_vdev_rx_ungate(vdev);
bad1:
	if (wbuf_next(wbuf) != NULL) {
		wbuf_t wbx = wbuf;
		while (wbx) {
			wbuf = wbuf_next(wbx);
			wbuf_free(wbx);
			wbx = wbuf;
		}
	} else {
		wbuf_free(wbuf);
	}
	return type;
#undef HAS_SEQ
#undef QOS_NULL
}

#ifdef IEEE80211_DWDS
/*
 * Handle WDS discovery on receipt of a 4-address frame in
 * ap mode.  Queue the frame and post an event for someone
 * to plumb the necessary WDS vap for this station.  Frames
 * received prior to the vap set running will then be reprocessed
 * as if they were just received.
 */
static void ieee80211_wds_discover(struct ieee80211_node *ni, wbuf_t wbuf)
{
#ifdef IEEE80211_DEBUG
	struct ieee80211vap *vap = ni->ni_vap;
#endif
	struct ieee80211com *ic = ni->ni_ic;
	int qlen, age;

	IEEE80211_NODE_WDSQ_LOCK(ni);
	if (_IF_QFULL(&ni->ni_wdsq)) {
		_IF_DROP(&ni->ni_wdsq);
		IEEE80211_NODE_WDSQ_UNLOCK(ni);
		QDF_TRACE( QDF_MODULE_ID_INPUT,
			       QDF_TRACE_LEVEL_DEBUG,
			       "[%s] Discard wds data frame: wds pending q overflow, drops %d (size %d)",
			       wlan_wbuf_getbssid(vdev,
						  (struct ieee80211_frame *)
						  wbuf_header(wbuf)),
			       ni->ni_wdsq.ifq_drops, IEEE80211_PS_MAX_QUEUE);
#ifdef IEEE80211_DEBUG
		if (ieee80211_msg_dumppkts(vap))
			ieee80211_dump_pkt(pdev, (u_int8_t *) wbuf_header(wbuf),
					   wbuf_get_pktlen(wbuf), -1, -1);
#endif
		wbuf_free(wbuf);
		return;
	}
	/*
	 * Tag the frame with it's expiry time and insert
	 * it in the queue.  The aging interval is 4 times
	 * the listen interval specified by the station.
	 * Frames that sit around too long are reclaimed
	 * using this information.
	 */
	/* XXX handle overflow? */
	/* XXX per/vap beacon interval? */
	age = ((ni->ni_intval * ic->ic_lintval) << 2) / 1024;	/* TU -> secs */
	_IEEE80211_NODE_WDSQ_ENQUEUE(ni, wbuf, qlen, age);
	IEEE80211_NODE_WDSQ_UNLOCK(ni);

	QDF_TRACE( QDF_MODULE_ID_WDS,
		       QDF_TRACE_LEVEL_DEBUG, "save frame, %u now queued", qlen);

	ieee80211_notify_wds_discover(ni);
}
#endif

static wbuf_t
ieee80211_decap(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf, size_t hdrspace,
		struct ieee80211_rx_status *rs)
{
	struct ieee80211_qosframe_addr4 wh;	/* max size address frame */
	struct ether_header *eh;
	struct llc *llc;
	u_int16_t ether_type = 0;
	struct ieee80211_frame *whhp;
	int nwifi = 0;

	if (wbuf_get_pktlen(wbuf) < (hdrspace + sizeof(*llc))) {
		/* XXX stat, msg */
		wbuf_free(wbuf);
		wbuf = NULL;
		goto done;
	}

	/*
	 * Store the WME QoS Priority in the wbuf before 802.11 header
	 * decapsulation so we can apply 802.1q/802.1p QoS Priority to the VLAN Header.
	 */
	whhp = (struct ieee80211_frame *)wbuf_header(wbuf);
	if (IEEE80211_QOS_HAS_SEQ(whhp)) {
		u_int8_t *qos;

		if ((whhp->i_fc[1] & IEEE80211_FC1_DIR_MASK) ==
		    IEEE80211_FC1_DIR_DSTODS) {
			qos =
			    &((struct ieee80211_qosframe_addr4 *)whhp)->
			    i_qos[0];
		} else {
			qos = &((struct ieee80211_qosframe *)whhp)->i_qos[0];
		}

		/* save priority */
		wbuf_set_qosframe(wbuf);
		wbuf_set_priority(wbuf, (qos[0] & IEEE80211_QOS_TID));
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

#ifdef not_yet
	ieee80211_smartantenna_input(vdev, wbuf, eh, rs);
#endif

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
	case IEEE80211_FC1_DIR_DSTODS:
		IEEE80211_ADDR_COPY(eh->ether_dhost, wh.i_addr3);
		IEEE80211_ADDR_COPY(eh->ether_shost, wh.i_addr4);
		break;
	}
	if (ADP_EXT_AP_RX_PROCESS(vdev, wbuf, &nwifi)) {
		wbuf_free(wbuf);
		wbuf = NULL;
		goto done;
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

/*
 * delivers the data to the OS .
 *  will deliver standard 802.11 frames (with qos control removed)
 *  if IEEE80211_DELIVER_80211 param is set.
 *  will deliver ethernet frames (with 802.11 header decapped)
 *  if IEEE80211_DELIVER_80211 param is not set.
 *  this funcction consumes the  passed in wbuf.
 */
static void
ieee80211_deliver_data(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf,
		       struct wlan_objmgr_peer *peer,
		       struct ieee80211_rx_status *rs, u_int32_t hdrspace,
		       int is_mcast, u_int8_t subtype)
{
	int igmp = 0;
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
#if defined (ATH_SUPPORT_HYFI_ENHANCEMENTS)
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
#endif
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
		wbuf = ieee80211_decap(vdev, wbuf, hdrspace, rs);
		if (!wbuf) {
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
		 *
		 * If HiFi feature enabled, multicast enhancement will be disabled.
		 * And the callback will return non-zero indicate IGMP or MLD packets,
		 */
		if (dp_vdev->ique_ops.wlan_me_inspect) {

			igmp =
			    dp_vdev->ique_ops.wlan_me_inspect(vdev, peer, wbuf);
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
			if (dp_vdev->me_hifi_enable
			    && igmp == IEEE80211_QUERY_FROM_STA
			    && dp_pdev->pdev_dropstaquery) {
				wbuf_complete(wbuf);
				QDF_TRACE(
					       QDF_MODULE_ID_IQUE,
					       QDF_TRACE_LEVEL_DEBUG,
					       "Dropping IGMP Query from STA.\n");
				return;
			}
#endif
		}
	}

#ifndef ATH_HTC_MII_DISCARD_BRIDGEWITHINAP
	/* perform as a bridge within the AP */
	if ((wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE) &&
	    !wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_NOBRIDGE)) {
		wbuf_t wbuf_cpy = NULL;

		if (is_mcast) {
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
			/* Enable/disable flooding Report packets. */
			if (igmp != IEEE80211_REPORT_FROM_STA
			    || !dp_pdev->pdev_blkreportflood) {
#endif
				wbuf_cpy = wbuf_clone(dp_pdev->osdev, wbuf);
#if ATH_RXBUF_RECYCLE
				if (wbuf_cpy)
					wbuf_set_cloned(wbuf_cpy);
#endif
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
			} else {
				QDF_TRACE(
					       QDF_MODULE_ID_IQUE,
					       QDF_TRACE_LEVEL_DEBUG,
					       "Received IGMP report. Don't flood it.\n");
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
				if (peer1 == NULL) {
					peer1 =
					    wlan_find_wds_peer(vdev,
							       eh->ether_dhost);
				}
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
			dp_vdev->vdev_evtable->
			    wlan_vap_xmit_queue(wlan_vdev_get_osifp(vdev),
						wbuf_cpy);
		}
	}
#endif
	if (wbuf != NULL) {
#ifdef IEEE80211_WDS
		if (is_mcast) {
			QDF_TRACE( QDF_MODULE_ID_WDS,
				       QDF_TRACE_LEVEL_DEBUG,
				       "%s: RX MCAST VAP from SA %s\n",
				       __func__,
				       ether_sprintf(eh->ether_shost));

			if (wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE) {
				ieee80211_internal_send_dwds_multicast(vap, m);
			}
		}
#endif

#if UMAC_SUPPORT_VI_DBG
		wlan_vdev_vi_dbg_input(vdev, wbuf);
#endif
#if ATH_SUPPORT_WRAP
		ath_wrap_mat_rx(vdev, wbuf);
#endif
		/*
		 * deliver the data frame to the os. the handler cosumes the wbuf.
		 */
		dp_vdev->vdev_evtable->wlan_receive(wlan_vdev_get_osifp(vdev),
						    wbuf,
						    IEEE80211_FC0_TYPE_DATA,
						    subtype, rs);
	}
}

struct wlan_iter_input_all_arg {
	wbuf_t wbuf;
	struct ieee80211_rx_status *rs;
	int type;
};

static INLINE void
ieee80211_iter_input_all(struct wlan_objmgr_pdev *pdev, void *obj, void *arg)
{
	struct wlan_iter_input_all_arg *params =
	    (struct wlan_iter_input_all_arg *)arg;
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)obj;
	struct wlan_objmgr_peer *bsspeer;
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	wbuf_t wbuf1;

	if (!vdev) {
		printk("Already vdev is deleted!!\n");
		return;
	}

	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_MONITOR_MODE ||
	    (wlan_vdev_chan_config_valid(vdev) != QDF_STATUS_SUCCESS)) {
		return;
	}

	bsspeer = wlan_vdev_get_bsspeer(vdev);

	if (!bsspeer)
		return;

	wbuf1 = wbuf_clone(dp_pdev->osdev, params->wbuf);
	if (wbuf1 == NULL) {
		/* XXX stat+msg */
		return;
	}

	params->type = dadp_input(bsspeer, wbuf1, params->rs);
}

int
dadp_input_all(struct wlan_objmgr_pdev *pdev,
	       wbuf_t wbuf, struct ieee80211_rx_status *rs)
{
	struct wlan_iter_input_all_arg params;

	params.wbuf = wbuf;
	params.rs = rs;
	params.type = -1;

	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_VDEV_OP,
					  ieee80211_iter_input_all,
					  (void *)&params, 1, WLAN_MLME_SB_ID);

	if (params.wbuf)
		wbuf_free(params.wbuf);
	return params.type;
}

#if !UMAC_SUPPORT_OPMODE_APONLY
/*
 * Process received data packet if opmode is STA, and return back to
 * dadp_input_data()
 */
int
dadp_input_data_sta(struct wlan_objmgr_peer *peer, wbuf_t wbuf, int dir,
		    int subtype)
{
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	struct dadp_vdev *dp_vdev = NULL;
	struct wlan_vdev_mac_stats *mac_stats;
	struct wlan_objmgr_peer *peer_wds = NULL;
	struct wlan_objmgr_peer *temp_peer = NULL;
	struct ieee80211_frame *wh;
	int is_mcast;
	uint8_t pdev_id;

	if(!peer) {
		qdf_print("%s:peer is null ", __func__);
		return 0;
	}

	vdev = wlan_peer_get_vdev(peer);
	if(!vdev) {
		qdf_print("%s:vdev is null ", __func__);
		return 0;
	}

	dp_vdev = wlan_get_dp_vdev(vdev);
	if(!dp_vdev) {
		qdf_print("%s:dp_vdev is null ", __func__);
		return 0;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if(!pdev) {
		qdf_print("%s:pdev is null ", __func__);
		return 0;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if(!psoc) {
		qdf_print("%s:psoc is null ", __func__);
		return 0;
	}

	pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);
	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	is_mcast = (dir == IEEE80211_FC1_DIR_DSTODS ||
		    dir == IEEE80211_FC1_DIR_TODS) ?
	    IEEE80211_IS_MULTICAST(IEEE80211_WH4(wh)->i_addr3) :
	    IEEE80211_IS_MULTICAST(wh->i_addr1);

	mac_stats =
	    is_mcast ? &dp_vdev->vdev_multicast_stats : &dp_vdev->
	    vdev_unicast_stats;
	/*
	 * allow all frames with FromDS bit set .
	 * allow Frames with DStoDS if the vap is WDS capable.
	 */
	if (!(dir == IEEE80211_FC1_DIR_FROMDS ||
	      (dir == IEEE80211_FC1_DIR_DSTODS
	       && (wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_WDS))))) {
		if (dir == IEEE80211_FC1_DIR_DSTODS) {
			QDF_TRACE(
				       QDF_MODULE_ID_INPUT,
				       QDF_TRACE_LEVEL_DEBUG,
				       "[%s] Discard 4-address data: WDS not enabled",
				       ether_sprintf(wlan_wbuf_getbssid
						     (vdev, wh)));
			dp_vdev->vdev_stats.is_rx_nowds++;

		} else {
			QDF_TRACE(
				       QDF_MODULE_ID_INPUT,
				       QDF_TRACE_LEVEL_DEBUG,
				       "[%s] Discard data frame: invalid dir 0x%x",
				       ether_sprintf(wlan_wbuf_getbssid
						     (vdev, wh)), dir);
			dp_vdev->vdev_stats.is_rx_wrongdir++;
		}
		mac_stats->ims_rx_discard++;
		return 0;
	}

	/*
	 * In IEEE802.11 network, multicast packet
	 * sent from me is broadcasted from AP.
	 * It should be silently discarded.
	 */
	if (is_mcast && ieee80211_is_mcastecho(vdev, wh)) {
		QDF_TRACE(
			       QDF_MODULE_ID_INPUT,
			       QDF_TRACE_LEVEL_DEBUG,
			       "[%s] Discard data frame: multicast echo dir 0x%x\n",
			       ether_sprintf(wlan_wbuf_getbssid(vdev, wh)),
			       dir);
		dp_vdev->vdev_stats.is_rx_mcastecho++;
		return 0;
	}

	if (is_mcast) {
		/* Report last multicast/broadcast frame to Power Save Module */
		if (!(wh->i_fc[1] & IEEE80211_FC1_MORE_DATA)) {
			if (dp_vdev->iv_txrx_event_info.
			    iv_txrx_event_filter &
			    IEEE80211_VAP_INPUT_EVENT_LAST_MCAST) {
				ieee80211_vap_txrx_event evt;
				OS_MEMZERO(&evt, sizeof(evt));
				/* initialize variable */
				evt.peer = peer;
				evt.type = IEEE80211_VAP_INPUT_EVENT_LAST_MCAST;
				ieee80211_vap_txrx_deliver_event(vdev, &evt);
			}
		}
	} else {
		/* Report unicast frames to Power Save Module */
		if (dp_vdev->iv_txrx_event_info.
		    iv_txrx_event_filter & (IEEE80211_VAP_INPUT_EVENT_UCAST |
					    IEEE80211_VAP_INPUT_EVENT_EOSP)) {
			if (dp_vdev->iv_txrx_event_info.
			    iv_txrx_event_filter &
			    IEEE80211_VAP_INPUT_EVENT_UCAST) {
				ieee80211_vap_txrx_event evt;
				OS_MEMZERO(&evt, sizeof(evt));
				evt.type = IEEE80211_VAP_INPUT_EVENT_UCAST;
				evt.peer = peer;
				evt.wh = wh;
				/* size of more_data is 1 bit */
				if (wh->i_fc[1] & IEEE80211_FC1_MORE_DATA) {
					evt.u.more_data = 1;
				} else {
					evt.u.more_data = 0;
				}
				ieee80211_vap_txrx_deliver_event(vdev, &evt);
			}
			if (dp_vdev->iv_txrx_event_info.
			    iv_txrx_event_filter &
			    IEEE80211_VAP_INPUT_EVENT_EOSP
			    && (subtype == IEEE80211_FC0_SUBTYPE_QOS
				|| subtype == IEEE80211_FC0_SUBTYPE_QOS_NULL)) {
				if (((struct ieee80211_qosframe *)wh)->
				    i_qos[0] & IEEE80211_QOS_EOSP) {
					ieee80211_vap_txrx_event evt;
					evt.type =
					    IEEE80211_VAP_INPUT_EVENT_EOSP;
					evt.peer = peer;
					ieee80211_vap_txrx_deliver_event(vdev,
									 &evt);
				}
			}
		}
	}

	if (dir == IEEE80211_FC1_DIR_DSTODS) {
		struct ieee80211_frame_addr4 *wh4;
		struct wlan_objmgr_peer *bsspeer = wlan_vdev_get_bsspeer(vdev);
		wh4 = (struct ieee80211_frame_addr4 *)wbuf_header(wbuf);
		peer_wds = wlan_find_wds_peer(vdev, wh4->i_addr4);

		if (peer_wds == NULL) {
			/*
			 * In STA mode, add wds entries for hosts behind us, but
			 * not for hosts behind the rootap.
			 */
			if (!IEEE80211_ADDR_EQ
			    (wh4->i_addr2, wlan_peer_get_bssid(bsspeer))) {
				temp_peer =
				    wlan_objmgr_get_peer(psoc,
							 pdev_id, wh4->i_addr4,
							 WLAN_MLME_SB_ID);
				if (temp_peer == NULL) {
					wlan_add_wds_addr(vdev, peer,
							  wh4->i_addr4,
							  IEEE80211_NODE_F_WDS_REMOTE);
				} else
				    if (!IEEE80211_ADDR_EQ
					(wlan_peer_get_macaddr(temp_peer),
					 wlan_vdev_mlme_get_macaddr(vdev))) {
					wlan_objmgr_peer_release_ref(temp_peer,
								     WLAN_MLME_SB_ID);
					wlan_add_wds_addr(vdev, peer,
							  wh4->i_addr4,
							  IEEE80211_NODE_F_WDS_REMOTE);
				}
			}
		} else {
			wlan_remove_wds_addr(vdev, wh4->i_addr4,
					     IEEE80211_NODE_F_WDS_BEHIND |
					     IEEE80211_NODE_F_WDS_REMOTE);
			wlan_objmgr_peer_release_ref(peer_wds, WLAN_MLME_SB_ID);	/* Decr ref count */
		}
	}
	return 1;
}

/*
 * Process received data packet if opmode is IBSS, and return back to
 * dadp_input_data()
 */
int dadp_input_data_ibss(struct wlan_objmgr_peer *peer, wbuf_t wbuf, int dir)
{
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	struct wlan_vdev_mac_stats *mac_stats;
	struct ieee80211_frame *wh;
	int is_mcast;

	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	is_mcast = (dir == IEEE80211_FC1_DIR_DSTODS ||
		    dir == IEEE80211_FC1_DIR_TODS) ?
	    IEEE80211_IS_MULTICAST(IEEE80211_WH4(wh)->i_addr3) :
	    IEEE80211_IS_MULTICAST(wh->i_addr1);

	mac_stats =
	    is_mcast ? &dp_vdev->vdev_multicast_stats : &dp_vdev->
	    vdev_unicast_stats;

	if (dir != IEEE80211_FC1_DIR_NODS) {
		mac_stats->ims_rx_discard++;
		return 0;
	}

	/*
	 * If it is a data frame from a peer, also update the receive
	 * time stamp. This can reduce false beacon miss detection.
	 */
	wlan_peer_set_beacon_rstamp(peer);
	wlan_peer_set_probe_ticks(peer, 0);
	return 1;
}

/*
 * Process received data packet if opmode is WDS, and return back to
 * dadp_input_data()
 */
int dadp_input_data_wds(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf, int dir)
{
	struct ieee80211_frame *wh;
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);

	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	if (dir != IEEE80211_FC1_DIR_DSTODS) {
		QDF_TRACE( QDF_MODULE_ID_ANY,
			       QDF_TRACE_LEVEL_DEBUG,
			       "[%s] Discard data frame: invalid dir 0x%x",
			       ether_sprintf(wlan_wbuf_getbssid(vdev, wh)),
			       dir);
		dp_vdev->vdev_stats.is_rx_wrongdir++;
		return 0;
	}
#ifdef IEEE80211_WDSLEGACY
	if ((vap->iv_flags_ext & IEEE80211_FEXT_WDSLEGACY) == 0) {
		QDF_TRACE( QDF_MODULE_ID_ANY,
			       QDF_TRACE_LEVEL_DEBUG,
			       "[%s] Discard data frame: not legacy wds, flags 0x%x",
			       ether_sprintf(wlan_wbuf_getbssid(vdev, wh)),
			       vap->iv_flags_ext);
		vap->iv_stats.is_rx_nowds++;	/* XXX */
		return 0;
	}
#endif
	return 1;
}

/*
 * Process received packet if opmode is STA, and return back to
 * ieee80211_input()
 */
int dadp_input_sta(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
		   struct ieee80211_rx_status *rs)
{
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	struct wlan_objmgr_peer *bsspeer = wlan_vdev_get_bsspeer(vdev);
	struct dadp_peer *dp_bsspeer = wlan_get_dp_peer(bsspeer);
	struct ath_softc_net80211 *scn = dp_pdev->scn;
	struct ieee80211_frame *wh;
	struct wlan_vdev_mac_stats *mac_stats;
	int type = -1, subtype, dir;
	u_int8_t *bssid;

	UNREFERENCED_PARAMETER(pdev);

	if (!dp_bsspeer) {
		qdf_print("%s:dp_bsspeer is null ", __func__);
		return 0;
	}

	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
	dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;
	mac_stats =
	    IEEE80211_IS_MULTICAST(wh->i_addr1) ? &dp_vdev->
	    vdev_multicast_stats : &dp_vdev->vdev_unicast_stats;

	bssid = wh->i_addr2;
	if (peer != bsspeer && dir != IEEE80211_FC1_DIR_DSTODS)
		bssid = wh->i_addr3;

	if (!IEEE80211_ADDR_EQ(bssid, wlan_peer_get_bssid(peer))) {
		mac_stats->ims_rx_discard++;
		return 0;
	}

	/*
	 * WAR for excessive beacon miss on SoC.
	 * Reset bmiss counter when we receive a non-probe request
	 * frame from our home AP, and save the time stamp.
	 */
	if ((wlan_vdev_is_up(vdev) == QDF_STATUS_SUCCESS) &&
	    (!((type == IEEE80211_FC0_TYPE_MGT) &&
	       (subtype == IEEE80211_FC0_SUBTYPE_PROBE_REQ))) &&
	    (wlan_vdev_get_lastbcn_phymode_mismatch(vdev) == 0)) {

		if (wlan_vdev_get_bmiss_count(vdev) > 0) {
#ifdef ATH_SWRETRY
			/* Turning on the sw retry mechanism. This should not
			 * produce issues even if we are in the middle of
			 * cleaning sw retried frames
			 */
			scn->sc_ops->set_swretrystate(scn->sc_dev,
						      (dp_bsspeer->an), TRUE);
#endif
			QDF_TRACE( QDF_MODULE_ID_SCAN,
				       QDF_TRACE_LEVEL_DEBUG,
				       "clear beacon miss. frm type=%02x, subtype=%02x\n",
				       type, subtype);
			wlan_vdev_reset_bmiss(vdev);
		}

		/*
		 * Beacon timestamp will be set when beacon is processed.
		 * Set directed frame timestamp if frame is not multicast or
		 * broadcast.
		 */
		if (!IEEE80211_IS_MULTICAST(wh->i_addr1)
#if ATH_SUPPORT_WRAP
		    && IEEE80211_ADDR_EQ(wh->i_addr1,
					 wlan_vdev_mlme_get_macaddr(vdev))
#endif
		    ) {
			wlan_vdev_set_last_directed_frame(vdev);
		}
	}
	return 1;
}

/*
 * Process received packet if opmode is IBSS, and return back to
 * ieee80211_input()
 */
int dadp_input_ibss(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
		    struct ieee80211_rx_status *rs)
{
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	struct wlan_vdev_mac_stats *mac_stats;
	struct wlan_objmgr_peer *bsspeer = wlan_vdev_get_bsspeer(vdev);
	struct ieee80211_frame *wh;
	int type = -1, subtype, dir;
	u_int8_t *bssid;

	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
	dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;
	mac_stats =
	    IEEE80211_IS_MULTICAST(wh->i_addr1) ? &dp_vdev->
	    vdev_multicast_stats : &dp_vdev->vdev_unicast_stats;

	if (dir != IEEE80211_FC1_DIR_NODS)
		bssid = wh->i_addr1;
	else if (type == IEEE80211_FC0_TYPE_CTL)
		bssid = wh->i_addr1;
	else {
		if (wbuf_get_pktlen(wbuf) < sizeof(struct ieee80211_frame)) {
			mac_stats->ims_rx_discard++;
			return 0;
		}
		bssid = wh->i_addr3;
	}

	if (type != IEEE80211_FC0_TYPE_DATA)
		return 1;

	/*
	 * Data frame, validate the bssid.
	 */
	if (!IEEE80211_ADDR_EQ(bssid, wlan_peer_get_bssid(bsspeer)) &&
	    !IEEE80211_ADDR_EQ(bssid, dp_pdev->pdev_broadcast) &&
	    subtype != IEEE80211_FC0_SUBTYPE_BEACON) {
		/* not interested in */
		mac_stats->ims_rx_discard++;
		return 0;
	}
	return 1;
}

/*
 * Process received packet if opmode is WDS, and return back to
 * ieee80211_input()
 */
int dadp_input_wds(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
		   struct ieee80211_rx_status *rs)
{
	struct ieee80211_frame *wh;
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	struct wlan_objmgr_peer *bsspeer = wlan_vdev_get_bsspeer(vdev);
	struct wlan_stats *vdev_stats;
	int type = -1, subtype;
	u_int8_t *bssid;

	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
	vdev_stats = &dp_vdev->vdev_stats;

	if (wbuf_get_pktlen(wbuf) < sizeof(struct ieee80211_frame_addr4)) {
		if ((type != IEEE80211_FC0_TYPE_MGT) &&
		    (subtype != IEEE80211_FC0_SUBTYPE_DEAUTH)) {
			vdev_stats->is_rx_tooshort++;
			return 0;
		}
	}
	bssid = wh->i_addr1;
	if (!IEEE80211_ADDR_EQ(bssid, wlan_peer_get_bssid(bsspeer)) &&
	    !IEEE80211_ADDR_EQ(bssid, dp_pdev->pdev_broadcast)) {
		/* not interested in */
		QDF_TRACE( QDF_MODULE_ID_INPUT,
			       QDF_TRACE_LEVEL_DEBUG,
			       "*** WDS *** WDS ADDR1 %s, BSSID %s\n",
			       ether_sprintf(bssid),
			       ether_sprintf(wlan_peer_get_bssid(bsspeer)));
		vdev_stats->is_rx_wrongbss++;
		return 0;
	}
	return 1;
}

static INLINE void
ieee80211_input_monitor_iter_func(struct wlan_objmgr_pdev *pdev, void *obj,
				  void *arg)
{
	struct wlan_iter_input_all_arg *params =
	    (struct wlan_iter_input_all_arg *)arg;
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)obj;
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);

	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		return;
	}

	if (!params->wbuf)
		return;

	/*
	 * deliver the frame to the os.
	 */
	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_MONITOR_MODE
	    && (wlan_vdev_is_up(vdev) == QDF_STATUS_SUCCESS)) {
		/* remove padding from header */
		u_int8_t *header = (u_int8_t *) wbuf_header(params->wbuf);
		u_int32_t hdrspace = ieee80211_anyhdrspace(pdev, header);
		u_int32_t realhdrsize = ieee80211_anyhdrsize(header);	/* no _F_DATAPAD */
		u_int32_t padsize = hdrspace - realhdrsize;

		if (padsize > 0) {
			memmove(header + padsize, header, realhdrsize);
			wbuf_pull(params->wbuf, padsize);
		}

		if (dp_vdev->vdev_evtable &&
		    dp_vdev->vdev_evtable->wlan_receive_monitor_80211)
			dp_vdev->vdev_evtable->
		    wlan_receive_monitor_80211(wlan_vdev_get_osifp(vdev),
					       params->wbuf, params->rs);

		/* For now, only allow one vap to be monitoring */
		params->wbuf = NULL;
	}
}

/*
 * this should be called only if there exists a monitor mode vap.
 */
int
dadp_input_monitor(struct wlan_objmgr_pdev *pdev, wbuf_t wbuf,
		   struct ieee80211_rx_status *rs)
{
	struct wlan_iter_input_all_arg params;

	params.wbuf = wbuf;
	params.rs = rs;
	params.type = -1;

	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_VDEV_OP,
					  ieee80211_input_monitor_iter_func,
					  (void *)&params, 1, WLAN_MLME_SB_ID);

	if (params.wbuf)
		wbuf_free(params.wbuf);
	return 0;
}

#endif /* ! UMAC_SUPPORT_OPMODE_APONLY */

#if ATH_SUPPORT_IWSPY
int wlan_input_iwspy_update_rssi(struct wlan_objmgr_pdev *pdev,
				 u_int8_t * address, int8_t rssi)
{
	struct wlan_objmgr_vdev *vdev = NULL;
	struct dadp_vdev *dp_vdev = NULL;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, 0, WLAN_MLME_SB_ID);
	if (!vdev) {
		qdf_print("%s:Failed to get vdev", __func__);
		return QDF_STATUS_E_INVAL;
	}

	dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
		return QDF_STATUS_E_INVAL;
	}

	if (dp_vdev->vdev_evtable->wlan_iwspy_update)
		dp_vdev->vdev_evtable->
		    wlan_iwspy_update(wlan_vdev_get_osifp(vdev), address, rssi);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);

	return 0;
}
#endif

#if !UMAC_SUPPORT_OPMODE_APONLY
void
ath_net80211_rx_monitor(struct wlan_objmgr_pdev *pdev, wbuf_t wbuf,
			ieee80211_rx_status_t * rx_status)
{
	struct ath_softc_net80211 *scn;
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	scn = dp_pdev->scn;
	/*
	 * Monitor mode: discard anything shorter than
	 * an ack or cts, clean the skbuff, fabricate
	 * the Prism header existing tools expect,
	 * and dispatch.
	 */
	if (wbuf_get_pktlen(wbuf) < IEEE80211_ACK_LEN) {
		QDF_TRACE( QDF_MODULE_ID_OUTPUT,
			       QDF_TRACE_LEVEL_DEBUG, "%s: runt packet %d\n",
			       __func__, wbuf_get_pktlen(wbuf));
		wbuf_free(wbuf);
	} else {
		struct ieee80211_rx_status rs;
		ATH_RXSTAT2IEEE(scn, rx_status, &rs);
		dadp_input_monitor(pdev, wbuf, &rs);
	}
	return;
}
#endif /* ! UMAC_SUPPORT_OPMODE_APONLY */

struct wlan_objmgr_peer *wlan_find_rxpeer(struct wlan_objmgr_pdev *pdev,
					  struct ieee80211_frame_min *wh)
{
	struct wlan_objmgr_psoc *psoc = NULL;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		qdf_print("%s:psoc is NULL ", __func__);
		return NULL;
	}
#define	IS_CTL(wh)  \
    ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_CTL)
#define	IS_PSPOLL(wh)   \
    ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) == IEEE80211_FC0_SUBTYPE_PS_POLL)
#define	IS_BAR(wh) \
    ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) == IEEE80211_FC0_SUBTYPE_BAR)
	if (IS_CTL(wh) && !IS_PSPOLL(wh) && !IS_BAR(wh))
		return wlan_objmgr_get_peer(psoc, wlan_objmgr_pdev_get_pdev_id(pdev),
					    wh->i_addr1,
					    WLAN_MLME_SB_ID);
#if ATH_SUPPORT_WRAP
	return wlan_objmgr_get_peer_by_mac_n_vdev(psoc,
						  wlan_objmgr_pdev_get_pdev_id(pdev),
						  wh->i_addr1,
						  wh->i_addr2,
						  WLAN_MLME_SB_ID);
#else
	return wlan_objmgr_get_peer(psoc, wlan_objmgr_pdev_get_pdev_id(pdev),
				    wh->i_addr2, WLAN_MLME_SB_ID);
#endif

#undef IS_BAR
#undef IS_PSPOLL
#undef IS_CTL
}

#if ATH_SUPPORT_WRAP

int
wrap_psta_input_multicast(struct ath_softc_net80211 *scn, wbuf_t wbuf,
			  ieee80211_rx_status_t * rx_status)
{
	struct wlan_objmgr_peer *peer;
	struct dadp_peer *dp_peer = NULL;
	ATH_RX_TYPE status;
	int type;
	struct ieee80211_frame *wh =
	    (struct ieee80211_frame *)wbuf_header(wbuf);

	ASSERT(scn->sc_mcast_recv_vdev);
	ASSERT(wbuf);

	peer = wlan_vdev_get_bsspeer(scn->sc_mcast_recv_vdev);

	dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer) {
		qdf_print("%s:dp_peer is NULL ", __func__);
		return -EINVAL;
	}
	/*
	 * Let ath_dev do some special rx frame processing. If the frame is not
	 * consumed by ath_dev, indicate it up to the stack.
	 */
	type = scn->sc_ops->rx_proc_frame(scn->sc_dev, dp_peer->an,
					  IEEE80211_PEER_ISAMPDU(peer),
					  wbuf, rx_status, &status);

	/* For OWL specific HW bug, 4addr aggr needs to be denied in
	 * some cases. So check for delba send and send delba
	 */
	if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS) {
		if (IEEE80211_PEER_WDSWAR_ISSENDDELBA(peer)) {
			struct ieee80211_qosframe_addr4 *whqos_4addr;
			int tid;

			whqos_4addr = (struct ieee80211_qosframe_addr4 *)wh;
			tid = whqos_4addr->i_qos[0] & IEEE80211_QOS_TID;
			wlan_send_delba(peer, tid, 0,
					IEEE80211_REASON_UNSPECIFIED);
		}
	}

	if (status != ATH_RX_CONSUMED) {
		/*
		 * Not consumed by ath_dev for out-of-order delivery,
		 * indicate up the stack now.
		 */
		struct ieee80211_node *ni = wlan_mlme_get_peer_ext_hdl(peer);
		type = ath_net80211_input(ni, wbuf, rx_status);
	}

	return type;
}
#endif

int
ath_net80211_rx(struct wlan_objmgr_pdev *pdev, wbuf_t wbuf,
		ieee80211_rx_status_t * rx_status, u_int16_t keyix)
{
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	struct ath_softc_net80211 *scn = NULL;
	struct wlan_objmgr_peer *peer = NULL;
#if ATH_SUPPORT_WRAP
	struct wlan_objmgr_vdev *vdev = NULL;
#endif
	struct dadp_peer *dp_peer = NULL;
	struct ieee80211_frame *wh;
	int type;
	ATH_RX_TYPE status;
	struct ieee80211_qosframe_addr4 *whqos_4addr;
	int tid;
	int frame_type, frame_subtype;

#if USE_MULTIPLE_BUFFER_RCV
	wbuf_t wbuf_last;
#endif
	struct wlan_objmgr_psoc *psoc = NULL;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		qdf_print("%s:psoc is NULL ", __func__);
		return -1;
	}

	if (!dp_pdev) {
		qdf_print("%s:dp_pdev is null ", __func__);
		return -1;
	}
	scn = dp_pdev->scn;

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
#if ATH_SUPPORT_IWSPY
	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	if (rx_status->flags & ATH_RX_RSSI_VALID) {
		wlan_input_iwspy_update_rssi(pdev, wh->i_addr2,
					     rx_status->rssi);
	}
#endif
	/*
	 * From this point on we assume the frame is at least
	 * as large as ieee80211_frame_min; verify that.
	 */
	if (wbuf_get_pktlen(wbuf) <
	    (dp_pdev->pdev_minframesize + IEEE80211_CRC_LEN)) {
		QDF_TRACE( QDF_MODULE_ID_OUTPUT,
			       QDF_TRACE_LEVEL_DEBUG, "%s: short packet %d\n",
			       __func__, wbuf_get_pktlen(wbuf));
		wbuf_free(wbuf);
		return -1;
	}
#ifdef ATH_SUPPORT_TxBF
	ath_net80211_bf_rx(pdev, wbuf, rx_status);
#endif
	/*
	 * Normal receive.
	 */
#if USE_MULTIPLE_BUFFER_RCV
	/* the CRC is at the end of the rx buf chain */
	wbuf_last = wbuf;
	while (wbuf_next(wbuf_last) != NULL)
		wbuf_last = wbuf_next(wbuf_last);
	wbuf_trim(wbuf_last, IEEE80211_CRC_LEN);
#else
	wbuf_trim(wbuf, IEEE80211_CRC_LEN);
#endif

	if (CHK_SC_DEBUG_SCN(scn, ATH_DEBUG_RECV)) {
		wh = (struct ieee80211_frame *)wbuf_header(wbuf);
		if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
		    IEEE80211_FC0_TYPE_DATA)
			ieee80211_dump_pkt(pdev, wbuf_header(wbuf),
					   wbuf_get_pktlen(wbuf) +
					   IEEE80211_CRC_LEN,
					   rx_status->rateKbps,
					   rx_status->rssi);
	}
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	/* TBD_DADP: Handle wep mbssid keymiss */
#else
	/*
	 * Handle packets with keycache miss if WEP on MBSSID
	 * is enabled.
	 */
	{
		struct ieee80211_rx_status rs;
		ATH_RXSTAT2IEEE(scn, rx_status, &rs);

		if (ieee80211_crypto_handle_keymiss(pdev, wbuf, &rs))
			return -1;
	}
#endif

	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	frame_type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	frame_subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

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
#define	IS_CTL(wh)  \
    ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_CTL)
#define	IS_PSPOLL(wh)   \
    ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) == IEEE80211_FC0_SUBTYPE_PS_POLL)
#define	IS_BAR(wh) \
    ((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) == IEEE80211_FC0_SUBTYPE_BAR)
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
	if (peer == NULL) {
		IEEE80211_KEYMAP_UNLOCK(scn);
		/*
		 * No key index or no entry, do a lookup and
		 * add the node to the mapping table if possible.
		 */
		peer = wlan_find_rxpeer(pdev, (struct ieee80211_frame_min *)
					wbuf_header(wbuf));
		if (peer == NULL) {
			struct ieee80211_rx_status rs;

#if ATH_SUPPORT_WRAP
			struct ath_softc *sc = scn->sc_dev;

			wh = (struct ieee80211_frame *)wbuf_header(wbuf);
			type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;

			/*
			 * If we are in ProxySTA (promiscuous) mode, we need to filter
			 * out not interested frames as early as possible. We just let
			 * broadcast/multicast frames go through - either data frames
			 * manage frames (beacon/probe request/probe response/etc). We
			 * also allow unicast class 2 frames (i.e Authentication) pass
			 * because node might not be allocated at this point. We assume
			 * each VAP will check A2 matches bssid.
			 */
			if (sc->sc_enableproxysta) {
				int drop_frame = 0;

				if (!IEEE80211_IS_MULTICAST(wh->i_addr1) &&
				    (type == IEEE80211_FC0_TYPE_MGT ||
				     type == IEEE80211_FC0_TYPE_CTL)) {
					int subtype =
					    wh->
					    i_fc[0] &
					    IEEE80211_FC0_SUBTYPE_MASK;
					struct wlan_objmgr_peer *peer1 =
					    wlan_objmgr_get_peer(psoc,
							         wlan_objmgr_pdev_get_pdev_id(pdev),
								 wh->i_addr1,
								 WLAN_MLME_SB_ID);
					struct wlan_objmgr_vdev *vdev1 = NULL;

					if (!peer1) {
						qdf_print("%s:peer1 is null ", __func__);
						wbuf_free(wbuf);
						return -1;
					}
					vdev1 = wlan_peer_get_vdev(peer1);

					/*
					 * Drop unicast management frames not directing
					 * to one of our AP BSS's except probe response.
					 */
					if ((subtype !=
					     IEEE80211_FC0_SUBTYPE_PROBE_RESP)
					    && (peer1 !=
						wlan_vdev_get_bsspeer(vdev1)
						||
						wlan_vdev_mlme_get_opmode(vdev1)
						!= QDF_SAP_MODE)) {
						drop_frame = 1;
					}
						wlan_objmgr_peer_release_ref
						    (peer1, WLAN_MLME_SB_ID);
				}
				if (drop_frame ||
				    (!IEEE80211_IS_MULTICAST(wh->i_addr1) &&
				     type == IEEE80211_FC0_TYPE_DATA)) {
					wbuf_free(wbuf);
					return -1;
				}
			}
#endif

			ATH_RXSTAT2IEEE(scn, rx_status, &rs);

#if ATH_SUPPORT_WRAP
			if (scn->sc_mcast_recv_vdev &&
	                    (wlan_vdev_chan_config_valid(scn->sc_mcast_recv_vdev)
                                                     == QDF_STATUS_SUCCESS) &&
			    IEEE80211_IS_MULTICAST(wh->i_addr1) &&
			    type == IEEE80211_FC0_TYPE_DATA) {
				return wrap_psta_input_multicast(scn, wbuf,
								 rx_status);
			} else
#endif
				return dadp_input_all(pdev, wbuf, &rs);
		}
	} else {
#if ATH_SUPPORT_WRAP
		vdev = wlan_peer_get_vdev(peer);
		KASSERT(!dadp_vdev_is_wrap(vdev)
			|| peer != wlan_vdev_get_bsspeer(vdev),
			("find ni %s for WRAP VAP from sc_keyixmap[%d] directly",
			 ether_sprintf(wlan_peer_get_macaddr(peer)), keyix));
#endif
		if (wlan_objmgr_peer_try_get_ref(peer, WLAN_MLME_SB_ID) != QDF_STATUS_SUCCESS) {
			IEEE80211_KEYMAP_UNLOCK(scn);
			wbuf_free(wbuf);
			return -EINVAL;
		}
		IEEE80211_KEYMAP_UNLOCK(scn);
	}

	dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer) {
		qdf_print("%s:dp_peer is NULL ", __func__);
		wlan_objmgr_peer_release_ref(peer, WLAN_MLME_SB_ID);
		wbuf_free(wbuf);
		return -EINVAL;
	}
	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	/*
	 * Let ath_dev do some special rx frame processing. If the frame is not
	 * consumed by ath_dev, indicate it up to the stack.
	 */
	type = scn->sc_ops->rx_proc_frame(scn->sc_dev, dp_peer->an,
					  IEEE80211_PEER_ISAMPDU(peer),
					  wbuf, rx_status, &status);

	/* For OWL specific HW bug, 4addr aggr needs to be denied in
	 * some cases. So check for delba send and send delba
	 */
	if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS) {
		if (IEEE80211_NODE_WDSWAR_ISSENDDELBA(peer)) {
			whqos_4addr = (struct ieee80211_qosframe_addr4 *)wh;
			tid = whqos_4addr->i_qos[0] & IEEE80211_QOS_TID;
			wlan_send_delba(peer, tid, 0,
					IEEE80211_REASON_UNSPECIFIED);
		}
	}

	if (status != ATH_RX_CONSUMED) {
		/*
		 * Not consumed by ath_dev for out-of-order delivery,
		 * indicate up the stack now.
		 */
		struct ieee80211_node *ni = wlan_mlme_get_peer_ext_hdl(peer);
		type = ath_net80211_input(ni, wbuf, rx_status);
	}

	wlan_objmgr_peer_release_ref(peer, WLAN_MLME_SB_ID);
	return type;
}

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

#include <da_output.h>
#include <da_frag.h>
#include <da_dp.h>
#include <da_amsdu.h>
#include <da_uapsd.h>
#include <if_ath_htc.h>
#include <wlan_utility.h>
#include "osif_private.h"

void wlan_complete_wbuf(wbuf_t wbuf, struct ieee80211_tx_status *ts);
void dadp_vdev_deliver_tx_event(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
				struct ieee80211_frame *wh,
				struct ieee80211_tx_status *ts);

#define IEEE80211_PEER_F_WDS_BEHIND   0x00001
#define IEEE80211_PEER_F_WDS_REMOTE   0x00002

#if ATH_DEBUG
extern unsigned long ath_rtscts_enable;	/* defined in ah_osdep.c  */
#endif

static INLINE int wlan_is_smps_set(struct dadp_vdev *dp_vdev)
{
	return (dp_vdev->vdev_smps == 1);
}

#define WLAN_GET_BCAST_ADDR(_dp_pdev)                ((_dp_pdev)->pdev_broadcast)

wbuf_t __ieee80211_encap(struct wlan_objmgr_peer * peer, wbuf_t wbuf)
{
	struct wlan_objmgr_vdev *vdev = NULL;

	vdev = wlan_peer_get_vdev(peer);
	if (!vdev) {
		qdf_print("%s:vdev is NULL ", __func__);
		return NULL;
	}
#ifndef QCA_PARTNER_PLATFORM
	if (wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_SEND_80211)) {
#else
	if (wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_SEND_80211)
	    || wbuf_is_encap_done(wbuf)) {
#endif /*QCA_PARTNER_PLATFORM */
		struct ieee80211_frame *wh;
		wh = (struct ieee80211_frame *)wbuf_header(wbuf);
		if (wbuf_is_moredata(wbuf)) {
			wh->i_fc[1] |= IEEE80211_FC1_MORE_DATA;
		}
		return ieee80211_encap_80211(peer, wbuf);
	} else {
		return ieee80211_encap_8023(peer, wbuf);
	}
}

os_if_t wlan_vdev_get_osifp(struct wlan_objmgr_vdev * vdev)
{
	struct vdev_osif_priv *vdev_osifp = NULL;
	osif_dev *osifp;

	vdev_osifp = wlan_vdev_get_ospriv(vdev);
	osifp = (osif_dev *) vdev_osifp->legacy_osif_priv;

	return (os_if_t) osifp;
}

static INLINE void
wlan_uapsd_pwrsave_check(wbuf_t wbuf, struct wlan_objmgr_peer *peer)
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

		if (IEEE80211_PEER_UAPSD_USETIM(dp_peer))
			VDEV_SET_TIM(peer, 1, false);
	}
}

struct wlan_objmgr_peer *wlan_find_peer(struct wlan_objmgr_vdev *vdev,
					u_int8_t * macaddr)
{
	struct wlan_objmgr_psoc *psoc = NULL;
	struct wlan_objmgr_peer *bsspeer = NULL, *peer = NULL;
	struct dadp_vdev *dp_vdev;
	uint8_t pdev_id;

	if (!vdev) {
		qdf_print("%s:vdev is NULL", __func__);
		return NULL;
	}

	dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		return NULL;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		qdf_print("%s:psoc is NULL ", __func__);
		return NULL;
	}

	pdev_id = wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(vdev));
	bsspeer = wlan_vdev_get_bsspeer(vdev);

	if (bsspeer) {
		if (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE ||
		    wlan_vdev_mlme_get_opmode(vdev) == QDF_WDS_MODE) {
			wlan_objmgr_peer_try_get_ref(bsspeer, WLAN_MLME_SB_ID);
			peer = bsspeer;
		} else if (IEEE80211_IS_MULTICAST(macaddr)) {
			if (wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE) {
				if (wlan_vdev_get_sta_assoc(vdev) > 0) {
					wlan_objmgr_peer_try_get_ref(bsspeer,
								     WLAN_MLME_SB_ID);
					peer = bsspeer;
				} else {
					/* No station associated to AP */
					dp_vdev->vdev_stats.is_tx_nonode++;
					peer = NULL;
				}
			} else {
				wlan_objmgr_peer_try_get_ref(bsspeer,
							     WLAN_MLME_SB_ID);
				peer = bsspeer;
			}
		} else {
			peer =
			    wlan_objmgr_get_peer(psoc, pdev_id, macaddr,
						 WLAN_MLME_SB_ID);
		}
	}

	return peer;
}

struct wlan_objmgr_peer *wlan_find_txpeer(struct wlan_objmgr_vdev *vdev,
					  u_int8_t * macaddr)
{
	struct wlan_objmgr_peer *peer;

	peer = wlan_find_peer(vdev, macaddr);

	if (peer == NULL) {
		if ((wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE)
		    && wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_WDS))
			peer = wlan_find_wds_peer(vdev, macaddr);
	}

	/*
	 * Since all vaps share the same node table, we may find someone else's
	 * node (sigh!).
	 */
	if (peer && wlan_peer_get_vdev(peer) != vdev) {
		wlan_objmgr_peer_release_ref(peer, WLAN_MLME_SB_ID);
		return NULL;
	}

	return peer;
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

	if (is_igmp && dp_pdev->pdev_override_igmp_dscp) {
		tid = dp_pdev->pdev_dscp_igmp_tid;
	} else if (iph && dp_pdev->pdev_override_hmmc_dscp &&
		   dp_vdev->ique_ops.wlan_me_hmmc_find &&
		   dp_vdev->ique_ops.wlan_me_hmmc_find(vdev,
						       be32toh(iph->daddr))) {
		tid = dp_pdev->pdev_dscp_hmmc_tid;
	} else if (dp_vdev->vdev_dscp_map_id) {
		if (wbuf_mark_eapol(wbuf))
			tid = OSDEP_EAPOL_TID;
		else
			tid =
			    dp_pdev->pdev_dscp_tid_map[dp_vdev->
						       vdev_dscp_map_id][tos >>
									 IP_DSCP_SHIFT]
			    & 0x7;
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

int ieee80211_classify(struct wlan_objmgr_peer *peer, wbuf_t wbuf)
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
	if (wbuf_is_eapol(wbuf))
		tid = TX_QUEUE_FOR_EAPOL_FRAME;
	else {
		if ((tid = ieee80211_dscp_override(vdev, wbuf)) < 0)
			tid =
			    wbuf_classify(wbuf,
					  dp_pdev->
					  pdev_tid_override_queue_mapping) &
			    0x7;
	}
	ac = TID_TO_WME_AC(tid);

	/* default priority */
	if (!(wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_QOS))) {
		wbuf_set_priority(wbuf, WME_AC_BE);
		wbuf_set_tid(wbuf, 0);
		return 0;
	}

	/* Check SmartNet feature. Only support Windows and STA mode from now on */
	if (wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_SEND_80211) &&
	    (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE)) {
		/*
		 ** Determine the SMARTNET AC
		 */
		if (dp_vdev->vdev_smartnet_enable) {
			int wbuf_ac;
			wbuf_ac = wbuf_UserPriority(wbuf);
			/* For first stage SmartNet, UserPriority <1 means low priority, using BK. */
			if (wbuf_ac < 1 && ac == 0) {
				ac = WME_AC_BK;
				tid = WME_AC_TO_TID(ac);
			}
			wbuf_SetWMMInfo(wbuf, tid);
		}
	}
#if ATH_SUPPORT_VLAN
	/*
	 ** If this is a QoS node (set after the above comparison, and there is a
	 ** VLAN tag associated with the packet, we need to ensure we set the
	 ** priority correctly for the VLAN
	 */

	if (qdf_net_vlan_tag_present(wbuf)) {
		unsigned short tag;
		unsigned short vlanID;
		osdev_t osifp = (osdev_t) wlan_vdev_get_osifp(vdev);

		vlanID = qdf_net_get_vlan(osifp);
#ifdef QCA_PARTNER_PLATFORM
		if (ath_pltfrm_vlan_tag_check(vdev, wbuf))
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

	/* Applying ACM policy */
	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE) {
		int initial_ac = ac;

		while ((ac != WME_AC_BK)
		       && wlan_pdev_get_wmep_acm(pdev, ac)
		       && wlan_pdev_get_wmep_acm(pdev, ac)
		       && !dp_vdev->vdev_tspecActive) {
			/* If admission control is mandatory (true) but tspec is not active,
			 * go down to the next lower level that doesn't have acm
			 */
			switch (ac) {
			case WME_AC_VI:
				ac = WME_AC_BE;
				break;
			case WME_AC_VO:
				ac = WME_AC_VI;
				break;
				/*
				 * The default case handles ac = WME_AC_BE as well
				 * as AC's other than WME_AC_VI & WME_AC_VO. Currently
				 * only four AC's (VI, VO, BE, & BK)  are defined.
				 * For explicit handling of any other AC's (defined
				 * in future), case statement needs to be expanded.
				 */
			default:
				ac = WME_AC_BK;
				break;
			}
		}
		if (initial_ac != ac) {
			/* Recalculate the new TID */
			tid = WME_AC_TO_TID(ac);
		}
	}

	wlan_admctl_classify(vdev, peer, &tid, &ac);

	wbuf_set_priority(wbuf, ac);
	wbuf_set_tid(wbuf, tid);

	return 0;
}

int osif_vap_hardstart_generic(struct sk_buff *skb, struct net_device *dev)
{
	osif_dev *osdev = ath_netdev_priv(dev);
	struct wlan_objmgr_vdev *vdev = osdev->ctrl_vdev;
	struct net_device *comdev = osdev->os_comdev;
	struct ether_header *eh = (struct ether_header *)skb->data;
	int send_err = 0;
	struct dadp_vdev *dp_vdev = NULL;

	if (!vdev) {
		qdf_print("%s:vdev is NULL ", __func__);
		goto bad1;
	}

	dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		goto bad1;
	}

	nbuf_debug_add_record(skb);
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

#if DBDC_REPEATER_SUPPORT
	if (wlan_dbdc_tx_process(vdev, &osdev, skb)) {
		spin_unlock(&osdev->tx_lock);
		return 0;
	}
#endif
	/* Raw mode or native wifi mode not
	 * supported in qwrap , revisit later
	 */
	DA_WRAP_TX_PROCESS(&osdev, vdev, &skb);

	if (ADP_EXT_AP_TX_PROCESS(vdev, &skb, 0)) {
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

	/*
	 * Find the node for the destination so we can do
	 * things like power save.
	 */
	eh = (struct ether_header *)skb->data;

	N_FLAG_KEEP_ONLY(skb, N_PWR_SAV);
#if UMAC_SUPPORT_WNM
	if (wlan_vdev_wnm_tfs_filter(vdev, (wbuf_t) skb)) {
		goto bad;
	}
#endif
#if UMAC_SUPPORT_PROXY_ARP
	if (wlan_do_proxy_arp(vdev, skb))
		goto bad;
#endif /* UMAC_SUPPORT_PROXY_ARP */
	send_err = wlan_vap_send(vdev, (wbuf_t) skb);
#if ATH_SUPPORT_FLOWMAC_MODULE
	if (send_err == -ENOBUFS && dp_vdev->vdev_flowmac) {
		/* pause the Ethernet and the queues as well */
		if (!(dp_vdev->vdev_dev_stopped)) {
			if (dp_vdev->vdev_evtable->wlan_pause_queue) {
				dp_vdev->vdev_evtable->
				    wlan_pause_queue(wlan_vdev_get_osifp(vdev),
						     1, dp_vdev->vdev_flowmac);
				dp_vdev->vdev_dev_stopped = 1;
			}
		}
	}
#endif
	spin_unlock(&osdev->tx_lock);

	return 0;

bad:
	dp_vdev->vdev_stats.is_tx_nobuf++;
	spin_unlock(&osdev->tx_lock);
bad1:
	if (skb != NULL)
		wbuf_free(skb);
	return 0;
}

/*
 * the main xmit data entry point from OS
 */
int wlan_vap_send(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf)
{
	struct wlan_objmgr_peer *peer;
	u_int8_t *daddr;
	int is_data, retval;
	struct dadp_vdev *dp_vdev = NULL;
	struct dadp_peer *dp_peer = NULL;

	if (!vdev) {
		qdf_print("%s:vdev is NULL ", __func__);
		goto free_wbuf;
	}

	dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		goto free_wbuf;
	}

	/*
	 * Initialize Completion function to NULL
	 */
	wbuf_set_complete_handler(wbuf, NULL, NULL);

	/*
	 * Find the node for the destination so we can do
	 * things like power save and fast frames aggregation.
	 */
	if (wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_SEND_80211)) {
		struct ieee80211_frame *wh;
		int type;

		/*
		 * WDS mode ?
		 */
		wh = (struct ieee80211_frame *)wbuf_header(wbuf);
		daddr = wh->i_addr1;

		/*
		 * Vista sometimes sends management frames from the stack,
		 * so we need to determine if it's a data frame here.
		 */
		type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
		is_data = (type == IEEE80211_FC0_TYPE_DATA) ? 1 : 0;
	} else {
		struct ether_header *eh;

		eh = (struct ether_header *)wbuf_header(wbuf);
		daddr = eh->ether_dhost;
		is_data = 1;	/* ethernet frame */

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
		    ((wlan_vdev_get_sta_assoc(vdev)) > 0) &&
		    !IEEE80211_IS_BROADCAST(eh->ether_dhost) &&
		    (wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE)) {
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
			if (dp_vdev->me_hifi_enable
			    && dp_vdev->ique_ops.wlan_me_hifi_convert) {
				if (!dp_vdev->ique_ops.
				    wlan_me_hifi_convert(vdev, wbuf))
					return 0;
			} else
#endif
			{
#if ATH_SUPPORT_ME_SNOOP_TABLE
				/*
				 * if the convert function returns some value larger
				 * than 0, it means that one or more frames have been
				 * transmitted and we are safe to return from here.
				 */
				if (dp_vdev->ique_ops.
				    wlan_me_convert(vdev, wbuf) > 0) {
					return 0;
				}
#endif
			}
		}
	}

	peer = wlan_find_txpeer(vdev, daddr);
	if (peer == NULL) {
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
	if (ieee80211_classify(peer, wbuf)) {
		QDF_TRACE( QDF_MODULE_ID_OUTPUT,
			       QDF_TRACE_LEVEL_DEBUG,
			       "%s: discard, classification failure (%s)\n",
			       __func__, ether_sprintf(daddr));
		goto bad;
	}

	if (is_data) {
		/* No data frames go out unless we're running. */
		if (wlan_vdev_is_up(vdev) != QDF_STATUS_SUCCESS) {
			QDF_TRACE(
				       QDF_MODULE_ID_OUTPUT,
				       QDF_TRACE_LEVEL_DEBUG,
				       "%s: ignore data packet, vdev is not active\n",
				       __func__);
			goto bad;
		}
		/* function returns 1 if frame is to be fwded
		 * and 0 if the frame can be dropped
		 * when stubbed returns 1
		 */
		if (!ieee80211_apclient_fwd(vdev, wbuf)) {
			QDF_TRACE(
				       QDF_MODULE_ID_OUTPUT,
				       QDF_TRACE_LEVEL_DEBUG,
				       "%s: drop data packet, vdev is in sta_fwd\n",
				       __func__);
			goto bad;
		}

#ifdef IEEE80211_WDS
		if (IEEE80211_IS_MULTICAST(daddr)) {
			if ((wlan_vdev_mlme_get_opmode(vdev) == QDF_WDS_MODE) &&
			    (!wlan_vdev_mlme_feat_cap_get
			     (vdev, WLAN_VDEV_F_WDS_STATIC))) {
				QDF_TRACE(
					       QDF_MODULE_ID_WDS,
					       QDF_TRACE_LEVEL_DEBUG,
					       "%s: drop multicast frame on WDS extender; destined for %s\n",
					       __func__, ether_sprintf(daddr));
				wbuf_complete(wbuf);
				return 0;
			}

			if (wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE) {
				/*
				 *      ieee80211_internal_send_dwds_multicast(vdev, m);
				 */
			}
		}
#endif

		if (wlan_get_aid(peer) == 0 &&
		    (wlan_vdev_mlme_get_opmode(vdev) != QDF_IBSS_MODE)) {
			/*
			 * Destination is not associated; must special
			 * case WDS where we point iv_bss at the node
			 * for the associated station.
			 */
			if (peer != wlan_vdev_get_bsspeer(vdev) ||
			    wlan_vdev_mlme_get_opmode(vdev) == QDF_WDS_MODE) {
				QDF_TRACE(
					       QDF_MODULE_ID_OUTPUT,
					       QDF_TRACE_LEVEL_DEBUG,
					       "%s: could not send packet, DA (%s) is not yet associated\n",
					       __func__, ether_sprintf(daddr));
				goto bad;
			}
		}

		if (!wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_AUTH) &&
		    peer != wlan_vdev_get_bsspeer(vdev) && !wbuf_is_eapol(wbuf)
#ifdef ATH_SUPPORT_WAPI
		    && !wbuf_is_wai(wbuf)
#endif
		    ) {
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
		if (wbuf_is_eapol(wbuf)) {
			QDF_TRACE(
				       QDF_MODULE_ID_CRYPTO,
				       QDF_TRACE_LEVEL_DEBUG,
				       "%s: TX EAPOL Frame \n", __func__);
		}
		/*
		 *  Headline block removal: if the state machine is in
		 *  BLOCKING or PROBING state, transmision of UDP data frames
		 *  are blocked untill swtiches back to ACTIVE state.
		 */
		if (!wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_SEND_80211)) {
			if (dp_vdev->ique_ops.wlan_hbr_dropblocked) {
				if (dp_vdev->ique_ops.
				    wlan_hbr_dropblocked(vdev, peer, wbuf)) {
					QDF_TRACE(
						       QDF_MODULE_ID_IQUE,
						       QDF_TRACE_LEVEL_DEBUG,
						       "%s: packet dropped coz it blocks the headline\n",
						       __func__);
					goto bad;
				}
			}
		}
	}

	/* is_data */
	/*
	 * XXX When ni is associated with a WDS link then
	 * the vap will be the WDS vap but ni_vap will point
	 * to the ap vap the station associated to.  Once
	 * we handoff the packet to the driver the callback
	 * to ieee80211_encap won't be able to tell if the
	 * packet should be encapsulated for WDS or not (e.g.
	 * multicast frames will not be handled correctly).
	 * We hack this by marking the mbuf so ieee80211_encap
	 * can do the right thing.
	 */
#ifdef IEEE80211_WDS
	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_WDS_MODE)
		wbuf_set_wdsframe(wbuf);
	else
		wbuf_clear_wdsframe(wbuf);
#endif

	wbuf_set_peer(wbuf, peer);	/* associate node with wbuf */

	if (wlan_is_smps_set(dp_vdev))
		wbuf_set_smpsframe(wbuf);

	/* notify the sta PM module about xmit queue start to synchronize its network sleep operation */
	wlan_vdev_sta_power_tx_start(vdev);

    /**
     * deliver event to the registered handlers (one of them is PS SM )
     * this needs to be delivered beofore the following logic of queuing the frames
     * if the node is in paused state. this will ensure that the STA Power Save  SM
     * will move to active state while the node is in paused state.
     */
	if (dp_vdev->iv_txrx_event_info.
	    iv_txrx_event_filter & IEEE80211_VAP_OUTPUT_EVENT_DATA) {
		ieee80211_vap_txrx_event evt;
		evt.type = IEEE80211_VAP_OUTPUT_EVENT_DATA;
		ieee80211_vap_txrx_deliver_event(vdev, &evt);
	}

	/* power-save checks */
	if ((!UAPSD_AC_ISDELIVERYENABLED(wbuf_get_priority(wbuf), dp_peer)) &&
	    (wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_PAUSED)) &&
	    !(wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_TEMP))) {
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
#if !LMAC_SUPPORT_POWERSAVE_QUEUE
		wbuf_set_peer(wbuf, NULL);
#endif
		wlan_peer_saveq_queue(peer, wbuf,
				      (is_data ? IEEE80211_FC0_TYPE_DATA :
				       IEEE80211_FC0_TYPE_MGT));
		wlan_peer_unpause(peer);	/* unpause it if we are the last one, the frame will be flushed out */
#if !LMAC_SUPPORT_POWERSAVE_QUEUE
		wlan_objmgr_peer_release_ref(peer, WLAN_MLME_SB_ID);
#endif
		wlan_vdev_sta_power_tx_end(vdev);
#if !LMAC_SUPPORT_POWERSAVE_QUEUE
		return 0;
#endif
	}
	ieee80211_vap_pause_update_xmit_stats(dp_vdev, wbuf);	/* update the stats for vap pause module */
	retval = ieee80211_send_wbuf_internal(vdev, wbuf);

	/* notify the sta PM module about xmit queue end to synchronize it its network sleep operation */
	wlan_vdev_sta_power_tx_end(vdev);
#if QCA_SUPPORT_SON
	if (wlan_son_is_vdev_enabled(vdev) &&
	    !IEEE80211_IS_MULTICAST(daddr) && !IEEE80211_IS_BROADCAST(daddr)) {
		wbuf_set_bsteering(wbuf);
	}
#endif

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

free_wbuf:
	/* NB: callee's responsibilty to complete the packet */
	wbuf_set_status(wbuf, WB_STATUS_TX_ERROR);
	wbuf_complete(wbuf);

	return -EIO;
}

static INLINE int
ieee80211_send_wbuf_internal(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf)
{
	int retval;
	struct dadp_vdev *dp_vdev = NULL;

	if (!vdev) {
		qdf_print("%s:vdev is NULL ", __func__);
		return -EIO;
	}

	dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		return -EIO;
	}

	/*
	 * Wake up the H/W first
	 */
	if (wlan_vdev_is_up(vdev) != QDF_STATUS_SUCCESS) {
		struct ieee80211_tx_status ts;
		ts.ts_flags = IEEE80211_TX_ERROR;
		ts.ts_retries = 0;
		/*
		 * complete buf will decrement the pending count.
		 */
		wlan_complete_wbuf(wbuf, &ts);
		return -EIO;
	}

	/* propagate packets to NAWDS repeaters */
	if (wlan_nawds_send_wbuf(vdev, wbuf) == -1) {
		/* NAWDS node packet but mode is off, drop packet */
		return 0;
	}

	wlan_vdev_set_lastdata_timestamp(vdev);
	dp_vdev->vdev_txrxbytes += wbuf_get_pktlen(wbuf);
	wlan_vdev_sta_power_tx_start(vdev);

	/* Check SmartNet feature. Only support Windows and STA mode from now on */
	if (wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_SEND_80211) &&
	    (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE)) {
		/*
		 ** Determine the SMARTNET AC
		 */
		if (dp_vdev->vdev_smartnet_enable) {
			struct ieee80211_frame *wh;
			int is8021q, use4addr;
			u_int16_t hdrsize;
			u_int16_t *type = NULL;

			wh = (struct ieee80211_frame *)wbuf_header(wbuf);
			is8021q =
			    (((struct llc *)&wh[1])->llc_snap.ether_type ==
			     htons(ETHERTYPE_8021Q)) ? 1 : 0;
			/* If Vlan ID is 0, remove the 802.1q VLAN tag if it is included. */
			if (is8021q && (wbuf_VlanId(wbuf) == 0)) {
				use4addr =
				    ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) ==
				     IEEE80211_FC1_DIR_DSTODS) ? 1 : 0;
				if (use4addr) {
					hdrsize =
					    sizeof(struct
						   ieee80211_frame_addr4);
				} else {
					hdrsize =
					    sizeof(struct ieee80211_frame);
				}
				hdrsize = hdrsize + sizeof(struct llc);
				/* Find 802.1Q header and get the true ethernet type */
				type =
				    (u_int16_t
				     *) (wbuf_get_scatteredbuf_header(wbuf,
								      (hdrsize +
								       2)));
				/* Replace the ethernet type */
				if (type) {
					((struct llc *)&wh[1])->llc_snap.
					    ether_type = *type;;
					/* Remove 8021Q header */
					wbuf_tx_remove_8021q(wbuf);
				}
			}
		}
	}

	/*
	 * call back to shim layer to queue it to hardware device.
	 */
	retval =
	    dp_vdev->vdev_evtable->
	    wlan_dev_xmit_queue(wlan_vdev_get_osifp(vdev), wbuf);
	wlan_vdev_sta_power_tx_end(vdev);

	return retval;
}

int
dadp_vdev_send_wbuf(struct wlan_objmgr_vdev *vdev,
		    struct wlan_objmgr_peer *peer, wbuf_t wbuf)
{
	struct dadp_vdev *dp_vdev = NULL;
	dp_vdev = wlan_get_dp_vdev(vdev);
	/* send the data down to the ath */
	/* deliver event to the registered handlers (one of them is PS SM ) */
	if (dp_vdev->iv_txrx_event_info.
	    iv_txrx_event_filter & IEEE80211_VAP_OUTPUT_EVENT_DATA) {
		ieee80211_vap_txrx_event evt;
		evt.type = IEEE80211_VAP_OUTPUT_EVENT_DATA;
		ieee80211_vap_txrx_deliver_event(vdev, &evt);
	}
	wbuf_set_peer(wbuf, peer);
	return ieee80211_send_wbuf_internal(vdev, wbuf);
}

int ath_netdev_hardstart_generic(struct sk_buff *skb, struct net_device *dev)
{
	struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
	struct ath_softc *sc = ATH_DEV_TO_SC(scn->sc_dev);
	struct ieee80211_cb *cb;
	int error = 0;
	struct wlan_objmgr_peer *peer = NULL;
	struct ether_header *eh = (struct ether_header *)skb->data;
	int ismulti = IEEE80211_IS_MULTICAST(eh->ether_dhost) ? 1 : 0;
	u_int16_t addba_status;
	u_int32_t txq_depth, txq_aggr_depth;
	int buffer_limit;
	u_int32_t txbuf_freecount;
	struct ath_txq *txq;
	int qnum;
	int early_drop = 1;	/* allow tx buffer calculation to drop the packet by default */
	struct dadp_peer *dp_peer;

	/* make early_drop = 0 for important control plane packets like EAPOL and DHCP */

	cb = (struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb);
	peer = wbuf_get_peer(skb);
#if !ATH_SUPPORT_VOWEXT
	if (wbuf_is_highpriority(skb))
		early_drop = 0;
#endif

#if defined(ATH_SUPPORT_P2P)
	cb->complete_handler = NULL;
	cb->complete_handler_arg = NULL;
#endif /* ATH_SUPPORT_P2P */
	/*
	 * device must be up and running
	 */
	if ((dev->flags & (IFF_RUNNING | IFF_UP)) != (IFF_RUNNING | IFF_UP)) {
		error = -ENETDOWN;
		goto bad;
	}

	/*
	 * NB: check for valid node in case kernel directly sends packets
	 * on wifiX interface (such as broadcast packets generated by ipv6)
	 */
	if (peer == NULL) {
		wbuf_free(skb);
		return 0;
	}

	dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer) {
		qdf_print("%s:dp_peer is NULL ", __func__);
		wbuf_free(skb);
		return 0;
	}
        if (unlikely(atomic_read(&sc->sc_in_reset))){
            goto bad;
        }
#ifdef ATH_SUPPORT_UAPSD
	/* Limit UAPSD node queue depth to WME_UAPSD_NODE_MAXQDEPTH */
	if ((wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_UAPSD)) &&
	    scn->sc_ops->uapsd_depth(dp_peer->an) >= WME_UAPSD_NODE_MAXQDEPTH) {
		goto bad;
	}
#endif

	qnum = scn->sc_ac2q[skb->priority];
	txq = &sc->sc_txq[qnum];

	txq_depth =
	    scn->sc_ops->txq_depth(scn->sc_dev, scn->sc_ac2q[skb->priority]);
	txq_aggr_depth =
	    scn->sc_ops->txq_aggr_depth(scn->sc_dev,
					scn->sc_ac2q[skb->priority]);

	addba_status =
	    scn->sc_ops->addba_status(scn->sc_dev, dp_peer->an, cb->u_tid);

	/*
	 * This logic throttles legacy and unaggregated HT frames if they share the hardware
	 * queue with aggregates. This improves the transmit throughput performance to
	 * aggregation enabled nodes when they coexist with legacy nodes.
	 */
	/* Do not throttle EAPOL packets - this causes the REKEY packets
	 * to be dropped and station disconnects.
	 */
	DPRINTF(scn, ATH_DEBUG_RESET,
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
#if !ATH_SUPPORT_VOWEXT
	if (ismulti && early_drop) {
#else
	if (ismulti) {
#endif
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
	error = ath_tx_send(skb);
#else
    ATH_INTERNAL_RESET_LOCK(sc);
    error = ath_tx_send(skb);
    ATH_INTERNAL_RESET_UNLOCK(sc);
#endif

	if (error) {
		DPRINTF(scn, ATH_DEBUG_XMIT, "%s: Tx failed with error %d\n",
			__func__, error);
	}
	return 0;

bad:
	__11nstats(((struct ath_softc *)(scn->sc_dev)), tx_drops);
	sc->sc_stats.ast_tx_nobuf++;
	sc->sc_stats.ast_txq_packets[scn->sc_ac2q[skb->priority]]++;
	sc->sc_stats.ast_txq_nobuf[scn->sc_ac2q[skb->priority]]++;

	IEEE80211_TX_COMPLETE_WITH_ERROR(skb);
	DPRINTF(scn, ATH_DEBUG_XMIT, "%s: Tx failed with error %d\n",
		__func__, error);
	return 0;
}

#ifdef ENCAP_OFFLOAD
int ath_get_cipher_map(u_int32_t cipher, u_int32_t * halKeyType)
{
	if (cipher == IEEE80211_CIPHER_WEP)
		*halKeyType = HAL_KEY_TYPE_WEP;
	else if (cipher == IEEE80211_CIPHER_TKIP)
		*halKeyType = HAL_KEY_TYPE_TKIP;
	else if (cipher == IEEE80211_CIPHER_AES_OCB)
		*halKeyType = HAL_KEY_TYPE_AES;
	else if (cipher == IEEE80211_CIPHER_AES_CCM)
		*halKeyType = HAL_KEY_TYPE_AES;
	else if (cipher == IEEE80211_CIPHER_CKIP)
		*halKeyType = HAL_KEY_TYPE_WEP;
	else if (cipher == IEEE80211_CIPHER_NONE)
		*halKeyType = HAL_KEY_TYPE_CLEAR;
	else
		return 1;
	return 0;
}

int
ath_tx_data_prepare(struct ath_softc_net80211 *scn, wbuf_t wbuf,
		    int nextfraglen, ieee80211_tx_control_t * txctl)
{
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	struct wlan_crypto_key *key;
#else
	struct ieee80211_rsnparms *rsn = &vap->iv_rsn;
	struct ieee80211_key *key = NULL;
#endif
	struct ether_header *eh;
	int keyix = HAL_TXKEYIX_INVALID, pktlen = 0;
	u_int8_t keyid = 0;
	HAL_KEY_TYPE keytype = HAL_KEY_TYPE_CLEAR;

#if defined(ATH_SUPPORT_UAPSD)
	if (wbuf_is_uapsd(wbuf))
		return ath_tx_prepare(scn->sc_pdev, wbuf, nextfraglen, txctl);
#endif

	OS_MEMZERO(txctl, sizeof(ieee80211_tx_control_t));
	eh = (struct ether_header *)wbuf_header(wbuf);

	txctl->ismcast = IEEE80211_IS_MULTICAST(eh->ether_dhost);
	txctl->istxfrag = 0;	/* currently hardcoding to 0, revisit */

	pktlen = wbuf_get_pktlen(wbuf);

	/*
	 * Set per-packet exemption type
	 */
	if ((eh->ether_type != htons(ETHERTYPE_PAE)) &&
	    IEEE80211_VAP_IS_PRIVACY_ENABLED(vap))
		wbuf_set_exemption_type(wbuf, WBUF_EXEMPT_NO_EXEMPTION);
	else if ((eh->ether_type == htons(ETHERTYPE_PAE)) &&
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
                 (wlan_crypto_vdev_has_auth_mode(vap->vdev_obj, ((1 << WLAN_CRYPTO_AUTH_WPA) | (1 << WLAN_CRYPTO_AUTH_RSNA)))))
#else
		 (RSN_AUTH_IS_WPA(rsn) || RSN_AUTH_IS_WPA2(rsn)))
#endif
		wbuf_set_exemption_type(wbuf,
					WBUF_EXEMPT_ON_KEY_MAPPING_KEY_UNAVAILABLE);
	else
		wbuf_set_exemption_type(wbuf, WBUF_EXEMPT_ALWAYS);

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
	if (IEEE80211_VAP_IS_PRIVACY_ENABLED(vap)) {	/* crypto is on */
		/*
		 * Find the key that would be used to encrypt the frame if the
		 * frame were to be encrypted. For unicast frame, search the
		 * matching key in the key mapping table first. If not found,
		 * used default key. For multicast frame, only use the default key.
		 */
		if (vap->iv_opmode == IEEE80211_M_STA ||
		    !IEEE80211_IS_MULTICAST(eh->ether_dhost) ||
		    (vap->iv_opmode == IEEE80211_M_WDS &&
		     IEEE80211_VAP_IS_STATIC_WDS_ENABLED(vap)))
			/* use unicast key */
			key = &ni->ni_ucastkey;

		if (key && key->wk_valid) {
			txctl->key_mapping_key = 1;
			keyid = 0;
		} else if (vap->iv_def_txkey != IEEE80211_KEYIX_NONE) {
			key = &vap->iv_nw_keys[vap->iv_def_txkey];
			key = key->wk_valid ? key : NULL;
			keyid = key ? (uint8_t) vap->iv_def_txkey : 0;
		} else
			key = NULL;

		if (key) {
			keyix = (int8_t) key->wk_keyix;
			if (ath_get_cipher_map
			    (key->wk_cipher->ic_cipher, &keytype) != 0) {
				DPRINTF(scn, ATH_DEBUG_ANY,
					"%s: Failed to identify Hal Key Type for ic_cipher %d \n",
					__func__, key->wk_cipher->ic_cipher);
				return 1;
			}
		}
	} else if ((ni->ni_ucastkey.wk_cipher == &ieee80211_cipher_none) &&
		   (keyix != IEEE80211_KEYIX_NONE))
		keyix = ni->ni_ucastkey.wk_keyix;
	else
		keyix = HAL_TXKEYIX_INVALID;
#else
	if (IEEE80211_VAP_IS_PRIVACY_ENABLED(vap)) {	/* crypto is on */
		/*
		 * Find the key that would be used to encrypt the frame if the
		 * frame were to be encrypted. For unicast frame, search the
		 * matching key in the key mapping table first. If not found,
		 * used default key. For multicast frame, only use the default key.
		 */
		if (vap->iv_opmode == IEEE80211_M_STA ||
		    !IEEE80211_IS_MULTICAST(eh->ether_dhost) ||
		    (vap->iv_opmode == IEEE80211_M_WDS &&
		     IEEE80211_VAP_IS_STATIC_WDS_ENABLED(vap)))
			/* use unicast key */
			key = wlan_crypto_peer_getkey(ni->peer_obj, WLAN_CRYPTO_KEYIX_NONE)
		else
			key = wlan_crypto_vap_getkey(vap->vdev_obj, WLAN_CRYPTO_KEYIX_NONE)

		if (key && key->wk_valid) {
			txctl->key_mapping_key = 1;
			keyid = key->wk_keyix;
			keyix = (int8_t) key->wk_keyix;
		} else
			key = NULL;
	} else
		keyix = HAL_TXKEYIX_INVALID;
#endif

	pktlen += IEEE80211_CRC_LEN;

	txctl->frmlen = pktlen;
	txctl->keyix = keyix;
	txctl->keyid = keyid;
	txctl->keytype = keytype;
	txctl->txpower = wlan_peer_get_txpower(peer);
	txctl->nextfraglen = nextfraglen;

	/*
	 * NB: the 802.11 layer marks whether or not we should
	 * use short preamble based on the current mode and
	 * negotiated parameters.
	 */
	if (wlan_pdev_nif_feat_cap_get(pdev, WLAN_PDEV_F_SHPREAMBLE) &&
	    !wlan_pdev_nif_feat_cap_get(pdev, WLAN_PDEV_F_USEBARKER) &&
	    wlan_peer_check_cap(peer, IEEE80211_CAPINFO_SHORT_PREAMBLE))
		txctl->shortPreamble = 1;

#if !defined(ATH_SWRETRY) || !defined(ATH_SWRETRY_MODIFY_DSTMASK)
	txctl->flags = HAL_TXDESC_CLRDMASK;	/* XXX needed for crypto errs */
#endif

	txctl->isdata = 1;
	txctl->atype = HAL_PKT_TYPE_NORMAL;	/* default */

	if (txctl->ismcast)
		txctl->mcast_rate = vap->iv_mcast_rate;

	if (IEEE80211_PEER_USEAMPDU(peer)
	    || wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_QOS)) {
		int ac = wbuf_get_priority(wbuf);
		txctl->isqosdata = 1;

		/* XXX validate frame priority, remove mask */
		txctl->qnum = scn->sc_ac2q[ac & 0x3];
		if (wlan_pdev_get_wmep_noackPolicy(pdev, ac))
			txctl->flags |= HAL_TXDESC_NOACK;
	} else {
		/*
		 * Default all non-QoS traffic to the best-effort queue.
		 */
		txctl->qnum = scn->sc_ac2q[WME_AC_BE];
		wbuf_set_priority(wbuf, WME_AC_BE);
	}

	/*
	 * For HT capable stations, we save tidno for later use.
	 * We also override seqno set by upper layer with the one
	 * in tx aggregation state.
	 */
	if (!txctl->ismcast && wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_HT))
		txctl->ht = 1;

	/* Update the uapsd ctl for all frames */
	ath_uapsd_txctl_update(scn, wbuf, txctl);
	/*
	 * If we are servicing one or more stations in power-save mode.
	 */
	txctl->if_id = wlan_vdev_get_id(vdev);
	if (wlan_vdev_has_pssta(vdev))
		txctl->ps = 1;

	/*
	 * Calculate miscellaneous flags.
	 */
	if (wbuf_is_eapol(wbuf)) {
		txctl->use_minrate = 1;
	}

	if (txctl->ismcast) {
		txctl->flags |= HAL_TXDESC_NOACK;	/* no ack on broad/multicast */
	} else if (pktlen > wlan_vdev_get_rtsthreshold(vdev)) {
		txctl->flags |= HAL_TXDESC_RTSENA;	/* RTS based on frame length */
	}

	/* Frame to enable SM power save */
	if (wbuf_is_smpsframe(wbuf)) {
		txctl->flags |= HAL_TXDESC_LOWRXCHAIN;
	}

	IEEE80211_HTC_SET_NODE_INDEX(txctl, wbuf);

	return 0;

}
#endif

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

struct ieee80211_txctl_cap txctl_cap[IEEE80211_TYPE4TXCTL_MAX] = {
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

int
ath_tx_prepare(struct wlan_objmgr_pdev *pdev, wbuf_t wbuf, int nextfraglen,
	       ieee80211_tx_control_t * txctl)
{
	struct wlan_objmgr_peer *peer = wbuf_get_peer(wbuf);
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct dadp_pdev *dp_pdev;
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	struct ath_softc_net80211 *scn;
	struct dadp_peer *dp_peer = NULL;
	struct ieee80211_frame *wh;
	int keyix = 0, hdrlen, pktlen;
	int type, subtype;
	int txctl_tab_index;
	u_int32_t txctl_flag_mask = 0;
	u_int8_t acnum, use_ni_minbasicrate, use_mgt_rate;
	HAL_KEY_TYPE keytype = HAL_KEY_TYPE_CLEAR;
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	uint8_t macaddr[IEEE80211_ADDR_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	uint8_t *mac;
	static const HAL_KEY_TYPE keytype_table[WLAN_CRYPTO_CIPHER_MAX+1] = {
	HAL_KEY_TYPE_WEP, /*WLAN_CRYPTO_CIPHER_WEP */
	HAL_KEY_TYPE_TKIP,/*WLAN_CRYPTO_CIPHER_TKIP */
	HAL_KEY_TYPE_AES, /*WLAN_CRYPTO_CIPHER_AES_OCB */
	HAL_KEY_TYPE_AES, /*WLAN_CRYPTO_CIPHER_AES_CCM */
#if ATH_SUPPORT_WAPI
	HAL_KEY_TYPE_WAPI,/*WLAN_CRYPTO_CIPHER_WAPI_SMS4 */
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
	OS_MEMZERO(txctl, sizeof(ieee80211_tx_control_t));

	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		return -EIO;
	}

	dp_pdev = wlan_get_dp_pdev(pdev);
	if (!dp_pdev) {
		qdf_print("%s:dp_pdev is NULL ", __func__);
		return -EIO;
	}
	scn = dp_pdev->scn;

	dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer) {
		qdf_print("%s:dp_peer is NULL ", __func__);
		return -EIO;
	}
	wh = (struct ieee80211_frame *)wbuf_header(wbuf);

	txctl->iseap = wbuf_is_eapol(wbuf);
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
	if (type == IEEE80211_FC0_TYPE_CTL &&
	    (subtype == IEEE80211_FC0_SUBTYPE_CTS
	     || subtype == IEEE80211_FC0_SUBTYPE_ACK)) {
		/*
		 * For CTS and ACK, the hdr size is only 2+2+6.
		 * Skip the padding deduction for these frames.
		 */
	} else
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
		if (psoc == NULL) {
			qdf_print("%s[%d]psoc is NULL", __func__, __LINE__);
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
		if (type == IEEE80211_FC0_TYPE_CTL &&
			(subtype == IEEE80211_FC0_SUBTYPE_CTS
			 || subtype == IEEE80211_FC0_SUBTYPE_ACK)) {
			/*
			 * For CTS and ACK, the hdr size is only 2+2+6.
			 * Skip the padding deduction for these frames.
			 */
		} else
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
			QDF_TRACE( QDF_MODULE_ID_ANY,
				       QDF_TRACE_LEVEL_DEBUG,
				       "%s: ieee80211_crypto_encap failed \n",
				       __func__);
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

		if ((k->wk_flags & IEEE80211_KEY_SWMIC) == 0) {
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
		if (((k->wk_flags & IEEE80211_KEY_MFP)
		     && IEEE80211_IS_MFP_FRAME(wh))) {
			if (cip->ic_cipher == IEEE80211_CIPHER_TKIP) {
				QDF_TRACE(
					       QDF_MODULE_ID_CRYPTO,
					       QDF_TRACE_LEVEL_DEBUG,
					       "%s: extend MHDR IE\n",
					       __func__);
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
	    wlan_peer_check_cap(peer, IEEE80211_CAPINFO_SHORT_PREAMBLE))
		txctl->shortPreamble = 1;

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
#if ATH_SUPPORT_WIFIPOS
	if (wbuf_is_pos(wbuf) || wbuf_is_keepalive(wbuf)) {
		if (wbuf_is_keepalive(wbuf))
			txctl->use_minrate = 1;
		else
			txctl->use_minrate = 0;
	} else
#endif
		txctl->use_minrate = txctl_cap[txctl_tab_index].use_minrate;
	txctl->atype = txctl_cap[txctl_tab_index].atype;
	acnum = txctl_cap[txctl_tab_index].ac;
	use_ni_minbasicrate = txctl_cap[txctl_tab_index].use_ni_minbasicrate;
	use_mgt_rate = txctl_cap[txctl_tab_index].use_mgt_rate;

	/* set Tx delayed report indicator */
#ifdef ATH_SUPPORT_TxBF
	if ((type == IEEE80211_FC0_TYPE_MGT)
	    && (subtype == IEEE80211_FC0_SUBTYPE_ACTION)) {
		u_int8_t *v_cv_data =
		    (u_int8_t *) (wbuf_header(wbuf) +
				  sizeof(struct ieee80211_frame));

		if ((*(v_cv_data + 1) == IEEE80211_ACTION_HT_COMP_BF)
		    || (*(v_cv_data + 1) == IEEE80211_ACTION_HT_NONCOMP_BF)) {
			txctl->isdelayrpt = 1;
		}
	}
#endif
	/*
	 * In offchannel tx mode Update  txctl field to use minrate
	 */
	if (wbuf_is_offchan_tx(wbuf)) {
		txctl->use_minrate = 1;
	}
	/*
	 * Update some txctl fields
	 */
	if (type == IEEE80211_FC0_TYPE_DATA
	    && subtype != IEEE80211_FC0_SUBTYPE_NODATA) {
		if (wbuf_is_eapol(wbuf)) {
			txctl->use_minrate = 1;
		}
		if (txctl->ismcast) {
			txctl->mcast_rate = dp_vdev->vdev_mcast_rate;
#if UMAC_SUPPORT_WNM
			/* add FMS stuff to txctl */
			txctl->isfmss = wbuf_is_fmsstream(wbuf);
			txctl->fmsq_id = wbuf_get_fmsqid(wbuf);
#endif /* UMAC_SUPPORT_WNM */
		}
		if (subtype & IEEE80211_FC0_SUBTYPE_QOS) {
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
					//    ,__func__,txctl->flags,ic->ic_txbf.tx_staggered_sounding,ni->ni_txbf.rx_staggered_sounding);
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
	if (txctl->isnulldata)
		txctl->ht = 0;
	/*
	 * Set min rate and qnum in txctl based on acnum
	 */
	if (txctl->use_minrate) {
		if (use_ni_minbasicrate) {
			/*
			 * Send out all mangement frames except Probe request
			 * at minimum rate set by AP.
			 */
			if ((wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE) &&
			    (dp_peer->peer_minbasicrate != 0)) {
				txctl->min_rate = dp_peer->peer_minbasicrate;
			}
		}

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

	/*
	 * Calculate miscellaneous flags.
	 */
	if (txctl->ismcast) {
		txctl_flag_mask |= HAL_TXDESC_NOACK;	/* no ack on broad/multicast */
	} else if (pktlen > wlan_vdev_get_rtsthreshold(vdev)) {
		txctl_flag_mask |= HAL_TXDESC_RTSENA;	/* RTS based on frame length */
	} else if (dp_vdev->vdev_protmode == IEEE80211_PROTECTION_RTS_CTS) {
		txctl_flag_mask |= HAL_TXDESC_RTSENA;	/* RTS/CTS  */
	} else if (dp_vdev->vdev_protmode == IEEE80211_PROTECTION_CTSTOSELF) {
		txctl_flag_mask |= HAL_TXDESC_CTSENA;	/* CTS only */
	}

	/* Frame to enable SM power save */
	if (wbuf_is_smpsframe(wbuf)) {
		txctl_flag_mask |= HAL_TXDESC_LOWRXCHAIN;
	}
#if ATH_SUPPORT_WIFIPOS
	/*
	 * Update txctl flag for CTS frame
	 * This change has been done because we donot need any rts for cts2self pkt.
	 * Also, if the enable_duration is not set, HW will update the duration field
	 * of the pkt, irrespective of its type.
	 */
	if (wbuf_get_cts_frame(wbuf)) {
		txctl_flag_mask |= HAL_TXDESC_NOACK;
		txctl_flag_mask |= HAL_TXDESC_ENABLE_DURATION;
	}

	/* copy the locationing bit */
	if (wbuf_is_pos(wbuf)) {
		txctl_flag_mask |= HAL_TXDESC_POS;
		txctl->wifiposdata = wbuf_get_wifipos(wbuf);
		if (txctl->wifiposdata) {
			ieee80211_wifipos_reqdata_t *txchain_data =
			    (ieee80211_wifipos_reqdata_t *) txctl->wifiposdata;
			u_int8_t update_txchainmask = txchain_data->txchainmask;
			if (wbuf_is_vmf(wbuf))
				txctl_flag_mask |= HAL_TXDESC_VMF;
			if (update_txchainmask == 1)
				txctl_flag_mask |= HAL_TXDESC_POS_TXCHIN_1;
			if (update_txchainmask == 3)
				txctl_flag_mask |= HAL_TXDESC_POS_TXCHIN_2;
			if (update_txchainmask == 7)
				txctl_flag_mask |= HAL_TXDESC_POS_TXCHIN_3;
			//printk("update_txchainmask %x (%s) %d\n",update_txchainmask,__func__,__LINE__);
			if (txchain_data->hc_channel ==
			    txchain_data->oc_channel) {
				txctl->qnum = scn->sc_wifipos_hc_qnum;
			} else {
				txctl->qnum = scn->sc_wifipos_oc_qnum;
			}
		}
	}
	if (wbuf_is_keepalive(wbuf)) {
		txctl_flag_mask |= HAL_TXDESC_POS_KEEP_ALIVE;
	}
#endif

	/*
	 * Update txctl->flags based on the flag mask
	 */
	txctl->flags |= txctl_flag_mask;
	IEEE80211_HTC_SET_NODE_INDEX(txctl, wbuf);

	return 0;
}

/*
 * The function to send a frame (i.e., hardstart). The wbuf should already be
 * associated with the actual frame, and have a valid node instance.
 */

int ath_tx_send(wbuf_t wbuf)
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

	/*
	 * XXX TODO: Fast frame here
	 */

#ifdef ATH_SUPPORT_DFS
	/*
	 * EV 10538/79856
	 * If we detect radar on the current channel, stop sending data
	 * packets. There is a DFS requirment that the AP should stop
	 * sending data packet within 250 ms of radar detection
	 */

	if ((wlan_vdev_is_radar_channel(vdev)) ||
	    (wlan_vdev_is_up(vdev) != QDF_STATUS_SUCCESS)) {
		goto bad;
	}
#endif

	wlan_uapsd_pwrsave_check(wbuf, peer);

#ifdef ATH_AMSDU
	/* Check whether AMSDU is supported in this BlockAck agreement */
	if (IEEE80211_PEER_USEAMPDU(peer) &&
	    scn->sc_ops->get_amsdusupported(scn->sc_dev,
					    dp_peer->an, wbuf_get_tid(wbuf))) {
		wbuf = ath_amsdu_send(wbuf);
		if (wbuf == NULL)
			return 0;
	}
#endif

	/*
	 * Encapsulate the packet for transmission
	 */
#if defined(ENCAP_OFFLOAD) && defined(ATH_SUPPORT_UAPSD)
	if (wbuf_is_uapsd(wbuf))
		wbuf = ieee80211_encap_force(peer, wbuf);
	else
#endif
		wbuf = ieee80211_encap(peer, wbuf);
	if (wbuf == NULL) {
		DPRINTF(scn, ATH_DEBUG_ANY, "%s: ieee80211_encap failed \n",
			__func__);
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
		/* prepare this frame */
		if (ath_tx_prepare(pdev, wbuf, nextfraglen, txctl) != 0)
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

#if UMAC_PER_PACKET_DEBUG
		wbuf_set_rate(wbuf, dp_vdev->vdev_userrate);
		wbuf_set_retries(wbuf, dp_vdev->vdev_userretries);
		wbuf_set_txpower(wbuf, dp_vdev->vdev_usertxpower);
		wbuf_set_txchainmask(wbuf, dp_vdev->vdev_usertxchainmask);
#endif

		HTC_WBUF_TX_DATA_PREPARE(ic, scn);
		if (error == 0) {
			if (scn->sc_ops->tx(scn->sc_dev, wbuf, txctl) != 0) {
				goto bad;
			} else {
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

#if !ATH_SUPPORT_STATS_APONLY
INLINE void
ieee80211_update_stats_additional(struct wlan_objmgr_vdev *vdev,
				  wbuf_t wbuf,
				  struct ieee80211_frame *wh,
				  int type, int subtype,
				  struct ieee80211_tx_status *ts)
{
	struct wlan_pdev_phy_stats *phy_stats;
	int is_mcast;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct dadp_pdev *dp_pdev = NULL;
	struct dadp_vdev *dp_vdev = NULL;
	struct wlan_objmgr_peer *bsspeer;

	if (!vdev) {
		qdf_print("%s:vdev is NULL ", __func__);
		return;
	}

	dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		return;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		qdf_print("%s:pdev is NULL ", __func__);
		return;
	}

	dp_pdev = wlan_get_dp_pdev(pdev);
	if (!dp_pdev) {
		qdf_print("%s:dp_pdev is NULL ", __func__);
		return;
	}

	is_mcast = IEEE80211_IS_MULTICAST(wh->i_addr1) ? 1 : 0;
	phy_stats = &dp_pdev->pdev_phy_stats[wlan_vdev_get_curmode(vdev)];
	if (ts->ts_flags == 0) {
		phy_stats->ips_tx_fragments++;
		phy_stats->ips_tx_packets++;
		if (is_mcast)
			phy_stats->ips_tx_multicast++;

		if (ts->ts_retries != 0) {
			phy_stats->ips_tx_retries++;
			if (ts->ts_retries > 1)
				phy_stats->ips_tx_multiretries++;
		}

		/* update miscellous status */
		if ((wbuf_get_priority(wbuf) == WME_AC_VO) ||
		    (wbuf_get_priority(wbuf) == WME_AC_VI)) {
			dp_pdev->wme_hipri_traffic++;
		}

		/*
		 * If we're in station mode, notify the scan algorithm when a data frame is transmitted successfully.
		 */
		if ((wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE) ||
		    (wlan_vdev_mlme_get_opmode(vdev) == QDF_IBSS_MODE)) {
			/*
			 * Verify it was a directed frame (not broadcast)
			 */
			bsspeer = wlan_vdev_get_bsspeer(vdev);
			if (bsspeer
			    && IEEE80211_ADDR_EQ(wh->i_addr1,
						 wlan_peer_get_bssid(bsspeer)))
			{
				dp_vdev->vdev_last_ap_frame =
				    OS_GET_TIMESTAMP();

				if (!IEEE80211_ADDR_EQ
				    (wh->i_addr3,
				     WLAN_GET_BCAST_ADDR(dp_pdev))) {
					wlan_vdev_set_last_directed_frame(vdev);
				}
			}
		}
	} else {
		if (ts->ts_flags & IEEE80211_TX_XRETRY) {
			phy_stats->ips_tx_xretries++;
		}
	}
}
#endif /* not ATH_SUPPORT_STATS_APONLY */

void
ieee80211_update_stats(struct wlan_objmgr_vdev *vdev,
		       wbuf_t wbuf,
		       struct ieee80211_frame *wh,
		       int type, int subtype, struct ieee80211_tx_status *ts)
{
	struct wlan_vdev_mac_stats *mac_stats;
	int is_mcast;
	u_int32_t data_bytes = 0;
	u_int8_t crypto_field_bytes = 0;
	u_int16_t hdrsize = 0;
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
	struct ieee80211_key *key = NULL;
#endif
	struct wlan_objmgr_peer *peer = wbuf_get_peer(wbuf);

	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);

	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		return;
	}

	if (!dp_peer) {
		qdf_print("%s:dp_peer is NULL ", __func__);
		return;
	}
	is_mcast = IEEE80211_IS_MULTICAST(wh->i_addr1) ? 1 : 0;
	mac_stats =
	    is_mcast ? &dp_vdev->vdev_multicast_stats : &dp_vdev->
	    vdev_unicast_stats;

	if (ts->ts_flags == 0) {
		mac_stats->ims_tx_packets++;

		/* TODO: Rectify the below calculation, after checking if it
		   breaks anything. */
		mac_stats->ims_tx_bytes += wbuf_get_pktlen(wbuf);
		/*
		 * Store timestamp for actual (non-NULL) data frames.
		 * This provides other modules such as SCAN and LED with correct
		 * information about the actual data traffic in the system.
		 */
		if ((type == IEEE80211_FC0_TYPE_DATA)
		    && IEEE80211_CONTAIN_DATA(subtype)) {
			dp_vdev->vdev_last_traffic_indication =
			    OS_GET_TIMESTAMP();
		}

		if (type == IEEE80211_FC0_TYPE_DATA) {

			data_bytes = wbuf_get_pktlen(wbuf) + IEEE80211_CRC_LEN;

			/* Account for padding */
			hdrsize = ieee80211_hdrsize(wh);
			data_bytes -=
			    (wlan_hdrspace(wlan_vdev_get_pdev(vdev), wh) -
			     hdrsize);

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
			/* tbd_da_crypto: skip this if sw encryption is enabled */
			data_bytes += (dadp_peer_get_key_trailer(peer)
				       + dadp_peer_get_key_miclen(peer));

			crypto_field_bytes = dadp_peer_get_key_header(peer)
				+ dadp_peer_get_key_trailer(peer)
				+ dadp_peer_get_key_miclen(peer);
#else
			key = wlan_crypto_get_txkey(vdev, wbuf);

			if (key) {
				if (!(key->wk_flags & IEEE80211_KEY_SWENCRYPT)) {
					data_bytes +=
					    (key->wk_cipher->ic_trailer +
					     key->wk_cipher->ic_miclen);
				}

				crypto_field_bytes = key->wk_cipher->ic_header
				    + key->wk_cipher->ic_trailer
				    + key->wk_cipher->ic_miclen;
			}
#endif

			mac_stats->ims_tx_data_packets++;
			mac_stats->ims_tx_data_bytes += data_bytes;
			mac_stats->ims_tx_datapyld_bytes += (data_bytes -
							     hdrsize -
							     crypto_field_bytes
							     -
							     IEEE80211_CRC_LEN);

#if UMAC_SUPPORT_TXDATAPATH_NODESTATS
			if (!is_mcast && likely(dp_peer != NULL)) {
				IEEE80211_PEER_STAT(dp_peer, tx_data_success);

				IEEE80211_PEER_STAT_ADD(dp_peer,
							tx_bytes_success,
							data_bytes);
			}
#if UMAC_SUPPORT_STA_STATS_ENHANCEMENT
			/* HERE counting tx_ucast/mcast means 'success' like offload driver
			   AP-only: don't call/enter this function per test,
			   non AP-only: repeat with statistic before TX interrupt */
			if (dp_peer != NULL) {
				if (is_mcast) {
					IEEE80211_PEER_STAT(dp_peer, tx_mcast);
					IEEE80211_PEER_STAT_ADD(dp_peer,
								tx_mcast_bytes,
								data_bytes);
				} else {
					IEEE80211_PEER_STAT(dp_peer, tx_ucast);
					IEEE80211_PEER_STAT_ADD(dp_peer,
								tx_ucast_bytes,
								data_bytes);
				}
			}
#endif
#endif
		}
	}

	/* The below stats are normally gathered in ieee80211_update_stats_additional().
	   But if APONLY mode is used, then ieee80211_update_stats_additional() is
	   disabled. Hence, we gather them separately below. */
	if ((ts->ts_flags != 0) && !(ts->ts_flags & IEEE80211_TX_XRETRY)) {
		mac_stats->ims_tx_discard++;
#if UMAC_SUPPORT_TXDATAPATH_NODESTATS
		if (!is_mcast && likely(dp_peer != NULL)) {
			IEEE80211_PEER_STAT(dp_peer, tx_discard);
		}
#endif
	}
#if !ATH_SUPPORT_STATS_APONLY
	ieee80211_update_stats_additional(vdev, wbuf, wh, type, subtype, ts);
#endif
}

static INLINE void ieee80211_release_wbuf_internal(struct wlan_objmgr_peer
						   *peer, wbuf_t wbuf,
						   struct ieee80211_tx_status
						   *ts)
{
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct ieee80211_frame *wh;
	int type, subtype;
	uint32_t delayed_cleanup = 0;
	struct dadp_vdev *dp_vdev = NULL;

	dp_vdev = wlan_get_dp_vdev(vdev);
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
		wlan_peer_reload_inact(peer);
	}
	/*
	 * Tx post processing for non beacon frames.
	 * check if the vap is valid , it could have been deleted.
	 */
	if (vdev) {
#if 0
		ieee80211_vap_complete_buf_handler handler;
		void *arg;
#endif

		if (!
		    (type == IEEE80211_FC0_TYPE_MGT
		     && subtype == IEEE80211_FC0_SUBTYPE_BEACON)) {
			dadp_vdev_deliver_tx_event(peer, wbuf, wh, ts);
			/*
			 * beamforming status indicates that a CV update is required
			 */
#ifdef ATH_SUPPORT_TxBF
			/* should check status validation, not zero and TxBF mode first to avoid EV#82661 */
			if (!(ts->ts_txbfstatus & ~(TxBF_Valid_Status))
			    && (ts->ts_txbfstatus != 0))
				wlan_tx_bf_completion_handler(peer, ts);
#endif
			/*
			 * update tx statisitics
			 */
			if (type == IEEE80211_FC0_TYPE_DATA) {
				ieee80211_update_stats(vdev, wbuf, wh, type,
						       subtype, ts);
			}
		}
		if (dp_vdev->vdev_evtable
		    && dp_vdev->vdev_evtable->wlan_xmit_update_status) {
			dp_vdev->vdev_evtable->
			    wlan_xmit_update_status(wlan_vdev_get_osifp(vdev),
						    wbuf, ts);
		}
#if 0				/* below code is only for MGMT frames */
		/*
		 * Created callback function to be called upon completion of transmission of Channel Switch
		 * Request/Response frames. Any frame type now can have a completion callback function
		 * instead of only Management or NULL Data frames.
		 */
		/* All the frames have the handler function */
		wbuf_get_complete_handler(wbuf, (void **)&handler, &arg);
		if (handler) {
			handler(vap, wbuf, arg, wh->i_addr1, wh->i_addr2,
				wh->i_addr3, ts);
		}
#endif
	} else {
#if 0
		/* Modify for static analysis, if vap is NULL, don't use IEEE80211_DPRINTF */
		printk("%s vap is null node ni 0x%pK\n", __func__, ni);
#endif
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
		if (ts->ts_flags & IEEE80211_TX_FLUSH) {
			wbuf_complete_any(wbuf);
		} else {
			wbuf_complete(wbuf);
		}
	}
}

#if ATH_DEBUG
#define OFFCHAN_EXT_TID_NONPAUSE    19
#endif

static INLINE int wlan_pdev_has_pn_check_war(struct dadp_pdev *dp_pdev)
{
	return (dp_pdev->pdev_athextcap & IEEE80211_ATHEC_PN_CHECK_WAR);
}

void wlan_complete_wbuf(wbuf_t wbuf, struct ieee80211_tx_status *ts)
{
	struct wlan_objmgr_peer *peer = wbuf_get_peer(wbuf);
	struct ieee80211_frame *wh;
	int type;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct dadp_pdev *dp_pdev = NULL;
	struct dadp_vdev *dp_vdev = NULL;
	struct dadp_peer *dp_peer = NULL;
	u_int8_t tidno = wbuf_get_tid(wbuf);
#if UMAC_SUPPORT_WNM
	u_int32_t secured = 0;
#endif
	struct ath_softc_net80211 *scn = NULL;
	KASSERT((peer != NULL), ("peer can not be null"));
	if (peer == NULL) {
		printk
		    ("%s: Error wbuf node peer is NULL, so exiting. wbuf: 0x%pK \n",
		     __func__, wbuf);
		return;
	}
	vdev = wlan_peer_get_vdev(peer);
	KASSERT((vdev != NULL), ("vdev can not be null"));

	dp_vdev = wlan_get_dp_vdev(vdev);
	pdev = wlan_vdev_get_pdev(vdev);
	dp_pdev = wlan_get_dp_pdev(pdev);
	if (!dp_pdev) {
		qdf_print("%s:dp_pdev is NULL ", __func__);
		return;
	}

	scn = dp_pdev->scn;
	dp_peer = wlan_get_dp_peer(peer);
	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
#if UMAC_SUPPORT_WNM
	secured = wh->i_fc[1] & IEEE80211_FC1_WEP;
#endif

	if (dp_pdev && wlan_pdev_has_pn_check_war(dp_pdev)
	    && (!(ts->ts_flags & IEEE80211_TX_ERROR))
	    && (type == IEEE80211_FC0_TYPE_DATA))
		ieee80211_check_and_update_pn(wbuf);
#if ATH_SUPPORT_WIFIPOS
	if (wbuf_get_cts_frame(wbuf))
		ieee80211_cts_done(ts->ts_flags == 0);

	if ((wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_WAKEUP))
	    && (wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_UAPSD)))
		dp_peer->peer_consecutive_xretries = 0;
#endif

	/* Check SmartNet feature. Only support Windows and STA mode from now on */
	if (wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_SEND_80211) &&
	    (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE)) {
		if (dp_vdev->vdev_smartnet_enable) {
			wbuf_tx_recovery_8021q(wbuf);
		}
	}

	ieee80211_release_wbuf_internal(peer, wbuf, ts);

#if defined(ATH_SUPPORT_QUICK_KICKOUT) || UMAC_SUPPORT_NAWDS
	/* if not bss node, check the successive tx failed counts */
	if ((wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE) &&
	    (peer != wlan_vdev_get_bsspeer(vdev))) {
		if (ts->ts_flags & IEEE80211_TX_XRETRY) {
			dp_peer->peer_consecutive_xretries++;
#ifdef ATH_SUPPORT_QUICK_KICKOUT
			/* if the node is not a NAWDS repeater and failed count reaches
			 * a pre-defined limit, kick out the node
			 */
			if (((dp_peer->nawds) == 0) &&
			    (dp_peer->peer_consecutive_xretries >=
			     dp_vdev->vdev_sko_th)
			    && (!wlan_vdev_wnm_is_set(vdev))) {
				if (dp_vdev->vdev_sko_th != 0) {
					dp_peer->peer_consecutive_xretries = 0;
					wlan_kick_peer(peer);
				}
			}
#endif
			/*
			 * Force decrease the inactivity mark count of NAWDS node if
			 * consecutive tx fail count reaches a pre-defined limit.
			 * Stop tx to this NAWDS node until it activate again.
			 */
			if (dp_peer->nawds &&
			    (dp_peer->peer_consecutive_xretries) >= 5) {
				wlan_peer_set_inact(peer, 1);
			}
		} else {
			if (dp_peer->nawds &&
			    wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_HT)) {
				u_int16_t addba_status;
				/*
				 * NAWDS repeater just brought up so reset the ADDBA status
				 * so that AGGR could be enabled to enhance throughput
				 * 10 is taken from ADDBA_EXCHANGE_ATTEMPTS
				 */
				addba_status =
				    scn->sc_ops->addba_status(scn->sc_dev,
							      dp_peer->an,
							      tidno);
				if (addba_status
				    && (dp_peer->peer_consecutive_xretries) >=
				    10) {
					scn->sc_ops->addba_clear(scn->sc_dev,
								 dp_peer->an);
				}
			}

			if (!(ts->ts_flags & IEEE80211_TX_ERROR)) {
				/* node alive so reset the counts */
#if UMAC_SUPPORT_WNM
				wlan_wnm_bssmax_updaterx(peer, secured);
#endif
				dp_peer->peer_consecutive_xretries = 0;
			}
		}
	}
#endif /* defined(ATH_SUPPORT_QUICK_KICKOUT) || UMAC_SUPPORT_NAWDS */

#if ATH_SUPPORT_FLOWMAC_MODULE
	/* if this frame is getting completed due to lack of resources, do not
	 * try to wake the queue again.
	 */
	if ((ts->ts_flowmac_flags & IEEE80211_TX_FLOWMAC_DONE)
	    && dp_vdev->vdev_dev_stopped && dp_vdev->vdev_flowmac) {
		if (dp_vdev->vdev_evtable->wlan_pause_queue) {
			dp_vdev->vdev_evtable->
			    wlan_pause_queue(wlan_vdev_get_osifp(vdev), 0,
					     dp_vdev->vdev_flowmac);
			dp_vdev->vdev_dev_stopped = 0;

		}
	}
#endif
#ifdef ATH_SUPPORT_QUICK_KICKOUT
	if ((wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_KICK_OUT_DEAUTH)
	     && qdf_atomic_read(&peer->peer_objmgr.ref_cnt) == 1))
		/* checking node count to one to make sure that no more packets
		   are buffered in hardware queue */
	{
		struct wlan_objmgr_peer *tmp_peer;
		u_int16_t associd;
		wlan_peer_mlme_flag_clear(peer, WLAN_PEER_F_KICK_OUT_DEAUTH);
		tmp_peer =
		    wlan_objmgr_peer_obj_create(vdev, WLAN_PEER_STA_TEMP,
						wlan_peer_get_macaddr(peer));
		if (tmp_peer != NULL) {
			associd = wlan_peer_get_associd(peer);
			QDF_TRACE( QDF_MODULE_ID_AUTH,
				       QDF_TRACE_LEVEL_DEBUG,
				       "%s: sending DEAUTH to %s, sta kickout reason %d\n",
				       __func__,
				       ether_sprintf(wlan_peer_get_macaddr
						     (tmp_peer)),
				       IEEE80211_REASON_KICK_OUT);
			wlan_send_deauth(tmp_peer, IEEE80211_REASON_KICK_OUT);
			/* claim node immediately */
			wlan_peer_delete(tmp_peer);
			wlan_vdev_deliver_event_mlme_deauth_indication(vdev,
								       wlan_peer_get_macaddr
								       (peer),
								       associd,
								       IEEE80211_REASON_KICK_OUT);
		}
	}
#endif
	wlan_objmgr_peer_release_ref(peer, WLAN_MLME_SB_ID);
}

/*
 * Encapsulate the frame into 802.11 frame format.
 * the frame has the 802.11 header already.
 */
void ieee80211_ibss2ap_header(struct ieee80211_frame *wh)
{
	unsigned char DA[IEEE80211_ADDR_LEN];
	unsigned char SA[IEEE80211_ADDR_LEN];
	unsigned char BSSID[IEEE80211_ADDR_LEN];
	int i;

	wh->i_fc[1] &= ~IEEE80211_FC1_DIR_MASK;
	wh->i_fc[1] |= IEEE80211_FC1_DIR_FROMDS;
	for (i = 0; i < IEEE80211_ADDR_LEN; i++) {
		DA[i] = wh->i_addr1[i];
		SA[i] = wh->i_addr2[i];
		BSSID[i] = wh->i_addr3[i];
	}
	for (i = 0; i < IEEE80211_ADDR_LEN; i++) {
		wh->i_addr1[i] = DA[i];
		wh->i_addr2[i] = BSSID[i];
		wh->i_addr3[i] = SA[i];
	}
}

#ifdef ATH_COALESCING
static wbuf_t
ieee80211_tx_coalescing(struct wlan_objmgr_peer *peer, wbuf_t wbuf)
{
	wbuf_t wbuf_new;
	int status = 0;
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);

	wbuf_new = wbuf_coalesce(dp_pdev->osdev, wbuf);
	if (wbuf_new == NULL) {
		QDF_TRACE( QDF_MODULE_ID_OUTPUT,
			       QDF_TRACE_LEVEL_DEBUG,
			       "%s: coalescing failed, stick with the old wbuf.\n",
			       __func__);
		return wbuf;
	}

	wbuf_set_priority(wbuf_new, wbuf_get_priority(wbuf));
	wbuf_set_exemption_type(wbuf_new, wbuf_get_exemption_type(wbuf));
	wbuf_set_peer(wbuf_new, peer);

	return wbuf_new;
}

#endif /* ATH_COALESCING */

static wbuf_t ieee80211_encap_80211(struct wlan_objmgr_peer *peer, wbuf_t wbuf)
{
	//QDF_STATUS  status;
	int key_mapping_key = 0;
	struct ieee80211_frame *wh;
	int type, subtype;
	int useqos = 0, use4addr, usecrypto = 0;
	int ibssaddr;
	int hdrsize, datalen, pad, addlen;	/* additional header length we want to append */
	int ac = wbuf_get_priority(wbuf);
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	int key_set = 0;
#else
	struct ieee80211_key *key = NULL;
#endif

	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct dadp_pdev *dp_pdev = NULL;
	struct dadp_vdev *dp_vdev = NULL;
	struct dadp_peer *dp_peer = NULL;

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

	dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		goto bad;
	}

	dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer) {
		qdf_print("%s:dp_peer is NULL ", __func__);
		goto bad;
	}
#ifdef ATH_COALESCING
	if (!wbuf_is_fastframe(wbuf)
	    && (dp_pdev->pdev_tx_coalescing == IEEE80211_COALESCING_ENABLED)
	    && (wbuf->wb_type != WBUF_TX_INTERNAL)) {
		wbuf = ieee80211_tx_coalescing(peer, wbuf);
	}
#endif
	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
	use4addr = ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) ==
		    IEEE80211_FC1_DIR_DSTODS) ? 1 : 0;
	ibssaddr = ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) ==
		    IEEE80211_FC1_DIR_NODS) ? 1 : 0;
	if (use4addr)
		hdrsize = sizeof(struct ieee80211_frame_addr4);
	else
		hdrsize = sizeof(struct ieee80211_frame);

	if (dp_pdev->pdev_softap_enable && ibssaddr
	    && (type == IEEE80211_FC0_TYPE_DATA)) {
		ieee80211_ibss2ap_header(wh);
	}
	datalen = wbuf_get_pktlen(wbuf) - (hdrsize + sizeof(struct llc));	/* NB: w/o 802.11 header */

	if (!(wlan_vdev_mlme_feat_ext_cap_get(vdev, WLAN_VDEV_FEXT_SAFEMODE)) &&	/* safe mode disabled */
	    wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_PRIVACY) &&	/* crypto is on */
	    (type == IEEE80211_FC0_TYPE_DATA)) {	/* only for data frame */
		/*
		 * Find the key that would be used to encrypt the frame if the
		 * frame were to be encrypted. For unicast frame, search the
		 * matching key in the key mapping table first. If not found,
		 * used default key. For multicast frame, only use the default key.
		 */
		if (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE ||
		    !IEEE80211_IS_MULTICAST(wh->i_addr1) ||
		    ((wlan_vdev_mlme_get_opmode(vdev) == QDF_WDS_MODE) &&
		     wlan_vdev_mlme_feat_cap_get(vdev,
						 WLAN_VDEV_F_WDS_STATIC))) {
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
			key_set = ((dp_peer->keyix == WLAN_CRYPTO_KEYIX_NONE) ? 0 : 1);
#else
			key = dp_peer->key;
#endif
		}
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
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
			if (key_mapping_key)
				usecrypto = 1;
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
			if (!IEEE80211_IS_MULTICAST(wh->i_addr1))
				goto bad;
			else
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
	 *
	 * Have put EAPOL frame to TID 7, and we will flush the queue before sending EAPOL frame
	 * This bug is expected to not to happen.
	 * And wbuf_set_eapol has been called in wlan_vap_send.
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
		if (!(wbuf_is_eapol(wbuf)) && wlan_peer_get_bf_update_cv(peer)) {
			hdrsize += sizeof(struct ieee80211_htc);
		}
#endif

		/*
		 * XXX: we assume a QoS frame must come from ieee80211_encap_8023() function,
		 * meaning it's already padded. If OS sends a QoS frame (thus without padding),
		 * then it'll break.
		 */
		if (wlan_pdev_nif_feat_cap_get(pdev, WLAN_PDEV_F_DATAPAD)) {
			pad = roundup(hdrsize, sizeof(u_int32_t)) - hdrsize;
		}
	} else if (type == IEEE80211_FC0_TYPE_DATA &&
		   ((wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_QOS)) ||
		    IEEE80211_PEER_USEAMPDU(peer))) {
		useqos = 1;
		addlen += sizeof(struct ieee80211_qoscntl);
		/* For TxBF CV cache update add +HTC field */
#ifdef ATH_SUPPORT_TxBF
		if (!(wbuf_is_eapol(wbuf)) && wlan_peer_get_bf_update_cv(peer)) {
			addlen += sizeof(struct ieee80211_htc);
		}
#endif
	} else if (wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_SEND_80211) &&
		   type == IEEE80211_FC0_TYPE_DATA && use4addr) {

		/*
		 * If a non-QoS 4-addr frame comes from ieee80211_encap_8023() function,
		 * then it should be padded. Only need padding non-QoS 4-addr frames
		 * if OS sends it with 802.11 header already but without padding.
		 */
		addlen = roundup((hdrsize), sizeof(u_int32_t)) - hdrsize;
	}
#if ATH_WDS_WAR_UNENCRYPTED
	/* Owl 2.2 - WDS War - for non-encrypted QoS frames, add extra QoS Ctl field */
	if (use4addr && useqos && !usecrypto && IEEE80211_PEER_USEWDSWAR(peer)) {
		addlen += sizeof(struct ieee80211_qoscntl);
	}
#endif

	if (addlen) {
		if (wlan_pdev_nif_feat_cap_get(pdev, WLAN_PDEV_F_DATAPAD)) {
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
		}

		if (addlen) {
			struct ieee80211_frame *wh0;

			wh0 = wh;
			wh = (struct ieee80211_frame *)wbuf_push(wbuf, addlen);
			if (wh == NULL) {
				goto bad;
			}
			memmove(wh, wh0, hdrsize);
		}
	}

	if (useqos) {
		u_int8_t *qos;
		int tid;

		ac = wbuf_get_priority(wbuf);
		tid = wbuf_get_tid(wbuf);
		if (tid >= IEEE80211_TID_SIZE)
			goto bad;
		if (!use4addr)
			qos = ((struct ieee80211_qosframe *)wh)->i_qos;
		else
			qos = ((struct ieee80211_qosframe_addr4 *)wh)->i_qos;

		if ((dp_vdev->vdev_ccx_evtable
		     && dp_vdev->vdev_ccx_evtable->wlan_ccx_check_msdu_life)
		    && !(dp_vdev->vdev_ccx_evtable->
			 wlan_ccx_check_msdu_life(dp_vdev->vdev_ccx_arg,
						  tid))) {

			QDF_TRACE(
				       QDF_MODULE_ID_OUTPUT,
				       QDF_TRACE_LEVEL_DEBUG,
				       "%s, MSDU Lifetime Exceed discard QOS traffic\n",
				       __func__);
			goto bad;
		}

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

		if (dp_vdev->vdev_ccx_evtable
		    && dp_vdev->vdev_ccx_evtable->wlan_ccx_process_qos) {
			dp_vdev->vdev_ccx_evtable->
			    wlan_ccx_process_qos(dp_vdev->vdev_ccx_arg,
						 IEEE80211_TX_QOS_FRAME, tid);
		}
#ifdef ATH_SUPPORT_TxBF
		//IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,"==>%s:CV update\n",__func__);
		if (!(wbuf_is_eapol(wbuf))
		    && (wlan_peer_get_bf_update_cv(peer))) {

			wlan_request_cv_update(pdev, peer, wbuf, use4addr);
			/* clear flag */
			// ni->ni_bf_update_cv = 0;
		}
#endif

#if ATH_WDS_WAR_UNENCRYPTED
		/* Owl 2.2 WDS WAR - Extra QoS Ctrl field on non-encrypted frames */
		if (use4addr && !usecrypto && IEEE80211_PEER_USEWDSWAR(ni)) {
			struct ieee80211_qosframe_addr4 *qwh4 =
			    (typeof(qwh4)) wh;
			qwh4->i_wdswar[0] = qos[0];
			qwh4->i_wdswar[1] = qos[1];
		}
#endif
	} else {
		/* to avoid sw generated frame sequence the same as H/W generated frame,
		 * the value lower than min_sw_seq is reserved for HW generated frame */
		if (((wlan_peer_get_txseqs(peer, IEEE80211_NON_QOS_SEQ)) &
		     IEEE80211_SEQ_MASK) < MIN_SW_SEQ) {
			wlan_peer_set_txseqs(peer, IEEE80211_NON_QOS_SEQ,
					     MIN_SW_SEQ);
		}
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

	/*
	 * Set the PM bit of data frames so that it matches the state the Power
	 * Management module and the AP agreed upon. Only for station UAPSD.
	 */
	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE) {
		if (!IEEE80211_IS_MULTICAST(wh->i_addr1)) {
			if (wlan_vdev_forced_sleep_is_set(vdev)) {
				wh->i_fc[1] |= IEEE80211_FC1_PWR_MGT;
				wbuf_set_pwrsaveframe(wbuf);
			}
		}
	}

/*
 * Update node TX stats for non  smart antenna training packets only
 */

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

/*
 * Encapsulate the frame into 802.11 frame format.
 * If an error is encountered NULL is returned. The caller is required
 * to provide a node reference.
 */
static INLINE wbuf_t
ieee80211_encap_8023(struct wlan_objmgr_peer *peer, wbuf_t wbuf)
{
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
#if UMAC_SUPPORT_WNM
	struct ip_header *ip;
	int isBroadcast;
#endif
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct dadp_peer *dp_peer = NULL;
	struct wlan_objmgr_peer *bsspeer = NULL;


	vdev = wlan_peer_get_vdev(peer);
	if (!vdev) {
		qdf_print("%s:vdev is NULL ", __func__);
		goto bad;
	}

	bsspeer = wlan_vdev_get_bsspeer(vdev);

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		qdf_print("%s:pdev is NULL ", __func__);
		goto bad;
	}

	dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer) {
		qdf_print("%s:dp_peer is NULL ", __func__);
		goto bad;
	}

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
	addhtc = ((wlan_peer_get_bf_update_cv(peer)) == 1);

	if (addhtc && !wbuf_is_eapol(wbuf)) {
		hdrsize = sizeof(struct ieee80211_qosframe_htc);
	} else if (addqos)
#else
	if (addqos)
#endif
		hdrsize = sizeof(struct ieee80211_qosframe);
	else
		hdrsize = sizeof(struct ieee80211_frame);

	isMulticast = (IEEE80211_IS_MULTICAST(eh.ether_dhost)) ? 1 : 0;
#if UMAC_SUPPORT_WNM
	isBroadcast = (IEEE80211_IS_BROADCAST(eh.ether_dhost)) ? 1 : 0;
#endif

	use4addr = 0;
	switch (wlan_vdev_mlme_get_opmode(vdev)) {
	case QDF_SAP_MODE:
#ifdef IEEE80211_WDS
		if (wbuf_is_wdsframe(wbuf)) {
			hdrsize += IEEE80211_ADDR_LEN;
			use4addr = 1;
		}
#endif
		if (isMulticast == 0)
			use4addr = wlan_wds_is4addr(vdev, eh, peer);
		if (dp_peer->nawds)
			use4addr = 1;

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
		if (isMulticast &&
		    (peer != wlan_vdev_get_bsspeer(vdev)) &&
		    (wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_WDS)))
			use4addr = 1;
#endif
		break;
	case QDF_STA_MODE:
		/*
		 * When forwarding a frame and 4-address support is
		 * enabled craft a 4-address frame.
		 */
		if ((!IEEE80211_ADDR_EQ
		     (eh.ether_shost, wlan_vdev_mlme_get_macaddr(vdev)))
		    && (wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_WDS))) {
			use4addr = 1;
			isMulticast =
			    IEEE80211_IS_MULTICAST(wlan_peer_get_macaddr(peer));
			/* add a wds entry to the station vap */
			if (IEEE80211_IS_MULTICAST(eh.ether_dhost)) {
				struct wlan_objmgr_peer *peer_wds = NULL;
				peer_wds =
				    wlan_find_wds_peer(vdev, eh.ether_shost);
				if (peer_wds) {
					wlan_set_wds_node_time(vdev,
							       eh.ether_shost);
					wlan_objmgr_peer_release_ref(peer_wds, WLAN_MLME_SB_ID);	/* Decr ref count */
				} else {
					wlan_add_wds_addr(vdev, peer,
							  eh.ether_shost,
							  IEEE80211_NODE_F_WDS_BEHIND);
				}
			}
		} else
			isMulticast =
			    IEEE80211_IS_MULTICAST(wlan_peer_get_bssid(peer));

		break;
	case QDF_WDS_MODE:
		hdrsize += IEEE80211_ADDR_LEN;
		use4addr = 1;
		break;
	default:
		break;
	}

	hdrsize = hdrsize + (use4addr ? IEEE80211_ADDR_LEN : 0);
	if (wlan_pdev_nif_feat_cap_get(pdev, WLAN_PDEV_F_DATAPAD)) {
		hdrspace = roundup(hdrsize, sizeof(u_int32_t));
	} else {
		hdrspace = hdrsize;
	}

	if (!is_amsdu && htons(eh.ether_type) >= IEEE8023_MAX_LEN) {
		int useBTEP = (eh.ether_type == htons(ETHERTYPE_AARP)) ||
		    (eh.ether_type == htons(ETHERTYPE_IPX));
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
		if (useBTEP) {
			llc->llc_snap.org_code[0] = BTEP_SNAP_ORGCODE_0;	/* 0x0 */
			llc->llc_snap.org_code[1] = BTEP_SNAP_ORGCODE_1;	/* 0x0 */
			llc->llc_snap.org_code[2] = BTEP_SNAP_ORGCODE_2;	/* 0xf8 */
		} else {
			llc->llc_snap.org_code[0] = RFC1042_SNAP_ORGCODE_0;	/* 0x0 */
			llc->llc_snap.org_code[1] = RFC1042_SNAP_ORGCODE_1;	/* 0x0 */
			llc->llc_snap.org_code[2] = RFC1042_SNAP_ORGCODE_2;	/* 0x0 */
		}
		llc->llc_snap.ether_type = eh.ether_type;
#if UMAC_SUPPORT_WNM
		ip = (struct ip_header *)(wbuf_header(wbuf) + hdrspace +
					  sizeof(struct llc));
#endif
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
#if UMAC_SUPPORT_WNM
		ip = (struct ip_header *)(wbuf_header(wbuf) +
					  sizeof(struct ether_header));
#endif
	}

	wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA;
	*(u_int16_t *) wh->i_dur = 0;
    /** WDS FIXME */
    /** clean up wds table whenever sending out a packet */

	if ((wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE)
	    && (wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_WDS))) {
		struct wlan_objmgr_peer *peer_wds = NULL;
		u_int32_t wds_age;

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
				wds_age =
				    wlan_find_wds_peer_age(vdev,
							   eh.ether_shost);
				if (wds_age > 2000) {
					wlan_remove_wds_addr(vdev,
							     eh.ether_shost,
							     IEEE80211_NODE_F_WDS_REMOTE);
				}
			}
			wlan_objmgr_peer_release_ref(peer_wds, WLAN_MLME_SB_ID);	/* Decr ref count */
		}
	}
	if ((wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE)
	    && (wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_WDS))) {
		struct wlan_objmgr_peer *peer_wds = NULL;

		peer_wds = wlan_find_wds_peer(vdev, eh.ether_dhost);
		/* Last call increments ref count if !NULL */
		if (peer_wds != NULL) {
			wlan_remove_wds_addr(vdev, eh.ether_dhost,
					     IEEE80211_PEER_F_WDS_BEHIND);
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
	} else {
		switch (wlan_vdev_mlme_get_opmode(vdev)) {
		case QDF_STA_MODE:
			if (peer == bsspeer) {
				wh->i_fc[1] = IEEE80211_FC1_DIR_TODS;
			} else {
				wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
			}
			IEEE80211_ADDR_COPY(wh->i_addr1,
					    wlan_peer_get_macaddr(peer));
			IEEE80211_ADDR_COPY(wh->i_addr2, eh.ether_shost);
			/* If next hop is the destination, then use bssid as
			   addr3 otherwise use the real destination as addr3 */
			if (IEEE80211_ADDR_EQ
			    (wlan_peer_get_macaddr(peer), eh.ether_dhost)) {
				IEEE80211_ADDR_COPY(wh->i_addr3,
						    wlan_peer_get_bssid
						    (bsspeer));
			} else {
				IEEE80211_ADDR_COPY(wh->i_addr3,
						    eh.ether_dhost);
			}
			break;
		case QDF_IBSS_MODE:
		case QDF_AHDEMO_MODE:
			wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
			IEEE80211_ADDR_COPY(wh->i_addr1, eh.ether_dhost);
			IEEE80211_ADDR_COPY(wh->i_addr2, eh.ether_shost);
			/*
			 * NB: always use the bssid from iv_bss as the
			 *     neighbor's may be stale after an ibss merge
			 */
			IEEE80211_ADDR_COPY(wh->i_addr3,
					    wlan_peer_get_bssid(bsspeer));
			break;
		case QDF_SAP_MODE:
			wh->i_fc[1] = IEEE80211_FC1_DIR_FROMDS;
			IEEE80211_ADDR_COPY(wh->i_addr1, eh.ether_dhost);
			IEEE80211_ADDR_COPY(wh->i_addr2,
					    wlan_peer_get_bssid(peer));
			IEEE80211_ADDR_COPY(wh->i_addr3, eh.ether_shost);
			if (wbuf_is_moredata(wbuf)) {
				wh->i_fc[1] |= IEEE80211_FC1_MORE_DATA;
			}
			break;
		case QDF_WDS_MODE:
			wh->i_fc[1] = IEEE80211_FC1_DIR_DSTODS;
			IEEE80211_ADDR_COPY(wh->i_addr1,
					    wlan_peer_get_macaddr(peer));
			IEEE80211_ADDR_COPY(wh->i_addr2,
					    wlan_vdev_mlme_get_macaddr(vdev));
			IEEE80211_ADDR_COPY(wh->i_addr3, eh.ether_dhost);
			IEEE80211_ADDR_COPY(IEEE80211_WH4(wh)->i_addr4,
					    eh.ether_shost);
			break;
		case QDF_MONITOR_MODE:
			goto bad;
		default:
			goto bad;
		}
	}

	if (addqos) {
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
		if (wlan_crypto_vdev_has_auth_mode(vdev, (1 << WLAN_CRYPTO_AUTH_RSNA))
		    || wlan_crypto_vdev_has_auth_mode(vdev, (1 << WLAN_CRYPTO_AUTH_WPA))) {
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
	if (eh.ether_type == htons(ETHERTYPE_PAE)) {
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

#if UMAC_SUPPORT_WNM
	if (wlan_vdev_wnm_is_set(vdev) && wlan_vdev_wnm_fms_is_set(vdev)) {
		if ((isMulticast == 1) && (isBroadcast == 0)
		    && (wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE)) {
			if (ip != NULL) {
				wlan_fms_filter(peer, wbuf, eh.ether_type, ip,
						hdrsize);
			}
		}
	}
#endif /* UMAC_SUPPORT_WNM */
	return ieee80211_encap_80211(peer, wbuf);

bad:
	/* complete the failed wbuf here */
	IEEE80211_TX_COMPLETE_WITH_ERROR(wbuf);
	return NULL;
}

#ifdef ATH_AMSDU

int ieee80211_80211frm_amsdu_check(wbuf_t wbuf)
{
	struct ieee80211_frame *wh;
	int iseapol, ismulticast, isip, use4addr;
	u_int16_t hdrsize;
	struct ip_header *ip = NULL;

	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	if (!(wh->i_fc[0] & IEEE80211_FC0_TYPE_DATA)) {
		return 1;
	}

	ismulticast = (IEEE80211_IS_MULTICAST(wh->i_addr1)) ? 1 : 0;
	iseapol =
	    (((struct llc *)&wh[1])->llc_snap.ether_type ==
	     htons(ETHERTYPE_PAE)) ? 1 : 0;
	if ((!ismulticast) && (!iseapol)) {
		isip =
		    (((struct llc *)&wh[1])->llc_snap.ether_type ==
		     htons(ETHERTYPE_IP)) ? 1 : 0;
		if (isip) {
			use4addr =
			    ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) ==
			     IEEE80211_FC1_DIR_DSTODS) ? 1 : 0;
			if (use4addr) {
				hdrsize = sizeof(struct ieee80211_frame_addr4);
			} else {
				hdrsize = sizeof(struct ieee80211_frame);
			}
			hdrsize += sizeof(struct llc);
			ip = (struct ip_header
			      *)(wbuf_get_scatteredbuf_header(wbuf, hdrsize));
			if (ip == NULL) {
				return 1;
			}

			if (ip->protocol == IP_PROTO_TCP) {
				return 0;
			}
		}
		return 1;
	}

	return (iseapol || ismulticast);

}

int ieee80211_8023frm_amsdu_check(wbuf_t wbuf)
{
	struct ether_header *eh;

	eh = (struct ether_header *)(wbuf_header(wbuf));
	if (eh->ether_type == htons(ETHERTYPE_IP)) {
		const struct ip_header *ip =
		    (struct ip_header *)((u_int8_t *) eh +
					 sizeof(struct ether_header));
		int len;
		wbuf_t xbuf;

		len = wbuf_get_datalen_temp(wbuf);
		if (len <
		    sizeof(struct ether_header) + sizeof(struct ip_header)) {
			// IP Header is not in the first buffer, if there is a 2nd buffer, assume it's in there.
			xbuf = wbuf_next_buf(wbuf);

			if (xbuf != NULL) {
				ip = (struct ip_header *)wbuf_raw_data(xbuf);
			} else {
				return 1;
			}
		}

		if (ip->protocol == IP_PROTO_TCP) {
			return 0;
		}
	}
	return 1;
}

/*
 * Determine if this frame is not data, or is a multicast or EAPOL.
 * We do not send them in an AMSDU.
 */
int ieee80211_amsdu_check(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf)
{
	if (wbuf_is_uapsd(wbuf) || wbuf_is_moredata(wbuf)) {
		return 1;
	}
	if (wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_SEND_80211)) {
		return ieee80211_80211frm_amsdu_check(wbuf);
	} else {
		return ieee80211_8023frm_amsdu_check(wbuf);
	}
}

/*
 * Form AMSDU frames and encapsulate into 802.11 frame format.
 */
static void
ieee80211_80211frm_amsdu_encap(wbuf_t amsdu_wbuf,
			       wbuf_t wbuf, u_int16_t framelen, int append_wlan)
{
	struct ieee80211_frame *wh;
	struct ether_header *eh_inter;
	u_int16_t amsdu_len;
	int offset;
	u_int8_t *dest;
	int pad = 0, i, use4addr, hdrsize;
	/* Get WLAN header */
	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	use4addr = ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) ==
		    IEEE80211_FC1_DIR_DSTODS) ? 1 : 0;
	if (use4addr)
		hdrsize = sizeof(struct ieee80211_frame_addr4);
	else
		hdrsize = sizeof(struct ieee80211_frame);

	if (append_wlan) {
		/*
		 * Copy the original 802.11 header
		 */
		wbuf_init(amsdu_wbuf, AMSDU_BUFFER_HEADROOM);
		wbuf_push(amsdu_wbuf, hdrsize);
		OS_MEMCPY(wbuf_header(amsdu_wbuf), wh, hdrsize);
	} else {
		/* If the length of former subframe is not round 4, has to pad it here,
		   or AP could not parse the following subframes.
		 */
		u_int8_t *buf = (u_int8_t *) wbuf_header(amsdu_wbuf);
		pad = (wbuf_get_pktlen(amsdu_wbuf) - hdrsize) % 4;

		for (i = 0; i < pad; i++) {
			buf[i + wbuf_get_pktlen(amsdu_wbuf)] = 0;
		}
		wbuf_append(amsdu_wbuf, pad);
	}

	/* Get the start location for this AMSDU */
	offset = wbuf_get_pktlen(amsdu_wbuf);

	/* Compute AMSDU length, i.e. header + MSDU */
	amsdu_len =
	    (u_int16_t) sizeof(struct ether_header) + framelen - hdrsize;

	eh_inter =
	    (struct ether_header *)((caddr_t) wbuf_header(amsdu_wbuf) + offset);
	/* Prepare AMSDU sub-frame header */
	/* ether_type in amsdu is actually the size of subframe */
	eh_inter->ether_type = htons((framelen - hdrsize));
	switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
	case IEEE80211_FC1_DIR_TODS:	/* STA->AP  */
		IEEE80211_ADDR_COPY(eh_inter->ether_dhost, wh->i_addr3);
		IEEE80211_ADDR_COPY(eh_inter->ether_shost, wh->i_addr2);
		break;
	case IEEE80211_FC1_DIR_NODS:	/* STA->STA */
		IEEE80211_ADDR_COPY(eh_inter->ether_dhost, wh->i_addr1);
		IEEE80211_ADDR_COPY(eh_inter->ether_shost, wh->i_addr2);
		break;

	case IEEE80211_FC1_DIR_FROMDS:	/* AP ->STA */
		IEEE80211_ADDR_COPY(eh_inter->ether_dhost, wh->i_addr1);
		IEEE80211_ADDR_COPY(eh_inter->ether_shost, wh->i_addr3);
		break;

	default:
		break;
#if 0
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_OUTPUT,
				  "%s: Wrong direction.\n", __func__);
		wbuf_free(wbuf_new);
		goto bad;
#endif
	}

	/* Fill in the AMSDU payload */
	dest =
	    (u_int8_t *) ((u_int8_t *) eh_inter + sizeof(struct ether_header));
	wbuf_copydata(wbuf, hdrsize, wbuf_get_pktlen(wbuf), dest);

	/* Update AMSDU buffer length */
	wbuf_append(amsdu_wbuf, amsdu_len);
}

/*
 * Form AMSDU frames and encapsulate into 80223 frame format.
 */
static void
ieee80211_8023frm_amsdu_encap(wbuf_t amsdu_wbuf,
			      wbuf_t wbuf, u_int16_t framelen, int append_wlan)
{
	struct ether_header *eh;
	struct llc *llc;
	struct ether_header *eh_inter;
	u_int16_t amsdu_len;
	int offset;
	u_int8_t *dest, *src;
	wbuf_t xbuf;
	u_int32_t len_to_copy, len_in_buf, bytes_copied, start_offset;

	/* Get the Ethernet header from the new packet */
	eh = (struct ether_header *)wbuf_header(wbuf);
	if (append_wlan) {
		/*
		 * Copy the original 802.11 header
		 */
		OS_MEMCPY(wbuf_header(amsdu_wbuf), eh,
			  sizeof(struct ether_header));
		wbuf_append(amsdu_wbuf, sizeof(struct ether_header));
	}
	/* Get the start location for this AMSDU */
	offset = wbuf_get_pktlen(amsdu_wbuf);

	/* Compute AMSDU length, i.e. header + MSDU */
	amsdu_len = framelen + LLC_SNAPFRAMELEN;

	eh_inter =
	    (struct ether_header *)((u_int8_t *) (wbuf_raw_data(amsdu_wbuf)) +
				    offset);

	/* Prepare AMSDU sub-frame header */
	OS_MEMCPY(eh_inter, eh,
		  sizeof(struct ether_header) - sizeof eh->ether_type);
	/* Length field reflects the size of MSDU only */
	eh_inter->ether_type = htons(amsdu_len - sizeof(struct ether_header));

	/* Prepare LLC header */
	llc =
	    (struct llc *)((u_int8_t *) eh_inter + sizeof(struct ether_header));
	llc->llc_dsap = llc->llc_ssap = LLC_SNAP_LSAP;
	llc->llc_control = LLC_UI;
	llc->llc_snap.org_code[0] = 0;
	llc->llc_snap.org_code[1] = 0;
	llc->llc_snap.org_code[2] = 0;
	llc->llc_snap.ether_type = eh->ether_type;

	/* Fill in the AMSDU payload */
	/* The payload in the original packet could be in multiple wbufs. We need to gather these into a single buffer. */
	/* Note that the ethernet header is already copied and changed to type 1. Need to start at Payload. */
	xbuf = wbuf;
	len_to_copy = wbuf_get_pktlen(wbuf);
	len_in_buf = wbuf_get_datalen_temp(wbuf);
	bytes_copied = sizeof(struct ether_header);
	start_offset = sizeof(struct ether_header);
	src =
	    (u_int8_t *) ((u_int8_t *) (wbuf_header(wbuf)) +
			  sizeof(struct ether_header));
	dest = (u_int8_t *) ((u_int8_t *) llc + sizeof(struct llc));
	while (bytes_copied < len_to_copy) {
		if (start_offset >= len_in_buf) {
			xbuf = wbuf_next_buf(xbuf);
			if (xbuf == NULL) {
				break;
			}
			src = (u_int8_t *) wbuf_header(xbuf);
			len_in_buf = wbuf_get_datalen_temp(xbuf);
			start_offset = 0;
		}
		OS_MEMCPY(dest, src, len_in_buf);
		dest += len_in_buf;
		start_offset += len_in_buf;
		bytes_copied += len_in_buf;
		src += len_in_buf;
	}

	/* Update AMSDU buffer length */
	wbuf_append(amsdu_wbuf, amsdu_len);
}

void ieee80211_amsdu_encap(struct wlan_objmgr_vdev *vdev,
			   wbuf_t amsdu_wbuf,
			   wbuf_t wbuf, u_int16_t framelen, int append_wlan)
{
	if (wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_SEND_80211)) {
		ieee80211_80211frm_amsdu_encap(amsdu_wbuf, wbuf, framelen,
					       append_wlan);
	} else {
		ieee80211_8023frm_amsdu_encap(amsdu_wbuf, wbuf, framelen,
					      append_wlan);
	}
}
#endif /* ATH_AMSDU */

static __inline uint64_t
READ_6(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5)
{
	uint32_t iv32 = (b0 << 0) | (b1 << 8) | (b2 << 16) | (b3 << 24);
	uint16_t iv16 = (b4 << 0) | (b5 << 8);
	return (((uint64_t) iv16) << 32) | iv32;
}

/*
 * Function to "adjust" keytsc based on corruption noticed in
 * tx complete
 */
void ieee80211_check_and_update_pn(wbuf_t wbuf)
{
	struct wlan_objmgr_peer *peer = wbuf_get_peer(wbuf);
	struct wlan_objmgr_vdev *vdev = NULL;
	u_int64_t sent_tsc;
	u_int64_t correct_tsc;
	u_int8_t *wbuf_offset;
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	struct wlan_crypto_key *key = NULL;
#else
	struct ieee80211_key *key = NULL;
#endif
	struct dadp_peer *dp_peer = NULL;

	if (qdf_nbuf_get_ext_cb(wbuf) == NULL) {
		return;
	}
	if (peer == NULL) {
		return;
	}

	dp_peer = wlan_get_dp_peer(peer);

	vdev = wlan_peer_get_vdev(peer);
	/* do this only for unicast traffic to connected stations */
	if (wlan_vdev_get_bsspeer(vdev) == peer) {
		return;
	}

	/* check if ccmp node - else just return */
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	if(((dadp_peer_get_key_type(peer) != WLAN_CRYPTO_CIPHER_TKIP)
	     && (dadp_peer_get_key_type(peer) !=
		 WLAN_CRYPTO_CIPHER_AES_CCM))) {
		return;
	}
#else
	key = dp_peer->key;
	if ((key == NULL)
	    || ((key->wk_cipher->ic_cipher != IEEE80211_CIPHER_TKIP)
		&& (key->wk_cipher->ic_cipher != IEEE80211_CIPHER_AES_CCM))) {
		return;
	}
#endif
	if (wbuf_get_cboffset(wbuf) == 0) {
		return;
	}
	wbuf_offset = wbuf_raw_data(wbuf) + wbuf_get_cboffset(wbuf);
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	if (wlan_crypto_peer_get_cipher_type(key) == WLAN_CRYPTO_CIPHER_TKIP) {
#else
	if (key->wk_cipher->ic_cipher == IEEE80211_CIPHER_TKIP) {
#endif

		sent_tsc =
		    READ_6(*(wbuf_offset + 2), *wbuf_offset, *(wbuf_offset + 4),
			   *(wbuf_offset + 5), *(wbuf_offset + 6),
			   *(wbuf_offset + 7));
	} else {
		sent_tsc =
		    READ_6(*wbuf_offset, *(wbuf_offset + 1), *(wbuf_offset + 4),
			   *(wbuf_offset + 5), *(wbuf_offset + 6),
			   *(wbuf_offset + 7));
	}

	/* check other bytes also */
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	if (sent_tsc > key->keytsc) {
		correct_tsc = key->keytsc;
		key->keytsc = sent_tsc;
		QDF_TRACE( QDF_MODULE_ID_CRYPTO,
			       QDF_TRACE_LEVEL_DEBUG,
			       "%s.. HW PN Corruption detected correct tsc %llu corrupted %llu\n",
			       __func__, correct_tsc, sent_tsc);
	}
#else
	if (sent_tsc > key->wk_keytsc) {
		correct_tsc = key->wk_keytsc;
		key->wk_keytsc = sent_tsc;
		QDF_TRACE( QDF_MODULE_ID_CRYPTO,
			       QDF_TRACE_LEVEL_DEBUG,
			       "%s.. HW PN Corruption detected correct tsc %llu corrupted %llu\n",
			       __func__, correct_tsc, sent_tsc);
	}
#endif
}

void ieee80211_me_hifi_forward(struct wlan_objmgr_vdev *vdev,
			       wbuf_t wbuf, struct wlan_objmgr_peer *peer)
{
	struct dadp_vdev *dp_vdev = NULL;

	dp_vdev = wlan_get_dp_vdev(vdev);

	if (ieee80211_classify(peer, wbuf)) {
		QDF_TRACE( QDF_MODULE_ID_IQUE,
			       QDF_TRACE_LEVEL_DEBUG,
			       "%s: discard, classification failure", __func__);
		wbuf_complete(wbuf);
		goto out;
	}

	if (dp_vdev->ique_ops.wlan_hbr_dropblocked) {
		if (dp_vdev->ique_ops.wlan_hbr_dropblocked(vdev, peer, wbuf)) {
			QDF_TRACE( QDF_MODULE_ID_IQUE,
				       QDF_TRACE_LEVEL_DEBUG,
				       "%s: packet dropped coz it blocks the headline\n",
				       __func__);
			goto out;
		}
	}

	if (!(wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_WDS))) {
		struct ether_header *eh =
		    (struct ether_header *)wbuf_header(wbuf);
		IEEE80211_ADDR_COPY(eh->ether_dhost,
				    wlan_peer_get_macaddr(peer));
	}
#ifdef IEEE80211_WDS
	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_WDS_MODE)
		wbuf_set_wdsframe(wbuf);
	else
		wbuf_clear_wdsframe(wbuf);
#endif

	wbuf_set_peer(wbuf, peer);
	wbuf_set_complete_handler(wbuf, NULL, NULL);
	ieee80211_vap_pause_update_xmit_stats(dp_vdev, wbuf);
	dadp_vdev_send_wbuf(vdev, peer, wbuf);

out:
	wlan_objmgr_peer_release_ref(peer, WLAN_MLME_SB_ID);
}

int
ieee80211_me_Convert(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf,
		     u_int8_t newmac[][IEEE80211_ADDR_LEN], uint8_t newmaccnt)
{
	struct wlan_objmgr_peer *peer = NULL;
	struct dadp_peer *dp_peer = NULL;
	struct ether_header *eh;
	u_int8_t *dstmac;	/* reference to frame dst addr */
	u_int8_t srcmac[IEEE80211_ADDR_LEN];	/* copy of original frame src addr */
	/* table of tunnel group dest mac addrs */
	u_int8_t empty_entry_mac[IEEE80211_ADDR_LEN];
	uint8_t newmacidx = 0;	/* local index into newmac */
	int xmited = 0;		/* Number of packets converted and xmited */
	struct ether_header *eh2;	/* eth hdr of tunnelled frame */
	struct athl2p_tunnel_hdr *tunHdr;	/* tunnel header */
	wbuf_t wbuf1 = NULL;	/* cloned wbuf if necessary */
	struct dadp_vdev *dp_vdev = NULL;

	dp_vdev = wlan_get_dp_vdev(vdev);
	eh = (struct ether_header *)wbuf_header(wbuf);
	dstmac = eh->ether_dhost;
	IEEE80211_ADDR_COPY(srcmac, eh->ether_shost);

	OS_MEMSET(empty_entry_mac, 0, IEEE80211_ADDR_LEN);
	wbuf1 = wbuf_copy(wbuf);
	if (wbuf1 != NULL) {
		wbuf_set_complete_handler(wbuf1, NULL, NULL);
	}
	wbuf_complete(wbuf);
	wbuf = wbuf1;
	wbuf1 = NULL;

	/* loop start */
	do {
		if (wbuf == NULL)
			goto bad;

		if (newmaccnt > 0) {
			/* reference new dst mac addr */
			dstmac = &newmac[newmacidx][0];

			/*
			 * Note - cloned here pre-tunnel due to the way ieee80211_classify()
			 * currently works. This is not efficient.  Better to split
			 * ieee80211_classify() functionality into per node and per frame,
			 * then clone the post-tunnelled copy.
			 * Currently, the priority must be determined based on original frame,
			 * before tunnelling.
			 */
			if (newmaccnt > 1) {
				wbuf1 = wbuf_copy(wbuf);
				if (wbuf1 != NULL) {
					wbuf_set_peer(wbuf1, peer);
					wbuf_clear_flags(wbuf1);
					wbuf_set_complete_handler(wbuf1, NULL,
								  NULL);
				}
			}
		} else {
			goto bad;
		}

		/* In case of loop */
		if (IEEE80211_ADDR_EQ(dstmac, srcmac)) {
			goto bad;
		}

		/* In case the entry is an empty one, it indicates that
		 * at least one STA joined the group and then left. For this
		 * case, if mc_discard_mcast is enabled, this mcast frame will
		 * be discarded to save the bandwidth for other ucast streaming
		 */
		if (IEEE80211_ADDR_EQ(dstmac, empty_entry_mac)) {
			if (newmaccnt > 1 || dp_vdev->mc_discard_mcast) {
				goto bad;
			} else {
				/*
				 * If empty entry AND not to discard the mcast frames,
				 * restore dstmac to the mcast address
				 */
				newmaccnt = 0;
				dstmac = eh->ether_dhost;
			}
		}

		/* Look up destination */
		peer = wlan_find_txpeer(vdev, dstmac);
		/* Drop frame if dest not found in tx table */
		if (peer == NULL) {
			goto bad2;
		}

		dp_peer = wlan_get_dp_peer(peer);
		if (dp_peer == NULL) {
			goto bad2;
		}

		/* Drop frame if dest sta not associated */
		if (wlan_peer_get_associd(peer) == 0
		    && peer != wlan_vdev_get_bsspeer(vdev)) {
			/* the node hasn't been associated */

			if (peer != NULL) {
				wlan_objmgr_peer_release_ref(peer,
							     WLAN_MLME_SB_ID);
			}
#if ATH_SUPPORT_ME_SNOOP_TABLE
			if (newmaccnt > 0) {
				dp_vdev->ique_ops.wlan_me_clean(peer);
			}
#endif
			goto bad;
		}

		/* calculate priority so drivers can find the tx queue */
		if (ieee80211_classify(peer, wbuf)) {
			QDF_TRACE( QDF_MODULE_ID_IQUE,
				       QDF_TRACE_LEVEL_DEBUG,
				       "%s: discard, classification failure",
				       __func__);

			goto bad2;
		}

		/* Insert tunnel header
		 * eh is the pointer to the ethernet header of the original frame.
		 * eh2 is the pointer to the ethernet header of the encapsulated frame.
		 *
		 */
		if (newmaccnt > 0 /*&& vap->iv_me->mc_mcast_enable */ ) {
			/*Option 1: Tunneling */
			if (dp_vdev->mc_mcast_enable & 1) {
				/* Encapsulation */
				tunHdr =
				    (struct athl2p_tunnel_hdr *)wbuf_push(wbuf,
									  sizeof
									  (struct
									   athl2p_tunnel_hdr));
				eh2 =
				    (struct ether_header *)wbuf_push(wbuf,
								     sizeof
								     (struct
								      ether_header));

				/* ATH_ETH_TYPE protocol subtype */
				tunHdr->proto = 17;

				/* copy original src addr */
				IEEE80211_ADDR_COPY(&eh2->ether_shost[0],
						    srcmac);

				/* copy new ethertype */
				eh2->ether_type = htons(ATH_ETH_TYPE);

			} else if (dp_vdev->mc_mcast_enable & 2) {	/*Option 2: Translating */
				eh2 = (struct ether_header *)wbuf_header(wbuf);
			} else {	/* no tunnel and no-translate, just multicast */
				eh2 = (struct ether_header *)wbuf_header(wbuf);
			}

			/* copy new dest addr */
			IEEE80211_ADDR_COPY(&eh2->ether_dhost[0],
					    &newmac[newmacidx][0]);

			/*
			 *  Headline block removal: if the state machine is in
			 *  BLOCKING or PROBING state, transmision of UDP data frames
			 *  are blocked untill swtiches back to ACTIVE state.
			 */
			if (dp_vdev->ique_ops.wlan_hbr_dropblocked) {
				if (dp_vdev->ique_ops.
				    wlan_hbr_dropblocked(vdev, peer, wbuf)) {
					QDF_TRACE(
						       QDF_MODULE_ID_IQUE,
						       QDF_TRACE_LEVEL_DEBUG,
						       "%s: packet dropped coz it blocks the headline\n",
						       __func__);
					goto bad2;
				}
			}
		}
		if (wlan_vdev_is_up(vdev) != QDF_STATUS_SUCCESS) {
			QDF_TRACE(
				       QDF_MODULE_ID_OUTPUT,
				       QDF_TRACE_LEVEL_DEBUG,
				       "%s: ignore data packet, vap is not active\n",
				       __func__);
			goto bad2;
		}
#ifdef IEEE80211_WDS
		if (wlan_vdev_mlme_get_opmode(vdev) == QDF_WDS_MODE)
			wbuf_set_wdsframe(wbuf);
		else
			wbuf_clear_wdsframe(wbuf);
#endif

		wbuf_set_peer(wbuf, peer);

		/* power-save checks */
		if ((!UAPSD_AC_ISDELIVERYENABLED
		     (wbuf_get_priority(wbuf), dp_peer))
		    && (wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_PAUSED))
		    && !(wlan_peer_mlme_flag_get(peer, WLAN_PEER_F_TEMP))) {
			wlan_peer_pause(peer);	/* pause it to make sure that no one else unpaused it after the node_is_paused check above, pause operation is ref counted */
			/* pause it to make sure that no one else unpaused it
			   after the node_is_paused check above,
			   pause operation is ref counted */
			QDF_TRACE(
				       QDF_MODULE_ID_OUTPUT,
				       QDF_TRACE_LEVEL_DEBUG,
				       "%s: could not send packet, STA (%s) powersave %d paused %d\n",
				       __func__,
				       ether_sprintf(wlan_peer_get_macaddr
						     (peer)),
				       (wlan_peer_mlme_flag_get
					(peer, WLAN_PEER_F_PWR_MGT)) ? 1 : 0,
				       wlan_peer_mlme_flag_get(peer,
							       WLAN_PEER_F_PAUSED));
			wlan_peer_saveq_queue(peer, wbuf,
					      IEEE80211_FC0_TYPE_DATA);
			wlan_peer_unpause(peer);	/* unpause it if we are the last one, the frame will be flushed out */
#if LMAC_SUPPORT_POWERSAVE_QUEUE
			ieee80211_vap_pause_update_xmit_stats(dp_vdev, wbuf);	/* update the stats for vap pause module */
			dadp_vdev_send_wbuf(vdev, peer, wbuf);
#endif
			/* unpause it if we are the last one, the frame will be flushed out */
		} else {
			ieee80211_vap_pause_update_xmit_stats(dp_vdev, wbuf);	/* update the stats for vap pause module */
			dadp_vdev_send_wbuf(vdev, peer, wbuf);
		}

		/* ieee80211_send_wbuf will increase refcnt for frame to be sent, so decrease refcnt here for the increase by find_txnode. */
		wlan_objmgr_peer_release_ref(peer, WLAN_MLME_SB_ID);
		goto loop_end;

bad2:
		if (peer != NULL) {
			wlan_objmgr_peer_release_ref(peer, WLAN_MLME_SB_ID);
		}
bad:
		if (wbuf != NULL) {
			wbuf_complete(wbuf);
		}

		if (IEEE80211_IS_MULTICAST(dstmac))
			dp_vdev->vdev_multicast_stats.ims_tx_discard++;
		else
			dp_vdev->vdev_unicast_stats.ims_tx_discard++;

loop_end:
		/* loop end */
		if (wbuf1 != NULL) {
			wbuf = wbuf1;
		}
		wbuf1 = NULL;
		newmacidx++;
		xmited++;
		if (newmaccnt)
			newmaccnt--;
	} while (newmaccnt > 0 && dp_vdev->mc_snoop_enable);
	return xmited;
}

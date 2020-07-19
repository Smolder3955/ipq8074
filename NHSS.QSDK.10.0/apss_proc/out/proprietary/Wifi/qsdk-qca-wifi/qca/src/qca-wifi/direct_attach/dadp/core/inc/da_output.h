/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
#include <if_athproto.h>
#include <if_athvar.h>
#include <wlan_son_pub.h>
#include <da_mat.h>

/* Internal */
static INLINE int ieee80211_send_wbuf_internal(struct wlan_objmgr_vdev *vdev,
					       wbuf_t wbuf);
static INLINE wbuf_t ieee80211_encap_8023(struct wlan_objmgr_peer *peer,
					  wbuf_t wbuf);
static wbuf_t ieee80211_encap_80211(struct wlan_objmgr_peer *peer, wbuf_t wbuf);
int wlan_vap_send(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf);
int ieee80211_classify(struct wlan_objmgr_peer *peer, wbuf_t wbuf);
void ieee80211_check_and_update_pn(wbuf_t wbuf);

/* External */
/* TxBF */
#ifdef ATH_SUPPORT_TxBF
void
ieee80211_tx_bf_completion_handler(struct ieee80211_node *ni,
				   struct ieee80211_tx_status *ts);
#endif

/* WIFIPOS */
extern void ieee80211_cts_done(bool txok);

#ifdef QCA_PARTNER_PLATFORM
extern int ath_pltfrm_vlan_tag_check(struct ieee80211vap *vap, wbuf_t wbuf);
#endif

#ifdef ATH_COALESCING
static wbuf_t ieee80211_tx_coalescing(struct ieee80211_node *ni, wbuf_t wbuf);
#endif

extern void ieee80211_nawds_learn_defer(struct ieee80211vap *vap,
					u_int8_t * mac);
extern void ieee80211_set_wds_node_time(struct ieee80211_node_table *nt,
					const u_int8_t * macaddr);

extern int dbdc_tx_process(wlan_if_t vap, osif_dev ** osdev,
			   struct sk_buff *skb);

#if ATH_SUPPORT_WRAP
osif_dev *wlan_osif_wrap_wdev_find(struct wlan_objmgr_pdev *pdev,
				   unsigned char *mac);

static inline void wrap_tx_bridge_da(struct wlan_objmgr_vdev **vdev,
				     osif_dev ** osdev, struct sk_buff **skb)
{
	/* Assuming native wifi or raw mode is not
	 * enabled in beeliner, to be revisted later
	 */
	struct ether_header *eh = (struct ether_header *)((*skb)->data);
	struct wlan_objmgr_vdev *prev_vdev = *vdev;
	osif_dev *tx_osdev, *prev_osdev = *osdev;
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(prev_vdev);

	/* Mpsta vdev here, find the correct tx vdev from the wrap common based on src address */
	tx_osdev = wlan_osif_wrap_wdev_find(pdev, eh->ether_shost);
	if (tx_osdev) {
		if (qdf_unlikely
		    ((IEEE80211_IS_MULTICAST(eh->ether_dhost)
		      || IEEE80211_IS_BROADCAST(eh->ether_dhost)))) {
			*skb = skb_unshare(*skb, GFP_ATOMIC);
		}
		/* since tx vdev gets changed , handle tx vdev synchorization */
		spin_unlock(&prev_osdev->tx_lock);
		*vdev = tx_osdev->ctrl_vdev;
		*osdev = tx_osdev;
		spin_lock(&((*osdev)->tx_lock));
	}
}

#define DA_WRAP_TX_PROCESS(_osdev, _vdev, _skb) \
{ \
    if (qdf_unlikely(wlan_is_vdev_mpsta(_vdev)))   { \
        wrap_tx_bridge_da (&_vdev, _osdev , _skb); \
        if (*(_skb) == NULL) {\
            goto bad;\
        }\
        if (wlan_is_vdev_psta(_vdev))   { \
            ath_wrap_mat_tx(_vdev, (wbuf_t)*_skb); \
        } \
    } \
    if (!((*_osdev)->is_up)) {\
        goto bad; \
   } \
}

#else
#define DA_WRAP_TX_PROCESS(_osdev, _vdev, _skb)
#endif /* ATH_SUPPORT_WRAP */

#if UMAC_SUPPORT_STAFWD
static INLINE int
ieee80211_apclient_fwd(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf)
{
	wlan_if_t vap;

	struct ether_header *eh;
	struct ieee80211com *ic = vap->iv_ic;

	eh = (struct ether_header *)wbuf_header(wbuf);

	/* if the vap is configured in 3 address forwarding
	 * mode, then clone the MAC address if necessary
	 */
	if ((vap->iv_opmode == IEEE80211_M_STA)
	    && ieee80211_vap_sta_fwd_is_set(vap)) {
		if (IEEE80211_ADDR_EQ(eh->ether_shost, vap->iv_my_hwaddr)) {
			/* Station is in forwarding mode - Drop the packets
			 * originated from station except EAPOL packets
			 */
			if (eh->ether_type == ETHERTYPE_PAE) {
				IEEE80211_ADDR_COPY(eh->ether_shost,
						    vap->iv_myaddr);
				return 1;
			} else {
				/* Drop the packet */
				IEEE80211_DPRINTF(vap, IEEE80211_MSG_OUTPUT,
						  "%s: drop data packet, vap is in sta_fwd\n",
						  __func__);
				return 0;
			}
		}
		if (!IEEE80211_ADDR_EQ(eh->ether_shost, vap->iv_myaddr)) {
			/* Clone the mac address with the mac address end-device */
			if (wlan_mlme_deauth_request((wlan_if_t) vap,
						     vap->iv_bss->ni_bssid,
						     IEEE80211_REASON_ASSOC_LEAVE)
			    != 0) {
				IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
						  "%s: vap is in sta_fwd , but unable to disconnect \n",
						  __func__);
			}
			// wlan_mlme_connection_reset(vap);
			/* Mac address is cloned - re-associate with the AP
			 * again
			 */

			IEEE80211_ADDR_COPY(vap->iv_myaddr, eh->ether_shost);
			IEEE80211_ADDR_COPY(ic->ic_myaddr, eh->ether_shost);
			ic->ic_set_macaddr(ic, eh->ether_shost);
			IEEE80211_DELIVER_EVENT_STA_CLONEMAC(vap);
			// packet to be dropped
			return 0;
		} else {
			return 1;
		}
	} else {
		return 1;
	}

}
#else
#define ieee80211_apclient_fwd(_vdev, _wbuf) (1)
#endif

/* DA specific components still in UMAC */
ieee80211_pwrsave_mode wlan_vdev_get_powersave(struct wlan_objmgr_vdev *vdev);
void wlan_vdev_sta_power_tx_start(struct wlan_objmgr_vdev *vdev);
void wlan_vdev_sta_power_tx_end(struct wlan_objmgr_vdev *vdev);

/*
 * Should be available through converged structures
 * pdev/vdev/peer in future
 */
bool wlan_is_vdev_mpsta(struct wlan_objmgr_vdev *vdev);
bool wlan_is_vdev_psta(struct wlan_objmgr_vdev *vdev);
struct wlan_objmgr_peer *wlan_find_txpeer(struct wlan_objmgr_vdev *vdev,
					  u_int8_t * macaddr);
u_int16_t wlan_get_aid(struct wlan_objmgr_peer *peer);

/* Functions common to partial OL and DA */
int wlan_dbdc_tx_process(struct wlan_objmgr_vdev *vdev, osif_dev ** osdev,
			 struct sk_buff *skb);

void wlan_vdev_set_tim(struct wlan_objmgr_peer *peer, int set,
		       bool isr_context);
u_int8_t wlan_pdev_get_wmep_acm(struct wlan_objmgr_pdev *pdev, int ac);
int wlan_vdev_get_curmode(struct wlan_objmgr_vdev *vdev);
void wlan_kick_peer(struct wlan_objmgr_peer *peer);
void wlan_peer_set_inact(struct wlan_objmgr_peer *peer, u_int32_t val);
void wlan_wnm_bssmax_updaterx(struct wlan_objmgr_peer *peer, int secured);
u_int16_t wlan_peer_get_associd(struct wlan_objmgr_peer *peer);
int wlan_send_deauth(struct wlan_objmgr_peer *peer, u_int16_t reason);
void wlan_vdev_deliver_event_mlme_deauth_indication(struct wlan_objmgr_vdev
						    *vdev, u_int8_t * macaddr,
						    u_int16_t associd,
						    u_int16_t reason);
void wlan_peer_set_txseqs(struct wlan_objmgr_peer *peer, int tid, int val);
u_int32_t wlan_vdev_forced_sleep_is_set(struct wlan_objmgr_vdev *vdev);
void wlan_set_wds_node_time(struct wlan_objmgr_vdev *vdev,
			    const u_int8_t * macaddr);
u_int32_t wlan_find_wds_peer_age(struct wlan_objmgr_vdev *vdev,
				 const u_int8_t * macaddr);
void wlan_fms_filter(struct wlan_objmgr_peer *peer, wbuf_t wbuf, int ether_type,
		     struct ip_header *ip, int hdrsize);
int wlan_vdev_wnm_fms_is_set(struct wlan_objmgr_vdev *vdev);
#ifdef ATH_SUPPORT_TxBF
void
wlan_tx_bf_completion_handler(struct wlan_objmgr_peer *peer,
			      struct ieee80211_tx_status *ts);
#endif
int
dadp_vdev_send_wbuf(struct wlan_objmgr_vdev *vdev,
		    struct wlan_objmgr_peer *peer, wbuf_t wbuf);

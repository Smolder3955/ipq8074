/*
 * Copyright (c) 2017-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */
#ifndef _WLAN_MLME_DP_DISPATCHER_H
#define _WLAN_MLME_DP_DISPATCHER_H

#include <wlan_pdev_mlme_api.h>
#include <wlan_vdev_mlme_api.h>
/*
 * mcast enhancement ops used by DA datapath
 */
struct wlan_vdev_ique_ops {
	/*
	 * functions for mcast enhancement
	 */
	int (*wlan_me_inspect) (struct wlan_objmgr_vdev *,
				struct wlan_objmgr_peer *, wbuf_t);
#if ATH_SUPPORT_ME_SNOOP_TABLE
	int (*wlan_me_convert) (struct wlan_objmgr_vdev *, wbuf_t);
#endif
	int (*wlan_me_hmmc_find) (struct wlan_objmgr_vdev *, u_int32_t);
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
	int (*wlan_me_hifi_convert) (struct wlan_objmgr_vdev *, wbuf_t);
#endif

	/*
	 * functions for headline block removal (HBR)
	 */
	int (*wlan_hbr_dropblocked) (struct wlan_objmgr_vdev *,
				     struct wlan_objmgr_peer *, wbuf_t);
	void (*wlan_me_clean) (struct wlan_objmgr_peer * peer);
};
struct dadp_ops {
	int (*vdev_me_Convert) (struct wlan_objmgr_vdev * vdev, wbuf_t wbuf,
				u_int8_t newmac[][IEEE80211_ADDR_LEN],
				uint8_t newmaccnt);
	void (*vdev_me_hifi_forward) (struct wlan_objmgr_vdev * vdev,
				      wbuf_t wbuf,
				      struct wlan_objmgr_peer * peer);
	void (*vdev_detach) (struct wlan_objmgr_vdev * vdev);
	void (*peer_detach) (struct wlan_objmgr_peer * peer);
	int (*ique_attach) (struct wlan_objmgr_vdev * vdev,
			    struct wlan_vdev_ique_ops * ique);

	/* VDEV */
	void (*vdev_set_qos_map) (struct wlan_objmgr_vdev * vdev, void *qos_map,
				  int len);
	void (*vdev_set_privacy_filters) (struct wlan_objmgr_vdev * vdev,
					  void *filters, u_int32_t num);
	void (*vdev_set_osif_event_handlers) (struct wlan_objmgr_vdev * vdev,
					      wlan_event_handler_table * evtab);
	void (*vdev_register_ccx_events) (struct wlan_objmgr_vdev * vdev,
					  os_if_t osif,
					  wlan_ccx_handler_table * evtab);
	void (*vdev_set_me_hifi_enable) (struct wlan_objmgr_vdev * vdev,
					 u_int32_t me_hifi_enable);
	void (*vdev_set_mc_snoop_enable) (struct wlan_objmgr_vdev * vdev,
					  u_int32_t mc_snoop_enable);
	void (*vdev_set_mc_mcast_enable) (struct wlan_objmgr_vdev * vdev,
					  u_int32_t mc_mcast_enable);
	void (*vdev_set_mc_discard_mcast) (struct wlan_objmgr_vdev * vdev,
					   u_int32_t mc_discard_mcast);
	void (*vdev_set_ampdu_subframes) (struct wlan_objmgr_vdev * vdev,
					    u_int32_t ampdu);
	void (*vdev_set_amsdu) (struct wlan_objmgr_vdev * vdev,u_int32_t amsdu);
	void (*vdev_set_smps) (struct wlan_objmgr_vdev * vdev, int val);
	void (*vdev_set_smartnet_enable) (struct wlan_objmgr_vdev * vdev,
					  u_int32_t val);
	void (*vdev_set_dscp_map_id) (struct wlan_objmgr_vdev * vdev,
				      u_int32_t val);
	void (*vdev_set_tspecActive) (struct wlan_objmgr_vdev * vdev,
				      u_int8_t val);
	 u_int64_t(*vdev_get_txrxbytes) (struct wlan_objmgr_vdev * vdev);
	void (*vdev_set_txrxbytes) (struct wlan_objmgr_vdev * vdev,
				    u_int64_t val);
	 systime_t(*vdev_get_last_ap_frame) (struct wlan_objmgr_vdev * vdev);
	 systime_t(*vdev_get_last_traffic_indication) (struct wlan_objmgr_vdev *
						       vdev);
	void (*vdev_set_priority_dscp_tid_map) (struct wlan_objmgr_vdev * vdev,
				       u_int8_t tid);
	void (*vdev_set_dscp_tid_map) (struct wlan_objmgr_vdev * vdev,
				       u_int8_t tos, u_int8_t tid);
	void (*vdev_set_frag_threshold) (struct wlan_objmgr_vdev * vdev,
					 u_int16_t fragthreshold);
	void (*vdev_set_rtsthreshold) (struct wlan_objmgr_vdev * vdev,
				       u_int16_t rtsthreshold);
	void (*vdev_set_sko_th) (struct wlan_objmgr_vdev * vdev,
				 u_int8_t sko_th);
	void (*vdev_set_userrate) (struct wlan_objmgr_vdev * vdev, int16_t val);
	void (*vdev_set_userretries) (struct wlan_objmgr_vdev * vdev,
				      int8_t val);
	void (*vdev_set_txchainmask) (struct wlan_objmgr_vdev * vdev,
				      int8_t val);
	void (*vdev_set_txpower) (struct wlan_objmgr_vdev * vdev, int8_t val);
	void (*vdev_get_txrx_activity) (struct wlan_objmgr_vdev * vdev,
					ieee80211_vap_activity * activity);
	void (*vdev_deliver_tx_event) (struct wlan_objmgr_peer * peer,
				       wbuf_t wbuf, struct ieee80211_frame * wh,
				       struct ieee80211_tx_status * ts);
	int (*vdev_send_wbuf) (struct wlan_objmgr_vdev * vdev,
			       struct wlan_objmgr_peer * peer, wbuf_t wbuf);
	int (*vdev_send) (struct wlan_objmgr_vdev * vdev, wbuf_t wbuf);
	int (*vdev_txrx_register_event_handler) (struct wlan_objmgr_vdev * vdev,
						 ieee80211_vap_txrx_event_handler
						 evhandler, void *arg,
						 u_int32_t event_filter);
	int (*vdev_txrx_unregister_event_handler) (struct wlan_objmgr_vdev *
						   vdev,
						   ieee80211_vap_txrx_event_handler
						   evhandler, void *arg);
	void (*vdev_txrx_deliver_event) (struct wlan_objmgr_vdev * vdev,
					 ieee80211_vap_txrx_event * evt);
	void (*vdev_set_mcast_rate) (struct wlan_objmgr_vdev * vdev,
				     int mcast_rate);
	void (*vdev_set_prot_mode) (struct wlan_objmgr_vdev * vdev,
				    u_int32_t val);

	/* PEER */
	void (*peer_update_uapsd_ac_trigena) (struct wlan_objmgr_peer * peer,
					      u_int8_t * uapsd_ac_trigena,
					      int len);
	void (*peer_update_uapsd_ac_delivena) (struct wlan_objmgr_peer * peer,
					       u_int8_t * uapsd_ac_delivena,
					       int len);
	void (*peer_update_uapsd_dyn_trigena) (struct wlan_objmgr_peer * peer,
					       int8_t * uapsd_dyn_trigena,
					       int len);
	void (*peer_update_uapsd_dyn_delivena) (struct wlan_objmgr_peer * peer,
						int8_t * uapsd_dyn_delivena,
						int len);

	void (*peer_set_nawds) (struct wlan_objmgr_peer * peer);
	void (*peer_set_minbasicrate) (struct wlan_objmgr_peer * peer,
				       u_int8_t minbasicrate);
	void (*peer_set_uapsd_maxsp) (struct wlan_objmgr_peer * peer,
				      u_int8_t uapsd_maxsp);
	u_int16_t (*peer_get_keyix)(struct wlan_objmgr_peer * peer);

	/* VDEV to PDEV  configs */
#if ATH_COALESCING
	void (*pdev_set_tx_coalescing) (struct wlan_objmgr_pdev * pdev,
					u_int32_t val);
#endif
	void (*pdev_set_minframesize) (struct wlan_objmgr_pdev * pdev,
				       u_int32_t val);
	void (*pdev_set_mon_vdev) (struct wlan_objmgr_pdev * pdev,
				   struct wlan_objmgr_vdev * vdev);
	void (*pdev_set_addba_mode) (struct wlan_objmgr_pdev * pdev,
				     u_int32_t val);
	void (*pdev_set_amsdu_max_size) (struct wlan_objmgr_pdev * pdev,
					 u_int32_t val);
	void (*pdev_set_athextcap) (struct wlan_objmgr_pdev * pdev,
				    u_int32_t val);
	void (*pdev_clear_athextcap) (struct wlan_objmgr_pdev * pdev,
				      u_int32_t val);
	void (*pdev_timeout_fragments) (struct wlan_objmgr_pdev * pdev,
					u_int32_t lifetime);
	void * (*pdev_get_extap_handle) (struct wlan_objmgr_pdev * pdev);
};

/*==================== DADP to UMAC ====================*/

/*==================== UMAC to DADP  ====================*/

void wlan_vdev_set_txrx_event_filter(struct wlan_objmgr_vdev *vdev,
				     u_int32_t txrx_event_filter);
void wlan_peer_update_uapsd_dyn_delivena(struct wlan_objmgr_peer *peer,
					 int8_t * uapsd_dyn_delivena, int len);
void wlan_peer_update_uapsd_dyn_trigena(struct wlan_objmgr_peer *peer,
					int8_t * uapsd_dyn_trigena, int len);
#if DA_SUPPORT
void wlan_vdev_register_osif_events(struct wlan_objmgr_vdev *vdev,
				    wlan_event_handler_table * evtab);
void wlan_dp_vdev_detach(struct wlan_objmgr_vdev *vdev);
void wlan_dp_peer_detach(struct wlan_objmgr_peer *peer);
int wlan_vdev_ique_attach(struct wlan_objmgr_vdev *vdev);
void wlan_vdev_set_me_hifi_enable(struct wlan_objmgr_vdev *vdev,
				  u_int32_t me_hifi_enable);
void wlan_vdev_set_mc_snoop_enable(struct wlan_objmgr_vdev *vdev,
				   u_int32_t mc_snoop_enable);
void wlan_vdev_set_mc_mcast_enable(struct wlan_objmgr_vdev *vdev,
				   u_int32_t mc_mcast_enable);
void wlan_vdev_set_mc_discard_mcast(struct wlan_objmgr_vdev *vdev,
				    u_int32_t mc_discard_mcast);
void wlan_vdev_set_ampdu_subframes(struct wlan_objmgr_vdev *vdev,
				    u_int32_t ampdu);
void wlan_vdev_set_amsdu(struct wlan_objmgr_vdev *vdev, u_int32_t amsdu);
void wlan_vdev_set_dscp_map_id(struct wlan_objmgr_vdev *vdev,
			       u_int32_t dscp_map_id);
void wlan_peer_update_uapsd_ac_trigena(struct wlan_objmgr_peer *peer,
				       u_int8_t * uapsd_ac_trigena, int len);
void wlan_peer_update_uapsd_ac_delivena(struct wlan_objmgr_peer *peer,
					u_int8_t * uapsd_ac_delivena, int len);
void wlan_peer_set_uapsd_maxsp(struct wlan_objmgr_peer *peer,
			       u_int8_t uapsd_maxsp);
u_int16_t wlan_peer_get_keyix(struct wlan_objmgr_peer *peer);
void wlan_pdev_set_athextcap(struct wlan_objmgr_pdev *pdev, u_int32_t val);
void wlan_pdev_clear_athextcap(struct wlan_objmgr_pdev *pdev, u_int32_t val);
void wlan_pdev_timeout_fragments(struct wlan_objmgr_pdev *pdev,
				 u_int32_t lifetime);
void wlan_pdev_set_addba_mode(struct wlan_objmgr_pdev *pdev, u_int32_t val);
void wlan_pdev_set_amsdu_max_size(struct wlan_objmgr_pdev *pdev, u_int32_t val);
void wlan_pdev_set_mon_vdev(struct wlan_objmgr_pdev *pdev,
			    struct wlan_objmgr_vdev *vdev);
void wlan_vdev_set_frag_threshold(struct wlan_objmgr_vdev *vdev,
				  u_int16_t fragthreshold);
void wlan_vdev_set_priority_dscp_tid_map(struct wlan_objmgr_vdev *vdev,
				u_int8_t tid);
void wlan_vdev_set_rtsthreshold(struct wlan_objmgr_vdev *vdev,
				u_int16_t rtsthreshold);
void wlan_vdev_set_sko_th(struct wlan_objmgr_vdev *vdev, u_int8_t sko_th);
void wlan_vdev_set_smartnet_enable(struct wlan_objmgr_vdev *vdev,
				   u_int32_t val);
void wlan_vdev_register_ccx_event_handlers(struct wlan_objmgr_vdev *vdev,
					   os_if_t osif,
					   wlan_ccx_handler_table * evtab);
void wlan_vdev_update_last_traffic_indication(struct wlan_objmgr_vdev *vdev);
void wlan_vdev_update_last_ap_frame(struct wlan_objmgr_vdev *vdev);
void wlan_vdev_set_tspecActive(struct wlan_objmgr_vdev *vdev, u_int8_t val);
void wlan_peer_set_nawds(struct wlan_objmgr_peer *peer);
void wlan_vdev_get_txrx_activity(struct wlan_objmgr_vdev *vdev,
				 ieee80211_vap_activity * activity);
int wlan_vdev_send_wbuf(struct wlan_objmgr_vdev *vdev,
			struct wlan_objmgr_peer *peer, wbuf_t wbuf);
int wlan_vdev_send(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf);
void wlan_vdev_deliver_tx_event(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
				struct ieee80211_frame *wh,
				struct ieee80211_tx_status *ts);
int wlan_vdev_txrx_register_event_handler(struct wlan_objmgr_vdev *vdev,
					  ieee80211_vap_txrx_event_handler
					  evhandler, void *arg,
					  u_int32_t event_filter);
int wlan_vdev_txrx_unregister_event_handler(struct wlan_objmgr_vdev *vdev,
					    ieee80211_vap_txrx_event_handler
					    evhandler, void *arg);
void wlan_vdev_txrx_deliver_event(struct wlan_objmgr_vdev *vdev,
				  ieee80211_vap_txrx_event * evt);
void wlan_vdev_set_mcast_rate(struct wlan_objmgr_vdev *vdev, int mcast_rate);
void wlan_vdev_set_prot_mode(struct wlan_objmgr_vdev *vdev, u_int32_t val);
void wlan_peer_set_minbasicrate(struct wlan_objmgr_peer *peer,
				u_int8_t minbasicrate);
#ifdef ATH_COALESCING
void wlan_pdev_set_tx_coalescing(struct wlan_objmgr_pdev *pdev, u_int32_t val);
#endif
void *wlan_pdev_get_extap_handle(struct wlan_objmgr_pdev *pdev);
void wlan_pdev_set_minframesize(struct wlan_objmgr_pdev *pdev, u_int32_t val);
void wlan_vdev_set_privacy_filters(struct wlan_objmgr_vdev *vdev, void *filters,
				   u_int32_t num);
void wlan_vdev_set_qos_map(struct wlan_objmgr_vdev *vdev, void *qos_map,
			   int len);
void wlan_me_hifi_forward(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf,
			  struct wlan_objmgr_peer *peer);
#else
#define wlan_vdev_register_osif_events(vdev, evtab) /**/
#define wlan_dp_vdev_detach(vdev) /**/
#define wlan_dp_peer_detach(vdev) /**/
#define wlan_vdev_ique_attach(vdev) /**/
#define wlan_vdev_set_me_hifi_enable(vdev, me_hifi_enable) /**/
#define wlan_vdev_set_mc_snoop_enable(vdev, mc_snoop_enable) /**/
#define wlan_vdev_set_mc_mcast_enable(vdev, mc_mcast_enable) /**/
#define wlan_vdev_set_mc_discard_mcast(vdev, mc_discard_mcast) /**/
#define wlan_vdev_set_ampdu_subframes(vdev, ampdu) /**/
#define wlan_vdev_set_amsdu(vdev, amsdu) /**/
#define wlan_vdev_set_dscp_map_id(vdev, dscp_map_id) /**/
#define wlan_peer_update_uapsd_ac_trigena(peer, uapsd_ac_trigena, len) /**/
#define wlan_peer_update_uapsd_ac_delivena(peer, uapsd_ac_delivena, len) /**/ 
#define wlan_peer_set_uapsd_maxsp(peer, uapsd_maxsp) /**/
#define wlan_peer_get_keyix(peer) ((u_int16_t) -1)
#define wlan_pdev_set_athextcap(pdev, val) /**/
#define wlan_pdev_clear_athextcap(pdev, val) /**/
#define wlan_pdev_timeout_fragments(pdev, lifetime) /**/
#define wlan_pdev_set_addba_mode(pdev, val) /**/
#define wlan_pdev_set_amsdu_max_size(pdev, val) /**/
#define wlan_pdev_set_mon_vdev(pdev, vdev) /**/
#define wlan_vdev_set_frag_threshold(vdev, fragthreshold) /**/
#define wlan_vdev_set_priority_dscp_tid_map(vdev, tid) /**/
#define wlan_vdev_set_rtsthreshold(vdev, rtsthreshold) /**/
#define wlan_vdev_set_sko_th(vdev, sko_th) /**/
#define wlan_vdev_set_smartnet_enable(vdev, val) /**/
#define wlan_vdev_register_ccx_event_handlers(vdev, osif, evtab) /**/
#define wlan_vdev_update_last_traffic_indication(vdev) /**/
#define wlan_vdev_update_last_ap_frame(vdev) /**/
#define wlan_vdev_set_tspecActive(vdev, val) /**/
#define wlan_peer_set_nawds(peer) /**/
#define wlan_vdev_get_txrx_activity(vdev, activity) /**/
#define wlan_vdev_send_wbuf(vdev, peer, wbuf) /**/
#define wlan_vdev_send(vdev, wbuf) /**/
#define wlan_vdev_deliver_tx_event(peer, wbuf, wh, ts) /**/
#define wlan_vdev_txrx_register_event_handler(vdev, evhandler, arg, event_filter) /**/
#define wlan_vdev_txrx_unregister_event_handler(vdev, evhandler, arg) /**/
#define wlan_vdev_txrx_deliver_event(vdev, evt) /**/
#define wlan_vdev_set_mcast_rate(vdev, mcast_rate) /**/
#define wlan_vdev_set_prot_mode(vdev, val) /**/
#define wlan_peer_set_minbasicrate(peer, minbasicrate) /**/
#ifdef ATH_COALESCING
#define wlan_pdev_set_tx_coalescing(pdev, val) /**/
#endif
#define wlan_pdev_get_extap_handle(pdev) NULL
#define wlan_pdev_set_minframesize(pdev, val) /**/
#define wlan_vdev_set_privacy_filters(vdev, filters, num) /**/
#define wlan_vdev_set_qos_map(vdev, qos_map, len) /**/
#define wlan_me_hifi_forward(vdev, wbuf, peer) /**/
#endif
int wlan_me_Convert(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf,
		    u_int8_t newmac[][IEEE80211_ADDR_LEN], uint8_t newmaccnt);

static inline void *wlan_mlme_get_pdev_legacy_obj(struct wlan_objmgr_pdev *pdev)
{
   return wlan_pdev_mlme_get_ext_hdl(pdev);
}

static inline void *wlan_mlme_get_vdev_legacy_obj(struct wlan_objmgr_vdev *vdev)
{
   return wlan_vdev_mlme_get_ext_hdl(vdev);
}

static inline void *wlan_mlme_get_peer_legacy_obj(struct wlan_objmgr_peer *peer)
{
    return wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_MLME);
}
#endif

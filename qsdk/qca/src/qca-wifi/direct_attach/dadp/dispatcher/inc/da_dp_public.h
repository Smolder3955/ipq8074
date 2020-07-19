/*
 * Copyright (c) 2017-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include <wlan_pdev_mlme_api.h>
#include <wlan_vdev_mlme_api.h>

struct wlan_pdev_phy_stats {
	u_int64_t ips_tx_packets;	/* frames successfully transmitted */
	u_int64_t ips_tx_multicast;	/* multicast/broadcast frames successfully transmitted */
	u_int64_t ips_tx_fragments;	/* fragments successfully transmitted */
	u_int64_t ips_tx_xretries;	/* frames that are xretried. NB: not number of retries */
	u_int64_t ips_tx_retries;	/* frames transmitted after retries. NB: not number of retries */
	u_int64_t ips_tx_multiretries;	/* frames transmitted after more than one retry. */
	u_int64_t ips_rx_dup;	/* duplicated fragments */
	u_int64_t ips_rx_mdup;	/* multiple duplicated fragments */
	u_int64_t ips_rx_fragments;	/* fragments successfully received */
	u_int64_t ips_rx_packets;	/* frames successfully received */
	u_int64_t ips_rx_multicast;	/* multicast/broadcast frames successfully received */

};
void dadp_pdev_attach(struct wlan_objmgr_pdev *pdev,
		      struct ath_softc_net80211 *scn, osdev_t osdev);
void dadp_vdev_attach(struct wlan_objmgr_vdev *vdev);
void dadp_peer_attach(struct wlan_objmgr_peer *peer, void *an);

void dadp_pdev_detach(struct wlan_objmgr_pdev *pdev);
void dadp_vdev_detach(struct wlan_objmgr_vdev *vdev);
void dadp_peer_detach(struct wlan_objmgr_peer *peer);

/* Struct/vars set from umac and qca_da */
void wlan_peer_update_uapsd_dyn_delivena(struct wlan_objmgr_peer *peer,
					 int8_t * uapsd_dyn_delivena, int len);
void wlan_peer_update_uapsd_dyn_trigena(struct wlan_objmgr_peer *peer,
					int8_t * uapsd_dyn_trigena, int len);

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
void dadp_peer_set_key(struct wlan_objmgr_peer *peer,
		       struct ieee80211_key *key);
#endif
u_int16_t dadp_peer_get_keyix(struct wlan_objmgr_peer *peer);
u_int16_t dadp_peer_get_clearkeyix(struct wlan_objmgr_peer * peer);

struct wlan_pdev_phy_stats *dadp_pdev_get_phy_stats(struct wlan_objmgr_pdev
						    *pdev);
void dadp_pdev_set_dscp_hmmc_tid(struct wlan_objmgr_pdev *pdev, u_int8_t tid);
void dadp_pdev_set_dscp_igmp_tid(struct wlan_objmgr_pdev *pdev, u_int8_t tid);
void dadp_pdev_set_override_hmmc_dscp(struct wlan_objmgr_pdev *pdev,
				      u_int32_t val);
void dadp_pdev_set_override_igmp_dscp(struct wlan_objmgr_pdev *pdev,
				      u_int32_t val);
void dadp_pdev_set_override_dscp(struct wlan_objmgr_pdev *pdev, u_int32_t val);
void dadp_pdev_set_tid_override_queue_mapping(struct wlan_objmgr_pdev *pdev,
					      u_int32_t val);
void dadp_vdev_deliver_event_txq_empty(struct wlan_objmgr_vdev *vdev);
void dadp_vdev_set_psta(struct wlan_objmgr_vdev *vdev);
int dadp_vdev_is_psta(struct wlan_objmgr_vdev *vdev);
void dadp_vdev_set_mpsta(struct wlan_objmgr_vdev *vdev);
int dadp_vdev_is_mpsta(struct wlan_objmgr_vdev *vdev);
void dadp_vdev_set_wrap(struct wlan_objmgr_vdev *vdev);
int dadp_vdev_is_wrap(struct wlan_objmgr_vdev *vdev);
void dadp_vdev_set_use_mat(struct wlan_objmgr_vdev *vdev);
int dadp_vdev_use_mat(struct wlan_objmgr_vdev *vdev);
void dadp_vdev_set_mataddr(struct wlan_objmgr_vdev *vdev,
			   const u_int8_t * mataddr);
u_int8_t *dadp_vdev_get_mataddr(struct wlan_objmgr_vdev *vdev);
u_int8_t dadp_vdev_get_ampdu_subframes(struct wlan_objmgr_vdev *vdev);
u_int8_t dadp_vdev_get_amsdu(struct wlan_objmgr_vdev *vdev);
int ath_tx_prepare(struct wlan_objmgr_pdev *pdev, wbuf_t wbuf, int nextfraglen,
		   ieee80211_tx_control_t * txctl);
void dadp_vdev_set_flowmac(struct wlan_objmgr_vdev *vdev, int en);

int ath_amsdu_attach(struct wlan_objmgr_pdev *pdev);
void ath_amsdu_detach(struct wlan_objmgr_pdev *pdev);
int ath_amsdu_node_attach(struct wlan_objmgr_pdev *pdev,
			  struct wlan_objmgr_peer *peer);
void ath_amsdu_tx_drain(struct wlan_objmgr_pdev *pdev);

void dadp_pdev_set_dropstaquery(struct wlan_objmgr_pdev *pdev, u_int32_t val);
void
ieee80211_update_stats(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf,
		       struct ieee80211_frame *wh, int type, int subtype,
		       struct ieee80211_tx_status *ts);
u_int16_t wlan_pdev_get_curmode(struct wlan_objmgr_pdev *pdev);
u_int16_t wlan_pdev_get_curchan_freq(struct wlan_objmgr_pdev *pdev);
struct ieee80211_ath_channel *wlan_pdev_get_curchan(struct wlan_objmgr_pdev
						    *pdev);
int dadp_input(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
	       struct ieee80211_rx_status *rs);
void ieee80211_check_and_update_pn(wbuf_t wbuf);
void dadp_pdev_set_enable_min_rssi(struct wlan_objmgr_pdev *pdev, u_int8_t val);
void dadp_pdev_set_min_rssi(struct wlan_objmgr_pdev *pdev, int val);

void dadp_vdev_set_keyix(struct wlan_objmgr_vdev *vdev, uint16_t keyix,
			 int id);
void dadp_vdev_set_clearkeyix(struct wlan_objmgr_vdev *vdev,
			      uint16_t clearkeyix, int id);
uint16_t dadp_vdev_get_clearkeyix(struct wlan_objmgr_vdev *vdev, int id);
void dadp_vdev_set_key_type(struct wlan_objmgr_vdev *vdev, uint8_t key_type,
			    int id);
uint16_t dadp_vdev_get_keyix(struct wlan_objmgr_vdev *vdev, int id);
uint16_t dadp_vdev_get_default_keyix(struct wlan_objmgr_vdev *vdev);
uint16_t dadp_vdev_get_psta_keyix(struct wlan_objmgr_vdev *vdev);
uint8_t dadp_vdev_get_key_type(struct wlan_objmgr_vdev *vdev, int id);
uint8_t dadp_vdev_get_key_header(struct wlan_objmgr_vdev *vdev, int id);
uint8_t dadp_vdev_get_key_trailer(struct wlan_objmgr_vdev *vdev, int id);
uint8_t dadp_vdev_get_key_miclen(struct wlan_objmgr_vdev *vdev, int id);
void dadp_vdev_set_default_keyix(struct wlan_objmgr_vdev *vdev,
				 uint16_t keyix);
void dadp_vdev_set_psta_keyix(struct wlan_objmgr_vdev *vdev,
			      uint16_t keyix);
void dadp_vdev_set_key_valid(struct wlan_objmgr_vdev *vdev, uint8_t valid,
			     int id);
void dadp_vdev_set_key_header(struct wlan_objmgr_vdev *vdev, uint8_t header,
			      int id);
void dadp_vdev_set_key_trailer(struct wlan_objmgr_vdev *vdev, uint8_t trailer,
			       int id);
void dadp_vdev_set_key_miclen(struct wlan_objmgr_vdev *vdev, uint8_t miclen,
			      int id);

void dadp_peer_set_keyix(struct wlan_objmgr_peer *peer, uint16_t keyix);
void dadp_peer_set_clearkeyix(struct wlan_objmgr_peer *peer,
			      uint16_t keyix);
void dadp_peer_set_key_type(struct wlan_objmgr_peer *peer, uint8_t key_type);
uint8_t dadp_vdev_get_key_valid(struct wlan_objmgr_vdev *vdev, int id);
uint16_t dadp_peer_get_default_keyix(struct wlan_objmgr_peer *peer);
uint8_t dadp_peer_get_key_type(struct wlan_objmgr_peer *peer);
uint8_t dadp_peer_get_key_header(struct wlan_objmgr_peer *peer);
uint8_t dadp_peer_get_key_trailer(struct wlan_objmgr_peer *peer);
uint8_t dadp_peer_get_key_miclen(struct wlan_objmgr_peer *peer);
uint8_t dadp_peer_get_key_valid(struct wlan_objmgr_peer *peer);
void dadp_peer_set_default_keyix(struct wlan_objmgr_peer *peer,
				 uint16_t keyix);
void dadp_peer_set_key_valid(struct wlan_objmgr_peer *peer, uint8_t valid);
void dadp_peer_set_key_header(struct wlan_objmgr_peer *peer, uint8_t header);
void dadp_peer_set_key_trailer(struct wlan_objmgr_peer *peer, uint8_t trailer);
void dadp_peer_set_key_miclen(struct wlan_objmgr_peer *peer, uint8_t miclen);

static inline void *wlan_mlme_get_pdev_ext_hdl(struct wlan_objmgr_pdev *pdev)
{
   return wlan_pdev_mlme_get_ext_hdl(pdev);
}

static inline void *wlan_mlme_get_vdev_ext_hdl(struct wlan_objmgr_vdev *vdev)
{
   return wlan_vdev_mlme_get_ext_hdl(vdev);
}

static inline void *wlan_mlme_get_peer_ext_hdl(struct wlan_objmgr_peer *peer)
{
    return wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_MLME);
}
